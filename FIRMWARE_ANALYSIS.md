# maincode.bin / maincode_targeted.c — Complete Firmware Analysis
## Dish (DVB Satellite) Receiver Firmware — ALi MIPS SoC

> **Repo contents:** `maincode_targeted.c` (2,223,683 bytes, 73,790 lines) + `README.md` (`# H27`)
> **No `maincode.bin` binary is present in the repo.** `maincode_targeted.c` is the **Ghidra-decompiled C** of your `maincode.bin`. Every `FUN_80xxxxxx` / `DAT_80xxxxxx` / `s_xxx_80xxxxxx` name below is a Ghidra auto-name derived from the binary address. This report explains **what the binary is, how it is laid out, and what each subsystem does** — function-by-function where it matters.

---

## 1. Executive summary — what you have

| Item | Value |
|---|---|
| File | `maincode_targeted.c` — Ghidra decompilation of `maincode.bin` |
| Size | 2.22 MB, 73,790 lines, ~2.15 M chars of C |
| Architecture | **MIPS32 (MIPS coprocessor-0, `$gp` register, `setCopReg`, `EBase`/`Status`/`Cause`/`Config`)** — classic ALi (Ali Corp.) STB SoC layout |
| Load / execution base | **`0x80000000`** (KSEG0 cached) — entry at `0x80000e00` |
| Code span (decompiled) | `0x80000e00` – `0x8061aa18` = **6,396,952 bytes (~6.1 MB)** |
| Functions **defined** in this file | **1,321** (`FUN_80000e00` … `FUN_8061aa18`) |
| Distinct functions **called** | **3,003** `FUN_*` + 385 `func_0x*` indirect calls |
| Called but **not defined** (not targeted / external) | **1,687** — i.e. this is a **targeted** export: only ~44% of the call graph was decompiled. The rest (libc/OS/drivers) are referenced by address only |
| Strings | **844 unique** `s_*` symbols at `0x80626988`–`0x80950300` (`.rodata`) |
| Globals | **1,311 unique** `DAT_*` addresses; bulk at `0x806xxxxx` (rodata), `0x809xxxxx`–`0x80dxxxxx` (RAM/BSS), `0xb800xxxx` (SoC registers), `0xafc000a0` (KSEG1) |
| Decompile quality | 1,997 `WARNING` comments, 42 `halt_baddata()`, 4 hard failures (`DECOMPILE FAILED`) — normal for MIPS firmware with jump tables / hand-written asm |
| Vendor / SoC hints in strings | **ALi / `ALi_rt2870` (Ralink Wi-Fi), `ali3510c_v17`, `S3503`, `S3601_SD`, `S3601_SDHD`, `NIM_QAM_S3202`, `HDMI_PHY`, `EADAC`, `TSENSOR`, `OTP`, `e2p`** → **ALi S3601-class DVB-S/S2 + IPTV STB SoC** |

**In one sentence:** this is a **full-featured DVB satellite + IPTV set-top-box application firmware** (tuner/demod, TS demux, H.264/H.265/MPEG video, audio, CAS/CA, PVR, Wi-Fi, RTSP/IPTV, OSD/subtitles) running bare-metal / RTOS on an ALi MIPS SoC — not a tiny bootloader.

---

## 2. How to read this file (Ghidra conventions)

| Pattern | Meaning |
|---|---|
| `FUN_80000e00` | Function at address `0x80000e00`. Ghidra had no symbol, so it named by address |
| `func_0x8032c08c` | Indirect / computed call (jump table, callback, PLT-like stub) — Ghidra could not resolve target statically |
| `DAT_809001e0`, `_DAT_b8000002`, `uRamb8000098` | Global / RAM / MMIO variable at that address. `_DAT_` = wider overlapping symbol; `uRam` = uninitialized RAM |
| `s_TS_File_Decoder_806d7e3c` | String literal at `0x806d7e3c`; prefix is first ~30 chars of the string (truncated, underscores for spaces) |
| `LAB_800781e8` | Local label / goto target inside a function |
| `unaff_gp`, `unaff_*` | MIPS `$gp` (global pointer) / other registers Ghidra could not track — **very common in MIPS PIC code**; ignore for logic, it means "global base + offset" |
| `setCopReg(0, Status, …)` | MIPS Coprocessor-0 register write (exceptions, interrupts, TLB, cache) |
| `halt_baddata()` | Ghidra hit bytes it could not decode (data embedded in code, jump table, compressed/encrypted blob) and truncated control flow |
| `/* WARNING: … */` | Ghidra uncertainty: overlapping instructions, bad jump table, stack-pointer change, overlapping globals |
| `undefined4`, `undefined1`, `byte`, `uint` | Ghidra types: 4-byte / 1-byte / 8-bit / 32-bit unsigned. `undefined` = size unknown |
| `param_1`, `puVar1`, `iVar2`, `uStack_40` | Auto-named params / locals / stack slots |

