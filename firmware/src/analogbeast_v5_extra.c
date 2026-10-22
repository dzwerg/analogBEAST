/* SPDX-License-Identifier: GPL-3.0-only */
#include "analogbeast_v5_extra.h"
#include "analogbeast_nor_store.h"
_Static_assert(sizeof(ab_step_t)==5u,"v4 steps unexpectedly padded");
_Static_assert(sizeof(ab_song_row_t)==4u,"v4 arranger row unexpectedly padded");
_Static_assert(sizeof(ab5_pattern_t)==82u,"v5 pattern unexpectedly padded");
_Static_assert(sizeof(ab5_song_t)==258u,"v5 song unexpectedly padded");
_Static_assert(sizeof(ab5_album_t)==34u,"v5 album unexpectedly padded");
_Static_assert(sizeof(ab5_extra_t)==AB5_EXTRA_WIRE_BYTES,"v5 payload unexpectedly padded");
_Static_assert(AB5_EXTRA_WIRE_BYTES==13648u,"v5 payload size changed");
_Static_assert(AB5_EXTRA_WIRE_BYTES+AB5_NOR_HEADER_RESERVE<=AB5_COPY_BYTES,
               "v5 A/B record cannot fit in 16KiB");
_Static_assert(AB5_STORE_REGION == AB_SAMPLE_START + 2u*AB_SAMPLE_STRIDE + AB_NOR_SECTOR,
               "v5 must start after third sample slot header");
_Static_assert(AB5_STORE_END <= AB_SAMPLE_START + 3u*AB_SAMPLE_STRIDE,
               "v5 must fit entirely within third user sample slot");

static void zero_bytes(void *obj,uint32_t n) {
    uint8_t *p=(uint8_t *)obj;
    uint32_t i;
    for(i=0;i<n;i++)p[i]=0;
}

void ab5_extra_init(ab5_extra_t *m) {
    uint32_t p,s,j;
    if(!m)return;
    zero_bytes(m,sizeof *m);
    m->play_mode=AB5_MODE_PATTERN;
    for(p=0;p<AB5_EXTRA_PATTERNS;p++) {
        ab5_pattern_t *pat=&m->extra_patterns[p];
        pat->length=AB5_EXTRA_STEPS;
        pat->resolution=AB_SONG_DEFAULT_RES;
        for(s=0;s<AB5_EXTRA_STEPS;s++){
            pat->steps[s].velocity=96u;
            pat->steps[s].gate=64u;
        }
    }
    for(p=0;p<AB5_SONGS;p++) {
        ab5_song_t *song=&m->songs[p];
        song->count=1u;
        song->loop=0u;
        for(s=0;s<AB5_ROWS;s++){
            song->rows[s].bank=AB5_CMD_END;
            song->rows[s].pattern=AB5_CMD_END_PATTERN;
            song->rows[s].play_steps=1u;
        }
    }
    for(p=0;p<AB5_ALBUMS;p++) {
        ab5_album_t *album=&m->albums[p];
        for(j=0;j<AB5_ALBUM_SLOTS;j++)album->song_ids[j]=AB5_ALBUM_EMPTY;
    }
}

int ab5_extra_validate(const ab5_extra_t *m) {
    uint32_t p,s,a;
    if(!m||m->play_mode>AB5_MODE_ALBUM||m->bank>=AB5_BANKS||
       m->pattern>=AB5_PAT_PER_BANK||m->selected_song>=AB5_SONGS||
       m->selected_album>=AB5_ALBUMS||m->song_cursor>=AB5_ROWS||
       m->album_cursor>=AB5_ALBUM_SLOTS||m->edit_step>=AB5_EXTRA_STEPS)return -1;
    for(p=0;p<AB5_EXTRA_PATTERNS;p++){
        const ab5_pattern_t *pat=&m->extra_patterns[p];
        if(!pat->length||pat->length>AB5_EXTRA_STEPS||pat->resolution>9u)return -2;
        for(s=0;s<AB5_EXTRA_STEPS;s++){
            const ab_step_t *step=&pat->steps[s];
            if(step->note>127u||step->flags>31u||step->velocity<1u||step->velocity>127u||
               step->gate<1u||step->gate>127u||step->swing>100u)return -3;
        }
    }
    for(p=0;p<AB5_SONGS;p++){
        const ab5_song_t *song=&m->songs[p];
        if(!song->count||song->count>AB5_ROWS||song->loop>1u)return -4;
        for(s=0;s<AB5_ROWS;s++){
            const ab_song_row_t *row=&song->rows[s];
            if(!row->play_steps||row->play_steps>AB_STEPS)return -5;
            if(row->bank<AB5_BANKS){
                if(row->pattern>=AB5_PAT_PER_BANK)return -6;
                if(row->bank>=AB5_EXISTING_BANKS&&row->play_steps>AB5_EXTRA_STEPS)
                    return -7;
            }else if(row->bank==AB5_CMD_END){
                if(row->pattern!=AB5_CMD_END_PATTERN)return -8;
            }else if(row->bank==AB5_CMD_TEMPO){
                if(row->value<40u||row->value>240u)return -9;
            }else if(row->bank==AB5_CMD_PRESET){
                if(row->value>=32u)return -10;
            }else if(row->bank==AB5_CMD_REST){
                /* REST can cover 1..64 1/16 steps. */
            }else if(row->bank==AB5_CMD_ARP){
                if(row->value>1u)return -11;
            }else return -12;
        }
    }
    for(a=0;a<AB5_ALBUMS;a++){
        const ab5_album_t *album=&m->albums[a];
        if(album->count>AB5_ALBUM_SLOTS||album->loop>1u)return -13;
        for(s=0;s<AB5_ALBUM_SLOTS;s++){
            uint8_t id=album->song_ids[s];
            if(s<album->count){if(id>=AB5_SONGS)return -14;}
            else if(id!=AB5_ALBUM_EMPTY)return -15;
        }
    }
    return 0;
}

