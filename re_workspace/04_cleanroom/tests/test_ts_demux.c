#include "ts_demux.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define EXPECT(test) do { \
    if (!(test)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #test); \
        ++failures; \
    } \
} while (0)

/* Fixed, independently generated MPEG-2 CRC fixtures for a single-program
 * PAT (PMT PID 0x0100) and PMT (H.264 0x0101, AAC 0x0102). */
static const u8 pat[] = {
    0x00,0xb0,0x0d,0x00,0x01,0xc1,0x00,0x00,
    0x00,0x01,0xe1,0x00,0xe8,0xf9,0x5e,0x7d
};
static const u8 pmt[] = {
    0x02,0xb0,0x17,0x00,0x01,0xc1,0x00,0x00,
    0xe1,0x01,0xf0,0x00,0x1b,0xe1,0x01,0xf0,
    0x00,0x0f,0xe1,0x02,0xf0,0x00,0x9e,0x28,0xc6,0xdd
};

static void reseal(u8 *section, size_t len)
{
    const u32 crc = ts_crc32_mpeg2(section, len - 4);
    section[len-4] = (u8)(crc >> 24);
    section[len-3] = (u8)(crc >> 16);
    section[len-2] = (u8)(crc >> 8);
    section[len-1] = (u8)crc;
}

/* Build a packet with an adaptation field sized to expose precisely n
 * payload bytes. An adaptation-only packet has n=0. */
static void packet(u8 out[188], u16 pid, u8 cc, bool start,
                   const u8 *payload, size_t n)
{
    memset(out, 0xff, 188);
    EXPECT(n <= 184);
    out[0] = 0x47;
    out[1] = (u8)(((pid >> 8) & 0x1f) | (start ? 0x40 : 0));
    out[2] = (u8)pid;
    out[3] = (u8)(((n == 0 ? 2 : n == 184 ? 1 : 3) << 4) | (cc & 15));
    if (n != 184) {
        out[4] = (u8)(183 - n);
        if (out[4]) out[5] = 0; /* adaptation flags */
    }
    if (n) memcpy(out + 188 - n, payload, n);
}

static void section_packet(u8 out[188], u16 pid, u8 cc, const u8 *s, size_t n)
{
    u8 payload[1 + sizeof(pmt) + sizeof(pat)];
    EXPECT(n <= sizeof(payload) - 1);
    payload[0] = 0; /* pointer_field */
    memcpy(payload + 1, s, n);
    packet(out, pid, cc, true, payload, n + 1);
}

static void test_sections(void)
{
    u16 pid = TS_PID_INVALID, v = TS_PID_INVALID, a = TS_PID_INVALID;
    const u8 digits[] = "123456789";
    EXPECT(ts_crc32_mpeg2(digits, 9) == 0x0376e6e7u);
    EXPECT(ts_crc32_mpeg2(pat, sizeof(pat)) == 0);
    EXPECT(ts_crc32_mpeg2(pmt, sizeof(pmt)) == 0);
    EXPECT(ts_parse_pat(pat, sizeof(pat), &pid) == FW_OK && pid == 0x0100);
    EXPECT(ts_parse_pmt(pmt, sizeof(pmt), &v, &a) == FW_OK &&
           v == 0x0101 && a == 0x0102);
    EXPECT(ts_parse_pat(pat, sizeof(pat)-1, &pid) == FW_ERR_INVAL);
    EXPECT(ts_parse_pat(pat, sizeof(pat), NULL) == FW_ERR_INVAL);
    EXPECT(ts_parse_pmt(pmt, sizeof(pmt), NULL, &a) == FW_ERR_INVAL);

    u8 bad_pat[sizeof(pat)];
    memcpy(bad_pat, pat, sizeof(pat));
    bad_pat[9] ^= 1;
    EXPECT(ts_parse_pat(bad_pat, sizeof(bad_pat), &pid) == FW_ERR_CRC);
    memcpy(bad_pat, pat, sizeof(pat));
    bad_pat[9] = 0; /* program 0 = NIT only */
    reseal(bad_pat, sizeof(bad_pat));
    EXPECT(ts_parse_pat(bad_pat, sizeof(bad_pat), &pid) == FW_ERR_NOTSUP);

    u8 bad_pmt[sizeof(pmt)];
    memcpy(bad_pmt, pmt, sizeof(pmt));
    bad_pmt[16] = 0x40; /* impossible ES_info_length, valid CRC */
    reseal(bad_pmt, sizeof(bad_pmt));
    EXPECT(ts_parse_pmt(bad_pmt, sizeof(bad_pmt), &v, &a) == FW_ERR_INVAL);
}

