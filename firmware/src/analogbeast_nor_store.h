/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST sample-slot-backed, crash-resistant A/B NOR snapshots.
 * Each 80 KiB sample slot's FIRST 4 KiB always stays ERASED; stock Felucca's
 * FSMP parser therefore sees an empty sample. This is a compatibility measure,
 * not a guarantee for closed-source/other firmware.
 * IMPORTANT: code never erases sample headers or "foreign" sample sectors.
 */
#ifndef ANALOGBEAST_NOR_STORE_H
#define ANALOGBEAST_NOR_STORE_H
#include <stdint.h>

#define AB_NOR_SECTOR 4096u
#define AB_NOR_PAGE 256u
#define AB_SAMPLE_START 0xA0000u
#define AB_SAMPLE_STRIDE 0x14000u
#define AB_SAMPLE_COUNT 3u
#define AB_COPY_COUNT 2u
#define AB_RECORD_SECTOR_OFFSET 0x1000u
#define AB_PAYLOAD_OFFSET 0x2000u
#define AB_PAYLOAD_CAPACITY (AB_SAMPLE_STRIDE - AB_PAYLOAD_OFFSET)  /* 72 KiB each */
#define AB_STORE_MAGIC 0x54534241u /* bytes: 'A' 'B' 'S' 'T' */
#define AB_STORE_FORMAT 1u

typedef struct {
    void *ctx;
    int (*read)(void *ctx, uint32_t off, void *dst, uint32_t len);
    int (*erase4k)(void *ctx, uint32_t off);
    int (*program)(void *ctx, uint32_t off, const void *src, uint32_t len);
    void (*feed_watchdog)(void *ctx);
} ab_nor_io_t;

typedef struct {
    uint8_t valid;
    uint8_t active_copy;
    uint8_t sample_headers_blank;
    uint8_t foreign_copy[2];
    uint32_t generation;
    uint32_t payload_len;
} ab_nor_state_t;

/* get supplies n bytes at offset from a deterministic snapshot, without a
 * second 20+ KiB buffer. put receives bytes read back from flash on load. */
typedef int (*ab_read_snapshot_fn)(void *ctx, uint32_t offset, uint8_t *dst, uint32_t n);
typedef int (*ab_write_snapshot_fn)(void *ctx, uint32_t offset, const uint8_t *src, uint32_t n);

/* Returns 0 if probe completed; state.valid is 0 on an empty store. */
int ab_nor_probe(const ab_nor_io_t *io, ab_nor_state_t *state);
/* Returns 0 when a valid snapshot was recovered, 1 when there is none. */
int ab_nor_load(const ab_nor_io_t *io, ab_nor_state_t *state,
                ab_write_snapshot_fn put, void *ctx);
/* Refuses non-erased sample headers and foreign target records. Requires
 * idle/transport-stopped/REC-exited checks by the caller. */
int ab_nor_save(const ab_nor_io_t *io, ab_nor_state_t *state,
                uint32_t length, ab_read_snapshot_fn get, void *ctx);
/* Destructive: wipe exactly the three 80 KiB user sample slots. The caller
 * MUST require explicit user confirmation and verified external export. Not
 * invoked implicitly at boot, save, or firmware update. */
int ab_nor_erase_sample_slots(const ab_nor_io_t *io);
#endif
