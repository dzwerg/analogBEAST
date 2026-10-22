/* SPDX-License-Identifier: GPL-3.0-only */
#include "analogbeast_v5_runtime.h"

int ab5_boot_preview(ab5_extra_t *dst,ab5_nor_state_t *state,
                     const ab_snapshot_t *v4,const ab_nor_io_t *io)
{
    int rc=-1, probe;
    if(!dst||!state||!v4||ab_snapshot_validate(v4))return AB5_BOOT_FLASH_ERROR;
    ab5_extra_init(dst);
    state->valid=0u;
    state->active_copy=0u;
    state->generation=0u;
    state->copy_kind[0]=state->copy_kind[1]=AB5_NOR_COPY_FOREIGN;
    if(io && io->read) {
        rc=ab5_nor_load(io,state,ab5_extra_put,dst);
        if(rc==0 && ab5_extra_validate(dst)==0)return AB5_BOOT_V5_VALID;
    }
    /* A corrupt/partial load MUST NOT leak into the live RAM model. */
    ab5_extra_init(dst);
    if(v4->song_count>AB5_ROWS)return AB5_BOOT_V4_TOO_LONG;
    probe=ab5_from_v4_song1(dst,v4);
    if(probe)return AB5_BOOT_NO_MIGRATION;
    /* Read-only preview even when PDZ6 is FOREIGN in the 32-KiB area. */
    if(!io || !io->read || rc<0)return AB5_BOOT_FLASH_ERROR;
    return AB5_BOOT_V4_PREVIEW;
}

