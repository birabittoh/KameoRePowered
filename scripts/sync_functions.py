#!/usr/bin/env python3
"""Compare a known-good generated dir against the current one and patch the
config TOML so the normal (unpatched) SDK produces the same function set.

Usage:
    python scripts/sync_functions.py [--good DIR] [--bad DIR] [--config FILE] [--dry-run]

Defaults:
    --good   generated-good
    --bad    generated
    --config kameorepowered_config.toml
"""

import argparse
import os
import re
import sys


REGISTER_PATTERN = re.compile(r"SetFunction\((0x[0-9A-Fa-f]+),")
FUNCTIONS_HEADER = re.compile(r"^\[functions\]\s*$")
SECTION_HEADER = re.compile(r"^\[.*\]\s*$")
FUNC_ENTRY = re.compile(r"^(0x[0-9A-Fa-f]+)\s*=")


def extract_addresses(register_file):
    addrs = set()
    with open(register_file) as f:
        for line in f:
            m = REGISTER_PATTERN.search(line)
            if m:
                addrs.add(int(m.group(1), 16))
    return addrs


def find_register_file(directory):
    for name in os.listdir(directory):
        if name.endswith("_register.cpp"):
            return os.path.join(directory, name)
    print(f"error: no *_register.cpp found in {directory}", file=sys.stderr)
    sys.exit(1)


def read_config_addresses(config_path):
    addrs = set()
    in_functions = False
    with open(config_path) as f:
        for line in f:
            stripped = line.strip()
            if FUNCTIONS_HEADER.match(stripped):
                in_functions = True
                continue
            if SECTION_HEADER.match(stripped) and not FUNCTIONS_HEADER.match(stripped):
                in_functions = False
                continue
            if in_functions:
                m = FUNC_ENTRY.match(stripped)
                if m:
                    addrs.add(int(m.group(1), 16))
    return addrs


def patch_config(config_path, to_add, to_note):
    with open(config_path) as f:
        lines = f.readlines()

    last_func_line = None
    in_functions = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        if FUNCTIONS_HEADER.match(stripped):
            in_functions = True
            continue
        if SECTION_HEADER.match(stripped) and not FUNCTIONS_HEADER.match(stripped):
            in_functions = False
            continue
        if in_functions and FUNC_ENTRY.match(stripped):
            last_func_line = i

    if last_func_line is None:
        print("error: could not find [functions] entries in config", file=sys.stderr)
        sys.exit(1)

    new_lines = []
    if to_add:
        new_lines.append("\n# Gap-fill fixups: functions the SDK's gap-filler misses because it\n")
        new_lines.append("# incorrectly splits switch dispatchers at bctr.\n")
        for addr in sorted(to_add):
            new_lines.append(f"0x{addr:08X} = {{}}\n")

    insert_at = last_func_line + 1
    lines[insert_at:insert_at] = new_lines

    with open(config_path, "w") as f:
        f.writelines(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--good", default="generated-good",
                        help="Directory with the known-good generated code")
    parser.add_argument("--bad", default="generated",
                        help="Directory with the current (broken) generated code")
    parser.add_argument("--config", default="kameorepowered_config.toml",
                        help="Path to the codegen config TOML")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print what would be added without modifying the config")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(script_dir, ".."))
    os.chdir(root)

    good_addrs = extract_addresses(find_register_file(args.good))
    bad_addrs = extract_addresses(find_register_file(args.bad))
    config_addrs = read_config_addresses(args.config)

    missing = good_addrs - bad_addrs
    bogus = bad_addrs - good_addrs
    already_in_config = missing & config_addrs
    to_add = missing - config_addrs

    print(f"Good: {len(good_addrs)} functions")
    print(f"Bad:  {len(bad_addrs)} functions")
    print(f"Missing from bad SDK: {len(missing)}")
    if already_in_config:
        print(f"  Already in config:  {len(already_in_config)}")
    print(f"  To add:             {len(to_add)}")
    if bogus:
        print(f"Bogus splits in bad:  {len(bogus)}")
        for addr in sorted(bogus):
            print(f"  0x{addr:08X}")

    if not to_add:
        print("\nNothing to do — config is already up to date.")
        return

    if args.dry_run:
        print("\nWould add:")
        for addr in sorted(to_add):
            print(f"  0x{addr:08X} = {{}}")
        return

    patch_config(args.config, to_add, bogus)
    print(f"\nAdded {len(to_add)} function(s) to {args.config}.")
    print("Re-run codegen + build to verify.")


if __name__ == "__main__":
    main()
