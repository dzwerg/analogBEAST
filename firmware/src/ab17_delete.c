#include "ab17_delete.h"
void ab17_init(ab17_delete_t *s,uint16_t pattern_id){
    if(!s)return;
    s->step=1u;s->progress=0u;s->pending=0u;s->pattern_id=pattern_id;
}
void ab17_cancel(ab17_delete_t *s){if(s){s->pending=0u;s->progress=0u;}}
int ab17_k2(ab17_delete_t *s,int32_t detents,uint8_t length){
    int32_t next;
    if(!s||!detents||length<8u||length>128u)return AB17_NONE;
    if(s->pending){ab17_cancel(s);return AB17_CANCEL;}
    if(s->step==1u&&detents<0){
        s->pending=1u;s->progress=0u;return AB17_PROMPT;
    }
    next=(int32_t)s->step+detents;
    if(next<1)next=1;
    if(next>length)next=length;
    if(next==(int32_t)s->step)return AB17_NONE;
    s->step=(uint8_t)next;return AB17_STEP;
}
int ab17_k4(ab17_delete_t *s,int32_t detents,uint16_t current_pattern_id){
    uint32_t n;
    if(!s||!s->pending||!detents)return AB17_NONE;
    if(current_pattern_id!=s->pattern_id||detents>0){
        ab17_cancel(s);return AB17_CANCEL;
    }
    /* Signed panel detents, never accelerated. One counter increment per CCW click. */
    n=(uint32_t)(-(int64_t)detents);
    if(n>=AB17_CONFIRM_TICKS-s->progress){
        ab17_cancel(s);return AB17_CONFIRMED;
    }
    s->progress=(uint8_t)(s->progress+n);return AB17_PROGRESS;
}