**Targeted export:** the filename says it — someone ran Ghidra's "decompile targeted functions" (or a script) and exported only 1,321 functions. The other 1,687 callees exist in `maincode.bin` but their bodies are **not** in this `.c`. That is why the most-called helpers (see §7) have no body here.

---

## 3. Memory map (inferred from addresses)

```
0x80000000  KSEG0 — firmware code (cached)          ← entry 0x80000e00
0x80000e00–0x8061aa18  Decompiled code span (~6.1 MB)
0x80626988–0x806xxxxx  .rodata strings (844 strings, bulk 0x806d–0x806f)
0x808xxxxx–0x809xxxxx  .data / heap / driver state (DAT_808ff1e8, DAT_809001e0…)
0x80952318–0x80d4b270  BSS zeroed at boot (~3.9 MB — see §4)
0x80ac0a00, 0x80ac0a10  Small fixed buffers (memcpy targets in FUN_8061aa18)
0xafc000a0  KSEG1 (uncached) — SoC control register (bit 2 tested at boot)
0xb8000000–0xb802e098  MMIO / SoC registers (chip ID at 0xb8000002, config at 0xb802e090/98)
```

- **Code is at `0x800xxxxx`** — typical for ALi STBs where flash is mapped at `0x80000000` (or copied to DDR and executed from `0x80000000`).
- **Strings at `0x806xxxxx`** sit right after code — consistent with a single flat `maincode.bin` image.
- **RAM at `0x809xxxxx`–`0x80dxxxxx`** — heap, task stacks, TS buffers, frame buffers.
- **`0xb8xxxxxx`** — memory-mapped I/O (chip ID, clock/reset, tuner/demod registers). The boot code reads chip ID from `0xb8000002`.

---

## 4. Boot / entry — `FUN_80000e00` @ `0x80000e00` (the only true entry)

This is the reset entry (first function in file, lowest address). Line-by-line:

```c
void FUN_80000e00(void) {
  setCopReg(0, Config, Config & 0xf1ff7ff8 | 0x6230003);  // cache / MMU config
  setCopReg(0, cop0_reg22, cop0_reg22 | 1);               // CP0 reg 22 (often EntryHi / wired)
  DAT_b80000a4 = 0;                                        // clear SoC flag reg
  FUN_80003b2c();                                          // clock / PLL init (external, not exported)
  FUN_800012f8(0); FUN_800012f8(1);                        // cache flush / TLB / interrupt init x2
  if (_DAT_b8000002 != 0x3505) {                           // read Chip ID at 0xb8000002
    if (_DAT_b8000002 == 0x3503) { _DAT_b802e098=0xc2000d03; _DAT_b802e090=0x63162129; }
    else if (_DAT_b8000002 == 0x3281) { same }
    else if (_DAT_b8000002 == 0x3811) { same }
    else if (_DAT_b8000002 == 0x3603) { same }             // S3503/S3603 family → same clock table
    else {
      uRamb8000098 = 0xc2000d03; uRamb8000090 = 0x63162129; // default (S3601 etc.)
      if ((DAT_afc000a0 & 4) != 4) uRamb80000c0 |= 0x80000; // KSEG1 rev check
      FUN_80003970();                                      // DDR / memory controller init
    }
  }
  // else 0x3505 → skip (already configured / different board)
  puVar1 = &DAT_80952318;
  do { *puVar1=0; puVar1[1]=0; puVar1[2]=0; puVar1[3]=0; puVar1+=4; }
  while (puVar1 < 0x80d4b270);                             // zero BSS 0x80952318–0x80d4b270 (~3.9 MB)
  _DAT_80addbb0 = 0xabcdef12;                              // BSS sentinel / heap magic
  setCopReg(0, Status, Status | 0x400000);                 // enable interrupts (IE)
  setCopReg(0, EBase, 0x80002000);                         // exception vector base
  setCopReg(0, Status, 0x10000000);                        // BEV (bootstrap exception vectors)
  setCopReg(0, Cause, 0);                                  // clear exception cause
  _DAT_b8000038 = 0; _DAT_b800003c = 0;                    // clear interrupt pending
  FUN_801c26dc(&DAT_809001e0);                             // ← real main / OS kernel start (external)
  halt_baddata();                                          // never returns (idle loop / jump table)
}
```

