/* analogBEAST v1.2 Stage 6: read-only safe-staging sector map.
 * SPDX-License-Identifier: GPL-3.0-only
 * No NOR programming/erase functions are provided here.
 */
#ifndef AB12_STAGE6_MAP_H
#define AB12_STAGE6_MAP_H
#include <stdint.h>
#define AB12_S6_SECTOR 4096u
#define AB12_S6_TOTAL_BYTES (140u * 1024u)
#define AB12_S6_SAFE_SECTORS 35u
#define AB12_S6_PHYSICAL_BASE 0xA0000u
#define AB12_S6_PHYSICAL_END 0xDC000u
#define AB12_S6_OK 0
#define AB12_S6_EINVAL (-1)
#define AB12_S6_EIO (-2)
#define AB12_S6_OCCUPIED (-3)
typedef int (*ab12_s6_reader)(void *ctx, uint32_t physical_address,
                              void *dst, uint32_t n);
typedef struct {
    uint8_t valid;
    uint8_t blanks;
    uint8_t occupied;
    uint8_t unreadable;
    uint32_t capacity;
} ab12_s6_report;
int ab12_s6_sector(uint32_t logical_sector, uint32_t *physical_address);
int ab12_s6_read(ab12_s6_reader reader, void *ctx, uint32_t logical_offset,
                  void *dst, uint32_t length);
int ab12_s6_preflight(ab12_s6_reader reader, void *ctx, ab12_s6_report *report);
#endif
