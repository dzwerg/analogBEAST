/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v1.0 USB-SysEx v5 transfer, called by golana_dz_midi.c.
 * Pure bounded parser: no flash access, no heap, no MIDI side effects.
 * The caller checks STOP, persistence safety and marks dirty on writes.
 */
#include "analogbeast_v5_extra.h"

static int ab5_xfer_row_ok(const ab_song_row_t *r)
{
    if (!r->play_steps || r->play_steps > AB_STEPS) return 0;
    if (r->bank < AB5_BANKS) {
        if (r->pattern >= AB5_PAT_PER_BANK || r->value != 0u) return 0;
        return r->bank < AB5_EXISTING_BANKS || r->play_steps <= AB5_EXTRA_STEPS;
    }
    if (r->bank == AB5_CMD_END)
        return r->pattern == AB5_CMD_END_PATTERN && r->value == 0u;
    if (r->pattern != 0u) return 0;
    if (r->bank == AB5_CMD_TEMPO) return r->value >= 40u && r->value <= 240u;
    if (r->bank == AB5_CMD_PRESET) return r->value < 32u;
    if (r->bank == AB5_CMD_REST) return r->value == 0u;
    if (r->bank == AB5_CMD_ARP) return r->value <= 1u;
    if (r->bank == AB5_CMD_OCTAVE) return r->value <= 6u;
    if (r->bank == AB5_CMD_TRANSPOSE) return r->value <= 48u;
    if (r->bank == AB5_CMD_REPEAT || r->bank == AB5_CMD_JUMP ||
        r->bank == AB5_CMD_ENDSTEP) return r->value >= 1u && r->value <= 64u;
    if (r->bank == AB5_CMD_BEGIN) return r->value == 0u;
    return 0;
}

/* Returns -1 on invalid request, or the number of reply DATA bytes.
 * 23 SONG WRITE: song(0..31), start(0..63), count(1..24), count*5 row bytes (8-bit value encoded as 2x7)
 * 24 SONG READ : song, start, count; reply = same header + rows
 * 25 ALBUM WRITE: album(0..3), count(0..32), loop(0/1), 32 ids (0..31, 127=empty)
 * 26 ALBUM READ : album; reply = album, count, loop, 32 ids
 * 27 SONG META WRITE: song, count(1..64), loop(0/1)
 * 28 SONG META READ : song; reply = song, count, loop
 * 29 SONG COPY: src_song, src_start, dst_song, dst_start, count; reply echoes args
 * No implicit flash save: caller performs dirty mark after successful mutations.
 */
static int ab5_xfer_exec(ab5_extra_t *m,unsigned op,const uint8_t *a,
                         unsigned n,uint8_t *out)
{
    unsigned i,off,count,start,sid;
    if (!m || !a || !out) return -1;
    if (op == 23u || op == 24u) {
        if (n < 3u || a[0] >= AB5_SONGS || a[1] >= AB5_ROWS ||
            !a[2] || a[2] > 24u || (unsigned)a[1]+a[2] > AB5_ROWS) return -1;
        sid = a[0]; start = a[1]; count = a[2];
        if (op == 23u) {
            if (n != 3u+5u*count) return -1;
            /* Validate ALL rows before the first mutation. */
            for (i=0;i<count;i++) {
                const unsigned base=3u+5u*i;
                const unsigned val=a[base+3u] | ((unsigned)a[base+4u]<<7);
                const ab_song_row_t t={a[base],a[base+1u],a[base+2u],(uint8_t)val};
                if (val>255u || a[base+4u]>1u) return -1;
                if (!ab5_xfer_row_ok(&t)) return -1;
            }
            for (i=0;i<count;i++) {
                ab_song_row_t *r=&m->songs[sid].rows[start+i];
                off=3u+5u*i;
                r->bank=a[off];r->pattern=a[off+1u];
                r->play_steps=a[off+2u];r->value=(uint8_t)(a[off+3u] | (unsigned)a[off+4u]<<7);
            }
            if (m->songs[sid].count < start+count)
                m->songs[sid].count=(uint8_t)(start+count);
        } else if (n != 3u) return -1;
        out[0]=a[0];out[1]=a[1];out[2]=a[2];
        if (op==24u) for (i=0;i<count;i++) {
            const ab_song_row_t *r=&m->songs[sid].rows[start+i];
            off=3u+5u*i;
            out[off]=r->bank;out[off+1u]=r->pattern;
            out[off+2u]=r->play_steps;out[off+3u]=r->value&127u;
            out[off+4u]=r->value>>7;
        }
        return op==24u ? (int)(3u+5u*count) : 3;
    }
    if (op == 25u || op == 26u) {
        ab5_album_t *alb;
        if (n < 1u || a[0]>=AB5_ALBUMS) return -1;
        alb=&m->albums[a[0]];
        if (op==25u) {
            if (n!=35u || a[1]>AB5_ALBUM_SLOTS || a[2]>1u) return -1;
            for (i=0;i<AB5_ALBUM_SLOTS;i++) {
                uint8_t id=a[3u+i];
                if (i<a[1] ? id>=AB5_SONGS : id!=127u) return -1;
            }
            alb->count=a[1];alb->loop=a[2];
            for (i=0;i<AB5_ALBUM_SLOTS;i++)
                alb->song_ids[i]=a[3u+i]==127u ? AB5_ALBUM_EMPTY:a[3u+i];
        } else if (n!=1u) return -1;
        out[0]=a[0];out[1]=alb->count;out[2]=alb->loop;
        if (op==26u) for(i=0;i<AB5_ALBUM_SLOTS;i++)
            out[3u+i]=alb->song_ids[i]==AB5_ALBUM_EMPTY ? 127u:alb->song_ids[i];
        return op==26u ? 35:3;
    }
    if (op==27u || op==28u) {
        ab5_song_t *s;
        if (n<1u || a[0]>=AB5_SONGS) return -1;
        s=&m->songs[a[0]];
        if (op==27u) {
            if (n!=3u || !a[1] || a[1]>AB5_ROWS || a[2]>1u)return -1;
            s->count=a[1];s->loop=a[2];
        } else if(n!=1u)return -1;
        out[0]=a[0];out[1]=s->count;out[2]=s->loop;
        return 3;
    }
    if (op==29u) {
        unsigned from,to;
        ab5_song_t *src,*dst;
        if (n!=5u || a[0]>=AB5_SONGS || a[2]>=AB5_SONGS ||
            !a[4] || a[1]>=AB5_ROWS || a[3]>=AB5_ROWS ||
            (unsigned)a[1]+a[4]>AB5_ROWS || (unsigned)a[3]+a[4]>AB5_ROWS)
            return -1;
        src=&m->songs[a[0]];dst=&m->songs[a[2]];
        if ((unsigned)a[1]+a[4]>src->count)return -1;
        for(i=0;i<a[4];i++) {
            from=(src==dst && a[3]>a[1]) ? a[1]+a[4]-1u-i:a[1]+i;
            to=(src==dst && a[3]>a[1]) ? a[3]+a[4]-1u-i:a[3]+i;
            dst->rows[to]=src->rows[from];
        }
        if(dst->count < (unsigned)a[3]+a[4])dst->count=(uint8_t)(a[3]+a[4]);
        for(i=0;i<5u;i++)out[i]=a[i];
        return 5;
    }
    return -1;
}
