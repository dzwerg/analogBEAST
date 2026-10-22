/* SPDX-License-Identifier: GPL-3.0-only */
#include "analogbeast_snapshot.h"
_Static_assert(AB_SNAPSHOT_DATA_BYTES == 21648u, "unexpected v4 wire image length");
static uint8_t *step_field(ab_step_t *s,unsigned idx)
{
    switch(idx){case 0:return &s->note;case 1:return &s->flags;
    case 2:return &s->velocity;case 3:return &s->gate;default:return &s->swing;}
}
void ab_snapshot_init(ab_snapshot_t *m)
{
    uint32_t p,s;
    memset(m,0,sizeof *m);
    m->loop_mode=1;
    m->song_count=1;
    for(p=0;p<AB_PATTERNS;p++) {
        m->patterns[p].length=AB_STEPS;
        m->patterns[p].resolution=AB_SONG_DEFAULT_RES;
        for(s=0;s<AB_STEPS;s++) {
            m->patterns[p].step[s].velocity=96u;
            m->patterns[p].step[s].gate=64u;
        }
    }
    for(s=0;s<AB_SONG_ROWS;s++)m->song[s].play_steps=AB_STEPS;
}
int ab_snapshot_validate(const ab_snapshot_t *m)
{
    uint32_t p,s;
    if(!m||m->bank>=AB_BANKS||m->pattern>=AB_PATTERNS_PER_BANK||
       m->song_mode>1||m->loop_mode>1||m->song_step>=AB_SONG_ROWS||
       !m->song_count||m->song_count>AB_SONG_ROWS)return -1;
    for(p=0;p<AB_PATTERNS;p++) {
        const ab_pattern_t*t=&m->patterns[p];
        if(!t->length||t->length>AB_STEPS||t->resolution>9u)return -2;
        for(s=0;s<AB_STEPS;s++) {
            const ab_step_t*x=&t->step[s];
            if(x->note>127||x->flags>31||!x->velocity||x->velocity>127||
               !x->gate||x->gate>127||x->swing>100)return -3;
        }
    }
    for(s=0;s<AB_SONG_ROWS;s++) {
        const ab_song_row_t*x=&m->song[s];
        if(!x->play_steps||x->play_steps>64)return -4;
        if(x->bank<AB_BANKS){if(x->pattern>=AB_PATTERNS_PER_BANK)return -5;}
        else if(x->bank==AB_SONG_END){if(x->pattern!=16u)return -6;}
        else if(x->bank==AB_SONG_TEMPO){if(x->value<40||x->value>240)return -7;}
        else if(x->bank==AB_SONG_PRESET){if(x->value>=32)return -8;}
        else if(x->bank==AB_SONG_REST){}
        else if(x->bank==AB_SONG_ARP){if(x->value>1)return -9;}
        else return -10;
    }
    return 0;
}
/* All bytes are explicitly mapped; no implicit struct padding/endianness. */
static uint8_t *byte_at(ab_snapshot_t*m,uint32_t pos)
{
    static uint8_t zeros[8]; /* zero reserved bytes never store user data */
    uint32_t local,pat,which,row;
    if(pos<AB_SNAPSHOT_HEADER_BYTES){
        switch(pos){
        case 0:return &m->bank;case 1:return &m->pattern;
        case 2:return &m->song_mode;case 3:return &m->loop_mode;
        case 4:return ((uint8_t *)&m->song_step);case 5:return ((uint8_t *)&m->song_step)+1;
        case 6:return ((uint8_t *)&m->song_count);case 7:return ((uint8_t *)&m->song_count)+1;
        default:return &zeros[pos-8u];
        }
    }
    pos-=AB_SNAPSHOT_HEADER_BYTES;
    if(pos<AB_PATTERNS*AB_PATTERN_DATA_BYTES){
        pat=pos/AB_PATTERN_DATA_BYTES;local=pos%AB_PATTERN_DATA_BYTES;
        if(local==0)return &m->patterns[pat].length;
        if(local==1)return &m->patterns[pat].resolution;
        local-=2u;which=local/AB_STEP_DATA_BYTES;
        return step_field(&m->patterns[pat].step[which],local%AB_STEP_DATA_BYTES);
    }
    pos-=AB_PATTERNS*AB_PATTERN_DATA_BYTES;
    row=pos/4u;local=pos%4u;
    switch(local){case 0:return &m->song[row].bank;case 1:return &m->song[row].pattern;
        case 2:return &m->song[row].play_steps;default:return &m->song[row].value;}
}
int ab_snapshot_get(void *ctx,uint32_t pos,uint8_t *dst,uint32_t size)
{
    uint32_t i;
    ab_snapshot_t*m=ctx;
    if(!m||!dst||pos>AB_SNAPSHOT_DATA_BYTES||size>AB_SNAPSHOT_DATA_BYTES-pos)return -1;
    for(i=0;i<size;i++) {
        uint32_t at=pos+i;
        if(at==4)dst[i]=(uint8_t)m->song_step;
        else if(at==6)dst[i]=(uint8_t)m->song_count;
        else if(at==5)dst[i]=(uint8_t)(m->song_step>>8);
        else if(at==7)dst[i]=(uint8_t)(m->song_count>>8);
        else dst[i]=*byte_at(m,at);
    }
    return 0;
}
int ab_snapshot_put(void *ctx,uint32_t pos,const uint8_t *src,uint32_t size)
{
    uint32_t i;
    ab_snapshot_t*m=ctx;
    if(!m||!src||pos>AB_SNAPSHOT_DATA_BYTES||size>AB_SNAPSHOT_DATA_BYTES-pos)return -1;
    for(i=0;i<size;i++) {
        uint32_t at=pos+i;
        if(at>=8u&&at<AB_SNAPSHOT_HEADER_BYTES){
            if(src[i]!=0u)return -2; /* invalid reserved bytes: don't accept */
            continue;
        }
        if(at==4)m->song_step=(uint16_t)((m->song_step&0xFF00u)|src[i]);
        else if(at==5)m->song_step=(uint16_t)((m->song_step&0xFFu)|((uint16_t)src[i]<<8));
        else if(at==6)m->song_count=(uint16_t)((m->song_count&0xFF00u)|src[i]);
        else if(at==7)m->song_count=(uint16_t)((m->song_count&0xFFu)|((uint16_t)src[i]<<8));
        else *byte_at(m,at)=src[i];
    }
    return 0;
}