**What this tells you:**
- Chip ID `0x3505` is the "known-good" board; `0x3503/0x3281/0x3811/0x3603` are sibling SoCs sharing the same clock table (`0xc2000d03` / `0x63162129`).
- BSS is ~3.9 MB — this is a **large RTOS application**, not a bootloader.
- `FUN_801c26dc` is the actual application `main()` (scheduler / task init). It is **not decompiled** in this targeted file — you need to export it separately to see task creation.

---

## 5. Function inventory — how the 1,321 functions are distributed

Functions per 64 KB block (dense = subsystem):

| Block | # funcs | Dominant strings / subsystem |
|---|---|---|
| `0x8000` | 3 | Boot + stream-validator (`FUN_8000adf8`, `FUN_8000b1a8`) |
| `0x8001` | 1 | TS File Decoder dispatcher (`FUN_8001b8b4`) |
| `0x8004`–`0x8005` | 2 | Small stubs / task init (`FUN_80053594` creates 8 workers) |
| `0x8007`–`0x8008` | 62 | **CCcam / card-sharing client** (`cccam__keepalive` ×69), channel-change, keepalive |
| `0x800a`–`0x800c` | 19 | Media seek, **H.265 decoder** (`H265_decore`), Tehran/Honolulu city strings (timezone?) |
| `0x800e`–`0x8010` | 30 | Audio/WAV engine (`sicEngine__wav_init`), malloc/free debug |
| `0x8011`–`0x8016` | 76 | **MPEG/H.264 video decoder core** (`4MVBack` ×43, `UnpackPictureLayerAdvanced`, `Entry_Point_Header`, `MBMODETAB`, `CBPTAB`) |
| `0x8018`–`0x801a` | 11 | **SoC drivers**: DRAM clock, HDMI PHY, EADAC trim, TSENSOR, sampling rate |
| `0x801c`–`0x8020` | 22 | **H.264 SPS/VUI parser** (`VUI__*`, `lsb_minus4`, `S3601_SD/SDHD`, CVBS DAC) |
| `0x8022`–`0x802a` | 82 | **DVB tuner / TS routing / channel DB** (`__Preview_TP_s_PAT`, `NETWORK_LOCAL_SETTINGS`, `Invaldi_LNB_type`, `No_TP`, `5_614x_com`) |
| `0x802b`–`0x802d` | 110 | **Subtitles, OSD, media playback** (`Subtitle_Opened`, `MP_IMAGE_PLAYBACK_END`, `_INFO_NOTIFY` ×92) |
| `0x802e` | 28 | **CAS / Smart Card** (`TurnRoundCAclient_UpcallFunction`, `Tandberg`, `BXCAM_RECHARGE`) |
| `0x8030`–`0x8039` | 187 | **PVR / filesystem / network stack** (`_record_item`, `_skip_PV`, `RTSP`, `SOCKS`, `CONNECT`, `SSL_write`, `POP3S`, `curl`) |
| `0x803c`–`0x803f` | 131 | **IPTV / TurnRound / Foxcam** (`TurnRound_IPTV_Download_GOLDEN`, `FOXCAM_CONNECTION`, `TV_Download_DEBUG`, `192.168.25.3` logs) |
| `0x8040`–`0x8049` | 140 | **Wi-Fi driver** (`ALi_rt2870`, `WPA2PSK_AES`, `WPAPSK`, `HAELMICFAILURE`, `O_ReqIEs`, `entContext` — Ralink 802.11) |
| `0x804c`–`0x804f` | 33 | Wi-Fi + USB bulk (`55534243…` ×56 — USB descriptor / bulk callback) |
| `0x8050`–`0x805a` | 45 | **Crypto / TLS / image** (`EC_PRIVATE_KEY`, `DSA_PRIVATE_KEY`, `LZ_dictionary`, `Y2M2D2`, `png`, `zlib`) |
| `0x805b`–`0x8061` | 60 | **Flash / zone / crypto helpers** (`Zone_Failed` ×9, `Erase_Configuration_Failed`, `o_2_key_error`, `rypto_Failed`) |

