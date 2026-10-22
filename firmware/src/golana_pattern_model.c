/* SPDX-License-Identifier: GPL-3.0-only */
#include "golana_pattern_model.h"

static uint8_t dz_min_u8(uint8_t v, uint8_t hi) { return v > hi ? hi : v; }

uint8_t dz_seq_pat_index(uint8_t bank, uint8_t pattern)
{
    return (uint8_t)(dz_min_u8(bank, DZ_NBANK - 1u) * DZ_NPAT_PER_BANK +
                     dz_min_u8(pattern, DZ_NPAT_PER_BANK - 1u));
}

void dz_seq_model_init(dz_seq_model_t *m) { ab_snapshot_init(m); }

void dz_seq_select_pattern(dz_seq_model_t *m, uint8_t bank, uint8_t pattern)
{
    m->bank = dz_min_u8(bank, DZ_NBANK - 1u);
    m->pattern = dz_min_u8(pattern, DZ_NPAT_PER_BANK - 1u);
}
void dz_seq_select_song_step(dz_seq_model_t *m, uint16_t step)
{
    m->song_step = step < DZ_NSONG ? step : DZ_NSONG - 1u;
}
int dz_seq_song_is_end(const dz_song_entry_t *e)
{
    return e->bank == DZ_SONG_END_BANK;
}
static dz_song_entry_t *dz_song_edit(dz_seq_model_t *m)
{
    if (m->song_step >= m->song_count) m->song_count = m->song_step + 1u;
    return &m->song[m->song_step];
}
void dz_seq_set_song_bank(dz_seq_model_t *m, uint8_t bank)
{
    dz_song_entry_t *e = dz_song_edit(m);
    bank = dz_min_u8(bank, DZ_SONG_ARP);
    if (e->bank == bank) return;
    e->bank = bank;
    e->pattern = bank == DZ_SONG_END_BANK ? DZ_SONG_END_PATTERN : 0u;
    e->value = bank == DZ_SONG_TEMPO ? 120u : bank == DZ_SONG_ARP ? 0u : 0u;
}
void dz_seq_set_song_pattern(dz_seq_model_t *m, uint8_t pattern)
{
    dz_song_entry_t *e = dz_song_edit(m);
    if (e->bank == DZ_SONG_END_BANK && pattern < DZ_SONG_END_PATTERN) {
        /* Compatible with old END navigation: K4 away from END -> A/normal. */
        e->bank = 0u;
        e->value = 0u;
    }
    if (e->bank < DZ_NBANK) {
        if (pattern >= DZ_SONG_END_PATTERN) {
            e->bank = DZ_SONG_END_BANK;
            e->pattern = DZ_SONG_END_PATTERN;
            e->value = 0u;
        } else
            e->pattern = pattern;
    }
}
void dz_seq_set_song_value(dz_seq_model_t *m, uint8_t value)
{
    dz_song_entry_t *e = dz_song_edit(m);
    if (e->bank == DZ_SONG_TEMPO)
        e->value = value < DZ_TEMPO_MIN ? DZ_TEMPO_MIN :
                   value > DZ_TEMPO_MAX ? DZ_TEMPO_MAX : value;
    else if (e->bank == DZ_SONG_PRESET)
        e->value = dz_min_u8(value, DZ_PRESET_COUNT - 1u);
    else if (e->bank == DZ_SONG_ARP)
        e->value = value ? 1u : 0u;
}
void dz_seq_set_song_steps(dz_seq_model_t *m, uint8_t count)
{
    if (!count) count = 1u;
    dz_song_edit(m)->play_steps = dz_min_u8(count, DZ_NSTEP);
}

/* Execute zero-time commands, stopping on the next timed pattern or REST.
 * A bounded search prevents infinite looping in songs consisting only of commands. */
static int dz_seek_playable(const dz_seq_model_t *m, dz_seq_playback_t *p)
{
    uint32_t scans;
    p->tempo_changed = p->preset_changed = p->arp_changed = 0u;
    p->rest = 0u;
    for (scans = 0; scans < DZ_NSONG; ++scans) {
        const dz_song_entry_t *e;
        if (p->song_step >= m->song_count || dz_seq_song_is_end(&m->song[p->song_step])) {
            if (!p->loop_mode || !p->song_step) { p->running = 0u; return 0; }
            p->song_step = 0u;
            continue;
        }
        e = &m->song[p->song_step];
        if (e->bank < DZ_NBANK || e->bank == DZ_SONG_REST) {
            p->rest = e->bank == DZ_SONG_REST;
            if (!p->rest) {
                p->bank = dz_min_u8(e->bank, DZ_NBANK - 1u);
                p->pattern = dz_min_u8(e->pattern, DZ_NPAT_PER_BANK - 1u);
            }
            p->play_steps = e->play_steps ? dz_min_u8(e->play_steps, DZ_NSTEP) : 1u;
            return 1;
        }
        if (e->bank == DZ_SONG_TEMPO) {
            p->tempo = e->value < DZ_TEMPO_MIN ? DZ_TEMPO_MIN :
                       e->value > DZ_TEMPO_MAX ? DZ_TEMPO_MAX : e->value;
            p->tempo_changed = 1u;
        } else if (e->bank == DZ_SONG_PRESET) {
            p->preset = dz_min_u8(e->value, DZ_PRESET_COUNT - 1u);
            p->preset_changed = 1u;
        } else if (e->bank == DZ_SONG_ARP) {
            p->arp_on = !!e->value;
            p->arp_changed = 1u;
        }
        ++p->song_step;
    }
    p->running = 0u;
    return 0;
}
void dz_seq_play_start(const dz_seq_model_t *m, dz_seq_playback_t *p)
{
    uint8_t *dst = (uint8_t *)p;
    uint32_t i;
    for (i = 0; i < sizeof *p; ++i) dst[i] = 0;
    p->song_mode = !!m->song_mode;
    p->loop_mode = !!m->loop_mode;
    p->running = 1u;
    if (p->song_mode) {
        if (!dz_seek_playable(m, p)) p->play_steps = 1u;
    } else {
        p->bank = m->bank;
        p->pattern = m->pattern;
        p->play_steps = m->patterns[dz_seq_pat_index(m->bank, m->pattern)].length;
        if (!p->play_steps || p->play_steps > DZ_NSTEP) p->play_steps = DZ_NSTEP;
    }
}
int dz_seq_play_next(const dz_seq_model_t *m, dz_seq_playback_t *p)
{
    if (!p->running) return 0;
    if (!p->song_mode) {
        if (p->loop_mode) return 1;
        p->running = 0u;
        return 0;
    }
    ++p->song_step;
    return dz_seek_playable(m, p);
}
