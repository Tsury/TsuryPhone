#!/usr/bin/env python3
import argparse
import os
import sys
from typing import List, Tuple

_FORMATTING_CHARS = {" ", "-", "(", ")", ".", "\t", "\r", "\n"}


def _strip_formatting(value: str) -> str:
    result: list[str] = []
    for index, char in enumerate(value):
        if char in _FORMATTING_CHARS:
            continue
        if char == "+":
            if not result:
                result.append(char)
            continue
        if char.isdigit():
            result.append(char)
    return "".join(result)


def _strip_leading_zeros(digits: str) -> str:
    stripped = digits.lstrip("0")
    if stripped:
        return stripped
    return digits if digits and digits.count("0") == len(digits) else ""


def _localize_with_default(digits: str, sanitized_code: str) -> str | None:
    """Convert international digits into local dialing format."""

    if not sanitized_code or not digits.startswith(sanitized_code):
        return None

    remainder = digits[len(sanitized_code) :]
    if not remainder:
        return None

    return remainder if remainder.startswith("0") else f"0{remainder}"


def strip_to_digits(value: str) -> str:
    """Return only digit characters from value."""
    return "".join(ch for ch in value if ch.isdigit())


def sanitize_default_dialing_code(value: str | None) -> str:
    if not value:
        return ""
    digits = strip_to_digits(str(value))
    if not digits:
        return ""
    sanitized = _strip_leading_zeros(digits)
    return sanitized


def normalize_phone_number(
    raw_number: str | None, default_dialing_code: str | None
) -> str:
    if raw_number is None:
        return ""

    trimmed = str(raw_number).strip()
    if not trimmed:
        return ""

    cleaned = _strip_formatting(trimmed)
    if not cleaned:
        return ""

    digits_only = strip_to_digits(cleaned)
    if not digits_only:
        return ""

    has_plus = cleaned.startswith("+")

    sanitized_code = sanitize_default_dialing_code(default_dialing_code or "")

    if has_plus:
        localized = _localize_with_default(digits_only, sanitized_code)
        return localized if localized else digits_only

    if digits_only.startswith("00") and len(digits_only) > 2:
        stripped = digits_only[2:]
        localized = _localize_with_default(stripped, sanitized_code)
        return localized if localized else digits_only

    localized = _localize_with_default(digits_only, sanitized_code)
    if localized:
        return localized

    if digits_only.startswith("0"):
        return digits_only

    if sanitized_code and len(digits_only) >= 7:
        return f"0{digits_only}"

    return digits_only


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PB_FILE = os.path.join(SCRIPT_DIR, "pb.txt")
PRIORITY_FILE = os.path.join(SCRIPT_DIR, "priority.txt")
BLOCKED_FILE = os.path.join(SCRIPT_DIR, "blocked.txt")


def escape_c_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def main():
    parser = argparse.ArgumentParser(description="Generate phoneBook.h from pb.txt")
    parser.add_argument(
        "-o",
        "--output",
        required=True,
        help="Full path and filename for the generated file",
    )
    args = parser.parse_args()

    # Always generate phoneBook.h (legacy behavior) and additionally
    # priority_callers.h + blocked_numbers.h in the SAME output directory.

    # Read phone book entries
    with open(PB_FILE, "r", encoding="utf-8") as pb_file:
        lines = pb_file.readlines()

    default_code = ""
    entries: List[Tuple[str, str, str]] = []

    for raw_line in lines:
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        if not default_code and "," not in line:
            default_code = sanitize_default_code_line(line)
            continue

        parts = [segment.strip() for segment in line.split(",")]
        if len(parts) < 2:
            print("Skipping invalid line:", line)
            continue

        code = parts[0]
        number = parts[1]
        name = ",".join(parts[2:]).strip() if len(parts) > 2 else ""

        if not code or not number:
            print("Skipping line with missing code or number:", line)
            continue

        normalized_number = normalize_phone_number(number, default_code)
        if not normalized_number:
            print("Skipping line with un-normalizable number:", line)
            continue

        entries.append((code, normalized_number, name))

    if not entries:
        print("No phone book entries found in", PB_FILE)
        sys.exit(1)

    phonebook_header_content = generate_phonebook_header(entries, default_code)

    output_path = (
        args.output
    )  # path to phoneBook.h provided by caller (batch file unchanged)
    out_dir = os.path.dirname(output_path)
    os.makedirs(out_dir, exist_ok=True)
    with open(output_path, "w") as out_file:
        out_file.write(phonebook_header_content)
    print("Generated", output_path)

    # Read priority & blocked lists (optional seed files)
    priority_numbers = read_number_list(PRIORITY_FILE, default_code)
    blocked_numbers = read_number_list(BLOCKED_FILE, default_code)

    # Conflict detection: intersection must be empty; if not, fail generation.
    conflict = set(priority_numbers) & set(blocked_numbers)
    if conflict:
        print(
            "ERROR: Numbers present in BOTH priority and blocked lists:",
            ", ".join(sorted(conflict)),
        )
        print("Generation aborted due to list conflict (policy: fail on conflict).")
        sys.exit(1)

    # Deduplicate while preserving order
    priority_numbers = dedupe(priority_numbers)
    blocked_numbers = dedupe(blocked_numbers)

    # Write additional headers
    priority_header_path = os.path.join(out_dir, "priority_callers.h")
    blocked_header_path = os.path.join(out_dir, "blocked_numbers.h")

    with open(priority_header_path, "w") as f:
        f.write(
            generate_simple_number_header(
                header_name="PRIORITY_CALLERS",
                array_name="priorityCallerNumbers",
                func_name="isPriorityCaller",
                numbers=priority_numbers,
            )
        )
    print("Generated", priority_header_path)

    with open(blocked_header_path, "w") as f:
        f.write(
            generate_simple_number_header(
                header_name="BLOCKED_NUMBERS",
                array_name="blockedNumbers",
                func_name="isBlockedNumber",
                numbers=blocked_numbers,
            )
        )
    print("Generated", blocked_header_path)


