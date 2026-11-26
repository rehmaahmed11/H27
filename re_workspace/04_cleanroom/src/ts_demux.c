#include "ts_demux.h"
#include "fw_mem.h"

#include <limits.h>

int ts_packet_parse(const u8 *pkt, size_t length, ts_packet_t *out)
{
    if (!pkt || !out || length != TS_PACKET_SIZE) return FW_ERR_INVAL;
    fw_memset(out, 0, sizeof(*out));
    if (pkt[0] != 0x47) return FW_ERR_SYNC;
    if ((pkt[1] & 0x80u) != 0) return FW_ERR_INVAL; /* transport error */

    const u8 afc = (u8)((pkt[3] >> 4) & 3u);
    if (afc == 0) return FW_ERR_INVAL; /* reserved */
    out->pid = (u16)(((u16)(pkt[1] & 0x1fu) << 8) | pkt[2]);
    out->payload_unit_start = (u8)((pkt[1] >> 6) & 1u);
    out->continuity_counter = (u8)(pkt[3] & 0x0fu);

    size_t offset = 4;
    if (afc & 2u) {
        const u8 adaptation_length = pkt[4];
        if ((size_t)adaptation_length > TS_PACKET_SIZE - 5u) return FW_ERR_INVAL;
        if (adaptation_length) out->discontinuity = (pkt[5] & 0x80u) != 0;
        offset = 5u + adaptation_length;
    }
    out->has_payload = (afc & 1u) != 0;
    if (out->has_payload) {
        out->payload = pkt + offset;
        out->payload_len = (u16)(TS_PACKET_SIZE - offset);
    }
    return FW_OK;
}

u32 ts_crc32_mpeg2(const u8 *bytes, size_t size)
{
    u32 crc = 0xffffffffu;
    if (!bytes && size) return crc;
    for (size_t i = 0; i < size; ++i) {
        crc ^= (u32)bytes[i] << 24;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc << 1) ^ ((crc & 0x80000000u) ? 0x04c11db7u : 0u);
        }
    }
    return crc;
}

static int check_section(const u8 *s, size_t size, u8 table_id, u16 minimum_length)
{
    if (!s || size < 8u || s[0] != table_id) return FW_ERR_INVAL;
    /* section_syntax_indicator=1, '0' bit=0, reserved bits=3 */
    if ((s[1] & 0xf0u) != 0xb0u) return FW_ERR_INVAL;
    const u16 section_length = (u16)(((u16)(s[1] & 0x0fu) << 8) | s[2]);
    if (section_length < minimum_length || section_length > 1021u ||
        size != 3u + section_length) return FW_ERR_INVAL;
    if (ts_crc32_mpeg2(s, size) != 0u) return FW_ERR_CRC;
    /* Starter deliberately supports only current, section-zero tables. */
    if (!(s[5] & 1u) || s[6] != 0 || s[7] != 0) return FW_ERR_NOTSUP;
    return FW_OK;
}

int ts_parse_pat(const u8 *s, size_t size, u16 *out_pmt_pid)
{
    if (!out_pmt_pid) return FW_ERR_INVAL;
    const int result = check_section(s, size, 0x00u, 9u);
    if (result != FW_OK) return result;
    const size_t end = size - 4u; /* exclude CRC */
    if ((end - 8u) % 4u) return FW_ERR_INVAL;
    for (size_t i = 8; i < end; i += 4u) {
        const u16 program = (u16)(((u16)s[i] << 8) | s[i + 1u]);
        const u16 pid = (u16)(((u16)(s[i + 2u] & 0x1fu) << 8) | s[i + 3u]);
        if (program != 0 && pid != TS_PID_INVALID) {
            *out_pmt_pid = pid; /* first program; 0 denotes NIT, not a PMT */
            return FW_OK;
        }
    }
    return FW_ERR_NOTSUP;
}

