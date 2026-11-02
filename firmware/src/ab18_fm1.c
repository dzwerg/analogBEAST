/* Stage18 FM-1 hardware bridge. Explicit MD16 mount required for ALL writes.
 * NO on-device format and NO GC in dev18. Never called from an audio ISR.
 *
 * UI-FIX2: build a compact RAM index once and use direct record reads afterwards.
 * The old selector re-mounted and re-scanned the complete 292 KiB volume several
 * times for every encoder detent; that is why a pattern change took ~2 seconds.
 */
#if FELUCCA_FLASH
#define AB18_PAT_MAX AB16_PATTERN_ID_MAX
#define AB18_SONG_MAX 32u
#define AB18_ALBUM_MAX 4u
#define AB18_IDX_OFF_MASK 0x000FFFFFu
#define AB18_IDX_DELETED  0x80000000u
#define AB18_IDX_FACTORY_EMPTY 0x40000000u

typedef struct {
    /* MD16 record offsets are 4-byte aligned. Store a 17-bit (offset/4+1)
     * plus 2 state bitmaps. This extends 256 -> 512 selectable IDs without
     * consuming another 1 KiB of pool RAM (minimum pool headroom is strict). */
    uint16_t pat_lo[AB18_PAT_MAX+1u];
    uint8_t pat_hi[(AB18_PAT_MAX+8u)/8u];
    uint8_t pat_deleted[(AB18_PAT_MAX+8u)/8u];
    uint8_t pat_factory_empty[(AB18_PAT_MAX+8u)/8u];
    uint32_t song[AB18_SONG_MAX+1u];
    uint32_t album[AB18_ALBUM_MAX+1u];
    uint8_t mbox[(128u+7u)/8u]; /* M1-1..M1-128, latest manifests only */
    ab16_fs fs;
    uint8_t valid;
} ab18_index_t;
static ab18_index_t ab18_ix __attribute__((section(".pool")));

