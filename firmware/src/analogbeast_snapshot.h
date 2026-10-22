/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v4 portable layout (NOT a dump of in-memory structs).
 * Works with analogbeast_nor_store's 256-byte streaming callbacks.
 */
#ifndef ANALOGBEAST_SNAPSHOT_H
#define ANALOGBEAST_SNAPSHOT_H
#include <stdint.h>
#define AB_BANKS 4u
#define AB_PATTERNS_PER_BANK 16u
#define AB_PATTERNS (AB_BANKS * AB_PATTERNS_PER_BANK)
#define AB_STEPS 64u
#define AB_SONG_ROWS 256u
#define AB_SONG_END 4u
#define AB_SONG_TEMPO 5u
#define AB_SONG_PRESET 6u
#define AB_SONG_REST 7u
#define AB_SONG_ARP 8u
#define AB_SONG_DEFAULT_RES 2u /* Felucca P_SDIV=2 means 1/16 */
#define AB_STEP_DATA_BYTES 5u
#define AB_PATTERN_DATA_BYTES (2u + AB_STEPS * AB_STEP_DATA_BYTES)
#define AB_SNAPSHOT_HEADER_BYTES 16u
#define AB_SNAPSHOT_DATA_BYTES (AB_SNAPSHOT_HEADER_BYTES + AB_PATTERNS * AB_PATTERN_DATA_BYTES + AB_SONG_ROWS * 4u)

typedef struct {uint8_t note, flags, velocity, gate, swing;} ab_step_t;
typedef struct {uint8_t length, resolution; ab_step_t step[AB_STEPS];} ab_pattern_t;
typedef struct {uint8_t bank, pattern, play_steps, value;} ab_song_row_t;
typedef struct {
    uint8_t bank, pattern, song_mode, loop_mode;
    uint16_t song_step, song_count;
    ab_pattern_t patterns[AB_PATTERNS];
    ab_song_row_t song[AB_SONG_ROWS];
} ab_snapshot_t;

void ab_snapshot_init(ab_snapshot_t *m);
int ab_snapshot_validate(const ab_snapshot_t *m);
int ab_snapshot_get(void *ctx,uint32_t pos,uint8_t *dst,uint32_t size);
int ab_snapshot_put(void *ctx,uint32_t pos,const uint8_t *src,uint32_t size);
#endif