int ts_parse_pmt(const u8 *s, size_t size, u16 *out_vpid, u16 *out_apid)
{
    if (!out_vpid || !out_apid) return FW_ERR_INVAL;
    const int result = check_section(s, size, 0x02u, 13u);
    if (result != FW_OK) return result;
    const size_t end = size - 4u;
    const size_t info_len = ((size_t)(s[10] & 0x0fu) << 8) | s[11];
    if (info_len > end - 12u) return FW_ERR_INVAL;

    u16 video = TS_PID_INVALID, audio = TS_PID_INVALID;
    size_t i = 12u + info_len;
    while (i < end) {
        if (end - i < 5u) return FW_ERR_INVAL;
        const u8 stream_type = s[i];
        const u16 pid = (u16)(((u16)(s[i + 1u] & 0x1fu) << 8) | s[i + 2u]);
        const size_t es_info_len = ((size_t)(s[i + 3u] & 0x0fu) << 8) | s[i + 4u];
        if (es_info_len > end - i - 5u) return FW_ERR_INVAL;
        if (pid != TS_PID_INVALID) {
            /* Only unambiguous stream types; private stream 0x06 needs
             * descriptor inspection and is intentionally not guessed. */
            if (video == TS_PID_INVALID &&
                (stream_type == 0x01u || stream_type == 0x02u ||
                 stream_type == 0x10u || stream_type == 0x1bu ||
                 stream_type == 0x24u)) video = pid;
            if (audio == TS_PID_INVALID &&
                (stream_type == 0x03u || stream_type == 0x04u ||
                 stream_type == 0x0fu || stream_type == 0x11u)) audio = pid;
        }
        i += 5u + es_info_len;
    }
    *out_vpid = video;
    *out_apid = audio;
    return FW_OK;
}

void ts_demux_init(ts_demux_t *d)
{
    if (!d) return;
    fw_memset(d, 0, sizeof(*d));
    d->state = TS_SEEK_SYNC;
    d->pmt_pid = TS_PID_INVALID;
    d->video_pid = TS_PID_INVALID;
    d->audio_pid = TS_PID_INVALID;
}

static void psi_drop(ts_psi_buffer_t *b)
{
    b->used = 0;
    b->expected = 0;
}

static void psi_complete(ts_demux_t *d, const ts_psi_buffer_t *b, bool is_pat)
{
    if (is_pat) {
        u16 pid;
        const int result = ts_parse_pat(b->bytes, b->expected, &pid);
        if (result == FW_ERR_CRC) ++d->crc_errors;
        else if (result == FW_ERR_INVAL) ++d->malformed_packets;
        else if (result == FW_OK) {
            ++d->pat_sections;
            if (pid != d->pmt_pid) {
                if (d->service_ready) ++d->channel_changes;
                d->service_ready = false;
                d->pmt_pid = pid;
                d->video_pid = d->audio_pid = TS_PID_INVALID;
                fw_memset(&d->pmt, 0, sizeof(d->pmt));
            }
        }
    } else {
        u16 video, audio;
        const int result = ts_parse_pmt(b->bytes, b->expected, &video, &audio);
        if (result == FW_ERR_CRC) ++d->crc_errors;
        else if (result == FW_ERR_INVAL) ++d->malformed_packets;
        else if (result == FW_OK) {
            ++d->pmt_sections;
            if (d->service_ready && (video != d->video_pid || audio != d->audio_pid))
                ++d->channel_changes;
            d->video_pid = video;
            d->audio_pid = audio;
            d->service_ready = video != TS_PID_INVALID || audio != TS_PID_INVALID;
        }
    }
}