static int ab18_fm1_prog(void *ctx,uint32_t off,const void *src,uint32_t n){
    (void)ctx;
    if(!flash_ok||!src||!n||off>=AB16_SIZE||n>AB16_SIZE-off||
       !FL_IN(AB16_BASE+off,n,FL_DATA_LO,FL_DATA_HI))return -1;
    fm1_wdt_feed();
    {int rc=fl_write(AB16_BASE+off,(const uint8_t *)src,n);
     fm1_wdt_feed();return rc;}
}
static int ab18_mount(ab16_fs *fs,int writing){
    ab16_io io;int rc;
    if(!flash_ok)return AB16_EIO;
    io.ctx=0;io.read=ab16_fm1_read;
    io.prog=writing?ab18_fm1_prog:0;
    io.erase=0; /* NO GC/ERASE IN STAGE18 */
    rc=ab16_mount(fs,io);
    if(!rc)ab18_disk_owned=1u; /* Protect MD16 disk from legacy Felucca writers. */
    return rc;
}
static int ab18_ready(void){
    return !ab_m_playing&&!song.playing&&!transport_busy()&&!gu.save&&!gu.dz_edit_active&&
           !ab15_cache.active&&!ui.uboot;
}
/* Only songs/albums retain 32-bit slots; patterns use compact bitplanes. */
static uint32_t *ab18_ix_slot(uint16_t type,uint32_t id){
    if(type==AB16_SONG && id>=1u && id<=AB18_SONG_MAX)return &ab18_ix.song[id];
    if(type==AB16_ALBUM && id>=1u && id<=AB18_ALBUM_MAX)return &ab18_ix.album[id];
    return 0;
}
static uint8_t ab18_pat_bit(const uint8_t *bits,uint32_t id){
    return (uint8_t)((bits[id>>3u]>>(id&7u))&1u);
}
static void ab18_pat_bit_set(uint8_t *bits,uint32_t id,uint8_t on){
    uint32_t byte=id>>3u;uint8_t mask=(uint8_t)(1u<<(id&7u));
    if(on)bits[byte]|=mask;else bits[byte]&=(uint8_t)~mask;
}
static uint32_t ab18_ix_get(uint16_t type,uint32_t id){
    uint32_t *slot,q;
    if(type==AB16_MBOX){
        if(!id||id>128u)return 0u;
        return ab18_pat_bit(ab18_ix.mbox,id-1u)?1u:0u;
    }
    if(type==AB16_PAT){
        if(id<1u||id>AB18_PAT_MAX)return 0u;
        q=(uint32_t)ab18_ix.pat_lo[id] |
          ((uint32_t)ab18_pat_bit(ab18_ix.pat_hi,id)<<16u);
        if(!q)return 0u;
        return (((q-1u)<<2u)+1u) |
            (ab18_pat_bit(ab18_ix.pat_deleted,id)?AB18_IDX_DELETED:0u) |
            (ab18_pat_bit(ab18_ix.pat_factory_empty,id)?AB18_IDX_FACTORY_EMPTY:0u);
    }
    slot=ab18_ix_slot(type,id);return slot?*slot:0u;
}
static void ab18_ix_set(uint16_t type,uint32_t id,uint32_t val){
    uint32_t *slot,q;
    if(type==AB16_PAT && id>=1u && id<=AB18_PAT_MAX){
        /* val's low bits contain byte offset+1; zero means absent. */
        q=(val&AB18_IDX_OFF_MASK);
        q=q?((q-1u)>>2u)+1u:0u;
        ab18_ix.pat_lo[id]=(uint16_t)q;
        ab18_pat_bit_set(ab18_ix.pat_hi,id,(uint8_t)((q>>16u)&1u));
        ab18_pat_bit_set(ab18_ix.pat_deleted,id,(uint8_t)!!(val&AB18_IDX_DELETED));
        ab18_pat_bit_set(ab18_ix.pat_factory_empty,id,(uint8_t)!!(val&AB18_IDX_FACTORY_EMPTY));
    }else if((slot=ab18_ix_slot(type,id))!=0)*slot=val;
}
static uint32_t ab18_ix_max(uint16_t type){
    return type==AB16_PAT?AB18_PAT_MAX:type==AB16_SONG?AB18_SONG_MAX:
           type==AB16_ALBUM?AB18_ALBUM_MAX:type==AB16_MBOX?128u:0u;
}
static int ab18_ix_record(ab16_fs *fs,uint32_t off,const ab16_ahdr *h,void *ctx){
    uint32_t *slot,old,flags=0u;uint8_t raw[AB16_HDR];ab16_ahdr prev;
    (void)ctx;
    if(h->type==AB16_MBOX){
        if(h->id>=1u&&h->id<=128u)
            ab18_pat_bit_set(ab18_ix.mbox,h->id-1u,!(h->flags&AB16_DEL));
        return 0;
    }
    slot=ab18_ix_slot(h->type,h->id);
    if(!slot && !(h->type==AB16_PAT && h->id>=1u && h->id<=AB18_PAT_MAX))return 0;
    if(h->type==AB16_PAT && (off&3u))return AB16_EINVAL;
    old=ab18_ix_get(h->type,h->id);
    if(old){
        uint32_t oo=(old&AB18_IDX_OFF_MASK)-1u;
        if(ab16_rd(fs,oo,raw,sizeof raw))return AB16_EIO;
        if(!ab16_decode(raw,&prev))return AB16_ECRC;
        if(prev.seq>=h->seq)return 0;
    }
    if(h->flags&AB16_DEL)flags|=AB18_IDX_DELETED;
    /* Legacy P14..P128 were empty on the OLD first boot. New first-boot
     * imports 128 real melodies with the identical (seq==id,length==88)
     * metadata, so metadata-only empty detection would SILENCE ALL P14+.
     * Inspect the actual payload instead and only suppress truly blank ones. */
    if(h->type==AB16_PAT && h->id>(uint32_t)NPATTERNS && h->id<=128u &&
       h->seq==h->id && h->length==88u && !(h->flags&AB16_DEL)){
        uint8_t wire[88];ab16_file f;uint32_t k;int has_note=0;
        f.offset=off;f.length=h->length;
        if(ab16_rawread(fs,&f,0u,wire,sizeof wire))return AB16_ECRC;
        for(k=0u;k<16u;k++){
            if(wire[8u+5u*k+1u]&AB13_STEP_PRESENT){has_note=1;break;}
        }
        if(!has_note)flags|=AB18_IDX_FACTORY_EMPTY;
    }
    if(off+1u>AB18_IDX_OFF_MASK)return AB16_EINVAL;
    ab18_ix_set(h->type,h->id,(off+1u)|flags);
    return 0;
}
/* Fast header walk after ab16_mount has already validated every sector tail.
 * Unlike ab16_walk this does not re-read every unused 0xff byte. */
