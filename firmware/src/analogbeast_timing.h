/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST: per-step swing with exact pattern-length conservation.
 * Every step has its own start offset relative to the straight grid.
 * Even steps are early, odd steps late; 100 on all steps reproduces
 * Felucca's original +/- 40% swing at a paired boundary. With different
 * per-step values, lengths telescope: sum(length[i]) == n * period.
 * Values outside 0..100 are clamped defensively (flash validates them).
 */
#ifndef ANALOGBEAST_TIMING_H
#define ANALOGBEAST_TIMING_H
#include <stdint.h>
static inline int32_t ab_swing_offset(uint32_t period, uint32_t idx, uint32_t swing)
{
    uint32_t a = swing > 100u ? 100u : swing;
    int32_t delta = (int32_t)((period / 500u) * a + (period % 500u) * a / 500u);
    return (idx & 1u) ? delta : -delta;
}
static inline uint32_t ab_swing_step_length(uint32_t period, uint32_t idx,
                                             uint8_t swing, uint8_t next_swing,
                                             uint32_t next_idx)
{
    int32_t x = (int32_t)period + ab_swing_offset(period, next_idx, next_swing)
                 - ab_swing_offset(period, idx, swing);
    return x > 0 ? (uint32_t)x : 1u;
}
#endif
