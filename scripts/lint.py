"""Run project lint checks for TsuryPhone."""

import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
RUFF = [
    "uvx",
    "--from",
    "ruff==0.17.6",
    "ruff",
    "check",
    "--isolated",
    "--select",
    "E4,E7,E9,F,W",
]
CLANG_FORMAT = [
    "uvx",
    "--from",
    "clang-format==21.1.2",
    "clang-format",
    "--dry-run",
    "--Werror",
]
CLANG_TIDY = [
    "uvx",
    "--from",
    "clang-tidy==23.1.4",
    "clang-tidy",
    "--checks=-*,clang-diagnostic-*,clang-analyzer-*",
    "--config={}",
    "tests/modem_audio_queue.cpp",
    "--",
    "-std=c++14",
    "-Wall",
    "-Wextra",
    "-I",
    "src",
    "-I",
    "tests/fixtures/arduino",
]
HOST_CPP_INPUTS = {
    "src/common/ringBuffer.h",
    "tests/fixtures/arduino/Arduino.h",
    "tests/modem_audio_queue.cpp",
}


def repo_files(pattern: str) -> list[str]:
    output = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z", pattern],
        cwd=ROOT,
        text=True,
    )
    return sorted(path for path in output.split("\0") if path and (ROOT / path).is_file())


def normalize_paths(raw_paths: list[str]) -> list[str]:
    paths: list[str] = []
    for raw in raw_paths:
        candidate = Path(raw)
        resolved = (candidate if candidate.is_absolute() else ROOT / candidate).resolve()
        if resolved.is_file() and resolved.is_relative_to(ROOT):
            paths.append(resolved.relative_to(ROOT).as_posix())
    return sorted(set(paths))


def run_command(command: list[str]) -> int:
    return subprocess.run(command, cwd=ROOT, check=False).returncode


def check_json(paths: list[str]) -> int:
    failed = 0
    for rel in paths:
        try:
            json.loads((ROOT / rel).read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
            print(f"{rel}: invalid JSON: {error}", file=sys.stderr)
            failed = 1
    return failed


def main(argv: list[str]) -> int:
    failed = 0
    if not argv:
        py_files = repo_files("*.py")
        json_files = repo_files("*.json")
        if py_files:
            failed |= run_command([*RUFF, "--", *py_files])
        failed |= run_command(CLANG_TIDY)
        if json_files:
            failed |= check_json(json_files)
        return 1 if failed else 0

    selected = normalize_paths(argv)
    py_files = [path for path in selected if path.endswith(".py")]
    cpp_files = [path for path in selected if path.endswith((".cpp", ".h"))]
    json_files = [path for path in selected if path.endswith(".json")]

    if py_files:
        failed |= run_command([*RUFF, "--", *py_files])
    if cpp_files:
        failed |= run_command([*CLANG_FORMAT, *cpp_files])
        if HOST_CPP_INPUTS.intersection(cpp_files):
            failed |= run_command(CLANG_TIDY)
    if json_files:
        failed |= check_json(json_files)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
