/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v5 Stage 2. ISOLATED HOST-TESTED driver, NOT connected to FM-1.
 * 32 KiB in third freed sample slot; NEVER erase PDZ6 project sectors.
 */
#ifndef ANALOGBEAST_V5_NOR_H
#define ANALOGBEAST_V5_NOR_H
#include <stdint.h>
#include "analogbeast_v5_extra.h"
#include "analogbeast_nor_store.h" /* reuse the existing v4 read/erase/program IO ABI */

#define AB5_NOR_MAGIC 0x53354241u /* bytes "AB5S" */
#define AB5_NOR_FORMAT 1u
#define AB5_NOR_COMMIT 0x5AA55AA5u
#define AB5_NOR_HEADER_BYTES 32u
#define AB5_NOR_DATA_OFFSET 256u
#define AB5_NOR_SECTOR_BYTES 4096u
#define AB5_NOR_PAGE_BYTES 256u
#define AB5_NOR_PAYLOAD_BYTES AB5_EXTRA_WIRE_BYTES
#define AB5_NOR_COPY_EMPTY 0u
#define AB5_NOR_COPY_VALID 1u
#define AB5_NOR_COPY_TORN 2u
#define AB5_NOR_COPY_FOREIGN 3u

typedef struct {
    uint8_t valid, active_copy;
    uint8_t copy_kind[2]; /* EMPTY, VALID, TORN AB5S, or FOREIGN/PDZ6 */
    uint32_t generation;
} ab5_nor_state_t;

/* All probing and loading is READ ONLY. No implicit reclamation of old PDZ6! */
int ab5_nor_probe(const ab_nor_io_t *io, ab5_nor_state_t *state);
int ab5_nor_load(const ab_nor_io_t *io, ab5_nor_state_t *state,
                 ab_write_snapshot_fn put, void *ctx);
/* Writes one entire inactive 16-KiB copy only if already owned by v5 or blank.
 * On FIRST save, BOTH copies and the surrounding third sample slot must
 * be completely blank. No PDZ6 migration/reclamation is necessary.
 * Returns a negative code without any erase when blocked by foreign data.
 * A power cut mid-save leaves the previously valid copy untouched. */
int ab5_nor_save(const ab_nor_io_t *io, ab5_nor_state_t *state,
                 ab_read_snapshot_fn get, void *ctx);
#endif
