/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v5 - ADDITIONAL data in former 32 KiB legacy-project area.
 * DEVELOPMENT ONLY. Does not integrate into felucca.c or access flash.
 * v4 snapshot in sample slots remains the source of A-D pattern data.
 */
#ifndef ANALOGBEAST_V5_EXTRA_H
#define ANALOGBEAST_V5_EXTRA_H
#include <stdint.h>
#include "analogbeast_snapshot.h"

#define AB5_BANKS 8u
#define AB5_EXISTING_BANKS 4u
#define AB5_EXTRA_BANKS 4u
#define AB5_PAT_PER_BANK 16u
#define AB5_EXTRA_PATTERNS 64u
#define AB5_EXTRA_STEPS 16u
#define AB5_SONGS 32u
#define AB5_ROWS 64u
#define AB5_ALBUMS 4u
#define AB5_ALBUM_SLOTS 32u
#define AB5_ALBUM_EMPTY 255u
#define AB5_MODE_PATTERN 0u
#define AB5_MODE_SONG 1u
#define AB5_MODE_ALBUM 2u
/* Bank values 0..7 are PATTERN A..H; commands must not overlap. */
#define AB5_CMD_END 8u
#define AB5_CMD_TEMPO 9u
#define AB5_CMD_PRESET 10u
#define AB5_CMD_REST 11u
#define AB5_CMD_ARP 12u
#define AB5_CMD_END_PATTERN 16u
#define AB5_STORE_REGION 0xC9000u /* 3rd user sample slot, AFTER blank FSMP header */
#define AB5_STORE_BYTES 32768u
#define AB5_COPY_BYTES 16384u
#define AB5_STORE_END (AB5_STORE_REGION + AB5_STORE_BYTES)
#define AB5_EXTRA_WIRE_BYTES (8u + 64u*(2u + 16u*5u) + 32u*(2u + 64u*4u) + 4u*(2u+32u))
/* NOR header/commit records will be separate from the payload. */
#define AB5_NOR_HEADER_RESERVE 256u

typedef struct {
    uint8_t length, resolution;
    ab_step_t steps[AB5_EXTRA_STEPS];
} ab5_pattern_t;
typedef struct {
    uint8_t count, loop;
    ab_song_row_t rows[AB5_ROWS];
} ab5_song_t;
typedef struct {
    uint8_t count, loop;
    uint8_t song_ids[AB5_ALBUM_SLOTS]; /* 0..31; 255 = empty */
} ab5_album_t;
typedef struct {
    uint8_t play_mode, bank, pattern, selected_song;
    uint8_t selected_album, song_cursor, album_cursor, edit_step;
    ab5_pattern_t extra_patterns[AB5_EXTRA_PATTERNS];
    ab5_song_t songs[AB5_SONGS];
    ab5_album_t albums[AB5_ALBUMS];
} ab5_extra_t;

void ab5_extra_init(ab5_extra_t *m);
int ab5_extra_validate(const ab5_extra_t *m);
int ab5_extra_get(void *ctx,uint32_t off,uint8_t *dst,uint32_t n);
int ab5_extra_put(void *ctx,uint32_t off,const uint8_t *src,uint32_t n);
/* Convert ONLY the v4 arranger to v5 Song 01, leaving all v4 A-D patterns
 * and its storage completely unchanged. Nontrivial >64-row songs are refused.
 * Does not write NOR or modify the v4 source model. */
int ab5_from_v4_song1(ab5_extra_t *dst,const ab_snapshot_t *v4);
/* Returns -1 for an unpopulated album slot. */
int ab5_album_song(const ab5_extra_t *m,uint8_t album,uint8_t slot);
#endif
