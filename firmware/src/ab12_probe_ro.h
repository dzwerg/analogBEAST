/* analogBEAST v1.2 Stage 4: READ-ONLY diagnostic probe.
 * This file never issues a NOR erase or program command.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef AB12_PROBE_RO_H
#define AB12_PROBE_RO_H
#include <stdint.h>

#define AB12_RO_SIZE (240u*1024u)
#define AB12_RO_DATA_BASE (16u*1024u)
#define AB12_RO_MANIFEST_SIZE (8u*1024u)
#define AB12_RO_MAX_ENTRIES ((AB12_RO_MANIFEST_SIZE - 256u)/12u)

enum ab12_ro_status {
    AB12_RO_EMPTY = 0,
    AB12_RO_LEGACY = 1,
    AB12_RO_VALID = 2,
    AB12_RO_FOREIGN = 3,
    AB12_RO_DAMAGED = 4,
    AB12_RO_IO_ERROR = 5
};

typedef int (*ab12_ro_read)(void *ctx, uint32_t offset, void *dest, uint32_t length);
typedef struct {
    uint8_t status;
    uint8_t active_bank;
    uint16_t files;
    uint32_t generation;
    uint32_t user_bytes;
    uint32_t physical_bytes;
} ab12_ro_report;

/* offset 0 means physical 0xA0000 in analogBEAST. Streaming, low RAM.
 * Validates header, directory, every file header and payload CRC. Never writes.
 * 'DAMAGED' is reported when MD12 markers exist but no valid manifest survives.
 */
void ab12_ro_probe(ab12_ro_read read_fn, void *ctx, ab12_ro_report *out);
const char *ab12_ro_status_name(uint8_t status);
#endif
