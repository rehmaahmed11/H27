#!/usr/bin/env python3
"""Reject obvious wrong-architecture / entry / flat-image mistakes.

This verifies a diagnostic build ONLY. It does not validate a flash header,
security signature, loader, SoC initialization, or hardware bootability.
"""

import argparse
from pathlib import Path
import struct
import sys

BASE = 0x80000000
ENTRY = 0x80000E00
MACHINE_MIPS = 8


def check(elf_path: Path, bin_path: Path, endian: str) -> None:
    elf = elf_path.read_bytes()
    image = bin_path.read_bytes()
    if len(elf) < 52 or elf[:4] != b"\x7fELF" or elf[4] != 1:
        raise ValueError("expected an ELF32 executable")
    expected_byte_order = 1 if endian == "little" else 2
    if elf[5] != expected_byte_order:
        raise ValueError(f"ELF endian is {elf[5]}, expected {expected_byte_order}")
    order = "<" if endian == "little" else ">"
    header = struct.unpack_from(order + "HHIIIIIHHHHHH", elf, 16)
    e_type, e_machine, _, entry, ph_offset, _, _, _, ph_size, ph_count, *_ = header
    if e_type != 2 or e_machine != MACHINE_MIPS:
        raise ValueError(f"not MIPS ELF32 EXEC (type={e_type}, machine={e_machine})")
    if entry != ENTRY:
        raise ValueError(f"ELF entry 0x{entry:08x} != expected 0x{ENTRY:08x}")
    if len(image) < ENTRY - BASE + 4:
        raise ValueError("flat image too short to contain entry instruction")
    if any(image[: ENTRY - BASE]):
        raise ValueError("reserved 0xE00-byte image prefix is not blank")
    if image[ENTRY - BASE : ENTRY - BASE + 4] in (b"\0" * 4, b"\xff" * 4):
        raise ValueError("entry word is empty; objcopy may have dropped the prefix")
    if ph_size != 32 or ph_count == 0 or ph_offset + ph_size * ph_count > len(elf):
        raise ValueError("invalid ELF32 program headers")
    loads = []
    for i in range(ph_count):
        p = struct.unpack_from(order + "IIIIIIII", elf, ph_offset + i * ph_size)
        if p[0] == 1:  # PT_LOAD
            _, offset, vaddr, _, file_size, _, _, _ = p
            if offset + file_size > len(elf):
                raise ValueError("invalid loadable segment size")
            loads.append((offset, vaddr, file_size))
    if len(loads) != 1 or loads[0][1] != BASE or loads[0][2] != len(image):
        raise ValueError("expected exactly one flat segment loaded at 0x80000000")
    start, _, length = loads[0]
    if elf[start : start + length] != image:
        raise ValueError("raw binary bytes do not match ELF's loadable segment")
    print(f"Diagnostic MIPS ELF/binary structure OK: {len(image)} bytes, "
          f"base=0x{BASE:08x}, entry=0x{ENTRY:08x}, {endian}-endian")
    print("NOT a validated receiver maincode or flashable update")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--endian", choices=("little", "big"), required=True)
    args = parser.parse_args()
    try:
        check(args.elf, args.binary, args.endian)
    except (OSError, ValueError) as error:
        print(f"MIPS diagnostic check failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
