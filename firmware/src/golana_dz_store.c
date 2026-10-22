/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v4: streaming, CRC-checked A/B persistence in user-sample slots.
 * Former project-sector storage is READ ONLY and can seed a fresh v4 model.
 * Never erase any project, preset or original sample header automatically.
 * Included after storage.c, golana_pattern_model.c in felucca.c.
 */
#include "analogbeast_nor_store.c"

static uint8_t dz_store_enabled, dz_store_dirty, dz_store_have_commit, dz_store_error;
/* On a first boot without a verified v4 snapshot, EVERY normal write path is
 * locked. Only an explicitly confirmed USB-MIDI commit may unlock it. */
static uint8_t dz_store_write_locked, dz_store_import_pending;
static uint32_t dz_store_edited_ms, dz_store_last_try_ms;
static uint8_t dz_store_tried;
static ab_nor_state_t dz_store_nor;
static int dz_ab_read(void *ctx, uint32_t off, void *dst, uint32_t n)
{ (void)ctx; return st_read(off, dst, n); }
static int dz_ab_erase(void *ctx, uint32_t off)
{ (void)ctx; return st_erase(off); }
static int dz_ab_program(void *ctx, uint32_t off, const void *src, uint32_t n)
{ (void)ctx; return st_prog(off, src, n); }
static void dz_ab_watchdog(void *ctx) { (void)ctx; fm1_wdt_feed(); }
static const ab_nor_io_t dz_ab_io = {0, dz_ab_read, dz_ab_erase, dz_ab_program, dz_ab_watchdog};

/* Decode ab3.5 PDZ6 format into the new model, without ever writing to
 * the original four Felucca project pairs. This data can still be recovered
 * with original ab3.5 even if a first new-storage write fails. */
#define DZ_OLD_MAGIC 0x365A4450u
#define DZ_OLD_BYTES 3632u
#define DZ_OLD_HALF 32u
#define DZ_OLD_STEP_BYTES (DZ_OLD_HALF * DZ_NSTEP * 3u / 2u)
typedef struct {
    uint32_t magic, generation;
    uint8_t version, half;
    uint8_t bank, pattern, song_mode, loop_mode, song_step, song_count;
    uint8_t length[DZ_OLD_HALF];
    uint8_t step[DZ_OLD_STEP_BYTES];
    uint8_t song[128u * 4u];
} dz_old_page_t;
_Static_assert(sizeof(dz_old_page_t) == DZ_OLD_BYTES, "invalid legacy PDZ6 layout");

