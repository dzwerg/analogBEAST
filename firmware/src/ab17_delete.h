/* Stage17: hardware delete gesture. No flash API in this module. */
#ifndef AB17_DELETE_H
#define AB17_DELETE_H
#include <stdint.h>
#define AB17_CONFIRM_TICKS 5u
typedef struct {
    uint8_t step;          /* visible step number, 1..128 */
    uint8_t progress;      /* 0..4 while armed */
    uint8_t pending;
    uint16_t pattern_id;   /* pin identity, prevent deleting another selection */
} ab17_delete_t;
enum { AB17_NONE=0, AB17_STEP=1, AB17_PROMPT=2, AB17_PROGRESS=3,
       AB17_CONFIRMED=4, AB17_CANCEL=5 };
void ab17_init(ab17_delete_t *s,uint16_t pattern_id);
void ab17_cancel(ab17_delete_t *s);
int ab17_k2(ab17_delete_t *s,int32_t detents,uint8_t length);
int ab17_k4(ab17_delete_t *s,int32_t detents,uint16_t current_pattern_id);
#endif
