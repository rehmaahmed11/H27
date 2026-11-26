/* Target-specific hardware integration boundary. No vendor calls here. */
#ifndef CLEANROOM_HAL_H
#define CLEANROOM_HAL_H

#include "fw_types.h"

/* Nonblocking read: bytes read >=0, negative on error/unavailable.
 * A real implementation must be written ONLY after identifying the tuner,
 * demodulator, TS DMA path, RTOS, and validated device memory map. */
int hal_read_ts(u8 *dst, size_t capacity);
void hal_idle(void);

#endif