def dedupe(items):
    seen = set()
    result = []
    for x in items:
        if x not in seen:
            seen.add(x)
            result.append(x)
    return result


def read_number_list(path, default_code):
    if not os.path.exists(path):
        return []
    numbers = []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            normalized = normalize_phone_number(line, default_code)
            if not normalized:
                print(f"Skipping invalid number in {os.path.basename(path)}: {line}")
                continue
            numbers.append(normalized)
    return numbers


def generate_phonebook_header(entries, default_code):
    entries_lines = []
    for code, number, name in entries:
        entries_lines.append(
            '    { "%s", "%s", "%s" }'
            % (
                escape_c_string(code),
                escape_c_string(number),
                escape_c_string(name),
            )
        )
    entries_str = ",\n".join(entries_lines)

    sanitized_code = sanitize_default_dialing_code(default_code)
    prefix = f"+{sanitized_code}" if sanitized_code else ""

    header = f"""// This is a generated file. Do not edit manually.
#pragma once
#include <cstring>

struct PhoneBookEntry {{
    const char* entry;
    const char* number;
    const char* name;
}};

static constexpr const char kPhoneBookDefaultDialingCode[] = "{escape_c_string(sanitized_code)}";
static constexpr const char kPhoneBookDefaultDialingPrefix[] = "{escape_c_string(prefix)}";

static const PhoneBookEntry phoneBookEntries[] = {{
{entries_str}
}};

inline bool isPartialOfFullPhoneBookEntry(const char* number) {{
    for (size_t i = 0; i < sizeof(phoneBookEntries)/sizeof(phoneBookEntries[0]); ++i) {{
        size_t len = std::strlen(number);
        if (std::strncmp(phoneBookEntries[i].entry, number, len) == 0) {{
            return true;
        }}
    }}
    return false;
}}

inline bool isPhoneBookEntry(const char* number) {{
    for (size_t i = 0; i < sizeof(phoneBookEntries)/sizeof(phoneBookEntries[0]); ++i) {{
        if (std::strcmp(phoneBookEntries[i].entry, number) == 0) {{
            return true;
        }}
    }}
    return false;
}}
"""
    return header


def sanitize_default_code_line(value: str) -> str:
    sanitized = sanitize_default_dialing_code(value)
    if not sanitized:
        print("WARNING: Default dialing code line contains no digits; leaving empty")
    return sanitized


def generate_simple_number_header(header_name, array_name, func_name, numbers):
    # numbers: list[str]
    entries_lines = []
    for n in numbers:
        entries_lines.append(f'    "{n}"')
    entries_str = ",\n".join(entries_lines)
    header = f"""// This is a generated file. Do not edit manually.
#pragma once
#ifndef GENERATED_{header_name}_H
#define GENERATED_{header_name}_H

#include <cstring>

static const char* {array_name}[] = {{
{entries_str}
}};

inline bool {func_name}(const char* number) {{
    if (!number) return false;
    for (size_t i = 0; i < sizeof({array_name})/sizeof({array_name}[0]); ++i) {{
        if (std::strcmp({array_name}[i], number) == 0) {{
            return true;
        }}
    }}
    return false;
}}

inline size_t get{header_name.title().replace('_','')}Count() {{
    return sizeof({array_name})/sizeof({array_name}[0]);
}}

#endif // GENERATED_{header_name}_H
"""
    return header


if __name__ == "__main__":
    main()
