# What the decompilation informs — and what it does not

This is a **behavioral reference map**, not a translation of vendor code. Observations below come from the partial `../../maincode_targeted.c`; the independent parser in `src/ts_demux.c` was written from MPEG-2 TS/PSI rules. Function roles in a decompiler can be wrong: confirm against the original binary before connecting to hardware.

| Observed reference | Conservative takeaway | Independent implementation | Confidence / gap |
| --- | --- | --- | --- |
| `FUN_80000e00` (lowest exported function), user reports `0x80000000` base and `0x80000e00` entry | MIPS-like startup region exists | `startup.S` uses *provisional* entry only; no CP0/clock/DDR work copied | **Unverified:** original payload and bootloader absent; cannot infer flash mapping or load contract |
| `FUN_8001b8b4` and `TS_File_Decoder` strings | Transport-stream handling is a feature | Independent 188-byte TS parser; bounded PAT/PMT reassembly in `src/ts_demux.c` | **Partial:** not a clone of vendor dispatcher; vendor numeric states are not reused |
| `FUN_80212f58`, tuner / channel strings | Receiver manages service tables and channel changes | First-program PMT/video/audio PID selection, host-only inspector | **Partial:** no tuner, channel DB, DVB-S lock, DVB-SI/NIT/SDT/EIT |
| `FUN_8015401c`, H.264/H.265/MPEG strings | Video decode/playback exists | None | Requires codec SDK/driver; identifying PID is **not** decoding |
| `FUN_802ea15c`, smart-card/CAS strings | Conditional access exists | None | Deliberately no CAS/descrambling/entitlement bypass |
| `FUN_8037568c`, RTSP/network strings | Streaming clients exist | None | No TCP/IP, RTSP, IPTV or Wi-Fi |
| `FUN_80053594`, worker/task references | RTOS/task integration likely required | None | Startup stub has no scheduler, timer, mutex, ISR or exception handlers |

## Behavior-first milestones

1. **Done:** TS packet/PSI subset with deterministic tests; host tool for unencrypted `.ts` samples.
2. **Next:** Acquire original payload, header/layout and board specs; validate endian, ISA, loader entry and memory map; write hardware-independent tests for DVB-SI service selection.
3. **Later:** Test with documented hardware HAL, independently implemented drivers or authorized SDK hooks, then only consider an image container after verified recovery and header/checksum/signature rules.

The names `FUN_...` and feature strings identify *places to investigate*. They are **not** linkable APIs and their bodies are **not** recompiled or imported.