> Full per-function table (signature, size, strings, callees) is in **`FUNCTION_INVENTORY.md`** (1,321 rows) generated alongside this report.

**Largest functions** (decompiled size — these are the core state machines):
- `FUN_8042d4dc` @ `0x8042d4dc` (647 lines) and `FUN_804cb034` @ `0x804cb034` (624 lines) — **near-duplicate Wi-Fi / 802.11 state machines** (same `param_3` dispatch `0x103/0x104/0x107/0x109/0x507/0x510/0x60b`, same `FUN_803380f4` memcpy pattern; offsets differ by ~0xd4 → compiled twice for two radios / two build variants).
- `FUN_80212f58` @ `0x80212f58` (677 lines) — **TS / channel-info builder** (copies `param_1[0x6e]`…`param_1[0x89]` tables into `param_2+0x724…0xca8`).
- `FUN_8050b218` @ `0x8050b218` (564 lines) — crypto / TLS record handler.
- `FUN_8015401c` @ `0x8015401c` (647 lines) — **H.264 slice-header / MB parser** (`MBMODETAB`, `layField__Frame`, `CBPTAB`, `POSTPROC`).

---

## 6. Subsystem-by-subsystem — what each part does

### 6.1 Boot & low-level (0x80000e00–0x8007xxxx)
- `FUN_80000e00` — reset entry (see §4).
- `FUN_8000adf8` — **stream / index validator**: checks `param_1+0x88/0x8c` (sample count ≥ `0x15888` = 88,200), UTF-8 validity (`0xaa`, `0x62`), ASCII `0x20–0x7e`, frame-sync / metadata search, `REAM_DECODER_*` errors. Returns 1/0 + error string via `param_3`. This is the **demux / PES validator**.
- `FUN_8000b1a8` — **string / UTF-8 validator** for decoder init (`T_STATUS_OK`, `REAM_DECODER_INIT_STATUS_INVALID`).
- `FUN_8001b8b4` — **TS File Decoder dispatcher**: `param_2+0x3c` state `0x101` → `TS_File_Decoder` / `mant_>__121` check → `FUN_8001c174`; `0x201` → channel-change counter (`param_1+0xb8/c4`, returns `0x20`).
- `FUN_80053594` — **task / worker pool init**: allocates 8 contexts (`0x1230000` pool, `0x16f8` each), sets callbacks `0x800534f8` / `0x8005362c`, inits 8 sub-structs via `FUN_800619a0`, registers with `FUN_802e0f78`. Returns `0x1f03` on failure (see §8).
- `FUN_80078098` — **file download with timeout** (`e_size_too_large`, 3000 ms default, 10 ms poll via `FUN_801c1ce0`, cap `0x20000`): opens via `FUN_8032be14`, reads via `func_0x8032bf04`, mallocs via `func_0x80079db0`.

