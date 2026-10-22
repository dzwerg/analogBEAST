/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v5 Stage 3: read-only first-boot integration and playback planning.
 * Does NOT erase/program flash; v4 remains the live transport and sequencer.
 */
#ifndef ANALOGBEAST_V5_RUNTIME_H
#define ANALOGBEAST_V5_RUNTIME_H
#include "analogbeast_v5_nor.h"
#define AB5_BOOT_V5_VALID       0
#define AB5_BOOT_V4_PREVIEW     1
#define AB5_BOOT_NO_MIGRATION   2
#define AB5_BOOT_FLASH_ERROR    3
/* A v4 song longer than 64 entries is never truncated. */
#define AB5_BOOT_V4_TOO_LONG    4

int ab5_boot_preview(ab5_extra_t *dst,ab5_nor_state_t *state,
                     const ab_snapshot_t *v4,const ab_nor_io_t *io);
/* A-D steps are in the v4 store; E-H steps are in the separate v5 model. */
const ab_step_t *ab5_step_read(const ab5_extra_t *m,const ab_snapshot_t *v4,
                              uint8_t bank,uint8_t pattern,uint8_t step);
uint8_t ab5_pattern_length(const ab5_extra_t *m,const ab_snapshot_t *v4,
                           uint8_t bank,uint8_t pattern);

/* Atomic 32-bit UI -> audio transport request: kind in bits 24..31,
 * album/bank in bits 8..15, pattern/song/slot in bits 0..7. Zero = none. */
#define AB5_REQ_PATTERN    1u
#define AB5_REQ_SONG       2u
#define AB5_REQ_ALBUM_SLOT 3u
#define AB5_REQ_ALBUM      4u
#define AB5_REQ(kind, high, low) (((uint32_t)(kind)<<24) | ((uint32_t)(high)<<8) | (uint32_t)(low))
typedef struct {
    uint8_t running, mode, bank, pattern, play_steps, rest, pattern_loop;
    uint8_t song_id, album_id, album_pos, row_pos;
    uint8_t tempo, preset, arp_on;
    uint8_t tempo_changed, preset_changed, arp_changed;
} ab5_play_t;
/* Pure RAM playback iterator: returns 1 if a timed row/pattern is ready, else 0.
 * No audio, UI, MIDI or flash side effects. Album mode ignores per-song loop;
 * only the album's loop flag repeats the playlist. */
int ab5_play_start(const ab5_extra_t *m,const ab_snapshot_t *v4,ab5_play_t *p);
int ab5_play_next(const ab5_extra_t *m,const ab_snapshot_t *v4,ab5_play_t *p);
/* Consume a queued live selection only at a complete pattern or song boundary.
 * The caller clears the request atomically only when *taken becomes 1. */
int ab5_play_next_request(const ab5_extra_t *m,const ab_snapshot_t *v4,
                          ab5_play_t *p,uint32_t request,uint8_t *taken);
#endif