static void test_packet_bounds(void)
{
    u8 data[188];
    ts_packet_t p;
    const u8 payload[] = {0, 1, 2, 3};
    packet(data, 0x0100, 15, true, payload, sizeof(payload));
    EXPECT(ts_packet_parse(data, sizeof(data), &p) == FW_OK);
    EXPECT(p.pid == 0x0100 && p.continuity_counter == 15 &&
           p.payload_unit_start && p.payload_len == sizeof(payload) &&
           p.payload[0] == 0 && p.payload[3] == 3);
    data[0] = 0;
    EXPECT(ts_packet_parse(data, sizeof(data), &p) == FW_ERR_SYNC);
    data[0] = 0x47;
    data[1] |= 0x80; /* transport_error_indicator */
    EXPECT(ts_packet_parse(data, sizeof(data), &p) == FW_ERR_INVAL);
    data[1] &= (u8)~0x80;
    data[4] = 184; /* overlong adaptation_field_length */
    EXPECT(ts_packet_parse(data, sizeof(data), &p) == FW_ERR_INVAL);
    data[4] = 1;
    data[5] = 0x80;
    EXPECT(ts_packet_parse(data, sizeof(data), &p) == FW_OK && p.discontinuity);
    data[3] = 0; /* reserved adaptation_field_control */
    EXPECT(ts_packet_parse(data, sizeof(data), &p) == FW_ERR_INVAL);
    packet(data, 0, 0, false, NULL, 0);
    EXPECT(ts_packet_parse(data, sizeof(data), &p) == FW_OK && !p.has_payload && !p.payload);
    EXPECT(ts_packet_parse(NULL, sizeof(data), &p) == FW_ERR_INVAL);
    EXPECT(ts_packet_parse(data, 4, &p) == FW_ERR_INVAL);
}

static void test_stream(void)
{
    ts_demux_t d;
    ts_demux_init(&d);
    EXPECT(d.state == TS_SEEK_SYNC && d.pmt_pid == TS_PID_INVALID);
    EXPECT(ts_demux_feed(&d, NULL, 0) == 0);
    EXPECT(ts_demux_feed(&d, NULL, 1) == FW_ERR_INVAL);

    u8 bytes[3 + 2*188];
    bytes[0]=0; bytes[1]=0x03; bytes[2]=0xff;
    section_packet(bytes + 3, 0, 0, pat, sizeof(pat));
    section_packet(bytes + 3 + 188, 0x0100, 0, pmt, sizeof(pmt));
    int got = 0;
    for (size_t i=0; i<sizeof(bytes); i += 7) {
        size_t n = sizeof(bytes)-i;
        if (n > 7) n = 7;
        got += ts_demux_feed(&d, bytes+i, n);
    }
    EXPECT(got == 2);
    EXPECT(d.sync_errors == 3 && d.packets_ok == 2 && d.packets_seen == 2);
    EXPECT(d.pat_sections == 1 && d.pmt_sections == 1);
    EXPECT(d.service_ready && d.pmt_pid == 0x0100 &&
           d.video_pid == 0x0101 && d.audio_pid == 0x0102);
    EXPECT(d.channel_changes == 0);

    /* Duplicate CC on the same PMT PID is ignored. */
    EXPECT(ts_demux_feed(&d, bytes + 3 + 188, 188) == 1);
    EXPECT(d.pmt_sections == 1 && d.continuity_errors == 0);

    u8 changed[sizeof(pmt)];
    memcpy(changed, pmt, sizeof(changed));
    changed[19] = 0x03; /* AAC PID now 0x0103 */
    reseal(changed, sizeof(changed));
    u8 pkt[188];
    section_packet(pkt, 0x0100, 1, changed, sizeof(changed));
    EXPECT(ts_demux_feed(&d, pkt, 188) == 1);
    EXPECT(d.audio_pid == 0x0103 && d.channel_changes == 1);

    changed[19] ^= 1; /* bad CRC: must NOT alter current selection */
    section_packet(pkt, 0x0100, 2, changed, sizeof(changed));
    EXPECT(ts_demux_feed(&d, pkt, 188) == 1);
    EXPECT(d.audio_pid == 0x0103 && d.crc_errors == 1);
}

