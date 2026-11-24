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
python3 scripts/generate.py
pio run -e releaseHA
```

`scripts/generate.py` takes `TSURYPHONE_INPUTS` from the environment or from
`~/.config/tsuryphone/private.env`. Its `--verbose` output can contain private
values; keep that output out of issues, commits and logs.
