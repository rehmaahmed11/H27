#include "fw_mem.h"

void *fw_memcpy(void *dst, const void *src, size_t size)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    for (size_t i = 0; i < size; ++i) d[i] = s[i];
    return dst;
}

void *fw_memset(void *dst, int value, size_t size)
{
    u8 *d = (u8 *)dst;
    for (size_t i = 0; i < size; ++i) d[i] = (u8)value;
    return dst;
}

int fw_memcmp(const void *a, const void *b, size_t size)
{
    const u8 *x = (const u8 *)a;
    const u8 *y = (const u8 *)b;
    for (size_t i = 0; i < size; ++i) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}
