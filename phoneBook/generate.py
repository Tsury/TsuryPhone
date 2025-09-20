#!/usr/bin/env python3
import argparse
import os
import sys

PB_FILE = "pb.txt"
PRIORITY_FILE = "priority.txt"
BLOCKED_FILE = "blocked.txt"


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
    with open(PB_FILE, "r") as pb_file:
        lines = pb_file.readlines()

    entries = []
    for line in lines:
        line = line.strip()
        if not line:
            continue
        parts = line.split(",")
        if len(parts) != 2:
            print("Skipping invalid line:", line)
            continue
        entry = parts[0].strip()
        number = parts[1].strip()
        entries.append((entry, number))

    phonebook_header_content = generate_phonebook_header(entries)

    output_path = args.output  # path to phoneBook.h provided by caller (batch file unchanged)
    out_dir = os.path.dirname(output_path)
    os.makedirs(out_dir, exist_ok=True)
    with open(output_path, "w") as out_file:
        out_file.write(phonebook_header_content)
    print("Generated", output_path)

    # Read priority & blocked lists (optional seed files)
    priority_numbers = read_number_list(PRIORITY_FILE)
    blocked_numbers = read_number_list(BLOCKED_FILE)

    # Conflict detection: intersection must be empty; if not, fail generation.
    conflict = set(priority_numbers) & set(blocked_numbers)
    if conflict:
        print("ERROR: Numbers present in BOTH priority and blocked lists:", ", ".join(sorted(conflict)))
        print("Generation aborted due to list conflict (policy: fail on conflict).")
        sys.exit(1)

    # Deduplicate while preserving order
    priority_numbers = dedupe(priority_numbers)
    blocked_numbers = dedupe(blocked_numbers)

    # Write additional headers
    priority_header_path = os.path.join(out_dir, "priority_callers.h")
    blocked_header_path = os.path.join(out_dir, "blocked_numbers.h")

    with open(priority_header_path, "w") as f:
        f.write(generate_simple_number_header(
            header_name="PRIORITY_CALLERS", array_name="priorityCallerNumbers", func_name="isPriorityCaller", numbers=priority_numbers))
    print("Generated", priority_header_path)

    with open(blocked_header_path, "w") as f:
        f.write(generate_simple_number_header(
            header_name="BLOCKED_NUMBERS", array_name="blockedNumbers", func_name="isBlockedNumber", numbers=blocked_numbers))
    print("Generated", blocked_header_path)


def dedupe(items):
    seen = set()
    result = []
    for x in items:
        if x not in seen:
            seen.add(x)
            result.append(x)
    return result


def read_number_list(path):
    if not os.path.exists(path):
        return []
    numbers = []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # Treat as already normalized; accept as-is
            numbers.append(line)
    return numbers


def generate_phonebook_header(entries):
    entries_lines = []
    for entry, number in entries:
        entries_lines.append('    { "%s", "%s" }' % (entry, number))
    entries_str = ",\n".join(entries_lines)

    header = f"""// This is a generated file. Do not edit manually.
#pragma once
#include <cstring>

struct PhoneBookEntry {{
    const char* entry;
    const char* number;
}};

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
