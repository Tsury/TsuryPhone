"""Build the firmware and install it on the phone over Wi-Fi, then verify it.

Reads TSURYPHONE_HOST (and TSURYPHONE_INPUTS, through scripts/generate.py) from the
environment or ~/.config/tsuryphone/private.env, and the OTA password from the 1Password
agent vault (item TSURYPHONE_OTA_ITEM, default "TsuryPhone OTA password"). Secrets stay in
memory and in the build's environment; nothing here prints secrets, the host or other
private values.

Exit status: 0 on a verified update, 1 on a failed or rolled-back update, 2 on bad inputs.
"""

import argparse
import base64
import hashlib
import http.client
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
PRIVATE_ENV = Path.home() / ".config" / "tsuryphone" / "private.env"
ENVIRONMENT = "releaseHA"
FIRMWARE = ROOT / ".pio" / "build" / ENVIRONMENT / "firmware.bin"
SLOT_LIMIT = 0x1E0000  # app0/app1 size in partitions.csv
API_PORT = 8080
DEFAULT_OTA_ITEM = "TsuryPhone OTA password"
POLL_SECONDS = 5


def fail(message: str, status: int = 1) -> None:
    print(f"error: {message}", file=sys.stderr)
    sys.exit(status)


def say(message: str) -> None:
    print(message, flush=True)


def private_value(name: str) -> str:
    value = os.environ.get(name, "")
    if not value and PRIVATE_ENV.is_file():
        for line in PRIVATE_ENV.read_text(encoding="utf-8").splitlines():
            key, _, rest = line.partition("=")
            if key.strip() == name:
                value = rest.strip().strip("'\"")
    return value


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def vault_password(item: str) -> str:
    result = subprocess.run(["op-agent", "read", item], capture_output=True, text=True)
    if result.returncode != 0:
        fail(f"could not read vault item {item!r} (op-agent exit {result.returncode})", 2)
    try:
        fields = json.loads(result.stdout).get("fields", [])
    except json.JSONDecodeError:
        fail(f"vault item {item!r} is not item JSON", 2)
    for field in fields:
        if field.get("purpose") == "PASSWORD" or field.get("id") in ("password", "credential"):
            value = field.get("value")
            if isinstance(value, str) and value:
                return value
    fail(f"vault item {item!r} has no password field", 2)
    return ""


class Phone:
    def __init__(self, host: str, password: str) -> None:
        self._host = host
        self._auth = base64.b64encode(f"tsuryphone:{password}".encode()).decode()

    def _request(self, method: str, path: str, body: bytes = b"", headers=None, timeout=10):
        connection = http.client.HTTPConnection(self._host, API_PORT, timeout=timeout)
        try:
            connection.request(method, path, body=body or None, headers=headers or {})
            response = connection.getresponse()
            return response.status, response.read()
        finally:
            connection.close()

    def diagnostics(self):
        """Return the diagnostics JSON, or None when the phone does not answer."""
        try:
            status, data = self._request("GET", "/api/diagnostics")
            return json.loads(data) if status == 200 else None
        except (OSError, http.client.HTTPException, json.JSONDecodeError):
            return None

    def upload(self, image: bytes):
        boundary = "tsuryphone-" + secrets.token_hex(12)
        body = b"".join(
            [
                f"--{boundary}\r\n".encode(),
                b'Content-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\n',
                b"Content-Type: application/octet-stream\r\n\r\n",
                image,
                f"\r\n--{boundary}--\r\n".encode(),
            ]
        )
        headers = {
            "Authorization": f"Basic {self._auth}",
            "Content-Type": f"multipart/form-data; boundary={boundary}",
        }
        try:
            status, data = self._request("POST", "/api/system/firmware", body, headers, timeout=180)
        except (OSError, http.client.HTTPException) as error:
            fail(f"upload failed: {type(error).__name__}")
        try:
            reply = json.loads(data)
            return status, str(reply.get("errorCode", "")), str(reply.get("message", ""))
        except (json.JSONDecodeError, AttributeError):
            return status, "", ""


def wait_until_idle(phone: Phone, deadline: float) -> dict:
    waiting_said = False
    while time.monotonic() < deadline:
        diag = phone.diagnostics()
        if diag is None:
            fail("the phone does not answer /api/diagnostics on port 8080")
        firmware = diag.get("firmware")
        if not isinstance(firmware, dict):
            fail(
                "the phone firmware has no update route; install this version through the "
                "first-install path once"
            )
        if not firmware.get("busy"):
            return diag
        if not waiting_said:
            say("waiting: a call is active or the phone is ringing")
            waiting_said = True
        time.sleep(POLL_SECONDS)
    fail("the phone stayed busy with a call; nothing was installed")
    return {}


