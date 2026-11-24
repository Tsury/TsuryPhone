"""Regenerate src/generated/ from the private build inputs in $TSURYPHONE_INPUTS."""

import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
PRIVATE_ENV = Path.home() / ".config" / "tsuryphone" / "private.env"
REQUIRED = [
    "phoneBook/pb.txt",
    "phoneBook/priority.txt",
    "phoneBook/blocked.txt",
    "mp3/numbers.txt",
    "mp3/mp3",
]


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    sys.exit(2)


def inputs_dir() -> Path:
    value = os.environ.get("TSURYPHONE_INPUTS", "")
    if not value and PRIVATE_ENV.is_file():
        for line in PRIVATE_ENV.read_text(encoding="utf-8").splitlines():
            key, _, rest = line.partition("=")
            if key.strip() == "TSURYPHONE_INPUTS":
                value = rest.strip().strip("'\"")
    if not value:
        fail(f"TSURYPHONE_INPUTS is not set in the environment or in {PRIVATE_ENV}")
    path = Path(value).expanduser()
    missing = [name for name in REQUIRED if not (path / name).exists()]
    if missing:
        fail(f"missing in $TSURYPHONE_INPUTS: {', '.join(missing)}")
    if not any(path.glob("mp3/mp3/*.mp3")):
        fail("no .mp3 files in $TSURYPHONE_INPUTS/mp3/mp3")
    return path


def run(label: str, command: list[str], verbose: bool) -> None:
    result = subprocess.run(command, cwd=ROOT, capture_output=not verbose, text=True)
    if result.returncode != 0:
        # Generator output can contain private numbers and names; show it only on request.
        fail(f"{label} generator failed (exit {result.returncode}); rerun with --verbose")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="show generator output, which can contain private numbers and names",
    )
    args = parser.parse_args()
    inputs = inputs_dir()
    out = ROOT / "src" / "generated"
    run(
        "phone book",
        [sys.executable, "phoneBook/generate.py", "--input-dir", str(inputs / "phoneBook"),
         "-o", str(out / "phoneBook.h")],
        args.verbose,
    )
    run(
        "mp3",
        [sys.executable, "mp3/generate.py", str(inputs / "mp3" / "mp3"),
         "--numbers", str(inputs / "mp3" / "numbers.txt"), "-o", str(out / "mp3.h")],
        args.verbose,
    )
    print(f"regenerated {out.relative_to(ROOT)}/ and mp3/generated/")


if __name__ == "__main__":
    main()