### 6.2 Video decode — H.264 / H.265 / MPEG (0x800c–0x801d, 0x8050)
- Strings: `H265_decore`, `4MVBack`/`4MVDirect`/`1MVDirect`, `Inter4x4`/`Inter8x4`, `UnpackPictureLayerAdvanced`, `Entry_Point_Header`, `MBMODETAB`, `CBPTAB`, `SDCTAB`, `POSTPROC`, `CodedWidth/CodedHeight/MaxCoded`, `VUI__*`, `SPS__*`, `SEI__*`, `SH__*` (slice header), `pic_order_cnt`, `luma_offset`, `chroma_offset`.
- `FUN_8015401c` / `FUN_80151c94` / `FUN_8014a448` — **H.264 MB/slice parser** (reads `MBMODETAB`, `layField`, `CBPTAB`, `POSTPROC`; calls bit-reader `FUN_8014f5a8` / `FUN_8014f65c`, logger `FUN_8014de5c`).
- `FUN_800c9b78` / `FUN_800cbec0` / `FUN_80108598` — **H.265 (HEVC) decoder entry** (`H265_decore`).
- `FUN_80112300` / `FUN_80112c68` — TS→decoder glue (`TS_File_Decoder`).
- `FUN_80198484` — **HDMI PHY** (`HDMI_PHY_R_SEL_2_0_`).
- `FUN_801a7300` / `FUN_801a73dc` — audio/video sync primitives used by `FUN_80053594`.

### 6.3 Audio (0x800e–0x8010, 0x8018)
- `sicEngine__wav_init___MALLOC_fail`, `invalid_parameter_lpcm_file_`, `Invalid_sampling_rate_code`, `A_PCM_INT_LIT`, `IME_QDM2`, `EADAC_L_TRIM_2_0_`.
- WAV/LPCM file player + I2S DAC trim. `FUN_800f6a9c`, `FUN_80086f14` are the WAV engine.

### 6.4 DVB tuner / demod / TS routing (0x8022–0x802a)
- `__Preview_TP_s_PAT__drop_it`, `ret__d__tuner__d__device_ID__04x`, `After_update____ts_route_sim_id_0`, `Invaldi_LNB_type`, `No_TP`, `san_jose`, `5_614x_com`, `not_specified__1_63_`.
- LNB type check, transponder (TP) preview, PAT parsing, TS route simulation, tuner device ID. `FUN_80202818`, `FUN_8020b5a4`, `FUN_80212f58` are the channel/TP builders.
- `NIM_QAM_S3202`, `S3503_0`, `S3601_SD/SDHD` — demod / NIM chip IDs.

### 6.5 CAS / Conditional Access / Smart Card (0x802e, 0x803c–0x803f)
- `TurnRoundCAclient_UpcallFunction`, `TurnRoundCAS_BissAddKey_Fails`, `TurnRound_BXCAM_CONNECTION`, `TurnRound_BXCAM_RECHARGE_Handle`, `Tandberg_INFO_Handle`, `Smart_Card_driver_init_Failed`, `BissAddKey`, `PowerVU_Key_Find`, `QST_filter_matched_caid`, `hid_0x_x_netid_0x_x_provid_0x_x`, `caid/pid/chid`, `CW` (control word), `ECM` (`nRound_CACORE_DEBUG___ECM_section`).
- `FUN_802ec794` / `FUN_802ed354` — smart-card driver init.
- `FUN_802ea15c` — BISS key add. `FUN_803c6b60` / `FUN_803c6e44` — BXCAM connection. `FUN_803f9b04` — IPTV golden-channel download.
- **CCcam client** (`0x8008` block, `cccam__keepalive` ×69, `KEE YALIVE_from_server`, `ANNEL_CHANGE_FAILED`, `CMD_er_tm__d_time__d_FAILED`): full card-sharing protocol (login, keepalive, ECM exchange).

### 6.6 PVR / storage / filesystem (0x8033–0x8035, 0x8060)
- `_record_item____NULL_` (×36), `_skip_PV` (×25), `not_playback`, `PVR__cre_flag_pvr_flag_id_failed`, `pvr_task_exit`, `SD_Card`, `Device_busy__please_format_later`, `DD_MM_YYYY_HH_MM_SS`, `NETWORK_LOCAL_SETTINGS`, `k_Mainmenu_Place1`.
- Record/playback, playlist (`ent_playlist_add_once`, `ent_playlist_sorttype`), SD/USB formatting, FAT handling. `FUN_80378278`, `FUN_8039a8a4` are PVR core.

