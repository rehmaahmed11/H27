/* Independent application skeleton. No receiver hardware is initialized.
 * This is intentionally not a drop-in replacement for the original image. */
#include "hal.h"
#include "ts_demux.h"

static ts_demux_t demux;
static u8 ts_data[TS_PACKET_SIZE];
volatile int cleanroom_status;

void device_main(void)
{
    ts_demux_init(&demux);
    cleanroom_status = FW_OK;
    for (;;) {
        const int size = hal_read_ts(ts_data, sizeof(ts_data));
        if (size < 0) {
            cleanroom_status = size; /* with stub: FW_ERR_NOTSUP */
            for (;;) hal_idle();
        }
        if (size > 0 && (size_t)size <= sizeof(ts_data)) {
            int result = ts_demux_feed(&demux, ts_data, (size_t)size);
            if (result < 0) cleanroom_status = result;
        }
        hal_idle();
    }
}
