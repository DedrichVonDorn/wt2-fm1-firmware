#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Give a .fwsc package another model name / version (the identity M-VAVE's M-UPGRADE reads and compares with the
keyboard's: "SMK-37+VA_101"). The identity is one marker byte after each of the first 20 interleave blocks, outside
every CRC, so the flash image is untouched.

  rename_pkg.py IN.fwsc OUT.fwsc "SMK-37+VA_016"
"""
import sys
sys.path.insert(0, __file__.rsplit("/", 1)[0])
import fm1_install as f

def main(argv):
    if len(argv) != 4 or len(argv[3]) > f.BLOCKS:
        print(__doc__); return 2
    raw = bytearray(open(argv[1], "rb").read())
    old = f.product_of(raw)
    name = argv[3]
    for i in range(f.BLOCKS):
        raw[i * f.BLK + f.KEEP] = ((ord(name[i]) + i + 1) & 0xFF) if i < len(name) else 0x7D
    new = f.product_of(raw)
    img, n = f.deinterleave(bytes(raw))
    dmg = f.package_damage(img)
    if new != name or dmg:
        print("failed:", new, dmg); return 1
    open(argv[2], "wb").write(raw)
    print(f"{old} -> {new}  ({n} blocks, package checks ok)")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