### 6.7 Network / IPTV / streaming (0x8036–0x803f)
- `ANNOUNCE`, `TEARDOWN`, `RTSP_request__RTSPREQ_NONE`, `invalid_RTSP_request`, `libnetrtsp_process_describe/setup`, `Range__bytes`, `server_response`, `o_connect_to__s`, `ed_sending_CONNECT_to_proxy`, `SOCKS4`, `SSL_write`, `POP3S`, `curl` (`CURLOPT_FTP`), `Mozilla_4_0__compatible__MSIE_7_`, `http___192_168_25_3_v7_log`, `TV_Download_DEBUG___55555555`.
- RTSP client (`FUN_8037568c`, `FUN_80375ce0`), HTTP range download, SOCKS proxy, TLS, IPTV golden-channel parser (`IPHD_Information_parser_success`, `IPHD_IPTV_Group_parser_error`).
- `FUN_803df60c` / `FUN_803e0d48` — IPTV download debug servers.

### 6.8 Wi-Fi — Ralink `rt2870` (0x8041–0x804f)
- `ALi_rt2870`, `802_11b`, `802_11b_g_n`, `WPA2PSK_AES`, `WPAPSK`, `WPANONE`, `PSK_TKIPAES`, `EncrypType`, `AuthMode`, `RSNIE`, `HAELMICFAILURE_indication`, `O_ReqIEs_`, `entContext`, `ALi_PeerBeacon`, `PeerBeacon___CurrentAddress`, `CtlLow`, `CPU_TX`, `SSICBA`, `HANNEL`, `W_Mode`, `switch__v_0x12d1__p_0x1505`.
- Full 802.11 STA driver: scan, auth, WPA2-PSK, beacon, TX/RX, power save. The two giant duplicates `FUN_8042d4dc` / `FUN_804cb034` are the **MLME / state-machine** for two interfaces.
- `FUN_80495edc` — WPA open/none handler. `FUN_80429ec4` / `FUN_804c80b0` — WPA-PSK.

### 6.9 USB / peripherals (0x804d–0x804e)
- `55534243d84c04820002000080000afd` (×56) — USB bulk descriptor / `ic_read_bulk_callback`, `usb_interface`, `progressive_sequence`, `sample_precision`.
- 3G modem strings: `Huawei_E173__Moviestar_`, `70STA` — USB 3G dongle support.

### 6.10 Crypto / TLS / image (0x8055–0x8061)
- `EC_PRIVATE_KEY`, `IN_DSA_PRIVATE_KEY`, `BEGIN_EC_PRIVATE_KEY`, `509_CRL`, `FLUSHED_bit_not_set_on_flag_pack`, `Could_not_determine_local_IP_add`, `LZ_dictionary`, `Y2M2D2`, `rs_to_zlib`, `png`, `Zone_Failed`, `Erase_Configuration_Failed`, `o_2_key_error`, `rypto_Failed`, `to_alloc_run_loop_ctx`, `turn_or_timeout_filter`.
- OpenSSL-style TLS, zlib/PNG image decode (`E_draw_image_failed`), flash zone management.

### 6.11 OSD / UI / subtitles (0x8026, 0x802b–0x802d)
- `Subtitle_Opened`, `Not_support_Subtitle`, `Subtitle__d__d`, `PARENTAL_LOCK`, `Getting_Next_Page__Please_wait_`, `Next_Page_Failed`, `USIC_PLAYBACK_END`, `MP_IMAGE_PLAYBACK_END`, `_INFO_NOTIFY` (×92), `Inject_task_err_`, `Movie`, `Tehran`, `Honolulu` (timezone/city list), `Wait_1_minute__and_press_ok`.
- EPG / page navigation, parental lock, subtitle rendering, music/image playback, OSD notify queue.

---

## 7. The "invisible" 1,687 functions — what the external calls are

The targeted file calls but does not define these. By frequency + context they are **libc / RTOS / driver primitives**:

