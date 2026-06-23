#!/usr/bin/env python3
"""Map a vanilla data-global VA to its relocated address in the patched (TU) image.

Data sections relocate by a non-uniform delta (unlike a single code shift), so we
can't extrapolate from one global to another. Instead we work through the *code*
that loads each global: PPC materializes a 32-bit data address as a hi/lo pair,
`lis rX, HI` then an `addi/lwz/stw/lbz/... rY, LO(rX)`. We:

  1. find every vanilla site that computes the target address that way,
  2. locate the surrounding instructions in the TU stream (opcode-only match, so
     the relocated immediates don't break the search),
  3. re-decode the hi/lo pair at the TU site to read the relocated address.

A site whose enclosing code was recompiled won't match; try another site (popular
globals have many). Agreement across independent sites is the confidence signal.

Usage: find_data_reloc.py <van_gen> <tu_gen> <data_va_hex> [window]
"""
import os
import re
import sys

import find_reloc as fr  # parse_dir, norm

# `lis rT, IMM`  (IMM may be signed decimal or hex)
LIS = re.compile(r"^lis\s+(r\d+),\s*(-?\d+|0x[0-9a-fA-F]+)$")
# Low half: `addi rD,rA,IMM` or a load/store `op rD,IMM(rA)`.
ADDI = re.compile(r"^addi\s+(r\d+),(r\d+),\s*(-?\d+|0x[0-9a-fA-F]+)$")
MEM = re.compile(r"^[a-z]+\s+r\d+,\s*(-?\d+|0x[0-9a-fA-F]+)\((r\d+)\)$")


def imm(s):
    return int(s, 16) if s.lower().startswith("0x") else int(s)


def hi(v):  # value loaded by `lis rX, v` (low 16 bits << 16)
    return (v & 0xFFFF) << 16


def lo(v):  # sign-extended 16-bit
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def find_sites(va_map, target):
    """Return [(lo_va, reg)] vanilla sites whose hi/lo pair resolves to target."""
    vas = sorted(va_map)
    sites = []
    for i, va in enumerate(vas):
        m = LIS.match(va_map[va])
        if not m:
            continue
        reg, base = m.group(1), hi(imm(m.group(2)))
        # look a few instructions ahead for a low-half use of the same register
        for j in range(i + 1, min(i + 7, len(vas))):
            mn = va_map[vas[j]]
            a = ADDI.match(mn)
            if a and a.group(2) == reg and base + lo(imm(a.group(3))) == target:
                sites.append((vas[j], a.group(1)))
                break
            mem = MEM.match(mn)
            if mem and mem.group(2) == reg and base + lo(imm(mem.group(1))) == target:
                sites.append((vas[j], reg))
                break
            # register clobbered by another lis -> stop tracking
            mc = LIS.match(mn)
            if mc and mc.group(1) == reg:
                break
    return sites


def decode_at(va_map, lo_va):
    """Re-read the hi/lo pair ending at lo_va to get the resolved address."""
    mn = va_map.get(lo_va, "")
    a = ADDI.match(mn)
    if a:
        reg, low = a.group(2), lo(imm(a.group(3)))
    else:
        mem = MEM.match(mn)
        if not mem:
            return None
        reg, low = mem.group(2), lo(imm(mem.group(1)))
    # walk back for the lis of `reg`
    va = lo_va - 4
    for _ in range(8):
        m = LIS.match(va_map.get(va, ""))
        if m and m.group(1) == reg:
            return hi(imm(m.group(2))) + low
        va -= 4
    return None


def locate_in_tu(van, tu, lo_va, win=12):
    """Opcode-match a window centered on lo_va; return the TU VA of lo_va."""
    start = lo_va - win * 4 // 2
    sig = [fr.norm(van[v], "ops") for v in range(start, start + win * 4, 4) if v in van]
    if len(sig) < win:
        return []
    tu_vas = sorted(tu)
    tu_norm = [fr.norm(tu[v], "ops") for v in tu_vas]
    off = (lo_va - start) // 4
    hits = []
    n = len(sig)
    for i in range(len(tu_norm) - n + 1):
        if tu_norm[i:i + n] == sig:
            hits.append(tu_vas[i + off])
    return hits


def main():
    van_dir, tu_dir, target = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
    win = int(sys.argv[4]) if len(sys.argv) > 4 else 12
    van, tu = fr.parse_dir(van_dir), fr.parse_dir(tu_dir)

    sites = find_sites(van, target)
    print(f"data 0x{target:08X}: {len(sites)} vanilla load site(s)")
    results = {}
    for lo_va, _ in sites:
        tu_hits = locate_in_tu(van, tu, lo_va, win)
        for h in tu_hits:
            d = decode_at(tu, h)
            if d is not None:
                results.setdefault(d, []).append((lo_va, h))
    if not results:
        print("  no TU match (enclosing code recompiled?) — try another global/site")
        return
    for d, pairs in sorted(results.items(), key=lambda kv: -len(kv[1])):
        ex = ", ".join(f"0x{a:08X}->0x{b:08X}" for a, b in pairs[:3])
        print(f"  -> TU 0x{d:08X}  (delta +0x{d - target:X})  [{len(pairs)} site(s): {ex}]")


if __name__ == "__main__":
    main()
