#!/usr/bin/env python3
"""Reconstruct a guest-VA -> PPC mnemonic map from rexglue's generated C++.

The recompiler emits one `// <mnemonic>` comment per guest instruction, in VA
order, with `loc_XXXXXXXX:` labels at branch targets. PPC instructions are a
fixed 4 bytes, so we anchor on each label's VA and increment by 4 per emitted
instruction. Lets us diff the actual instruction stream of two images by VA,
independent of how the analyzer split functions.

Usage: va_disasm.py <generated_dir> <start_hex> <end_hex>
"""
import os
import re
import sys

LABEL = re.compile(r"^\s*loc_([0-9A-Fa-f]{8}):")
DEFN = re.compile(r"DEFINE_REX_FUNC\(sub_([0-9A-Fa-f]{8})\)")
INSN = re.compile(r"^\s*// ([a-z][a-z0-9_.]*(?:\s.*)?)$")


def parse_dir(gen_dir):
    """Return {va: mnemonic} across all recomp files."""
    va_map = {}
    for name in sorted(os.listdir(gen_dir)):
        if not re.match(r".*recomp\.\d+\.cpp$", name):
            continue
        cur = None
        for line in open(os.path.join(gen_dir, name), encoding="utf-8", errors="replace"):
            m = DEFN.search(line)
            if m:
                cur = int(m.group(1), 16)
                continue
            m = LABEL.match(line)
            if m:
                cur = int(m.group(1), 16)
                continue
            m = INSN.match(line)
            if m and cur is not None:
                # Only record the first mnemonic seen at a VA (a few pseudo
                # expansions emit multiple C++ lines per instruction; the leading
                # comment is the real one and we advance once per comment).
                va_map.setdefault(cur, m.group(1).strip())
                cur += 4
    return va_map


def main():
    gen_dir, start, end = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
    va_map = parse_dir(gen_dir)
    for va in range(start, end, 4):
        print(f"0x{va:08X}  {va_map.get(va, '<none>')}")


if __name__ == "__main__":
    main()