| External | Calls | Inferred role (from call sites) |
|---|---|---|
| `FUN_803398b4` | 527 | **`printf` / log** — called with format strings (`s_o_2_key_error`, `s_ERROR__s__L__d`); `/* Subroutine does not return */` after it = `assert`/`panic` path |
| `FUN_8014de5c` | 326 | **Decoder trace / `printf`-like** — called with `MBMODETAB`, `layField`, `CBPTAB` + bit values |
| `FUN_80338324` | 310 | **`memset`** — `FUN_80338324(dst, 0, size)` pattern everywhere |
| `FUN_803380f4` | 224 | **`memcpy`** — `FUN_803380f4(dst, src, len)`; Ghidra marks it noreturn due to overlapping-symbol confusion |
| `FUN_802ee794` | 213 | **Mutex / lock** (CAS + PVR paths) |
| `FUN_80338574` | 176 | **`strlen` / string op** |
| `FUN_805379c0` | 132 | **TLS / crypto helper** (0x805x block) |
| `FUN_803385b8` | 127 | **Allocator index / `malloc` wrapper** (used with `DAT_808ff1e8` pool in `FUN_80053594`) |
| `FUN_803398e8` | 127 | **Free / release** (paired with above) |
| `FUN_8033a86c` | 120 | **Free buffer** (paired with `FUN_80339c5c` malloc) |
| `FUN_803397b8` | 118 | **Queue / list op** |
| `FUN_80339c5c` | 115 | **`malloc(size)`** — `FUN_80339c5c(0x16f8)` in `FUN_80053594` |
| `FUN_8014f5a8` / `FUN_8014f65c` | 107 / 29 | **Bit-reader** (`read_bits(n)`, `read_ue/se`) for H.264 |
| `FUN_803309e0` | 89 | **Socket / network** |
| `FUN_8037726c` | 84 | **RTSP / HTTP helper** |
| `FUN_800858c4` / `FUN_8008a378` / `FUN_8008a3ac` | 77 / 77 / 69 | **CCcam / CAS helpers** |
| `FUN_801c1ce0` | 74 | **`sleep` / delay (ms)** — `FUN_801c1ce0(10)` poll loops |
| `FUN_8009618c` | 48 | **`strncmp` / string compare** — `FUN_8009618c(str, "TS_File_Decoder", 3)` |
| `FUN_800032a0` | 54 | **Low-level / cache / MMIO** (0x8000 block) |

> **To fully understand `maincode.bin`, export these 1,687 bodies too** (or at least the top 40 above). Without them, control flow stops at every `memcpy`/`malloc`/`printf`.

---

## 8. Error codes, returns, and conventions

- `return 0` = success / not-handled; `return 1` = handled/valid (validators); `return 0x20` = TS packet consumed (`FUN_8001b8b4`).
- `return 0xffffffff` (-1) ×79 — generic failure.
- `return 0xfe` (254) ×63 — **H.264 decoder error** (bitstream invalid).
- `return 0x1f03` — **resource / task-init failure** (`FUN_80053594` pool exhausted).
- `return 0xffffffa1` (-95) — **Wi-Fi state-machine invalid** (`FUN_8042d4dc` / `FUN_804cb034` default case).
- `return 0xffffffd3` (-45), `0xffffff74` (-140), `0xffffff37` (-201) — subsystem-specific (PVR / network / CAS).
- Validators return error **string pointer** via `param_3` (`*param_3 = s_...`) instead of code — e.g. `FUN_8000adf8` sets `s_must_have_at_least_one_index_poi`, `s_ion_string_must_be_valid_UTF_8`, `s_REAM_DECODER_*`.

---

## 9. Decompile failures & warnings — what is unreliable

| Artifact | Count | Meaning |
|---|---|---|
| `WARNING` comments | 1,997 | Overlapping instructions, bad jump tables, stack-pointer writes, overlapping globals — **logic near these lines needs assembly verification** |
| `halt_baddata()` | 42 | Ghidra truncated function at undecodable bytes (embedded data / jump table / encrypted blob) |
| `DECOMPILE FAILED` | 4 | `FUN_8014eb14`, `FUN_8014ee1c`, `FUN_801552d8`, `FUN_8044cc6c` — "Bad instruction count exceeded / No pcode at r0x…" — these are **hand-written asm / compressed code**; read disassembly, not C |
| `func_0x*` indirect calls | 548 | Callbacks / vtables / jump tables — target unknown statically |
| `unaff_gp` | 3,647 | MIPS PIC global-pointer accesses — offsets are correct, base register is not tracked |

