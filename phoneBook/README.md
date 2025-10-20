The generator consumes the files in this folder to seed the firmware's factory phone book.

## File structure

```
+972
211,0545689234,Amit
212,0522347784,Almog
213,0505479357,Dina
```

- The **first non-empty, non-comment line** defines the default dialing code. Only digits
  are stored; a leading `+` is optional.
- Each subsequent line is a quick-dial entry: `code,number[,name]`.
  - `code` is the handset quick-dial shortcut typed on the keypad.
  - `number` is the normalized destination phone number (digits only or with leading `0`).
  - `name` is optional. When provided it seeds the factory name shown in Home Assistant and on
    the device.
- Additional commas inside the name are supported; everything after the second comma is treated
  as part of the name.

Comments beginning with `#` and blank lines are ignored.

## Priority and blocked lists

- `priority.txt` lists callers that should always ring through. Put **one number per line**; the
  generator will normalize digits the same way it handles quick-dial entries.
- `blocked.txt` lists callers that should be rejected automatically; it follows the same format.
- Keep the files in sync with Home Assistant: the generator prevents conflicts (for example, a
  number that appears in both priority and blocked lists, or a blocked number that also has a
  quick-dial entry).

## Regenerating firmware assets

1. Edit `pb.txt`, `priority.txt`, and/or `blocked.txt`.
2. From the repository root run the generator:
   - On Windows: double-click `phoneBook/generate.bat`, or run it from PowerShell.
   - Cross-platform: `python phoneBook/generate.py --output src/generated`.
3. The script updates:
   - `src/generated/phoneBook.h`
   - `src/generated/priority_callers.h`
   - `src/generated/blocked_numbers.h`

The firmware picks up the new data on the next build/flash.
