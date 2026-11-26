/* Small freestanding memory routines; no vendor SDK or heap dependency. */
#ifndef CLEANROOM_FW_MEM_H
#define CLEANROOM_FW_MEM_H

#include "fw_types.h"

void *fw_memcpy(void *dst, const void *src, size_t size);
void *fw_memset(void *dst, int value, size_t size);
int fw_memcmp(const void *a, const void *b, size_t size);

#endif