static int ab18_ix_scan_headers(ab16_fs *fs){
    uint32_t si,off,stop;uint8_t raw[AB16_HDR];ab16_ahdr h;int rc;
    for(si=0u;si<AB16_NSECTORS;si++){
        off=si*AB16_SECTOR;stop=off+AB16_SECTOR;
        while(stop-off>=AB16_HDR){
            rc=ab16_rd(fs,off,raw,sizeof raw);if(rc)return rc;
            if(ab16_ld32(raw)==0xFFFFFFFFu)break;
            if(!ab16_decode(raw,&h)||ab16_total(h.length)>stop-off)break;
            rc=ab18_ix_record(fs,off,&h,0);if(rc)return rc;
            off+=ab16_total(h.length);
        }
    }
    return 0;
}
static int ab18_index_refresh(void){
    uint32_t i;ab16_fs fs;int rc;
    ab18_ix.valid=0u;
    memset(ab18_ix.pat_lo,0,sizeof ab18_ix.pat_lo);
    memset(ab18_ix.pat_hi,0,sizeof ab18_ix.pat_hi);
    memset(ab18_ix.pat_deleted,0,sizeof ab18_ix.pat_deleted);
    memset(ab18_ix.pat_factory_empty,0,sizeof ab18_ix.pat_factory_empty);
    memset(ab18_ix.mbox,0,sizeof ab18_ix.mbox);
    for(i=0u;i<=AB18_SONG_MAX;i++)ab18_ix.song[i]=0u;
    for(i=0u;i<=AB18_ALBUM_MAX;i++)ab18_ix.album[i]=0u;
    rc=ab18_mount(&fs,0);if(rc)return rc;
    rc=ab18_ix_scan_headers(&fs);if(rc)return rc;
    ab18_ix.fs=fs;ab18_ix.valid=1u;return 0;
}
static int ab18_index_ensure(void){return ab18_ix.valid?0:ab18_index_refresh();}
static int ab18_ix_file(uint16_t type,uint32_t id,int allow_factory_empty,ab16_file *out){
    uint32_t v,off;uint8_t raw[AB16_HDR];ab16_ahdr h;int rc;
    if(!out)return AB16_EINVAL;
    rc=ab18_index_ensure();if(rc)return rc;
    v=ab18_ix_get(type,id);if(!v)return AB16_ENOTFOUND;
    if(v&AB18_IDX_DELETED)return AB16_ENOTFOUND;
    if(!allow_factory_empty && (v&AB18_IDX_FACTORY_EMPTY))return AB16_ENOTFOUND;
    off=(v&AB18_IDX_OFF_MASK)-1u;
    rc=ab16_rd(&ab18_ix.fs,off,raw,sizeof raw);if(rc)return rc;
    if(!ab16_decode(raw,&h)||h.type!=type||h.id!=id||(h.flags&AB16_DEL))return AB16_ECRC;
    out->offset=off;out->seq=h.seq;out->id=h.id;out->type=h.type;
    out->length=h.length;out->deleted=0u;return 0;
}
/* Direct CRC-verified read of the latest indexed object. Foreground only. */
static int ab18_fast_read(uint16_t type,uint32_t id,void *dst,uint32_t cap,
                          uint32_t *length,int allow_factory_empty){
    ab16_file f;int rc;
    rc=ab18_ix_file(type,id,allow_factory_empty,&f);if(rc)return rc;
    if(f.length>cap)return AB16_EINVAL;
    rc=ab16_rawread(&ab18_ix.fs,&f,0u,dst,f.length);if(rc)return rc;
    if(length)*length=f.length;return 0;
}
static int ab18_fast_next(uint16_t type,uint32_t current,int dir,uint32_t *selected){
    uint32_t max,id,n,v;int rc;
    if(!selected||(dir!=1&&dir!=-1)||(max=ab18_ix_max(type))==0u)return AB16_EINVAL;
    rc=ab18_index_ensure();if(rc)return rc;
    id=(current>=1u&&current<=max)?current:(dir>0?max:1u);
    for(n=0u;n<max;n++){
        id=dir>0?(id>=max?1u:id+1u):(id<=1u?max:id-1u);
        v=ab18_ix_get(type,id);
        if(v && !(v&AB18_IDX_DELETED) && !(type==AB16_PAT&&(v&AB18_IDX_FACTORY_EMPTY))){
            *selected=id;return 0;
        }
    }
    return AB16_ENOTFOUND;
}
static int ab18_fast_first(uint16_t type,uint32_t *selected){
    uint32_t max,id,v;int rc;
    if(!selected||(max=ab18_ix_max(type))==0u)return AB16_EINVAL;
    rc=ab18_index_ensure();if(rc)return rc;
    for(id=1u;id<=max;id++){
        v=ab18_ix_get(type,id);
        if(v && !(v&AB18_IDX_DELETED) && !(type==AB16_PAT&&(v&AB18_IDX_FACTORY_EMPTY))){
            *selected=id;return 0;
        }
    }
    return AB16_ENOTFOUND;
}
/* Foreground-only codec scratch; keep the pool reserve for audio. */
static uint8_t ab18_pattern_wire[AB13_PATTERN_MAX_WIRE];
static int ab18_fast_pattern(uint16_t id,ab13_pattern *p,int allow_factory_empty){
    uint32_t n=0u;int rc;
    if(!p||!id)return AB16_EINVAL;
    rc=ab18_fast_read(AB16_PAT,id,ab18_pattern_wire,sizeof ab18_pattern_wire,&n,allow_factory_empty);if(rc)return rc;
    return ab13_pattern_parse(p,ab18_pattern_wire,n)?AB16_ECRC:0;
}
#define AB18_UI_SONG_ROWS 64u
#define AB18_UI_SONG_WIRE (8u+AB18_UI_SONG_ROWS*8u)
static uint8_t ab18_song_wire[AB18_UI_SONG_WIRE];
static int ab18_load_song_obj(uint16_t id,ab13_song *s){
    uint32_t n=0u,i;int rc;
    if(!s||!id||id>AB18_SONG_MAX)return AB16_EINVAL;
    rc=ab18_fast_read(AB16_SONG,id,ab18_song_wire,sizeof ab18_song_wire,&n,0);if(rc)return rc;
    if(n<16u||n>sizeof ab18_song_wire)return AB16_EINVAL;
    if(ab13_song_parse(s,ab18_song_wire,n)||s->count>AB18_UI_SONG_ROWS)return AB16_ECRC;
    for(i=s->count;i<AB18_UI_SONG_ROWS;i++){
        s->row[i].pattern_id=0u;s->row[i].steps=0u;s->row[i].value=0;
        s->row[i].op=AB13_OP_END;s->row[i].flags=0u;
    }
    s->count=AB18_UI_SONG_ROWS;
    return 0;
}
static int ab18_save_song_obj(uint16_t id,const ab13_song *s){
    ab16_fs fs;ab16_file got;uint32_t n;int rc;
    if(!ab18_ready()||!s||!id||id>AB18_SONG_MAX||s->count!=AB18_UI_SONG_ROWS||ab13_song_check(s))return AB16_ESTATE;
    n=ab13_song_size(s);if(n>AB18_UI_SONG_WIRE)return AB16_EINVAL;
    rc=ab18_mount(&fs,1);if(rc)return rc;
    rc=ab16_put(&fs,AB16_SONG,id,(uint16_t)n,ab13_song_emit,(void *)s);if(rc)return rc;
    rc=ab16_lookup(&fs,AB16_SONG,id,&got);if(rc)return rc;
    if(got.length!=n)return AB16_ECRC;
    ab18_ix.valid=0u;return ab18_index_refresh();
}
static int ab18_load_album_obj(uint16_t id,ab13_album *a){
    uint8_t wire[8u+AB13_ALBUM_SLOTS*2u];uint32_t n=0u;int rc;
    if(!a||!id||id>AB18_ALBUM_MAX)return AB16_EINVAL;
    rc=ab18_fast_read(AB16_ALBUM,id,wire,sizeof wire,&n,0);if(rc)return rc;
    return ab13_album_parse(a,wire,n)?AB16_ECRC:0;
}
static int ab18_save_album_obj(uint16_t id,const ab13_album *a){
    ab16_fs fs;ab16_file got;uint32_t n;int rc;
    if(!ab18_ready()||!a||!id||id>AB18_ALBUM_MAX||ab13_album_check(a))return AB16_ESTATE;
    n=ab13_album_size(a);if(!n||n>8u+AB13_ALBUM_SLOTS*2u)return AB16_EINVAL;
    rc=ab18_mount(&fs,1);if(rc)return rc;
    rc=ab16_put(&fs,AB16_ALBUM,id,(uint16_t)n,ab13_album_emit,(void *)a);if(rc)return rc;
    rc=ab16_lookup(&fs,AB16_ALBUM,id,&got);if(rc)return rc;
    if(got.length!=n)return AB16_ECRC;
    ab18_ix.valid=0u;return ab18_index_refresh();
}

