# Boot-readiness and repacking gap (2026-11-26)

## Status: host prototype; MIPS binary is only a diagnostic output

The *actual* `maincode_decompressed_mips.bin`, full flash image, receiver model/board revision and bootloader behavior are **not in the repo**. Size **9,773,336 bytes**, base **`0x80000000`** and entry **`0x80000e00`** come from the user's note / partial decompilation and have not been checked against bytes. A decompiled function at `0x80000e00` is not proof of the loader's actual entry convention. Endianness is **unknown**. This MIPS target has not been compiled in this environment (no cross tools installed).

## Known missing dependencies for real boot

| Area | What must be measured / implemented |
| --- | --- |
| Image container | Real partition offsets, sizes, alignment, compression, checksums, signatures, bootloader verification. A decompressed payload may not be the on-flash partition. |
| CPU + memory | Actual MIPS ISA and byte order; CP0 setup, cache coherency, clocks/PLL, DDR, accessible RAM, stack and small-data pointer; exception vectors. The blank `0xE00` prefix is **not** a vector table. |
| Bootloader contract | Load address, decompression, entry calling convention, state of MMU/interrupts, board ID and fallback/rollback recovery. |
| RTOS/kernel | Initialization, tasks, scheduler, heaps, mutexes, timers, IPC, interrupt handlers and any calls between maincode and kernel. |
| Reception | Tuner and demod ICs, I2C/GPIO, LNB power/control, frontend lock, TS DMA ring and timing. `hal_read_ts` is deliberately a `FW_ERR_NOTSUP` stub. |
| Playback | TS routing to hardware demux, video/audio decode, display clocks, HDMI, OSD, subtitles and audio DAC. Our parser only identifies PIDs. |
| Storage/network | PVR filesystems, USB/Wi-Fi stack, network config, safe updates. |
| Testing | Board-specific emulator or documented UART/JTAG recovery, serial logs, memory/flash map and known-good image/restore procedure. |

**No usable repacker can be inferred just from a decompiled `.c` file or three partition names.** Naïve concatenation, arbitrary size padding, or overwriting a real flash partition risks a permanent boot failure. Do not distribute or flash the diagnostic output.

## Safest next inputs

1. Original (unaltered) decompressed payload: size, SHA-256, first 4 KB and 32 bytes at the proposed entry. `../tools/inspect_payload.py` reports these locally without changing the file.
2. Original vendor update file or full flash dump with redacted personal secrets, plus device model/SoC ID and board revision. Keep backups outside version control.
3. Trusted bootloader/flash-layout specification and recovery method. If a verified container has signatures, a working replacement needs the authorized signing toolchain; modifying the image without it will not boot.

Only after (1)–(3) should a layout-aware tool be designed with size/hash checks, in-place offsets, checksums, round-trip tests on a **copy**, and an explicit non-flashing output. The current project intentionally has no `pack_flash_image.py`.
