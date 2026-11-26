# H27 receiver firmware study

The repository has a **partial Ghidra decompilation** in [`maincode_targeted.c`](maincode_targeted.c), not the original `maincode.bin`. See [`FIRMWARE_ANALYSIS.md`](FIRMWARE_ANALYSIS.md) for observations and [`FUNCTION_INVENTORY.md`](FUNCTION_INVENTORY.md) for the function inventory. Inferences from decompiled code require confirmation with the original binary.

An **independently written, host-testable MPEG-TS/PAT/PMT prototype** lives in [`re_workspace/`](re_workspace/README.md). It includes an optional, **non-bootable** MIPS diagnostic binary target. **It is not a flashable replacement.** Do not convert or flash the decompiled `.c`, diagnostic `.bin`, or an unverified concatenated flash image.

To try the prototype on Linux:

```sh
cd re_workspace/04_cleanroom
make -f Makefile.host all test
./build-host/inspect_ts /path/to/unencrypted/recording.ts
```

For boot/repacking blockers see [`re_workspace/04_cleanroom/BOOT_READINESS_GAP_REPORT.md`](re_workspace/04_cleanroom/BOOT_READINESS_GAP_REPORT.md).