const ab_step_t *ab5_step_read(const ab5_extra_t *m,const ab_snapshot_t *v4,
                              uint8_t bank,uint8_t pattern,uint8_t step)
{
    if(!m||!v4||bank>=AB5_BANKS||pattern>=AB5_PAT_PER_BANK)return 0;
    if(bank<AB5_EXISTING_BANKS){
        if(step>=AB_STEPS)return 0;
        return &v4->patterns[(uint32_t)bank*AB5_PAT_PER_BANK+pattern].step[step];
    }
    if(step>=AB5_EXTRA_STEPS)return 0;
    return &m->extra_patterns[(uint32_t)(bank-AB5_EXISTING_BANKS)*AB5_PAT_PER_BANK+pattern].steps[step];
}
uint8_t ab5_pattern_length(const ab5_extra_t *m,const ab_snapshot_t *v4,
                           uint8_t bank,uint8_t pattern)
{
    if(!m||!v4||bank>=AB5_BANKS||pattern>=AB5_PAT_PER_BANK)return 0u;
    if(bank<AB5_EXISTING_BANKS)
        return v4->patterns[(uint32_t)bank*AB5_PAT_PER_BANK+pattern].length;
    return m->extra_patterns[(uint32_t)(bank-AB5_EXISTING_BANKS)*AB5_PAT_PER_BANK+pattern].length;
}
static void ab5_zero(void *ptr,uint32_t n)
{
    uint32_t i;uint8_t *d=(uint8_t *)ptr;
    for(i=0;i<n;i++)d[i]=0u;
}
static int ab5_next_song(const ab5_extra_t *m,ab5_play_t *p)
{
    const ab5_album_t *album=&m->albums[p->album_id];
    uint8_t next=(uint8_t)(p->album_pos+1u);
    if(next>=album->count){
        if(!album->loop||!album->count)return 0;
        next=0u;
    }
    p->album_pos=next;
    p->song_id=album->song_ids[next];
    p->row_pos=0u;
    return 1;
}
/* Bound command-only songs/albums so infinite END/ARPs never lock the ISR. */
static int ab5_seek_request(const ab5_extra_t *m,ab5_play_t *p,
                            uint32_t request,uint8_t *taken)
{
    uint32_t scanned;
    p->tempo_changed=p->preset_changed=p->arp_changed=0u;
    p->rest=0u;
    for(scanned=0u;scanned<(uint32_t)AB5_SONGS*AB5_ROWS+1u;scanned++){
        const ab5_song_t *song=&m->songs[p->song_id];
        const ab_song_row_t *row;
        if(p->row_pos>=song->count || song->rows[p->row_pos].bank==AB5_CMD_END){
            /* Manual choice wins over LOOP and ONCE, but only at SONG END. */
            if(request && taken && !*taken){
                uint32_t kind=request>>24, hi=(request>>8)&255u, lo=request&255u;
                if(p->mode==AB5_MODE_SONG && kind==AB5_REQ_SONG && lo<AB5_SONGS){
                    p->song_id=(uint8_t)lo;p->row_pos=0u;*taken=1u;continue;
                }
                if(p->mode==AB5_MODE_ALBUM){
                    uint8_t slot=kind==AB5_REQ_ALBUM ? 0u : (uint8_t)lo;
                    if((kind==AB5_REQ_ALBUM || kind==AB5_REQ_ALBUM_SLOT) &&
                       hi<AB5_ALBUMS && slot<m->albums[hi].count){
                        p->album_id=(uint8_t)hi;
                        p->album_pos=slot;
                        p->song_id=m->albums[hi].song_ids[slot];
                        p->row_pos=0u;*taken=1u;continue;
                    }
                }
            }
            if(p->mode==AB5_MODE_ALBUM){
                if(!ab5_next_song(m,p))break;
            }else if(song->loop && p->row_pos!=0u){
                p->row_pos=0u;
            }else break;
            continue;
        }
        row=&song->rows[p->row_pos];
        if(row->bank<AB5_BANKS || row->bank==AB5_CMD_REST){
            p->rest=(uint8_t)(row->bank==AB5_CMD_REST);
            if(!p->rest){p->bank=row->bank;p->pattern=row->pattern;}
            p->play_steps=row->play_steps;
            return 1;
        }
        if(row->bank==AB5_CMD_TEMPO){
            p->tempo=row->value;p->tempo_changed=1u;
        }else if(row->bank==AB5_CMD_PRESET){
            p->preset=row->value;p->preset_changed=1u;
        }else if(row->bank==AB5_CMD_ARP){
            p->arp_on=(uint8_t)!!row->value;p->arp_changed=1u;
        }
        p->row_pos++;
    }
    p->running=0u;
    return 0;
}
int ab5_play_start(const ab5_extra_t *m,const ab_snapshot_t *v4,ab5_play_t *p)
{
    if(!m||!v4||!p||ab5_extra_validate(m)||ab_snapshot_validate(v4))return 0;
    ab5_zero(p,sizeof *p);
    p->mode=m->play_mode;p->running=1u;p->pattern_loop=1u;
    if(p->mode==AB5_MODE_PATTERN){
        p->bank=m->bank;p->pattern=m->pattern;
        p->play_steps=ab5_pattern_length(m,v4,p->bank,p->pattern);
        return p->play_steps!=0u;
    }
    if(p->mode==AB5_MODE_ALBUM){
        const ab5_album_t *a=&m->albums[m->selected_album];
        if(!a->count){p->running=0u;return 0;}
        p->album_id=m->selected_album;
        p->song_id=a->song_ids[0];
    }else p->song_id=m->selected_song;
    return ab5_seek_request(m,p,0u,0);
}
int ab5_play_next_request(const ab5_extra_t *m,const ab_snapshot_t *v4,
                          ab5_play_t *p,uint32_t request,uint8_t *taken)
{
    if(taken)*taken=0u;
    if(!m||!p||!p->running)return 0;
    if(p->mode==AB5_MODE_PATTERN){
        if((request>>24)==AB5_REQ_PATTERN && taken){
            uint8_t bank=(uint8_t)((request>>8)&255u), pat=(uint8_t)(request&255u);
            uint8_t n=ab5_pattern_length(m,v4,bank,pat);
            if(n){p->bank=bank;p->pattern=pat;p->play_steps=n;*taken=1u;return 1;}
        }
        if(!p->pattern_loop){p->running=0u;return 0;}
        return 1;
    }
    p->row_pos++;
    return ab5_seek_request(m,p,request,taken);
}
int ab5_play_next(const ab5_extra_t *m,const ab_snapshot_t *v4,ab5_play_t *p)
{
    return ab5_play_next_request(m,v4,p,0u,0);
}