static int ab18_save_current(void){
    ab16_fs fs;int rc;
    if(!ab18_ready()||!ab15_cache.loaded)return AB16_ESTATE;
    rc=ab18_mount(&fs,1);if(rc)return rc;
    rc=ab18_save_pattern(&fs,ab15_cache.pattern_id,&ab15_cache.pattern);
    if(!rc){
        ab15_cache.dirty=0u;ab5_store_enabled=0u;ab18_ix.valid=0u;
        rc=ab18_index_refresh();
    }
    return rc;
}
static int ab18_load_disk(uint16_t id){
    static ab13_pattern parsed;int rc;
    if(!ab18_ready()||!id)return AB16_ESTATE;
    rc=ab18_fast_pattern(id,&parsed,0);if(rc)return rc;
    return ab15_select(&ab15_cache,id,&parsed);
}
static int ab18_nav(int32_t clicks){
    uint32_t selected,i,n;int rc,dir;
    if(!ab18_ready()||!ab15_cache.loaded||!clicks||ab15_cache.dirty)return AB16_ESTATE;
    selected=ab15_cache.pattern_id;dir=clicks>0?1:-1;
    n=(uint32_t)(clicks>0?clicks:-(int64_t)clicks);if(n>8u)n=8u;
    for(i=0;i<n;i++){
        rc=ab18_fast_next(AB16_PAT,selected,dir,&selected);if(rc)return rc;
    }
    if(selected==ab15_cache.pattern_id)return 0;
    rc=ab18_load_disk((uint16_t)selected);
    if(!rc)ab17_init(&ab17_ui_delete,(uint16_t)selected);
    return rc;
}
static int ab18_delete_current(void){
    ab16_fs fs;uint32_t next,current;int rc;
    if(!ab18_ready()||!ab15_cache.loaded)return AB16_ESTATE;
    current=ab15_cache.pattern_id;
    rc=ab18_mount(&fs,1);if(rc)return rc;
    rc=ab18_delete_pattern(&fs,current);if(rc)return rc;
    ab18_ix.valid=0u;rc=ab18_index_refresh();if(rc)return rc;
    rc=ab18_fast_next(AB16_PAT,current,1,&next);
    ab15_clear(&ab15_cache);
    if(rc==AB16_ENOTFOUND){ab17_init(&ab17_ui_delete,0u);return AB16_ENOTFOUND;}
    if(rc)return rc;
    rc=ab18_load_disk((uint16_t)next);
    if(!rc)ab17_init(&ab17_ui_delete,(uint16_t)next);
    return rc;
}
#if FELUCCA_CDC
static void ab18_console_save(void){
    int rc=ab18_save_current();
    con_kv("patsave_rc",rc);
    if(rc==AB16_ENOFS||rc==AB16_EFOREIGN)
        con_puts("MUSIC DISK not initialized. No writes; old data untouched.\r\n");
    else if(!rc)con_puts("PATTERN SAVED (CRC VERIFIED)\r\n");
}
#endif /* FELUCCA_CDC */
#endif
