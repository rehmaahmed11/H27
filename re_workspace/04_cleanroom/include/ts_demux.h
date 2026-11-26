/* Small independent MPEG-2 transport-stream/PSI parser.
 * Uses only ISO/IEC 13818-1 packet/section rules, not Ghidra-generated code.
 * No tuner, codec, CAS, hardware register, or RTOS implementation. */
#ifndef CLEANROOM_TS_DEMUX_H
#define CLEANROOM_TS_DEMUX_H

#include "fw_types.h"

#define TS_PACKET_SIZE 188u
#define TS_SECTION_MAX 1024u
#define TS_PID_INVALID 0x1fffu

typedef struct {
    u16 pid;
    u8 payload_unit_start;
    u8 continuity_counter;
    bool discontinuity;
    bool has_payload;
    const u8 *payload; /* points into caller's 188-byte packet; do not retain */
    u16 payload_len;
} ts_packet_t;

typedef struct {
    u8 bytes[TS_SECTION_MAX];
    u16 used;
    u16 expected;
    u8 last_cc;
    bool cc_valid;
} ts_psi_buffer_t;

typedef enum {
    TS_SEEK_SYNC = 0,
    TS_RUNNING = 1
} ts_state_t;

typedef struct {
    ts_state_t state;
    u8 pending[TS_PACKET_SIZE];
    u16 pending_len;
    ts_psi_buffer_t pat;
    ts_psi_buffer_t pmt;
    u16 pmt_pid;
    u16 video_pid;
    u16 audio_pid;
    bool service_ready;
    u32 packets_seen;
    u32 packets_ok;
    u32 sync_errors;
    u32 malformed_packets;
    u32 continuity_errors;
    u32 crc_errors;
    u32 pat_sections;
    u32 pmt_sections;
    u32 channel_changes;
} ts_demux_t;

/* A packet's payload pointer borrows pkt188. Length must be exactly 188. */
int ts_packet_parse(const u8 *pkt188, size_t length, ts_packet_t *out);

/* MPEG-2 PSI CRC-32, init 0xffffffff, poly 0x04c11db7, no reflection/XOR.
 * A complete valid section INCLUDING its four CRC bytes has CRC == 0. */
u32 ts_crc32_mpeg2(const u8 *bytes, size_t size);

/* Parse one COMPLETE, CRC-checked section. First program is selected in PAT.
 * PMT reports first recognized video/audio stream, or TS_PID_INVALID.
 * Multi-section programs, CA descriptors, private audio, PES and video decode
 * are not supported by this starter. */
int ts_parse_pat(const u8 *section, size_t size, u16 *out_pmt_pid);
int ts_parse_pmt(const u8 *section, size_t size, u16 *out_vpid, u16 *out_apid);

void ts_demux_init(ts_demux_t *d);
/* Accept arbitrary byte chunks; returns number of valid 188-byte packets,
 * or FW_ERR_INVAL for bad arguments. All state/section buffers are bounded. */
int ts_demux_feed(ts_demux_t *d, const u8 *bytes, size_t size);

#endif
