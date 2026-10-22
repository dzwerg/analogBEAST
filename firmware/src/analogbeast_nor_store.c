/* SPDX-License-Identifier: GPL-3.0-only */
#include "analogbeast_nor_store.h"

/* Commit goes LAST in its own 4 KiB sector. No pointer into a flash/XIP area
 * is used while an erase/program callback has switched off XIP. */
typedef struct {
    uint32_t magic, format, generation, length, payload_crc;
    uint32_t reserved[2], header_crc;
} ab_record_hdr_t;
_Static_assert(sizeof(ab_record_hdr_t) == 32u, "NOR commit header must fit 32 bytes");
static uint32_t slot_base(unsigned slot) { return AB_SAMPLE_START + (uint32_t)slot * AB_SAMPLE_STRIDE; }
static uint32_t copy_record(unsigned c) { return slot_base(c) + AB_RECORD_SECTOR_OFFSET; }
static uint32_t copy_body(unsigned c) { return slot_base(c) + AB_PAYLOAD_OFFSET; }

static uint32_t crc_add(uint32_t c, const void *data, uint32_t len)
{
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x4DB26158u, 0x5005713Cu, 0xEDB88320u,
        0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0xA00AE278u,
        0xBDBDF21Cu, 0x6B6B51F4u, 0xB1D29AFAu, 0x2C6B0A9Eu
    };
    const uint8_t *p = (const uint8_t *)data;
    while (len--) {
        c ^= *p++;
        c = (c >> 4) ^ t[c & 15u];
        c = (c >> 4) ^ t[c & 15u];
    }
    return c;
}
static uint32_t crc32(const void *p, uint32_t len) { return ~crc_add(0xFFFFFFFFu, p, len); }
static void feed(const ab_nor_io_t *io) { if (io->feed_watchdog) io->feed_watchdog(io->ctx); }
static int well_formed(const ab_nor_io_t *io)
{
    return io && io->read && io->erase4k && io->program;
}
static int blank_sector(const ab_nor_io_t *io, uint32_t addr)
{
    uint32_t off, j;
    uint8_t buf[AB_NOR_PAGE];
    for (off = 0; off < AB_NOR_SECTOR; off += sizeof buf) {
        if (io->read(io->ctx, addr + off, buf, sizeof buf)) return -1;
        for (j = 0; j < sizeof buf; j++) if (buf[j] != 0xFFu) return 0;
    }
    return 1;
}
static int read_header(const ab_nor_io_t *io, unsigned c, ab_record_hdr_t *hdr)
{
    if (io->read(io->ctx, copy_record(c), hdr, sizeof *hdr)) return -1;
    if (hdr->magic != AB_STORE_MAGIC || hdr->format != AB_STORE_FORMAT ||
        !hdr->generation || !hdr->length || hdr->length > AB_PAYLOAD_CAPACITY ||
        hdr->header_crc != crc32(hdr, sizeof *hdr - 4u)) return 1;
    return 0;
}
static int body_crc(const ab_nor_io_t *io, unsigned c, uint32_t len, uint32_t *out)
{
    uint32_t off, sum = 0xFFFFFFFFu;
    uint8_t buf[AB_NOR_PAGE];
    for (off = 0; off < len; off += AB_NOR_PAGE) {
        uint32_t n = len - off < AB_NOR_PAGE ? len - off : AB_NOR_PAGE;
        if (io->read(io->ctx, copy_body(c) + off, buf, n)) return -1;
        sum = crc_add(sum, buf, n);
        feed(io);
    }
    *out = ~sum;
    return 0;
}
static int copy_valid(const ab_nor_io_t *io, unsigned c, ab_record_hdr_t *hdr)
{
    uint32_t crc;
    int r = read_header(io, c, hdr);
    if (r) return r;
    if (body_crc(io, c, hdr->length, &crc)) return -1;
    return crc == hdr->payload_crc ? 0 : 1;
}
int ab_nor_probe(const ab_nor_io_t *io, ab_nor_state_t *state)
{
    ab_record_hdr_t h[2];
    uint8_t valid[2] = {0,0};
    uint32_t i;
    if (!well_formed(io) || !state) return -1;
    memset(state, 0, sizeof *state);
    state->sample_headers_blank = 1u;
    for (i = 0; i < AB_SAMPLE_COUNT; i++) {
        int b = blank_sector(io, slot_base(i));
        if (b < 0) return -2;
        if (!b) state->sample_headers_blank = 0;
    }
    for (i = 0; i < AB_COPY_COUNT; i++) {
        int rc = copy_valid(io, i, &h[i]);
        if (rc < 0) return -2;
        valid[i] = rc == 0;
        if (!valid[i]) {
            int blank = blank_sector(io, copy_record(i));
            if (blank < 0) return -2;
            state->foreign_copy[i] = !blank && h[i].magic != AB_STORE_MAGIC;
        }
    }
    if (valid[0] || valid[1]) {
        unsigned selected = !valid[0] ? 1 : !valid[1] ? 0 :
            (int32_t)(h[1].generation - h[0].generation) > 0 ? 1 : 0;
        state->valid = 1u;
        state->active_copy = (uint8_t)selected;
        state->generation = h[selected].generation;
        state->payload_len = h[selected].length;
    }
    return 0;
}
int ab_nor_load(const ab_nor_io_t *io, ab_nor_state_t *state,
                ab_write_snapshot_fn put, void *ctx)
{
    uint32_t off;
    uint8_t buf[AB_NOR_PAGE];
    if (!put || ab_nor_probe(io, state)) return -1;
    if (!state->valid) return 1;
    for (off = 0; off < state->payload_len; off += AB_NOR_PAGE) {
        uint32_t n = state->payload_len - off < AB_NOR_PAGE ? state->payload_len - off : AB_NOR_PAGE;
        if (io->read(io->ctx, copy_body(state->active_copy) + off, buf, n) || put(ctx, off, buf, n)) return -2;
    }
    return 0;
}
int ab_nor_save(const ab_nor_io_t *io, ab_nor_state_t *state,
                uint32_t length, ab_read_snapshot_fn get, void *ctx)
{
    ab_record_hdr_t h, check;
    uint8_t buf[AB_NOR_PAGE];
    uint32_t n, off, erase_end, rolling = 0xFFFFFFFFu, check_crc;
    unsigned c;
    if (!get || !length || length > AB_PAYLOAD_CAPACITY || ab_nor_probe(io, state)) return -1;
    if (!state->sample_headers_blank) return -3; /* no FSMP/foreign header overwrite */
    c = state->valid ? (unsigned)(1u - state->active_copy) : 0u;
    if (state->foreign_copy[c]) return -4; /* don't overwrite a foreign slot */
    erase_end = (length + AB_NOR_SECTOR - 1u) / AB_NOR_SECTOR;
    /* All target payload sectors are erased before touching the commit sector.
     * The other copy, if any, remains untouched through a failed write. */
    for (off = 0; off < erase_end; off++) {
        if (io->erase4k(io->ctx, copy_body(c) + off * AB_NOR_SECTOR)) return -5;
        feed(io);
    }
    for (off = 0; off < length; off += AB_NOR_PAGE) {
        n = length - off < AB_NOR_PAGE ? length - off : AB_NOR_PAGE;
        if (get(ctx, off, buf, n)) return -6;
        rolling = crc_add(rolling, buf, n);
        if (io->program(io->ctx, copy_body(c) + off, buf, n)) return -7;
        feed(io);
    }
    if (body_crc(io, c, length, &check_crc) || check_crc != ~rolling) return -8;
    if (io->erase4k(io->ctx, copy_record(c))) return -9;
    h.magic = AB_STORE_MAGIC;
    h.format = AB_STORE_FORMAT;
    h.generation = state->valid ? state->generation + 1u : 1u;
    if (!h.generation) h.generation = 1u;
    h.length = length;
    h.payload_crc = ~rolling;
    h.reserved[0] = h.reserved[1] = 0xFFFFFFFFu;
    h.header_crc = crc32(&h, sizeof h - 4u);
    if (io->program(io->ctx, copy_record(c), &h, sizeof h)) return -10;
    if (read_header(io, c, &check) || memcmp(&h, &check, sizeof h) ||
        body_crc(io, c, length, &check_crc) || check_crc != h.payload_crc) return -11;
    state->valid = 1;
    state->active_copy = (uint8_t)c;
    state->generation = h.generation;
    state->payload_len = length;
    return 0;
}
int ab_nor_erase_sample_slots(const ab_nor_io_t *io)
{
    uint32_t i, sector;
    if (!well_formed(io)) return -1;
    for (i = 0; i < AB_SAMPLE_COUNT; i++)
        for (sector = 0; sector < AB_SAMPLE_STRIDE; sector += AB_NOR_SECTOR) {
            if (io->erase4k(io->ctx, slot_base(i) + sector)) return -2;
            feed(io);
        }
    for (i = 0; i < AB_SAMPLE_COUNT; i++)
        if (blank_sector(io, slot_base(i)) != 1) return -3;
    return 0;
}
