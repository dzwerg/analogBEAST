/* analogBEAST v1.2 / Stage12: MD12 A/B transaction engine, manual CDC writes.
 * Include AFTER golana_ui.c; Stage10 shadow stays intact until transaction commit.
 * NO automatic save / no legacy erase / no disk erasure outside the two manifests.
 */
#include "ab12_s12_md12.c"
/* Directory arrays live in POOL: 2 x 256 x 12 bytes = 6144 bytes. */
static md12_disk ab12_s12_disk __attribute__((section(".pool")));
static uint8_t ab12_s12_loaded;
static int32_t ab12_s12_generation(void) { return (int32_t)ab12_s12_disk.generation; }
static int32_t ab12_s12_count(void) { return (int32_t)ab12_s12_disk.count; }
static int32_t ab12_s12_bank(void) { return (int32_t)ab12_s12_disk.active_bank; }
static int32_t ab12_s12_ram_mounted(void) { return (int32_t)ab12_s11_mounted; }
/* Stage11 already allocated an 1100B staging buffer for file payloads. */
static int ab12_s12_io_read(void *ctx, uint32_t off, void *out, uint32_t n)
{
    (void)ctx;
    return ab12_s10_logical_read(off, out, n);
}
static int ab12_s12_io_erase(void *ctx, uint32_t off)
{
    uint32_t sector, phys;
    (void)ctx;
    if (!ab12_s12_modify_active || off >= MD12_DATA_BASE || (off & 4095u)) return -1;
    sector = off / 4096u;
    if (sector >= 4u || ab12_s6_sector(sector, &phys)) return -1;
    return st_erase(phys);
}
static int ab12_s12_io_prog(void *ctx, uint32_t off, const void *src, uint32_t n)
{
    uint32_t phys;
    (void)ctx;
    if (!ab12_s12_modify_active || !n || n > 256u ||
        off >= MD12_SIZE || n > MD12_SIZE - off ||
        (off & 255u) + n > 256u ||
        ab12_s6_sector(off / 4096u, &phys)) return -1;
    return st_prog(phys + off % 4096u, src, n);
}
static md12_io ab12_s12_io(void)
{
    md12_io io;
    io.ctx = 0;
    io.read = ab12_s12_io_read;
    io.erase4k = ab12_s12_io_erase;
    io.program = ab12_s12_io_prog;
    return io;
}
static int ab12_s12_reload(void)
{
    int rc;
    ab12_s12_loaded = 0u;
    if (!flash_ok) return -80;
    rc = md12_mount(&ab12_s12_disk, ab12_s12_io());
    if (rc || ab12_s12_disk.count != 179u || ab12_s12_disk.active_bank < 0)
        return rc ? rc : -81;
    ab12_s12_loaded = 1u;
    return 0;
}
/* Stage11 preview uses these three functions: no uncommitted data is visible. */
static int ab12_s12_verify(uint32_t *complete)
{
    int rc = ab12_s12_reload();
    if (complete) *complete = !rc;
    return rc;
}
static int ab12_s12_lookup(uint8_t type, uint16_t id, uint32_t *offset, uint32_t *len)
{
    md12_entry e;
    int rc;
    if (!ab12_s12_loaded || !offset || !len) return -82;
    rc = md12_lookup(&ab12_s12_disk, type, id, &e);
    if (rc) return rc;
    *offset = e.offset + 16u;
    *len = e.length;
    return 0;
}
static int ab12_s12_file_read(uint8_t type, uint16_t id, uint32_t off, void *out, uint32_t n)
{
    if (!ab12_s12_loaded) return -82;
    return md12_read(&ab12_s12_disk, type, id, off, out, n);
}
static void ab12_s12_put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
/* Export one currently edited RAM object, using the existing Stage11 wire codec.
 * This is a manual SAVE of an existing key only; no new IDs or silent truncation.
 */