/* All persisted fields are uint8_t and their sizes are static-asserted.
 * This explicitly separates the new v5 store from v4's 21,648-byte record. */
static uint8_t *at(ab5_extra_t *m,uint32_t off){
    const uint32_t patterns=(uint32_t)AB5_EXTRA_PATTERNS*sizeof(ab5_pattern_t);
    const uint32_t songs=(uint32_t)AB5_SONGS*sizeof(ab5_song_t);
    if(off<8u)return &((uint8_t *)m)[off];
    off-=8u;
    if(off<patterns)return &((uint8_t *)m->extra_patterns)[off];
    off-=patterns;
    if(off<songs)return &((uint8_t *)m->songs)[off];
    off-=songs;
    return &((uint8_t *)m->albums)[off];
}
int ab5_extra_get(void *ctx,uint32_t off,uint8_t *dst,uint32_t n){
    uint32_t i;
    ab5_extra_t *m=(ab5_extra_t*)ctx;
    if(!m||!dst||off>AB5_EXTRA_WIRE_BYTES||n>AB5_EXTRA_WIRE_BYTES-off)return -1;
    for(i=0;i<n;i++)dst[i]=*at(m,off+i);
    return 0;
}
int ab5_extra_put(void *ctx,uint32_t off,const uint8_t *src,uint32_t n){
    uint32_t i;
    ab5_extra_t *m=(ab5_extra_t*)ctx;
    if(!m||!src||off>AB5_EXTRA_WIRE_BYTES||n>AB5_EXTRA_WIRE_BYTES-off)return -1;
    for(i=0;i<n;i++)*at(m,off+i)=src[i];
    return 0;
}

static int translate_v4(const ab_song_row_t *src,ab_song_row_t *dst){
    *dst=*src;
    if(src->bank<AB_BANKS)return 0;
    switch(src->bank){
        case AB_SONG_END:dst->bank=AB5_CMD_END;return 0;
        case AB_SONG_TEMPO:dst->bank=AB5_CMD_TEMPO;return 0;
        case AB_SONG_PRESET:dst->bank=AB5_CMD_PRESET;return 0;
        case AB_SONG_REST:dst->bank=AB5_CMD_REST;return 0;
        case AB_SONG_ARP:dst->bank=AB5_CMD_ARP;return 0;
        default:return -1;
    }
}
int ab5_from_v4_song1(ab5_extra_t *dst,const ab_snapshot_t *v4){
    uint32_t i;
    if(!dst||!v4||ab_snapshot_validate(v4)||ab5_extra_validate(dst))return -1;
    /* Never silently truncate an existing 65..256 row arrangement. */
    if(v4->song_count>AB5_ROWS)return -2;
    /* Prevent accidental overwrites of already edited v5 Song 01. */
    if(dst->songs[0].count!=1u||dst->songs[0].rows[0].bank!=AB5_CMD_END)
        return -3;
    dst->songs[0].count=(uint8_t)v4->song_count;
    dst->songs[0].loop=v4->loop_mode;
    for(i=0;i<v4->song_count;i++){
        if(translate_v4(&v4->song[i],&dst->songs[0].rows[i]))return -4;
    }
    return ab5_extra_validate(dst);
}
int ab5_album_song(const ab5_extra_t *m,uint8_t album,uint8_t slot){
    if(!m||album>=AB5_ALBUMS||slot>=m->albums[album].count)return -1;
    return m->albums[album].song_ids[slot];
}
