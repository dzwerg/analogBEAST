/* analogBEAST v1.2 Stage 5 - zero-write legacy verification.
 * SPDX-License-Identifier: GPL-3.0-only
 * Reads the physical 0xA0000..0xDBFFF sample area with a 256-byte buffer.
 */
#ifndef AB12_LEGACY_PREFLIGHT_H
#define AB12_LEGACY_PREFLIGHT_H
#include <stdint.h>
#define AB12_LG_AREA_SIZE 0x3C000u
#define AB12_LG_SECTORS 60u
#define AB12_LG_GOOD 1u
#define AB12_LG_BAD 0u
#define AB12_LG_READ_ERROR 2u

typedef int (*ab12_lg_reader)(void *ctx, uint32_t offset, void *dst, uint32_t length);
typedef struct {
    uint8_t present;      /* recognized magic at expected address */
    uint8_t valid;        /* entire payload, header checksum & lengths valid */
    uint32_t generation;
} ab12_lg_copy;
typedef struct {
    ab12_lg_copy abst[2];
    ab12_lg_copy ab5s[2];
    int8_t selected_abst, selected_ab5s; /* -1 if neither valid */
    uint8_t status;
    uint8_t erased_96k;      /* 24 sectors, A8000-B3FFF and BC000-C7FFF */
    uint8_t erased_tail44k;  /* 11 sectors, D1000-DBFFF */
    uint8_t blank_sectors;   /* of 60 within A0000-DBFFF */
} ab12_lg_report;

/* Does not read/write outside the specified 240KiB legacy area. Never erases or programs. */
void ab12_lg_preflight(ab12_lg_reader rd, void *ctx, ab12_lg_report *out);
#endif
