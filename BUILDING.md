# Building the firmware

The firmware includes generated headers in `src/generated/` (phone book,
priority and blocked callers, call audio). Git ignores them and their inputs,
because the inputs hold private numbers, names and audio. Keep the inputs in one
private folder outside every checkout, laid out like the repository:

```
$TSURYPHONE_INPUTS/
  phoneBook/pb.txt
  phoneBook/priority.txt
  phoneBook/blocked.txt
  mp3/numbers.txt
  mp3/mp3/*.mp3
```

`phoneBook/README.md` and `mp3/README.md` describe the file formats.

## Build

```sh
uv tool install platformio   # once, if `pio` is missing
python3 scripts/generate.py
export TSURYPHONE_OTA_PASSWORD_SHA256=<sha256 of the OTA password>
export TSURYPHONE_SMS_WEBHOOK_ID=<SMS webhook ID> TSURYPHONE_SMS_ACK_KEY=<SMS ack key>
pio run -e releaseHA
```

`scripts/generate.py` takes `TSURYPHONE_INPUTS` from the environment or from
`~/.config/tsuryphone/private.env`. Its `--verbose` output can contain private
values; keep that output out of issues, commits and logs.

`scripts/build_flags.py` runs before every build. It compiles the build-time
secrets in from the environment and stops the build when one is missing:

- `TSURYPHONE_OTA_PASSWORD_SHA256`: lowercase hex SHA-256 of the OTA password.
- `TSURYPHONE_SMS_WEBHOOK_ID`: the SMS-only Home Assistant webhook ID.
- `TSURYPHONE_SMS_ACK_KEY`: the key Home Assistant sends with each SMS
  acknowledgement. `homeassistant/SMS_CONTRACT.md` describes the SMS contract.

Live builds use the 1Password agent vault values. Development and bench builds
use disposable synthetic values. Never publish build output. The script also
writes the version, source commit and build time to `src/generated/buildInfo.h`.

## Update the phone over Wi-Fi

```sh
python3 scripts/update_phone.py
```

The command reads `TSURYPHONE_HOST` from `~/.config/tsuryphone/private.env` and
the OTA password from the vault item `TSURYPHONE_OTA_ITEM` (default
`TsuryPhone OTA password`). It needs a clean checkout. It checks the private
inputs, builds `releaseHA`, checks that the image fits the app slot, waits until
no call is active, uploads, and waits until the phone reports the new version and
commit with a valid image and healthy flags. It exits non-zero on missing inputs
or secrets, a size overflow, a mismatch or a rollback. For a bench board, pass
`--host` and `--bench-password-env NAME`.

The phone accepts firmware only with the OTA password, as HTTP Basic auth with
any user name, on two paths: `POST /api/system/firmware` (multipart, port 8080)
and the Wi-Fi setup portal's `/update` page. It refuses uploads during a call or
while ringing. A new image must reach Wi-Fi, Home Assistant and SIM network
registration within 2 minutes of boot. Otherwise the phone restarts into the
previous version. A timer restarts it after 3 minutes even if start-up hangs.
`/api/diagnostics` reports the result under `firmware`.
