/* analogBEAST v1.2 Stage 3 - portable C host port of the Stage 2 format.
 * Freestanding portable core; physical 140KiB mapping and write guards supplied by host/firmware adapter.
 * Format: 140KiB whitelisted region, 2x8KiB manifests, 31x4KiB data sectors.
 * Copyright (c) 2026 analogBEAST contributors. SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef AB12_MD12_H
#define AB12_S12_MD12_H
#include <stdint.h>
#define MD12_SIZE (140u * 1024u)
#define MD12_SECTOR 4096u
#define MD12_PAGE 256u
#define MD12_BANK_SIZE (2u * MD12_SECTOR)
#define MD12_DATA_BASE (2u * MD12_BANK_SIZE)
#define MD12_MAX_ENTRIES 256u
#define MD12_PAT1 1u
#define MD12_SO1 2u
#define MD12_A1 3u
#define MD12_PRE1 4u
#define MD12_OK 0
#define MD12_EIO -1
#define MD12_ECORRUPT -2
#define MD12_EINVAL -3
#define MD12_ENOSPACE -4
#define MD12_EFOREIGN -5
#define MD12_ENOTFOUND -6
#define MD12_EVERIFY -7

typedef struct {
    void *ctx;
    int (*read)(void *ctx, uint32_t offset, void *dst, uint32_t n);
    int (*erase4k)(void *ctx, uint32_t offset);
    int (*program)(void *ctx, uint32_t offset, const void *src, uint32_t n);
} md12_io;

typedef struct {
    uint32_t offset;
    uint32_t length;
    uint16_t number;
    uint8_t type;
    uint8_t flags;
} md12_entry;

/* The caller provides a stable data callback. Called with offsets for one file.
 * To delete, set deleted=1 and leave callback NULL. */
typedef struct {
    uint8_t type, flags;
    uint16_t number;
    uint32_t length;
    uint8_t deleted;
    void *ctx;
    int (*fill)(void *ctx, uint32_t offset, void *dst, uint32_t n);
} md12_change;

/* In-memory directory (2 arrays: active+candidate). Allocate in appropriate
 * static/pool memory, NOT stack, if integrated in later firmware stage. */
typedef struct {
    md12_io io;
    md12_entry active[MD12_MAX_ENTRIES];
    md12_entry candidate[MD12_MAX_ENTRIES];
    uint16_t count;
    uint32_t generation, cursor;
    int8_t active_bank;
} md12_disk;

/* On old ABST/AB5S contents: returns EFOREIGN; never formats/erases them. */
int md12_mount(md12_disk *disk, md12_io io);
int md12_lookup(const md12_disk *disk, uint8_t type, uint16_t number, md12_entry *out);
int md12_read(const md12_disk *disk, uint8_t type, uint16_t number,
              uint32_t offset, void *dst, uint32_t n);
/* Atomic across all changed objects. NO automatic erasure of legacy data. */
int md12_transaction(md12_disk *disk, const md12_change *changes, uint32_t count);
/* Only erases sectors with NO currently committed record overlapping them.
 * Host development helper; production wear/GC scheduler not yet included. */
int md12_sweep_dead(md12_disk *disk, uint32_t *sectors_erased);
const char *md12_error(int error);
#endif
