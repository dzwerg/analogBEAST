/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v4 Stage 3 – editor SysEx command 96.
 * Request: op, token, arguments (all bytes 7-bit); response: op, token, rc, data.
 * rc: 0 OK, 1 invalid, 2 busy, 3 RAM-only, 4 flash error.
 * v3 0..9 opcodes kept for older backup tools and 128-row chunks.
 * v4 10..17 access 256 rows, extended 5-field steps, and pattern resolution.
 * v4 18: read migration lock status; 19: explicitly confirmed NOR commit.
 * Every received data byte is 7-bit; offsets/counts use explicit two-byte
 * little-endian base-128 representation, never silent wrapping.
 * The song value remains 8-bit; two 7-bit bytes are wire encoding only.
 * Legacy op 2/3 pattern transfers are retained; they modify NOTE/FLAGS only.
 * Included after golana_ui.c; executed in main loop, never in audio ISR.
 */
#include "analogbeast_v5_transfer.c"
/* Validate one entire row before writing ANY rows in a chunk. */
static int dz_midi_row_valid(uint32_t kind, uint32_t pattern, uint32_t steps, uint32_t value)
{
    if (!steps || steps > DZ_NSTEP || value > 255u) return 0;
    if (kind < DZ_NBANK) return pattern < DZ_NPAT_PER_BANK && value == 0u;
    if (kind == DZ_SONG_END_BANK) return pattern == DZ_SONG_END_PATTERN && value == 0u;
    if (pattern != 0u) return 0;
    if (kind == DZ_SONG_TEMPO) return value >= DZ_TEMPO_MIN && value <= DZ_TEMPO_MAX;
    if (kind == DZ_SONG_PRESET) return value < DZ_PRESET_COUNT;
    if (kind == DZ_SONG_REST) return value == 0u;
    if (kind == DZ_SONG_ARP) return value <= 1u;
    return 0;
}
static uint16_t dz_wire14(const uint8_t *p)
{ return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 7); }

