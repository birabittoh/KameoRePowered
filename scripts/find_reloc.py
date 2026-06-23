#!/usr/bin/env python3
"""Locate where a vanilla function relocated to in the patched (TU) image.

Title updates typically insert code and shift later functions to new VAs while
leaving the instruction logic intact. We build a guest-VA -> mnemonic map for
both images, normalize away absolute branch/call targets (which shift with
relocation), then search the TU stream for the vanilla function's normalized
instruction signature. A unique hit gives the relocated entry VA and delta.

Usage: find_reloc.py <van_gen> <tu_gen> <van_va_hex> [window]
"""
import os
import re
import sys

LABEL = re.compile(r"^\s*loc_([0-9A-Fa-f]{8}):")
DEFN = re.compile(r"DEFINE_REX_FUNC\(sub_([0-9A-Fa-f]{8})\)")
INSN = re.compile(r"^\s*// ([a-z][a-z0-9_.]*(?:\s.*)?)$")
HEXADDR = re.compile(r"0x[0-9a-fA-F]{6,8}")


def parse_dir(gen_dir):
    va_map = {}
    for name in sorted(os.listdir(gen_dir)):
        if not re.match(r".*recomp\.\d+\.cpp$", name):
            continue
        cur = None
        for line in open(os.path.join(gen_dir, name), encoding="utf-8", errors="replace"):
            m = DEFN.search(line)
            if m:
                cur = int(m.group(1), 16); continue
            m = LABEL.match(line)
            if m:
                cur = int(m.group(1), 16); continue
            m = INSN.match(line)
            if m and cur is not None:
                va_map.setdefault(cur, m.group(1).strip())
                cur += 4
    return va_map


def norm(mn):
    """Relocation-invariant form: drop absolute address operands."""
    return HEXADDR.sub("@", mn)


def contiguous(va_map, start, count):
    out = []
    va = start
    for _ in range(count):
        if va not in va_map:
            break
        out.append(va_map[va]); va += 4
    return out


def main():
    van_dir, tu_dir, van_va = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
    window = int(sys.argv[4]) if len(sys.argv) > 4 else 24

    van = parse_dir(van_dir)
    tu = parse_dir(tu_dir)

    sig_raw = contiguous(van, van_va, window)
    sig = [norm(x) for x in sig_raw]
    if len(sig) < window:
        print(f"warning: only {len(sig)} insns available at vanilla 0x{van_va:08X}")

    # Build ordered TU VA list and normalized stream.
    tu_vas = sorted(tu)
    tu_norm = [norm(tu[v]) for v in tu_vas]

    hits = []
    n = len(sig)
    for i in range(len(tu_norm) - n + 1):
        if tu_norm[i:i + n] == sig:
            hits.append(tu_vas[i])

    print(f"vanilla 0x{van_va:08X} signature ({n} insns):")
    for r in sig_raw[:8]:
        print(f"    {r}")
    print(f"  -> {len(hits)} exact hit(s): " + ", ".join(f"0x{h:08X}" for h in hits))
    for h in hits:
        print(f"     delta = 0x{h - van_va:X} ({h - van_va:+d})")


if __name__ == "__main__":
    main()