def wait_for_result(phone: Phone, before: dict, expected: dict, deadline: float) -> None:
    before_uptime = int(before.get("uptimeMs") or 0)  # reported in seconds despite the name
    before_commit = before["firmware"].get("commit")
    restarted = False
    last = "no answer"
    while time.monotonic() < deadline:
        time.sleep(POLL_SECONDS)
        diag = phone.diagnostics()
        if diag is None:
            last = "no answer"
            continue
        firmware = diag.get("firmware") or {}
        if (
            diag.get("uptimeMs", before_uptime) < before_uptime
            or firmware.get("commit") != before_commit
        ):
            restarted = True
        if not restarted:
            last = "waiting for the restart"
            continue
        commit = firmware.get("commit")
        state = firmware.get("imageState")
        result = firmware.get("lastUpdateResult")
        last = f"commit {str(commit)[:12]}, image {state}, last update {result}"
        if firmware.get("rolledBack") or result == "rolled_back":
            fail(f"the new firmware failed its checks and the phone rolled back ({last})")
        if commit != expected["commit"]:
            continue  # The new image may still be starting or rolling back.
        if state != "valid" or result != "success":
            continue
        health = firmware.get("health") or {}
        unhealthy = [
            name
            for name in ("wifi", "networkRegistered", "homeAssistant")
            if health.get(name) is not True
        ]
        if firmware.get("version") != expected["version"]:
            fail(f"version mismatch: phone reports {firmware.get('version')!r}")
        if unhealthy:
            fail(f"installed, but health flags are not set: {', '.join(unhealthy)}")
        say(
            f"verified: version {expected['version']}, commit {commit[:12]}, "
            f"slot {firmware.get('slot')}, image valid, health ok"
        )
        return
    fail(f"no verified result before the deadline (last seen: {last})")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--host", help="bench device address instead of TSURYPHONE_HOST")
    parser.add_argument(
        "--bench-password-env",
        metavar="NAME",
        help="bench only: read a synthetic OTA password from this variable instead of the vault",
    )
    parser.add_argument(
        "--idle-timeout",
        type=int,
        default=1800,
        help="seconds to wait for no active call (default 1800)",
    )
    parser.add_argument(
        "--result-timeout",
        type=int,
        default=600,
        help="seconds to wait for the restart and checks (default 600)",
    )
    args = parser.parse_args()

    host = args.host or private_value("TSURYPHONE_HOST")
    if not host:
        fail(f"TSURYPHONE_HOST is not set in the environment or in {PRIVATE_ENV}", 2)
    if git("status", "--porcelain", "--untracked-files=no"):
        fail("the checkout has uncommitted changes; commit them so the commit names the source", 2)

    if args.bench_password_env:
        password = os.environ.get(args.bench_password_env, "")
        if not password:
            fail(f"{args.bench_password_env} is empty", 2)
    else:
        password = vault_password(private_value("TSURYPHONE_OTA_ITEM") or DEFAULT_OTA_ITEM)

    expected = {
        "commit": git("rev-parse", "HEAD"),
        "version": git("describe", "--tags", "--always"),
    }
    build_env = dict(os.environ)
    build_env["TSURYPHONE_OTA_PASSWORD_SHA256"] = hashlib.sha256(password.encode()).hexdigest()
    build_env["TSURYPHONE_FW_VERSION"] = expected["version"]

    say("checking private build inputs")
    if (
        subprocess.run(
            [sys.executable, "-B", "scripts/generate.py"], cwd=ROOT, env=build_env
        ).returncode
        != 0
    ):
        fail("private build inputs are missing or invalid", 2)
    say(f"building {ENVIRONMENT} {expected['version']} ({expected['commit'][:12]})")
    build = subprocess.run(
        ["pio", "run", "-s", "-e", ENVIRONMENT],
        cwd=ROOT,
        env=build_env,
        capture_output=True,
        text=True,
    )
    if build.returncode != 0:
        sys.stderr.write(build.stdout[-4000:] + build.stderr[-4000:])
        fail("firmware build failed")

    image = FIRMWARE.read_bytes()
    phone = Phone(host, password)
    diag = wait_until_idle(phone, time.monotonic() + args.idle_timeout)
    limit = min(SLOT_LIMIT, int(diag["firmware"].get("slotSize") or SLOT_LIMIT))
    if len(image) > limit:
        fail(f"image is {len(image)} bytes; the slot holds {limit}")
    say(f"image {len(image)} bytes fits the {limit}-byte slot")

    idle_deadline = time.monotonic() + args.idle_timeout
    while True:
        say("uploading")
        status, code, message = phone.upload(image)
        if status == 200:
            break
        if code == "OTA_BUSY":
            diag = wait_until_idle(phone, idle_deadline)
            continue
        if status == 401:
            fail("the phone rejected the OTA password")
        fail(f"upload refused (HTTP {status}): {message or 'no message'}")

    say("uploaded; waiting for the restart and the health checks")
    wait_for_result(phone, diag, expected, time.monotonic() + args.result_timeout)


if __name__ == "__main__":
    main()