static uint32_t dz_midi_request(const uint8_t *a, uint32_t n, uint8_t *out)
{
    uint32_t op = n ? a[0] : 127u, i, k, len = 3u;
    out[0] = (uint8_t)op; out[1] = n > 1u ? a[1] : 0u; out[2] = 1u;
    if (n < 2u) return len;
    for (i = 0; i < n; i++) if (a[i] > 127u) return len;
    if (!op && n == 2u) {
        out[2] = 0; out[3] = 5;  /* Stage 3 protocol: new opcodes 10..17 */
        out[4] = dz_store_enabled; out[5] = dz_store_dirty;
        out[6] = dz_store_error; out[7] = dz_store_have_commit;
        out[8] = DZ_NPAT; out[9] = 0u; /* legacy op 4/5 address only 0..127 */
        out[10] = 3u; /* capability bits: v4 256-row + ARP, no legacy song GATE */
        out[11] = DZ_NSONG & 127u; out[12] = DZ_NSONG >> 7;
        return 13u;
    }
    if (op == 18u && n == 2u) { /* READ ONLY: v4 first-boot safety state */
        out[2] = 0u;
        out[3] = dz_store_write_locked;
        out[4] = dz_store_import_pending;
        out[5] = dz_store_enabled;
        out[6] = dz_store_dirty;
        out[7] = dz_store_have_commit;
        out[8] = dz_store_error;
        return 9u;
    }
    if (op == 20u && n == 2u) { /* v5 READ ONLY: boot status, no v5 write protocol */
        out[2] = 0u;
        out[3] = ab5_boot_status;
        out[4] = ab5_state.copy_kind[0];
        out[5] = ab5_state.copy_kind[1];
        out[6] = ab5_state.valid;
        out[7] = AB5_EXTRA_PATTERNS;
        out[8] = AB5_SONGS;
        out[9] = AB5_ALBUMS;
        return 10u;
    }
    if (!gu.ready || transport_busy() || gu.dz_edit_active || gu.save || dz_play_has_backup) {
        out[2] = 2; return len;
    }
    /* V5 op21/22: full E-H 16-step pattern transfer over the existing
     * SysEx command 96. No extra buffer, no touching old A-D steps.
     * 21: [bank4..7,pattern0..15,length1..16,res0..9, 16*5 step bytes].
     * 22: [bank4..7,pattern0..15] -> same model in reply. */
    if(op==21u || op==22u) {
        ab5_pattern_t *p;
        uint32_t j,base;
        if (!ab5_store_enabled ||
            (ab5_boot_status!=AB5_BOOT_V4_PREVIEW && ab5_boot_status!=AB5_BOOT_V5_VALID))
            {out[2]=3u;return len;}
        if(n<4u || a[2]<4u || a[2]>=8u || a[3]>=16u)return len;
        p=&ab5_model.extra_patterns[(a[2]-4u)*16u+a[3]];
        if(op==21u) {
            if(n!=86u || !a[4] || a[4]>16u || a[5]>5u)return len;
            for(j=0;j<16u;j++) {
                base=6u+j*5u;
                if(a[base+1u]>31u || !a[base+2u] || !a[base+3u] || a[base+4u]>100u)
                    return len; /* all steps validated before mutation */
            }
            p->length=a[4];p->resolution=a[5];
            for(j=0;j<16u;j++) {
                base=6u+j*5u;
                p->steps[j].note=a[base];p->steps[j].flags=a[base+1u];
                p->steps[j].velocity=a[base+2u];p->steps[j].gate=a[base+3u];
                p->steps[j].swing=a[base+4u];
            }
            ab5_mark_dirty();ab5_prepared_valid=0u;
            out[len++]=a[2];out[len++]=a[3];
            out[2]=0u;return len;
        }
        if(n!=4u)return len;
        out[len++]=a[2];out[len++]=a[3];
        out[len++]=p->length;out[len++]=p->resolution;
        for(j=0;j<16u;j++) {
            const ab_step_t *st=&p->steps[j];
            out[len++]=st->note;out[len++]=st->flags;
            out[len++]=st->velocity;out[len++]=st->gate;
            out[len++]=st->swing;
        }
        out[2]=0u;return len;
    }
    /* v1.0: full v5 Song/Album read/write + device-side Song range copy.
     * All writes are validated BEFORE mutation, in main loop, not audio ISR.
     * Existing v4 (0..19) and pattern (21/22) opcodes are unchanged. */
    if (op>=23u && op<=30u) {
        int nr;
        if(op==30u) { /* Read-only v5 persistence status. */
            if(n!=2u)return len;
            out[2]=0u;
            out[len++]=ab5_dirty;out[len++]=ab5_store_error;
            out[len++]=ab5_store_enabled;out[len++]=ab5_state.valid;
            out[len++]=ab5_boot_status;
            return len;
        }
        if (ab5_boot_status!=AB5_BOOT_V4_PREVIEW &&
            ab5_boot_status!=AB5_BOOT_V5_VALID) {out[2]=3u;return len;}
        if ((op==23u || op==25u || op==27u || op==29u) &&
            !ab5_store_enabled) {out[2]=3u;return len;}
        nr=ab5_xfer_exec(&ab5_model,op,a+2u,n-2u,out+len);
        if(nr<0)return len;
        len+=(uint32_t)nr;
        if(op==23u || op==25u || op==27u || op==29u) {
            ab5_mark_dirty();ab5_prepared_valid=0u;
        }
        out[2]=0u;
        return len;
    }
    if (op == 19u) { /* Explicit one-shot commit after external read-only backup */
        static const uint8_t key[9] = {'C','O','M','M','I','T',' ','V','4'};
        if (n != 11u || memcmp(a + 2u, key, sizeof key)) return len;
        out[2] = dz_store_explicit_commit(&dz_model);
        if (!out[2]) {
            out[len++] = dz_store_have_commit;
            out[len++] = dz_store_write_locked;
        }
        return len;
    }
    if (op == 1u) {
        /* v3's destructive project-sector CLEAR is retired. NEVER erase
         * legacy project sectors while migrating to user-sample NOR. */
        return len;
    }
    if (op == 2u || op == 3u) { /* pattern write/read; original wire format */
        if (n < 4u || a[2] >= DZ_NBANK || a[3] >= DZ_NPAT_PER_BANK) return len;
        k = (uint32_t)a[2] * 16u + a[3];
        if (op == 2u) {
            if (n != 133u || !a[4] || a[4] > 64u) return len;
            for (i = 0; i < 64u; i++) if (a[6u + 2u*i] > 31u) return len;
            dz_model.patterns[k].length = a[4];
            for (i = 0; i < 64u; i++) {
                dz_model.patterns[k].step[i].note = a[5u + 2u*i];
                dz_model.patterns[k].step[i].flags = a[6u + 2u*i];
            }
            dz_store_mark_dirty();
        } else {
            if (n != 4u) return len;
            out[len++] = dz_model.patterns[k].length;
            for (i = 0; i < 64u; i++) {
                out[len++] = dz_model.patterns[k].step[i].note;
                out[len++] = dz_model.patterns[k].step[i].flags;
            }
        }
    } else if (op == 4u || op == 5u) { /* v3 song-row chunks, max 24 x 5 bytes */
        uint32_t start, count;
        if (n < 4u) return len;
        start = a[2]; count = a[3];
        if (start >= 128u || !count || count > 24u || start + count > 128u) return len;
        if (op == 4u) {
            if (n != 4u + 5u * count) return len;
            /* Validate entire chunk before changing even a single row. */
            for (i = 0; i < count; i++) {
                uint32_t off = 4u + 5u*i, bank = a[off], pat = a[off+1u],
                         steps = a[off+2u], val = a[off+3u] | (uint32_t)a[off+4u]<<7;
                if (!steps || steps > DZ_NSTEP ||
                    !((bank < DZ_NBANK && pat < DZ_NPAT_PER_BANK && val == 0u) ||
                      (bank == DZ_SONG_END_BANK && pat == DZ_SONG_END_PATTERN && val == 0u) ||
                      (bank == DZ_SONG_TEMPO && pat == 0u && val >= DZ_TEMPO_MIN && val <= DZ_TEMPO_MAX) ||
                      (bank == DZ_SONG_PRESET && pat == 0u && val < DZ_PRESET_COUNT) ||
                      (bank == DZ_SONG_REST && pat == 0u && val == 0u) ||
                      (bank == DZ_SONG_ARP && pat == 0u && val <= 1u))) return len;
            }
            for (i = 0; i < count; i++) {
                uint32_t off = 4u + 5u*i;
                dz_song_entry_t *e = &dz_model.song[start+i];
                e->bank = a[off]; e->pattern = a[off+1u]; e->play_steps = a[off+2u];
                e->value = (uint8_t)(a[off+3u] | (uint32_t)a[off+4u]<<7);
            }
            dz_store_mark_dirty();
        } else {
            if (n != 4u) return len;
        }
        out[len++] = (uint8_t)start; out[len++] = (uint8_t)count;
        if (op == 5u)
            for (i = 0; i < count; i++) {
                const dz_song_entry_t *e = &dz_model.song[start+i];
                out[len++] = e->bank; out[len++] = e->pattern;
                out[len++] = e->play_steps;
                out[len++] = e->value & 127u; out[len++] = e->value >> 7;
            }
    } else if (op == 6u && n == 2u) { /* poll verified flash autosave */
        if (!dz_store_enabled || dz_store_write_locked) { out[2] = 3; return len; }
        dz_store_poll(&dz_model, 1);
        if (dz_store_error) { out[2] = 4; return len; }
        out[len++] = dz_store_dirty; out[len++] = dz_store_have_commit;
    } else if (op == 7u) { /* set song metadata: count 0=128, loop, song_mode */
        uint32_t count;
        if (n != 5u || a[3] > 1u || a[4] > 1u) return len;
        count = a[2] ? a[2] : 128u; /* legacy metadata command */
        if (count > DZ_NSONG) return len;
        dz_model.song_count = (uint16_t)count;
        dz_model.loop_mode = a[3]; dz_model.song_mode = a[4];
        dz_model.song_step = 0u;
        dz_store_mark_dirty();
        out[len++] = a[2]; out[len++] = a[3]; out[len++] = a[4];
    } else if (op == 8u && n == 2u) { /* read song metadata */
        out[len++] = dz_model.song_count >= 128u ? 0u : (uint8_t)dz_model.song_count;
        out[len++] = dz_model.loop_mode; out[len++] = dz_model.song_mode;
    } else if (op == 9u) { /* reset DZ-only RAM model; no sector erase, never touch presets */
        static const uint8_t key[] = {82, 69, 83, 69, 84}; /* RESET */
        if (n != 7u || memcmp(a+2, key, 5u)) return len;
        dz_seq_model_init(&dz_model);
        dz_store_mark_dirty();
    } else if (op == 10u || op == 11u) { /* v4 song rows, 256 addresses, chunks up to 24 */
        uint32_t start, count;
        if (n < 5u) return len;
        start = dz_wire14(a + 2u);
        count = a[4];
        if (start >= DZ_NSONG || !count || count > 24u || start + count > DZ_NSONG) return len;
        if (op == 10u) {
            if (n != 5u + 5u * count) return len;
            for (i = 0; i < count; ++i) {
                uint32_t off = 5u + 5u * i;
                if (!dz_midi_row_valid(a[off], a[off+1u], a[off+2u], dz_wire14(a + off + 3u)))
                    return len;
            }
            for (i = 0; i < count; ++i) {
                uint32_t off = 5u + 5u * i;
                dz_song_entry_t *e = &dz_model.song[start + i];
                e->bank = a[off]; e->pattern = a[off+1u];
                e->play_steps = a[off+2u]; e->value = (uint8_t)dz_wire14(a + off + 3u);
            }
            dz_store_mark_dirty();
        } else if (n != 5u) return len;
        out[len++] = a[2]; out[len++] = a[3]; out[len++] = (uint8_t)count;
        if (op == 11u) for (i = 0; i < count; ++i) {
            const dz_song_entry_t *e = &dz_model.song[start + i];
            out[len++] = e->bank; out[len++] = e->pattern;
            out[len++] = e->play_steps;
            out[len++] = e->value & 127u; out[len++] = e->value >> 7;
        }
    } else if (op == 12u) { /* v4 song metadata write, full 1..256 count */
        uint32_t count;
        if (n != 6u || a[4] > 1u || a[5] > 1u) return len;
        count = dz_wire14(a + 2u);
        if (!count || count > DZ_NSONG) return len;
        dz_model.song_count = (uint16_t)count;
        dz_model.loop_mode = a[4]; dz_model.song_mode = a[5];
        dz_model.song_step = 0u;
        dz_store_mark_dirty();
        out[len++] = a[2]; out[len++] = a[3]; out[len++] = a[4]; out[len++] = a[5];
    } else if (op == 13u && n == 2u) { /* v4 song metadata read */
        out[len++] = dz_model.song_count & 127u;
        out[len++] = dz_model.song_count >> 7;
        out[len++] = dz_model.loop_mode; out[len++] = dz_model.song_mode;
    } else if (op == 14u || op == 15u) { /* v4 5-byte step chunks (max 16) */
        uint32_t start, count;
        dz_pattern_t *p;
        if (n < 6u || a[2] >= DZ_NBANK || a[3] >= DZ_NPAT_PER_BANK) return len;
        start = a[4]; count = a[5];
        if (!count || count > 16u || start >= DZ_NSTEP || start + count > DZ_NSTEP) return len;
        p = &dz_model.patterns[dz_seq_pat_index(a[2],a[3])];
        if (op == 14u) {
            if (n != 6u + 5u * count) return len;
            for (i = 0; i < count; ++i) {
                uint32_t off = 6u + 5u * i;
                if (a[off+1u] > 31u || !a[off+2u] || !a[off+3u] || a[off+4u] > 100u)
                    return len;
            }
            for (i = 0; i < count; ++i) {
                const uint8_t *v = a + 6u + 5u * i;
                dz_pattern_step_t *t = &p->step[start+i];
                t->note=v[0]; t->flags=v[1]; t->velocity=v[2];
                t->gate=v[3]; t->swing=v[4];
            }
            dz_store_mark_dirty();
        } else if (n != 6u) return len;
        out[len++] = a[2]; out[len++] = a[3];
        out[len++] = (uint8_t)start; out[len++] = (uint8_t)count;
        if (op == 15u) for (i = 0; i < count; ++i) {
            const dz_pattern_step_t *t = &p->step[start+i];
            out[len++] = t->note; out[len++] = t->flags;
            out[len++] = t->velocity; out[len++] = t->gate;
            out[len++] = t->swing;
        }
    } else if (op == 16u || op == 17u) { /* v4 LENGTH and RES per pattern */
        dz_pattern_t *p;
        if (n < 4u || a[2] >= DZ_NBANK || a[3] >= DZ_NPAT_PER_BANK) return len;
        p = &dz_model.patterns[dz_seq_pat_index(a[2],a[3])];
        if (op == 16u) {
            if (n != 6u || !a[4] || a[4] > DZ_NSTEP || a[5] > 9u) return len;
            p->length = a[4]; p->resolution = a[5];
            dz_store_mark_dirty();
        } else if (n != 4u) return len;
        out[len++] = a[2]; out[len++] = a[3];
        out[len++] = p->length; out[len++] = p->resolution;
    } else return len;
    out[2] = 0;
    return len;
}