static void test_split_and_errors(void)
{
    ts_demux_t d;
    ts_demux_init(&d);
    u8 first_payload[1 + 7];
    first_payload[0] = 0;
    memcpy(first_payload + 1, pat, 7);
    u8 first[188], second[188];
    packet(first, 0, 0, true, first_payload, sizeof(first_payload));
    packet(second, 0, 1, false, pat + 7, sizeof(pat)-7);
    EXPECT(ts_demux_feed(&d, first, 188) == 1 && d.pat_sections == 0);
    EXPECT(ts_demux_feed(&d, second, 188) == 1 && d.pat_sections == 1);

    ts_demux_init(&d);
    packet(second, 0, 3, false, pat + 7, sizeof(pat)-7);
    EXPECT(ts_demux_feed(&d, first, 188) == 1);
    EXPECT(ts_demux_feed(&d, second, 188) == 1);
    EXPECT(d.continuity_errors == 1 && d.pat_sections == 0);
    u8 pkt[188];
    section_packet(pkt, 0, 4, pat, sizeof(pat));
    EXPECT(ts_demux_feed(&d, pkt, 188) == 1 && d.pat_sections == 1);

    u8 invalid_pointer[2] = { 5, 0 };
    packet(pkt, 0, 5, true, invalid_pointer, sizeof(invalid_pointer));
    EXPECT(ts_demux_feed(&d, pkt, 188) == 1 && d.malformed_packets == 1);
    pkt[0] = 0x46;
    EXPECT(ts_demux_feed(&d, pkt, 188) == 0 && d.sync_errors >= 1);
}

static void test_pointer_and_continuity_wrap(void)
{
    ts_demux_t d;
    ts_demux_init(&d);
    u8 payload[1 + sizeof(pat) - 6 + sizeof(pat)];
    u8 first[188], second[188], third[188];
    const u8 first_bytes[7] = { 0, pat[0], pat[1], pat[2], pat[3], pat[4], pat[5] };
    packet(first, 0, 14, true, first_bytes, sizeof(first_bytes));
    payload[0] = sizeof(pat) - 6; /* finish previous section before new one */
    memcpy(payload + 1, pat + 6, sizeof(pat) - 6);
    memcpy(payload + 1 + sizeof(pat) - 6, pat, sizeof(pat));
    packet(second, 0, 15, true, payload, sizeof(payload));
    section_packet(third, 0, 0, pat, sizeof(pat)); /* 15 -> 0 is valid */
    EXPECT(ts_demux_feed(&d, first, sizeof(first)) == 1);
    EXPECT(ts_demux_feed(&d, second, sizeof(second)) == 1);
    EXPECT(d.pat_sections == 2 && d.continuity_errors == 0);
    EXPECT(ts_demux_feed(&d, third, sizeof(third)) == 1);
    EXPECT(d.pat_sections == 3 && d.continuity_errors == 0);

    ts_demux_init(&d);
    packet(first, 0, 0, true, first_bytes, sizeof(first_bytes));
    u8 adaptation_only[188];
    packet(adaptation_only, 0, 0, false, NULL, 0);
    adaptation_only[5] = 0x80; /* discontinuity_indicator resets assembly */
    packet(second, 0, 1, false, pat + 6, sizeof(pat) - 6);
    EXPECT(ts_demux_feed(&d, first, 188) == 1);
    EXPECT(ts_demux_feed(&d, adaptation_only, 188) == 1);
    EXPECT(ts_demux_feed(&d, second, 188) == 1);
    EXPECT(d.pat_sections == 0);
}

static void test_random_inputs(void)
{
    /* Deterministic malformed-input smoke test; no success assumption. */
    ts_demux_t d;
    ts_demux_init(&d);
    u8 bytes[221];
    u32 seed = 0xa5b4c3d2u;
    for (int repeat = 0; repeat < 1000; ++repeat) {
        for (size_t i = 0; i < sizeof(bytes); ++i) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            bytes[i] = (u8)seed;
        }
        if ((repeat % 5) == 0) bytes[0] = 0x47;
        EXPECT(ts_demux_feed(&d, bytes, (size_t)(seed % sizeof(bytes))) >= 0);
    }
}

int main(void)
{
    test_sections();
    test_packet_bounds();
    test_stream();
    test_split_and_errors();
    test_pointer_and_continuity_wrap();
    test_random_inputs();
    if (failures) return 1;
    puts("TS parser tests passed");
    return 0;
}