**Do not trust** exact struct offsets or jump-table targets without checking the `.bin` disassembly. **Do trust** string references, call graphs, and high-level state machines — those are robust.

---

## 10. What is NOT in this file (gaps)

1. **`maincode.bin` itself** — not in repo. You cannot verify addresses, `.rodata` contents, or the 4 failed functions without it.
2. **1,687 external function bodies** — including `FUN_801c26dc` (real `main`), `FUN_80003b2c` (clock), `FUN_80003970` (DDR), all libc (`memcpy`/`malloc`/`printf`), and all drivers.
3. **Actual string contents** — Ghidra truncated names to ~30 chars (`s_..._806xxxxx`). Full strings (e.g. `s_4_connection_to__d__d__d__d__d___`) need the binary's `.rodata`.
4. **Types / structs** — everything is `undefined4` + raw offsets (`param_1 + 0x25dc`). No `struct Tuner`, `struct TS_Packet`, etc.
5. **Interrupt handlers / vectors** — `EBase = 0x80002000` points outside the decompiled span; exception handlers are not exported.

---

## 11. How to use this firmware image (practical next steps)

1. **Add `maincode.bin` to the repo** (or at least note its SHA-256 + size) so addresses can be verified.
2. **Re-export with Ghidra** including the top-40 external callees (§7) + `FUN_801c26dc`, `FUN_80003b2c`, `FUN_80003970`, and the 4 failed functions as **disassembly** (not decompilation).
3. **Rename by subsystem** using §6 (e.g. `FUN_8015401c` → `h264_parse_slice_header`, `FUN_8042d4dc` → `wifi_mlme_state_machine_0`).
4. **Recover structs** from the largest functions: `FUN_80212f58` (channel info, `+0x724…0xca8`), `FUN_8042d4dc` (Wi-Fi ctx, `+0x1740…0x2601`), `FUN_8015401c` (H.264 ctx, `+0x1a4…0x771`).
5. **For dish-receiver work specifically:** start at `FUN_8001b8b4` (TS dispatcher) → `FUN_80212f58` (TP/channel) → `FUN_8015401c` (video) → `FUN_802ea15c`/`FUN_803c6b60` (CAS) → `FUN_8037568c` (RTSP/IPTV). That chain is the live TV path.

---

## 12. Quick reference — key addresses

| Address | Symbol | Role |
|---|---|---|
| `0x80000e00` | `FUN_80000e00` | **Reset entry** (clock, chip ID, BSS zero, `FUN_801c26dc`) |
| `0x8000adf8` | `FUN_8000adf8` | Stream validator (UTF-8, frame-sync, REAM errors) |
| `0x8001b8b4` | `FUN_8001b8b4` | TS File Decoder dispatcher |
| `0x80053594` | `FUN_80053594` | 8-worker task pool init |
| `0x8015401c` | `FUN_8015401c` | H.264 slice/MB parser |
| `0x80212f58` | `FUN_80212f58` | TS/channel-info builder |
| `0x802ea15c` | `FUN_802ea15c` | TurnRound CAS / BISS key |
| `0x8037568c` | `FUN_8037568c` | RTSP client |
| `0x8042d4dc` / `0x804cb034` | `FUN_8042d4dc` / `FUN_804cb034` | Wi-Fi MLME state machines (duplicates) |
| `0x8061aa18` | `FUN_8061aa18` | Last function (memcpy to `0x80ac0a00`) |
| `0xb8000002` | `_DAT_b8000002` | **Chip ID** (`0x3505` = main board) |
| `0x80952318–0x80d4b270` | BSS | Zeroed at boot (~3.9 MB) |

---

*Generated by static analysis of `maincode_targeted.c` (73,790 lines). See `FUNCTION_INVENTORY.md` for the full 1,321-function table.*
