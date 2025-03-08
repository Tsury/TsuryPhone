#!/usr/bin/env python3
import argparse
import os

def main():
    parser = argparse.ArgumentParser(description="Generate phoneBook.h from pb.txt")
    parser.add_argument("-o", "--output", required=True, help="Full path and filename for the generated file")
    args = parser.parse_args()

    with open("pb.txt", "r") as pb_file:
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

    header_content = generate_header(entries)

    output_path = args.output
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as out_file:
        out_file.write(header_content)
    print("Generated", output_path)

def generate_header(entries):
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

inline const char* getPhoneBookNumberForEntry(const char* entry) {{
    for (size_t i = 0; i < sizeof(phoneBookEntries)/sizeof(phoneBookEntries[0]); ++i) {{
        if (std::strcmp(phoneBookEntries[i].entry, entry) == 0) {{
            return phoneBookEntries[i].number;
        }}
    }}
    return nullptr;
}}
"""
    return header

if __name__ == "__main__":
    main()