static void psi_append(ts_demux_t *d, ts_psi_buffer_t *b, const u8 *p,
                       size_t n, bool may_start_new, bool is_pat)
{
    while (n && (b->used || may_start_new)) {
        if (!b->used && p[0] == 0xffu) return; /* stuffing bytes */
        if (b->used < 3u) {
            size_t take = 3u - b->used;
            if (take > n) take = n;
            fw_memcpy(b->bytes + b->used, p, take);
            b->used = (u16)(b->used + take);
            p += take;
            n -= take;
            if (b->used < 3u) return;
            const u16 section_length = (u16)(((u16)(b->bytes[1] & 0x0fu) << 8) |
                                              b->bytes[2]);
            if ((b->bytes[1] & 0xf0u) != 0xb0u || section_length < 9u ||
                section_length > 1021u) {
                ++d->malformed_packets;
                psi_drop(b);
                return;
            }
            b->expected = (u16)(3u + section_length);
        }
        size_t take = b->expected - b->used;
        if (take > n) take = n;
        fw_memcpy(b->bytes + b->used, p, take);
        b->used = (u16)(b->used + take);
        p += take;
        n -= take;
        if (b->used == b->expected) {
            psi_complete(d, b, is_pat);
            psi_drop(b);
            /* Continuation packets may finish an old section; only a
             * payload-unit start can introduce a new section here. */
            if (!may_start_new) return;
        }
    }
}

static void psi_on_packet(ts_demux_t *d, ts_psi_buffer_t *b,
                          const ts_packet_t *pkt, bool is_pat)
{
    if (pkt->discontinuity) {
        psi_drop(b);
        b->cc_valid = false;
    }
    if (!pkt->has_payload || !pkt->payload_len) return;
    if (b->cc_valid) {
        if (pkt->continuity_counter == b->last_cc) return; /* duplicate */
        if (pkt->continuity_counter != (u8)((b->last_cc + 1u) & 0x0fu)) {
            ++d->continuity_errors;
            psi_drop(b);
        }
    }
    b->last_cc = pkt->continuity_counter;
    b->cc_valid = true;

    const u8 *p = pkt->payload;
    size_t n = pkt->payload_len;
    if (pkt->payload_unit_start) {
        const u8 pointer = p[0];
        ++p; --n;
        if (pointer > n) {
            ++d->malformed_packets;
            psi_drop(b);
            return;
        }
        if (b->used && pointer) psi_append(d, b, p, pointer, false, is_pat);
        p += pointer;
        n -= pointer;
        /* A new section starts after pointer bytes. Incomplete old sections
         * must not be joined to unrelated data. */
        psi_drop(b);
        psi_append(d, b, p, n, true, is_pat);
    } else if (b->used) {
        psi_append(d, b, p, n, false, is_pat);
    }
}

int ts_demux_feed(ts_demux_t *d, const u8 *bytes, size_t size)
{
    if (!d || (!bytes && size)) return FW_ERR_INVAL;
    int parsed = 0;
    while (size) {
        if (!d->pending_len && bytes[0] != 0x47u) {
            ++d->sync_errors;
            d->state = TS_SEEK_SYNC;
            ++bytes; --size;
            continue;
        }
        size_t take = TS_PACKET_SIZE - d->pending_len;
        if (take > size) take = size;
        fw_memcpy(d->pending + d->pending_len, bytes, take);
        d->pending_len = (u16)(d->pending_len + take);
        bytes += take;
        size -= take;
        if (d->pending_len != TS_PACKET_SIZE) continue;

        ++d->packets_seen;
        ts_packet_t pkt;
        const int result = ts_packet_parse(d->pending, TS_PACKET_SIZE, &pkt);
        if (result == FW_ERR_SYNC) {
            ++d->sync_errors;
            d->state = TS_SEEK_SYNC;
        } else if (result != FW_OK) {
            ++d->malformed_packets;
        } else {
            ++d->packets_ok;
            if (parsed < INT_MAX) ++parsed;
            d->state = TS_RUNNING;
            if (pkt.pid == 0u) psi_on_packet(d, &d->pat, &pkt, true);
            else if (pkt.pid == d->pmt_pid && d->pmt_pid != TS_PID_INVALID)
                psi_on_packet(d, &d->pmt, &pkt, false);
        }
        d->pending_len = 0;
    }
    return parsed;
}
