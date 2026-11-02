/* analogBEAST Stage13 bank-free MUSIC DISK, contiguous 0x97000..0xDFFFF.
 * 292KiB, 2x8KiB redundant manifests + 69x4KiB data sectors.
 * Host-tested; device mapping and writes are separate opt-in work. */
#ifndef AB13_MD13_H
#define AB13_MD13_H
#include <stdint.h>
#define MD13_SIZE (0xE0000u-0x97000u)
#define MD13_SECTOR 4096u
#define MD13_PAGE 256u
#define MD13_BANK_SIZE (2u * MD13_SECTOR)
#define MD13_DATA_BASE (2u * MD13_BANK_SIZE)
#define MD13_MAX_ENTRIES 384u
#define MD13_PAT1 1u
#define MD13_SO1 2u
#define MD13_A1 3u
#define MD13_PRE1 4u
#define MD13_OK 0
#define MD13_EIO -1
#define MD13_ECORRUPT -2
#define MD13_EINVAL -3
#define MD13_ENOSPACE -4
#define MD13_EFOREIGN -5
#define MD13_ENOTFOUND -6
#define MD13_EVERIFY -7

typedef struct {
    void *ctx;
    int (*read)(void *ctx, uint32_t offset, void *dst, uint32_t n);
    int (*erase4k)(void *ctx, uint32_t offset);
    int (*program)(void *ctx, uint32_t offset, const void *src, uint32_t n);
} md13_io;

typedef struct {
    uint32_t offset;
    uint32_t length;
    uint16_t number;
    uint8_t type;
    uint8_t flags;
} md13_entry;

/* The caller provides a stable data callback. Called with offsets for one file.
 * To delete, set deleted=1 and leave callback NULL. */
typedef struct {
    uint8_t type, flags;
    uint16_t number;
    uint32_t length;
    uint8_t deleted;
    void *ctx;
    int (*fill)(void *ctx, uint32_t offset, void *dst, uint32_t n);
} md13_change;

/* In-memory directory (2 arrays: active+candidate). Allocate in appropriate
 * static/pool memory, NOT stack, if integrated in later firmware stage. */
typedef struct {
    md13_io io;
    md13_entry active[MD13_MAX_ENTRIES];
    md13_entry candidate[MD13_MAX_ENTRIES];
    uint16_t count;
    uint32_t generation, cursor;
    int8_t active_bank;
} md13_disk;

/* On old ABST/AB5S contents: returns EFOREIGN; never formats/erases them. */
int md13_mount(md13_disk *disk, md13_io io);
int md13_lookup(const md13_disk *disk, uint8_t type, uint16_t number, md13_entry *out);
int md13_read(const md13_disk *disk, uint8_t type, uint16_t number,
              uint32_t offset, void *dst, uint32_t n);
/* Atomic across all changed objects. NO automatic erasure of legacy data. */
int md13_transaction(md13_disk *disk, const md13_change *changes, uint32_t count);
/* Only erases sectors with NO currently committed record overlapping them.
 * Host development helper; production wear/GC scheduler not yet included. */
int md13_sweep_dead(md13_disk *disk, uint32_t *sectors_erased);
const char *md13_error(int error);
#endif
