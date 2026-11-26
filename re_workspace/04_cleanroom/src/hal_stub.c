#include "hal.h"

int hal_read_ts(u8 *dst, size_t capacity)
{
    (void)dst;
    (void)capacity;
    return FW_ERR_NOTSUP; /* no real tuner/demux implementation */
}

void hal_idle(void)
{
    /* Diagnostic placeholder; NOT an RTOS idle implementation. */
    __asm__ volatile("nop");
}
