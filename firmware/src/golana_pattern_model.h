/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v4: 64 independent patterns, 64 steps per pattern,
 * 256 Arranger entries. Persistent wire format in analogbeast_snapshot.c.
 */
#ifndef GOLANA_PATTERN_MODEL_H
#define GOLANA_PATTERN_MODEL_H
#include "analogbeast_snapshot.h"
#define DZ_NBANK AB_BANKS
#define DZ_NPAT_PER_BANK AB_PATTERNS_PER_BANK
#define DZ_NPAT AB_PATTERNS
#define DZ_NSTEP AB_STEPS
#define DZ_NSONG AB_SONG_ROWS
#define DZ_SONG_END_BANK AB_SONG_END
#define DZ_SONG_TEMPO AB_SONG_TEMPO
#define DZ_SONG_PRESET AB_SONG_PRESET
#define DZ_SONG_REST AB_SONG_REST
#define DZ_SONG_ARP AB_SONG_ARP
#define DZ_SONG_END_PATTERN DZ_NPAT_PER_BANK
#define DZ_TEMPO_MIN 40u
#define DZ_TEMPO_MAX 240u
#define DZ_PRESET_COUNT 32u

typedef ab_step_t dz_pattern_step_t;
typedef ab_pattern_t dz_pattern_t;
typedef ab_song_row_t dz_song_entry_t;
typedef ab_snapshot_t dz_seq_model_t;

typedef struct {
    uint8_t song_mode, loop_mode, running;
    uint16_t song_step;
    uint8_t bank, pattern, play_steps;
    uint8_t rest, tempo_changed, preset_changed, arp_changed;
    uint8_t tempo, preset, arp_on;
} dz_seq_playback_t;

void dz_seq_model_init(dz_seq_model_t *m);
uint8_t dz_seq_pat_index(uint8_t bank, uint8_t pattern);
void dz_seq_select_pattern(dz_seq_model_t *m, uint8_t bank, uint8_t pattern);
void dz_seq_select_song_step(dz_seq_model_t *m, uint16_t step);
int dz_seq_song_is_end(const dz_song_entry_t *e);
void dz_seq_set_song_bank(dz_seq_model_t *m, uint8_t bank);
void dz_seq_set_song_pattern(dz_seq_model_t *m, uint8_t pattern);
void dz_seq_set_song_value(dz_seq_model_t *m, uint8_t value);
void dz_seq_set_song_steps(dz_seq_model_t *m, uint8_t count);
void dz_seq_play_start(const dz_seq_model_t *m, dz_seq_playback_t *p);
int dz_seq_play_next(const dz_seq_model_t *m, dz_seq_playback_t *p);
#endif