static int ab12_s12_export(uint8_t type, uint16_t id, uint32_t *size)
{
    uint32_t i,j,cap;
    uint8_t *b = ab12_s11_body;
    if (!size) return -83;
    if (type == 1u && id >= 1u && id <= 128u) {
        const ab_step_t *steps;
        uint32_t length, resolution;
        if (id <= 64u) {
            const ab_pattern_t *p = &dz_model.patterns[id - 1u];
            cap = 64u; steps = p->step; length = p->length; resolution = p->resolution;
        } else {
            const ab5_pattern_t *p = &ab5_model.extra_patterns[id - 65u];
            cap = 16u; steps = p->steps; length = p->length; resolution = p->resolution;
        }
        if (!length || length > cap || resolution > 9u) return -84;
        b[0] = (uint8_t)length; b[1] = (uint8_t)resolution;
        b[2] = (uint8_t)cap; b[3] = 0u; ab12_s12_put16(b + 4, cap);
        for (i = 0; i < cap; i++) {
            const uint8_t *p = (const uint8_t *)&steps[i];
            if (p[0] > 127u || p[1] > 31u || !p[2] || p[2] > 127u ||
                !p[3] || p[3] > 127u || p[4] > 100u) return -85;
            for (j = 0; j < 5u; j++) b[6u + 5u*i + j] = p[j];
        }
        *size = 6u + cap*5u;
        return 0;
    }
    if (type == 2u && id >= 1u && id <= 32u) {
        const ab5_song_t *s = &ab5_model.songs[id - 1u];
        if (!s->count || s->count > 64u || s->loop > 1u ||
            ab5_extra_validate(&ab5_model)) return -86;
        ab12_s12_put16(b, s->count);
        b[2] = s->loop; b[3] = 0u; ab12_s12_put16(b + 4, 64u);
        for (i = 0; i < 64u; i++) {
            const uint8_t *p = (const uint8_t *)&s->rows[i];
            for (j = 0; j < 4u; j++) b[6u + i*4u + j] = p[j];
        }
        *size = 262u;
        return 0;
    }
    if (type == 3u && id >= 1u && id <= 4u) {
        const ab5_album_t *a = &ab5_model.albums[id - 1u];
        const uint8_t *p = (const uint8_t *)a;
        if (a->count > 32u || a->loop > 1u || ab5_extra_validate(&ab5_model)) return -87;
        for (i = 0; i < 34u; i++) b[i] = p[i];
        *size = 34u;
        return 0;
    }
    return -88; /* presets/arranger and 128-step extensions not yet writable */
}
static int ab12_s12_fill(void *ctx, uint32_t offset, void *out, uint32_t n)
{
    uint8_t *dst = (uint8_t *)out;
    const uint8_t *src = (const uint8_t *)ctx;
    uint32_t i;
    if (!ctx || offset > AB12_S11_BMAX || n > AB12_S11_BMAX - offset) return -1;
    for (i = 0; i < n; i++) dst[i] = src[offset + i];
    return 0;
}
/* Caller must first explicitly mount Stage11 RAM and edit the existing model.
 * No disk write occurs until this function receives the exact explicit YES.
 */
static int ab12_s12_save(uint8_t type, uint16_t id)
{
    md12_change c;
    md12_entry previous;
    uint32_t n;
    int rc;
    if (!ab12_s11_mounted || !flash_ok || song.playing || transport_busy() ||
        gu.save || gu.dz_edit_active || ab12_s10_migration_active) return -90;
    if (ab12_s10_device_ready()) return -91; /* legacy must match original protected CRC */
    rc = ab12_s12_export(type, id, &n);
    if (rc) return rc;
    rc = ab12_s12_reload();
    if (rc) return rc;
    if (md12_lookup(&ab12_s12_disk, type, id, &previous)) return -92;
    c.type = type; c.flags = previous.flags; c.number = id;
    c.length = n; c.deleted = 0u; c.ctx = ab12_s11_body; c.fill = ab12_s12_fill;
    ab12_s12_modify_active = 1u;
    rc = md12_transaction(&ab12_s12_disk, &c, 1u);
    ab12_s12_modify_active = 0u;
    /* Never continue with stale mount state after a partial write. */
    if (ab12_s12_reload() != 0) return -93;
    return rc;
}
