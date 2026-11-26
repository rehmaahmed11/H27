# Receiver firmware independent-rewrite workspace

The repository contains `../maincode_targeted.c` (Ghidra-style decompiled text), **not** the original firmware bytes. The user reports a separate `maincode_decompressed_mips.bin` of **9,773,336 bytes**, nominal base `0x80000000`, entry `0x80000e00`; that binary is **not available here to verify**.

`04_cleanroom/` is a **new, independently written starter** informed by high-level behavior seen in the decompilation. It is not a translation, recompilation, or functional replacement for the vendor image. In particular, the existing diagnostic `cleanroom_mips.bin` build target is **NOT BOOTABLE OR SAFE TO FLASH**. A formal legal clean-room process would require separate spec and implementation teams; this is an independent rewrite, not a claim about legal provenance.

## What works today

- Pure C MPEG-TS packet parsing (188-byte packets, adaptation fields, continuity, bounds checks).
- Bounded PAT/PMT PSI reassembly across packets, MPEG-2 CRC-32, basic first-program video/audio PID selection.
- Host-native unit tests and `.ts` file inspector; no receiver hardware required.
- Optional freestanding MIPS32 **diagnostic** cross-build, not verified here because the MIPS toolchain and original binary are absent.

## Try the host prototype

```sh
cd re_workspace/04_cleanroom
make -f Makefile.host all test
./build-host/inspect_ts /path/to/unencrypted/recording.ts
```

See [`04_cleanroom/README.md`](04_cleanroom/README.md) for build notes and exact scope. Read [`04_cleanroom/BOOT_READINESS_GAP_REPORT.md`](04_cleanroom/BOOT_READINESS_GAP_REPORT.md) **before** considering any hardware boot or image packing.

## Reference and next input

- [`../FIRMWARE_ANALYSIS.md`](../FIRMWARE_ANALYSIS.md): observations from the partial decompilation (inferences must be validated against the original image).
- [`../FUNCTION_INVENTORY.md`](../FUNCTION_INVENTORY.md): function-level inventory.
- `tools/inspect_payload.py`: read-only size, hash and bytes-at-entry display; use once you have the *actual* decompressed payload, e.g. `python3 re_workspace/tools/inspect_payload.py /path/to/maincode_decompressed_mips.bin`.

**There is deliberately no flash-image repacker yet.** We cannot know partition offsets, compression, metadata, CRC, signature requirements, or loader ABI from decompiled text. Concatenating a bootloader, kernel and maincode or padding to the reported length would produce an unvalidated, potentially device-bricking image.