static int dz_old_read(uint32_t slot, uint32_t copy, uint32_t half)
{
    st_hdr_t h;
    uint32_t addr = 0x97000u + slot * 2u * ST_SECTOR + copy * ST_SECTOR;
    const dz_old_page_t *p;
    uint32_t i;
    if (st_read(addr, &h, sizeof h) || h.magic != ST_MAGIC ||
        h.type != OBJ_DZ_PAGE0 + half || h.slot != copy ||
        h.len != DZ_OLD_BYTES || h.hcrc != st_crc32(&h, sizeof h - 4u) ||
        st_read(addr + ST_PAYLOAD_OFF, st_buf, h.len) || st_crc32(st_buf, h.len) != h.crc)
        return 0;
    p = (const dz_old_page_t *)st_buf;
    if (p->magic != DZ_OLD_MAGIC || p->version != 2u || p->half != half ||
        !p->generation || p->bank >= DZ_NBANK || p->pattern >= DZ_NPAT_PER_BANK ||
        !p->song_count || p->song_count > 128u) return 0;
    for (i = 0; i < DZ_OLD_HALF; i++)
        if (!p->length[i] || p->length[i] > DZ_NSTEP) return 0;
    return 1;
}
static uint32_t dz_old_common(void)
{
    const dz_old_page_t *p = (const dz_old_page_t *)st_buf;
    uint32_t c = st_crc32_add(0xFFFFFFFFu, &p->bank, 6u);
    return ~st_crc32_add(c, p->song, sizeof p->song);
}
static void dz_old_decode(dz_seq_model_t *m)
{
    const dz_old_page_t *old = (const dz_old_page_t *)st_buf;
    uint32_t j, k, n = 0;
    for (j = 0; j < DZ_OLD_HALF; j++) {
        dz_pattern_t *pat = &m->patterns[(uint32_t)old->half * DZ_OLD_HALF + j];
        pat->length = old->length[j];
        /* Old RES was a synth preset global and cannot be recovered per bank. */
        pat->resolution = AB_SONG_DEFAULT_RES;
        for (k = 0; k < DZ_NSTEP; k += 2u, n += 3u) {
            uint16_t x = (uint16_t)old->step[n] | (uint16_t)(old->step[n+1u] & 15u) << 8;
            uint16_t y = (uint16_t)(old->step[n+1u] >> 4) | (uint16_t)old->step[n+2u] << 4;
            pat->step[k].note = (uint8_t)(x & 127u);
            pat->step[k].flags = (uint8_t)(x >> 7);
            pat->step[k+1u].note = (uint8_t)(y & 127u);
            pat->step[k+1u].flags = (uint8_t)(y >> 7);
            /* VELO/GATE/SWING defaults were set by ab_snapshot_init. */
        }
    }
    m->bank = old->bank;
    m->pattern = old->pattern;
    /* Old Song GATE occupies opcode 8 now used by ARP; don't import song.
     * The user explicitly approved reimporting its 18-row arrangement. */
    m->song_mode = 0u;
}
static int dz_old_try_import(dz_seq_model_t *m)
{
    uint32_t gen[2][4][2] = {{{0}}}, crc[2][4][2] = {{{0}}};
    uint32_t half, slot, copy, s0, c0, s1, c1, best = 0;
    uint8_t found = 0, bs0=0, bs1=0, bc0=0, bc1=0;
    for (half = 0; half < 2u; half++)
        for (slot = 0; slot < 4u; slot++)
            for (copy = 0; copy < 2u; copy++)
                if (dz_old_read(slot, copy, half)) {
                    const dz_old_page_t *p = (const dz_old_page_t *)st_buf;
                    gen[half][slot][copy] = p->generation;
                    crc[half][slot][copy] = dz_old_common();
                }
    for (s0 = 0; s0 < 4u; s0++) for (c0 = 0; c0 < 2u; c0++)
        for (s1 = 0; s1 < 4u; s1++) for (c1 = 0; c1 < 2u; c1++) {
            uint32_t g = gen[0][s0][c0];
            if (s0 != s1 && g && g == gen[1][s1][c1] &&
                crc[0][s0][c0] == crc[1][s1][c1] &&
                (!found || (int32_t)(g - best) > 0)) {
                bs0 = (uint8_t)s0; bc0 = (uint8_t)c0;
                bs1 = (uint8_t)s1; bc1 = (uint8_t)c1;
                best = g; found = 1u;
            }
        }
    if (!found) return 0;
    if (!dz_old_read(bs0, bc0, 0u)) return 0;
    dz_old_decode(m);
    if (!dz_old_read(bs1, bc1, 1u)) { ab_snapshot_init(m); return 0; }
    dz_old_decode(m);
    return ab_snapshot_validate(m) == 0;
}
static void dz_store_boot(dz_seq_model_t *m)
{
    int rc;
    dz_store_enabled = dz_store_dirty = dz_store_have_commit = dz_store_error = 0u;
    dz_store_write_locked = 1u; dz_store_import_pending = 0u;
    dz_store_tried = 0u;
    dz_store_last_try_ms = dz_store_edited_ms = 0u;
    memset(&dz_store_nor, 0, sizeof dz_store_nor);
    if (!flash_ok) return;
    rc = ab_nor_load(&dz_ab_io, &dz_store_nor, ab_snapshot_put, m);
    if (rc == 0 && ab_snapshot_validate(m) == 0) {
        dz_store_have_commit = dz_store_enabled = dz_store_nor.sample_headers_blank;
        dz_store_write_locked = !dz_store_enabled;
        return;
    }
    ab_snapshot_init(m); /* Never expose a partially read/corrupt object. */
    /* Legacy PDZ6 lives in the old Felucca project sectors, NOT in the sample
     * slots. Read it independently of the new NOR ownership checks. A sample
     * header or foreign new-storage sector must NEVER conceal recoverable
     * A01-D16 patterns; importing to RAM does not authorize a single write. */
    if (dz_old_try_import(m)) {
        dz_store_import_pending = 1u;
        dz_store_dirty = 0u;
    }
    if (rc < 0 || ab_nor_probe(&dz_ab_io, &dz_store_nor)) { dz_store_error = 1u; return; }
    if (!dz_store_nor.sample_headers_blank || dz_store_nor.foreign_copy[0] || dz_store_nor.foreign_copy[1] ||
        dz_store_nor.valid) {
        /* Sample slots/foreign records are not safe NOR targets. A corrupt or
         * semantically invalid v4 record also stays untouched. READ ONLY. */
        return;
    }
    dz_store_enabled = 1u;
}
static void dz_store_mark_dirty(void)
{
    dz_store_dirty = 1u;
    dz_store_edited_ms = fm1_ms;
}
/* Main loop only; never erase flash in REC/PLAY/SAVE/USB export. */
static void dz_store_poll(const dz_seq_model_t *m, int allowed)
{
    int rc;
    if (dz_store_write_locked || !dz_store_dirty || !dz_store_enabled || !flash_ok || !allowed ||
        transport_busy() || fm1_ms - dz_store_edited_ms < 1500u ||
        (dz_store_tried && fm1_ms - dz_store_last_try_ms < 5000u)) return;
    dz_store_tried = 1u;
    dz_store_last_try_ms = fm1_ms;
    if (ab_snapshot_validate(m)) { dz_store_error = 1u; return; }
    rc = ab_nor_save(&dz_ab_io, &dz_store_nor, AB_SNAPSHOT_DATA_BYTES, ab_snapshot_get, (void *)m);
    if (rc) { dz_store_error = 1u; return; }
    dz_store_have_commit = dz_store_nor.valid;
    dz_store_error = 0u;
    dz_store_dirty = 0u;
}

/* Called ONLY by authenticated-by-human SysEx op 19, never by autosave,
 * an importer, RESET, a button gesture or the 1.5-second timer.
 * rc: 0 confirmed, 1 invalid, 3 unavailable, 4 flash failure.
 * A failed commit stays locked; no scheduled retry can write flash. */
static uint8_t dz_store_explicit_commit(const dz_seq_model_t *m)
{
    int rc;
    if (!dz_store_write_locked || !dz_store_enabled || !flash_ok) return 3u;
    if (transport_busy() || ab_snapshot_validate(m)) return 1u;
    rc = ab_nor_save(&dz_ab_io, &dz_store_nor, AB_SNAPSHOT_DATA_BYTES,
                     ab_snapshot_get, (void *)m);
    if (rc || !dz_store_nor.valid) {
        dz_store_error = 1u;
        return 4u;
    }
    /* ab_nor_save checks the committed header and readback payload CRC.
     * Unlock only after the NOR has confirmed the entire snapshot. */
    dz_store_have_commit = 1u;
    dz_store_error = 0u;
    dz_store_dirty = 0u;
    dz_store_tried = 0u;
    dz_store_write_locked = 0u;
    dz_store_import_pending = 0u;
    return 0u;
}
