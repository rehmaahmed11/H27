/* Host-only inspection utility. Reads a local .ts file; no device access. */
#include "ts_demux.h"

#include <inttypes.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s recording.ts\n", argv[0]);
        return 2;
    }
    FILE *input = fopen(argv[1], "rb");
    if (!input) { perror(argv[1]); return 1; }

    ts_demux_t demux;
    ts_demux_init(&demux);
    u8 chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), input)) != 0) {
        if (ts_demux_feed(&demux, chunk, n) < 0) {
            fputs("could not parse input\n", stderr);
            fclose(input);
            return 1;
        }
    }
    if (ferror(input)) { perror("reading recording"); fclose(input); return 1; }
    fclose(input);

    printf("packets=%" PRIu32 ", valid=%" PRIu32 ", sync_errors=%" PRIu32
           ", malformed=%" PRIu32 ", continuity_errors=%" PRIu32
           ", CRC_errors=%" PRIu32 "\n", demux.packets_seen, demux.packets_ok,
           demux.sync_errors, demux.malformed_packets, demux.continuity_errors,
           demux.crc_errors);
    printf("PAT sections=%" PRIu32 ", PMT sections=%" PRIu32
           ", program changes=%" PRIu32 "\n", demux.pat_sections,
           demux.pmt_sections, demux.channel_changes);
    if (demux.pmt_pid != TS_PID_INVALID) {
        printf("first PMT PID: 0x%04x\n", (unsigned)demux.pmt_pid);
        if (demux.video_pid != TS_PID_INVALID)
            printf("video PID: 0x%04x\n", (unsigned)demux.video_pid);
        if (demux.audio_pid != TS_PID_INVALID)
            printf("audio PID: 0x%04x\n", (unsigned)demux.audio_pid);
    } else {
        puts("no supported PAT/PMT found (or capture contains no PSI)");
    }
    if (demux.pending_len) fprintf(stderr, "trailing partial packet: %u bytes\n",
                                   (unsigned)demux.pending_len);
    return 0;
}
