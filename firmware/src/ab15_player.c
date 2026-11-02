#include "ab15_player.h"
/* Stage15 intentionally doesn't touch the on-device NOR/flash APIs. */
static void ab15_zero(void *p,uint32_t n){uint8_t *b=p;while(n--)*b++=0;}
void ab15_clear(ab15_player *p){if(p){ab15_zero(p,sizeof *p);p->loop=1u;}}
int ab15_select(ab15_player *p,uint16_t id,const ab13_pattern *pat){
    if(!p||!pat||id<1u||ab13_pattern_check(pat))return -1;
    if(p->active)return -2;
    p->pattern=*pat;p->pattern_id=id;p->loaded=1u;p->armed=1u;
    p->dirty=0u;p->page=0u;p->loop=1u;return 0;
}
int ab15_set_length(ab15_player*p,uint8_t n){
    if(!p||!p->loaded||p->active||n<AB13_MIN_STEPS||n>AB13_MAX_STEPS)return -1;
    p->pattern.length=n;p->dirty=1u;return 0;
}
static ab13_step *ab15_edit(ab15_player*p,uint8_t step){
    if(!p||!p->loaded||step>=p->pattern.length)return 0; /* live REC caller holds IRQ lock; console has ab15_busy guard */
    return &p->pattern.steps[step];
}
int ab15_note(ab15_player*p,uint8_t step,uint8_t midi_note){
    ab13_step *s=ab15_edit(p,step);
    if(!s||midi_note>127u)return -1;
    s->note=midi_note;s->flags=(uint8_t)((s->flags&3u)|AB13_STEP_PRESENT);
    if(!s->velocity)s->velocity=96u;
    if(!s->gate)s->gate=64u;
    p->dirty=1u;return 0;
}
int ab15_rest(ab15_player*p,uint8_t step){
    ab13_step*s=ab15_edit(p,step);if(!s)return -1;
    s->flags=0u;s->note=0u;p->dirty=1u;return 0;
}
int ab15_tie(ab15_player*p,uint8_t step){
    ab13_step*s=ab15_edit(p,step);if(!s)return -1;
    s->flags=4u;s->note=0u;p->dirty=1u;return 0;
}
int ab15_step_flag(ab15_player*p,uint8_t step,uint8_t bit,uint8_t on){
    ab13_step*s=ab15_edit(p,step);if(!s||(bit!=1u&&bit!=2u))return -1;
    s->flags=(uint8_t)(on?(s->flags|bit):(s->flags&~bit));p->dirty=1u;return 0;
}
int ab15_start(ab15_player*p){
    if(!p||!p->loaded||!p->armed||ab13_pattern_check(&p->pattern))return -1;
    p->active=1u;p->page=0u;return 0;
}
int ab15_advance(ab15_player*p){
    if(!p||!p->active)return 0;
    if(p->page==0u&&p->pattern.length>AB15_PAGE_STEPS){p->page=1u;return 1;}
    if(p->loop){p->page=0u;return 1;}
    p->active=0u;p->page=0u;return 0;
}
uint8_t ab15_page_length(const ab15_player*p){
    uint32_t start,left;
    if(!p||!p->loaded)return 0u;
    start=p->page*AB15_PAGE_STEPS;
    if(start>=p->pattern.length)return 0u;
    left=p->pattern.length-start;
    return (uint8_t)(left>AB15_PAGE_STEPS?AB15_PAGE_STEPS:left);
}
const ab13_step *ab15_page_step(const ab15_player*p,uint8_t idx){
    uint32_t at;if(!p||!p->loaded||idx>=ab15_page_length(p))return 0;
    at=p->page*AB15_PAGE_STEPS+idx;
    return &p->pattern.steps[at];
}
uint8_t ab15_event_type(const ab13_step*s){
    if(!s)return AB15_REST;
    if(s->flags&4u)return AB15_TIE;
    return (s->flags&AB13_STEP_PRESENT)?AB15_NOTE:AB15_REST;
}
