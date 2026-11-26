#!/usr/bin/env python3
"""Read-only inspection of a user's original decompressed maincode payload.

No firmware data is included in this repository. This script does NOT
identify the correct MIPS endianness or rebuild a flash update.
"""

import argparse
import hashlib
from pathlib import Path
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("--base", type=lambda s: int(s, 0), default=0x80000000)
    parser.add_argument("--entry", type=lambda s: int(s, 0), default=0x80000E00)
    args = parser.parse_args()
    try:
        data = args.payload.read_bytes()
    except OSError as err:
        print(f"cannot read payload: {err}", file=sys.stderr)
        return 1
    offset = args.entry - args.base
    print(f"file: {args.payload}")
    print(f"bytes: {len(data):,}")
    print(f"SHA-256: {hashlib.sha256(data).hexdigest()}")
    if offset < 0 or offset >= len(data):
        print("entry is outside this flat file; check base / load format", file=sys.stderr)
        return 1
    sample = data[offset : offset + 32]
    print(f"at entry offset 0x{offset:x}: {sample.hex(' ')}")
    if len(sample) >= 4:
        word = sample[:4]
        print(f"same four bytes interpreted little-endian: 0x{int.from_bytes(word, 'little'):08x}")
        print(f"same four bytes interpreted big-endian:    0x{int.from_bytes(word, 'big'):08x}")
    print("Verify endianness/ISA with disassembly or SoC documentation, not this output alone.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
