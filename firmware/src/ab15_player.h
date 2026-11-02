/* Stage 15: single 128-step MD13 pattern cache and two-page playback.
 * No bank ID, no NOR operation and no heap in the ISR. */
#ifndef AB15_PLAYER_H
#define AB15_PLAYER_H
#include <stdint.h>
#include "ab13_bankfree.h"
#define AB15_PAGE_STEPS 64u
#define AB15_NOTE 1u
#define AB15_REST 0u
#define AB15_TIE 2u
typedef struct {
    ab13_pattern pattern;
    uint16_t pattern_id;
    uint8_t loaded, armed, active, page, loop, dirty;
} ab15_player;
void ab15_clear(ab15_player *p);
int ab15_select(ab15_player *p,uint16_t id,const ab13_pattern *pattern);
int ab15_set_length(ab15_player *p,uint8_t n);
int ab15_note(ab15_player *p,uint8_t step,uint8_t midi_note);
int ab15_rest(ab15_player *p,uint8_t step);
int ab15_tie(ab15_player *p,uint8_t step);
int ab15_step_flag(ab15_player *p,uint8_t step,uint8_t bit,uint8_t on);
int ab15_start(ab15_player *p);
int ab15_advance(ab15_player *p);
uint8_t ab15_page_length(const ab15_player *p);
const ab13_step *ab15_page_step(const ab15_player *p,uint8_t index);
uint8_t ab15_event_type(const ab13_step *s);
#endif
