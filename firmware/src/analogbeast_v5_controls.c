/* SPDX-License-Identifier: GPL-3.0-only */
#include "analogbeast_v5_controls.h"

static ab5_song_t *song_now(ab5_extra_t *m) { return &m->songs[m->selected_song]; }
static const ab5_song_t *song_const(const ab5_extra_t *m) { return &m->songs[m->selected_song]; }
static ab_song_row_t *row_now(ab5_extra_t *m) { return &song_now(m)->rows[m->song_cursor]; }
static const ab_song_row_t *row_const(const ab5_extra_t *m) { return &song_const(m)->rows[m->song_cursor]; }
static ab5_album_t *album_now(ab5_extra_t *m) { return &m->albums[m->selected_album]; }
static const ab5_album_t *album_const(const ab5_extra_t *m) { return &m->albums[m->selected_album]; }

static int extra_index(const ab5_extra_t *m)
{
    if(m->bank<AB5_EXISTING_BANKS || m->bank>=AB5_BANKS || m->pattern>=AB5_PAT_PER_BANK)
        return -1;
    return (int)(m->bank-AB5_EXISTING_BANKS)*AB5_PAT_PER_BANK+m->pattern;
}
static ab5_pattern_t *extra_pat(ab5_extra_t *m) { int i=extra_index(m); return i<0?0:&m->extra_patterns[i]; }
static const ab5_pattern_t *extra_pat_const(const ab5_extra_t *m) { int i=extra_index(m); return i<0?0:&m->extra_patterns[i]; }
static ab_step_t *extra_step(ab5_extra_t *m) { ab5_pattern_t *p=extra_pat(m); return p?&p->steps[m->edit_step]:0; }
static const ab_step_t *extra_step_const(const ab5_extra_t *m) { const ab5_pattern_t *p=extra_pat_const(m); return p?&p->steps[m->edit_step]:0; }
static int step_type(const ab_step_t *s) { return (s->flags&4u)?2:s->note?1:0; }
static int row_value(const ab_song_row_t *row)
{
    if (row->bank < AB5_BANKS) return (int)row->pattern + 1;
    if (row->bank == AB5_CMD_TEMPO) return row->value;
    if (row->bank == AB5_CMD_PRESET) return (int)row->value + 1;
    if (row->bank == AB5_CMD_ARP) return row->value;
    if (row->bank == AB5_CMD_REST) return row->play_steps;
    return 0;
}
int ab5_ui_range(const ab5_extra_t *m,unsigned id,int *lo,int *hi)
{
    const ab_song_row_t *row;
    if (!m || !lo || !hi || id >= AB5_UI_NPARAMS) return -1;
    if (id >= AB5_UI_EDIT_NOTE && extra_index(m)<0) return -1;
    *lo = 1;
    switch (id) {
    case AB5_UI_MODE: *lo=0; *hi=AB5_MODE_ALBUM; break;
    case AB5_UI_BANK: *lo=0; *hi=AB5_BANKS-1; break;
    case AB5_UI_PATTERN: *hi=AB5_PAT_PER_BANK; break;
    case AB5_UI_SELECT:
        *hi = m->play_mode == AB5_MODE_SONG ? AB5_SONGS :
              m->play_mode == AB5_MODE_ALBUM ? AB5_ALBUMS : 1;
        break;
    case AB5_UI_SONG: *hi=AB5_SONGS; break;
    case AB5_UI_ROW: *hi=AB5_ROWS; break;
    case AB5_UI_KIND: *lo=0; *hi=AB5_CMD_ARP; break;
    case AB5_UI_VALUE:
        row=row_const(m);
        if (row->bank<AB5_BANKS) *hi=AB5_PAT_PER_BANK;
        else if (row->bank==AB5_CMD_TEMPO) {*lo=40;*hi=240;}
        else if (row->bank==AB5_CMD_PRESET) *hi=32;
        else if (row->bank==AB5_CMD_ARP) {*lo=0;*hi=1;}
        else if (row->bank==AB5_CMD_REST) *hi=AB_STEPS;
        else {*lo=0;*hi=0;}
        break;
    case AB5_UI_STEPS:
        row=row_const(m);
        *hi=row->bank>=AB5_EXISTING_BANKS && row->bank<AB5_BANKS ?
            AB5_EXTRA_STEPS:AB_STEPS;
        break;
    case AB5_UI_SONG_LOOP: case AB5_UI_ALBUM_LOOP: *lo=0;*hi=1;break;
    case AB5_UI_ALBUM: *hi=AB5_ALBUMS;break;
    case AB5_UI_SLOT: *hi=AB5_ALBUM_SLOTS;break;
    case AB5_UI_ALBUM_SONG: *lo=0;*hi=AB5_SONGS;break;
    case AB5_UI_EDIT_STEP: *hi=AB5_EXTRA_STEPS;break;
    case AB5_UI_EDIT_NOTE: *lo=1; *hi=127;break;
    case AB5_UI_EDIT_TYPE: *lo=0;*hi=2;break;
    case AB5_UI_EDIT_FLAGS: *lo=0;*hi=3;break;
    case AB5_UI_EDIT_VELO: case AB5_UI_EDIT_GATE: *lo=1;*hi=127;break;
    case AB5_UI_EDIT_SWING: *lo=0;*hi=100;break;
    case AB5_UI_EDIT_LENGTH: *hi=AB5_EXTRA_STEPS;break;
    case AB5_UI_EDIT_RES: *lo=0;*hi=5;break;
    default: return -1;
    }
    return 0;
}
int ab5_ui_get(const ab5_extra_t *m,unsigned id)
{
    if (!m || id>=AB5_UI_NPARAMS) return -1;
    switch(id) {
    case AB5_UI_MODE: return m->play_mode;
    case AB5_UI_BANK: return m->bank;
    case AB5_UI_PATTERN: return m->pattern+1;
    case AB5_UI_SELECT: return m->play_mode==AB5_MODE_SONG ? m->selected_song+1 :
                               m->play_mode==AB5_MODE_ALBUM ? m->selected_album+1 : 1;
    case AB5_UI_SONG: return m->selected_song+1;
    case AB5_UI_ROW: return m->song_cursor+1;
    case AB5_UI_KIND: return row_const(m)->bank;
    case AB5_UI_VALUE: return row_value(row_const(m));
    case AB5_UI_STEPS: return row_const(m)->play_steps;
    case AB5_UI_SONG_LOOP: return song_const(m)->loop;
    case AB5_UI_ALBUM: return m->selected_album+1;
    case AB5_UI_SLOT: return m->album_cursor+1;
    case AB5_UI_ALBUM_SONG: {
        uint8_t id=album_const(m)->song_ids[m->album_cursor];
        return id==AB5_ALBUM_EMPTY ? 0 : id+1;
    }
    case AB5_UI_ALBUM_LOOP: return album_const(m)->loop;
    case AB5_UI_EDIT_STEP: return m->edit_step+1;
    case AB5_UI_EDIT_NOTE: { const ab_step_t *s=extra_step_const(m); return s?s->note?s->note:60:0; }
    case AB5_UI_EDIT_TYPE: { const ab_step_t *s=extra_step_const(m); return s?step_type(s):0; }
    case AB5_UI_EDIT_FLAGS: { const ab_step_t *s=extra_step_const(m); return s?s->flags&3:0; }
    case AB5_UI_EDIT_VELO: { const ab_step_t *s=extra_step_const(m); return s?s->velocity:0; }
    case AB5_UI_EDIT_GATE: { const ab_step_t *s=extra_step_const(m); return s?s->gate:0; }
    case AB5_UI_EDIT_SWING: { const ab_step_t *s=extra_step_const(m); return s?s->swing:0; }
    case AB5_UI_EDIT_LENGTH: { const ab5_pattern_t *p=extra_pat_const(m); return p?p->length:0; }
    case AB5_UI_EDIT_RES: { const ab5_pattern_t *p=extra_pat_const(m); return p?p->resolution:0; }
    default: return -1;
    }
}
static void kind_default(ab_song_row_t *r,uint8_t kind)
{
    r->bank=kind;
    r->pattern=0u;
    r->value=0u;
    if (!r->play_steps) r->play_steps=1u;
    if (kind>=AB5_EXISTING_BANKS && kind<AB5_BANKS && r->play_steps>AB5_EXTRA_STEPS)
        r->play_steps=AB5_EXTRA_STEPS;
    if (kind==AB5_CMD_END) r->pattern=AB5_CMD_END_PATTERN;
    if (kind==AB5_CMD_TEMPO) r->value=120u;
}
int ab5_ui_set(ab5_extra_t *m,unsigned id,int value)
{
    int lo,hi,old;
    ab_song_row_t *r;
    ab5_album_t *a;
    if (ab5_ui_range(m,id,&lo,&hi) || value<lo || value>hi) return -1;
    old=ab5_ui_get(m,id);
    if (old==value) return 0;
    switch(id) {
    case AB5_UI_EDIT_STEP: m->edit_step=(uint8_t)(value-1);return 1;
    case AB5_UI_EDIT_NOTE:
        extra_step(m)->note=(uint8_t)value;
        extra_step(m)->flags=(uint8_t)(extra_step(m)->flags & ~4u);
        return 2;
    case AB5_UI_EDIT_TYPE:
        if (value==0){extra_step(m)->note=0u;extra_step(m)->flags=0u;}
        else if (value==2){extra_step(m)->note=0u;extra_step(m)->flags=4u;}
        else {extra_step(m)->note=60u;extra_step(m)->flags&=(uint8_t)~4u;}
        return 2;
    case AB5_UI_EDIT_FLAGS:
        if(step_type(extra_step(m))!=1)return -1;
        extra_step(m)->flags=(uint8_t)((extra_step(m)->flags & ~3u) | (uint8_t)value);
        return 2;
    case AB5_UI_EDIT_VELO: extra_step(m)->velocity=(uint8_t)value;return 2;
    case AB5_UI_EDIT_GATE: extra_step(m)->gate=(uint8_t)value;return 2;
    case AB5_UI_EDIT_SWING: extra_step(m)->swing=(uint8_t)value;return 2;
    case AB5_UI_EDIT_LENGTH: extra_pat(m)->length=(uint8_t)value;return 2;
    case AB5_UI_EDIT_RES: extra_pat(m)->resolution=(uint8_t)value;return 2;
    case AB5_UI_MODE: m->play_mode=(uint8_t)value;return 1;
    case AB5_UI_BANK: m->bank=(uint8_t)value;return 1;
    case AB5_UI_PATTERN: m->pattern=(uint8_t)(value-1);return 1;
    case AB5_UI_SELECT:
        if (m->play_mode==AB5_MODE_SONG) m->selected_song=(uint8_t)(value-1);
        else if (m->play_mode==AB5_MODE_ALBUM) m->selected_album=(uint8_t)(value-1);
        else return -1;
        return 1;
    case AB5_UI_SONG: m->selected_song=(uint8_t)(value-1);return 1;
    case AB5_UI_ROW: m->song_cursor=(uint8_t)(value-1);return 1;
    case AB5_UI_ALBUM: m->selected_album=(uint8_t)(value-1);return 1;
    case AB5_UI_SLOT: m->album_cursor=(uint8_t)(value-1);return 1;
    case AB5_UI_KIND:
        r=row_now(m);
        kind_default(r,(uint8_t)value);
        if (song_now(m)->count<=m->song_cursor)
            song_now(m)->count=(uint8_t)(m->song_cursor+1u);
        return 2;
    case AB5_UI_VALUE:
        r=row_now(m);
        if (r->bank<AB5_BANKS) r->pattern=(uint8_t)(value-1);
        else if (r->bank==AB5_CMD_TEMPO) r->value=(uint8_t)value;
        else if (r->bank==AB5_CMD_PRESET) r->value=(uint8_t)(value-1);
        else if (r->bank==AB5_CMD_ARP) r->value=(uint8_t)value;
        else if (r->bank==AB5_CMD_REST) r->play_steps=(uint8_t)value;
        else return -1;
        if (song_now(m)->count<=m->song_cursor)
            song_now(m)->count=(uint8_t)(m->song_cursor+1u);
        return 2;
    case AB5_UI_STEPS:
        row_now(m)->play_steps=(uint8_t)value;
        if (song_now(m)->count<=m->song_cursor)
            song_now(m)->count=(uint8_t)(m->song_cursor+1u);
        return 2;
    case AB5_UI_SONG_LOOP: song_now(m)->loop=(uint8_t)value;return 2;
    case AB5_UI_ALBUM_LOOP: album_now(m)->loop=(uint8_t)value;return 2;
    case AB5_UI_ALBUM_SONG:
        a=album_now(m);
        if (value==0) {
            /* Only remove final slot: never silently discard following songs. */
            if (m->album_cursor+1u!=a->count) return -1;
            a->song_ids[m->album_cursor]=AB5_ALBUM_EMPTY;
            --a->count;
        } else {
            /* Contiguous append or replace only: no hidden holes. */
            if (m->album_cursor>a->count) return -1;
            a->song_ids[m->album_cursor]=(uint8_t)(value-1);
            if (m->album_cursor==a->count) ++a->count;
        }
        return 2;
    default: return -1;
    }
}

/* Existing album storage is a contiguous song list: END is count, not a separate row.
 * Explicitly turning knob 3 to END truncates trailing album references, NOT songs.
 * Only append a default S01 if selecting SONG exactly at the first empty slot. */
int ab5_album_set_kind(ab5_extra_t *m,int end)
{
    ab5_album_t *a;
    unsigned i;
    if(!m || (end!=0 && end!=1) || m->selected_album>=AB5_ALBUMS ||
       m->album_cursor>=AB5_ALBUM_SLOTS) return -1;
    a=album_now(m);
    if(end){
        if(m->album_cursor>=a->count)return 0;
        for(i=m->album_cursor;i<AB5_ALBUM_SLOTS;i++)a->song_ids[i]=AB5_ALBUM_EMPTY;
        a->count=m->album_cursor;
        return 2;
    }
    if(m->album_cursor < a->count)return 0;
    if(m->album_cursor > a->count)return -1;
    return ab5_ui_set(m,AB5_UI_ALBUM_SONG,1);
}
