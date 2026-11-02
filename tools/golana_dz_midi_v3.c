/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST ab3.3 – editor SysEx command 96.
 * Request: op, token, arguments (all bytes 7-bit); response: op, token, rc, data.
 * rc: 0 OK, 1 invalid, 2 busy, 3 RAM-only, 4 flash error.
 * v3 adds chunked 128-row songs: each row [kind,pattern,play_steps,value_lo7,value_hi7].
 * The stored model is unchanged: value is 8-bit; 14-bit is only a MIDI-wire encoding.
 * Legacy op 2/3 pattern transfers are retained.
 * Included after golana_ui.c; executed in main loop, never in audio ISR.
 */
static uint32_t dz_midi_request(const uint8_t *a, uint32_t n, uint8_t *out)
{
    uint32_t op = n ? a[0] : 127u, i, k, len = 3u;
    out[0] = (uint8_t)op; out[1] = n > 1u ? a[1] : 0u; out[2] = 1u;
    if (n < 2u) return len;
    for (i = 0; i < n; i++) if (a[i] > 127u) return len;
    if (!op && n == 2u) {
        out[2] = 0; out[3] = 3;  /* protocol v3 */
        out[4] = dz_store_enabled; out[5] = dz_store_dirty;
        out[6] = dz_store_error; out[7] = dz_store_have_commit;
        out[8] = DZ_NPAT; out[9] = 0u; /* 0 encodes DZ_NSONG=128 */
        return 10u;
    }
    if (!gu.ready || transport_busy() || gu.dz_edit_active || gu.save || dz_play_has_backup) {
        out[2] = 2; return len;
    }
    if (op == 1u) { /* OLD command: explicitly erase ONLY 0x97000..0x9efff with key CLEAR */
        static const uint8_t key[] = {67, 76, 69, 65, 82};
        if (n != 7u || memcmp(a + 2, key, 5u)) return len;
        if (!flash_ok) { out[2] = 4; return len; }
        dz_store_enabled = 0; gx_blocked = 1;
        for (i = 0; i < 8u; i++) {
            if (st_erase(0x97000u + i * ST_SECTOR)) { out[2] = 4; return len; }
            fm1_wdt_feed();
        }
        for (i = 0; i < 4u; i++)
            if (!dz_store_pair_blank((uint8_t)i)) { out[2] = 4; return len; }
        memset(proj_slot, 0, sizeof proj_slot);
        gx_boot(); gx_dirty = 0;
        dz_store_boot(&dz_model);
        if (!dz_store_enabled) { out[2] = 3; return len; }
        dz_store_mark_dirty();
        out[2] = 0; gu_say("PROJECTS CLEARED", 0); return len;
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
        if (start >= DZ_NSONG || !count || count > 24u || start + count > DZ_NSONG) return len;
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
                      (bank == DZ_SONG_REST && pat == 0u && val == 0u))) return len;
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
        if (!dz_store_enabled) { out[2] = 3; return len; }
        dz_store_poll(&dz_model, 1);
        if (dz_store_error) { out[2] = 4; return len; }
        out[len++] = dz_store_dirty; out[len++] = dz_store_have_commit;
    } else if (op == 7u) { /* set song metadata: count 0=128, loop, song_mode */
        uint32_t count;
        if (n != 5u || a[3] > 1u || a[4] > 1u) return len;
        count = a[2] ? a[2] : DZ_NSONG;
        if (count > DZ_NSONG) return len;
        dz_model.song_count = (uint8_t)count;
        dz_model.loop_mode = a[3]; dz_model.song_mode = a[4];
        dz_model.song_step = 0u;
        dz_store_mark_dirty();
        out[len++] = a[2]; out[len++] = a[3]; out[len++] = a[4];
    } else if (op == 8u && n == 2u) { /* read song metadata */
        out[len++] = dz_model.song_count == DZ_NSONG ? 0u : dz_model.song_count;
        out[len++] = dz_model.loop_mode; out[len++] = dz_model.song_mode;
    } else if (op == 9u) { /* reset DZ-only RAM model; no sector erase, never touch presets */
        static const uint8_t key[] = {82, 69, 83, 69, 84}; /* RESET */
        if (n != 7u || memcmp(a+2, key, 5u)) return len;
        dz_seq_model_init(&dz_model);
        dz_store_mark_dirty();
    } else return len;
    out[2] = 0;
    return len;
}
