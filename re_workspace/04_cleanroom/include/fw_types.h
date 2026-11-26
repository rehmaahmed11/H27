/* Independent implementation; maincode_targeted.c is a behavioral reference. */
#ifndef CLEANROOM_FW_TYPES_H
#define CLEANROOM_FW_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* Local API errors; these are NOT original firmware error codes. */
enum {
    FW_OK          = 0,
    FW_ERR_INVAL   = -1,
    FW_ERR_SYNC    = -2,
    FW_ERR_CRC     = -3,
    FW_ERR_NOTSUP  = -4
};

/* Provisional addresses reported for the decompressed reference image.
 * The original binary/loader is not in this checkout; verify before use. */
#define FW_BASE_ADDR  0x80000000u
#define FW_ENTRY_ADDR 0x80000e00u

#endif
