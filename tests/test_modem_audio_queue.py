import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory


def main() -> int:
    tests = Path(__file__).resolve().parent
    with TemporaryDirectory() as temporary:
        executable = Path(temporary) / "modem_audio_queue"
        subprocess.run(
            [
                "c++",
                "-std=c++14",
                "-I",
                tests.parent / "src",
                "-I",
                tests / "fixtures/arduino",
                tests / "modem_audio_queue.cpp",
                "-o",
                executable,
            ],
            check=True,
        )
        return subprocess.run([executable], check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
