# Independent receiver maincode prototype (not flashable)

**Current milestone: M0, host-testable MPEG-TS packet + PAT/PMT parser.** The rest of the receiver is *not implemented*. This project does **not** turn `maincode_targeted.c` into a binary; it is newly written C that uses broad observations from the decompilation only to prioritize subsystems. Packet/PSI parsing follows ISO/IEC 13818-1, not vendor code.

## Host build (available in this workspace)

```sh
cd re_workspace/04_cleanroom
make -f Makefile.host all test
./build-host/inspect_ts /path/to/unencrypted/capture.ts
```

Host tests use CRC-checked example PAT+PMT sections, split packets, discontinuity cases, malformed lengths, corrupted CRC and arbitrary feed chunk sizes. The inspector selects **the first program**; a multi-program transport stream, encrypted PES, H.264 bitstream, private/descriptor-described audio, tuner and network are outside this milestone. A TS packet is exactly 188 bytes (no 192/204-byte autodetection). The parser makes no attempt to descramble.

## Optional MIPS cross-build: diagnostic **only**

First inspect the original image and verify its byte order/ISA and load conventions; the original binary is **not** present in this checkout. On a machine with a MIPS cross toolchain installed:

```sh
# Examples ONLY after confirming byte order (NOT a default):
make -f Makefile.mips ENDIAN=little CROSS=mipsel-linux-gnu-
# or
make -f Makefile.mips ENDIAN=big CROSS=mips-linux-gnu-
```

This emits `build-mips/cleanroom_mips.elf`, `cleanroom_mips.bin`, and a map. `../tools/check_mips_build.py` checks ELF32 MIPS, entry address, endian, blank prefix and single load segment. It **cannot** verify bootability. No MIPS compiler or MIPS objcopy is installed in the current environment, so the MIPS target has not been executed/tested here; the host parser has.

The linker *provisionally* maps at `0x80000000` with a **blank** 0xE00-byte prefix and `_start` at `0x80000e00`. These numbers are user-reported/decompiler-derived, **not independently verified**. `startup.S` only sets a stack and zeros its own BSS; it assumes a loader has initialized and copied executable memory. The `hal_read_ts()` implementation is a stub that returns `FW_ERR_NOTSUP`. There are no vectors, interrupts, tuner registers, DDR init, display/audio decoders, kernel APIs or flash metadata. **Do not flash, run on a receiver, or pass the `.bin` to an image packer.**

## File map

- `src/ts_demux.c` — self-contained transport/PSI parsing and validation.
- `tests/test_ts_demux.c` — host regression tests with CRC-checked example PSI.
- `host_main.c` — `.ts` inspector for Linux.
- `include/hal.h`, `src/hal_stub.c` — explicit unimplemented hardware boundary.
- `device_main.c`, `startup.S`, `linker.ld` — non-bootable MIPS diagnostic illustration.
- `Makefile.host`, `Makefile.mips` — independent build paths.

## Roadmap (each step needs measured hardware facts)

1. Obtain **original decompressed payload** and original complete update/flash layout; compare SHA-256, loader headers, compression, MIPS ISA/endian, MMIO map and RAM regions. Until then, no repacker.
2. Validate boot flow with a **non-destructive** emulator / UART debugging and board documentation; replace assumed stack/entry/exception layout with measured values.
3. Implement HAL interfaces for tuner/demod, transport DMA, RTOS, storage and codec; test each on host with mocks, then hardware. No CAS/entitlement bypass is part of this prototype.
4. Build higher-level DVB service selection and UI/network only after end-to-end clear-channel playback is proven.

See `REFERENCE_MAP.md` for a traceable feature-by-feature comparison with the decompilation. Read `BOOT_READINESS_GAP_REPORT.md` for blockers and safety rules.
