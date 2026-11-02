/* MUSIC DISK over the EXISTING Web MIDI SysEx editor, command 90 (0x5A).
 * No USB CDC transport, no new transfer framebuffer: reuse the MD16 codec
 * scratch (ab18_pattern_wire), editor ed_out and editor ed_unpack7.
 * Format: F0 7D 46 4C 5A [op,...] F7.
 * Ops: 0=INFO, 1=LIST, 2=GET, 3=BEGIN, 4=DATA, 5=COMMIT,
 *      6=ABORT, 7=DELETE. Replies: [same op, status, ...].
 * Integers are little-endian 7-bit groups. Payload bytes use existing
 * MSB-packed groups of seven, like the preset/sample editor.
 * All disk access happens in the foreground, never in the audio ISR.
 */
#define MD_OP_INFO 0u
#define MD_OP_LIST 1u
#define MD_OP_GET 2u
#define MD_OP_BEGIN 3u
#define MD_OP_DATA 4u
#define MD_OP_COMMIT 5u
#define MD_OP_ABORT 6u
#define MD_OP_DELETE 7u
#define MD_RAW_STEP 112u  /* bounded frame/response, no extra FIFO */
typedef struct {
    uint32_t id;
    uint16_t size, used;
    uint8_t type, overwrite, active;
} ab_md_upload_t;
static ab_md_upload_t md_up;
static uint32_t md_v(const uint8_t *b,uint32_t n){
    uint32_t v=0,i;for(i=0;i<n;i++)v|=(uint32_t)(b[i]&127u)<<(7u*i);return v;
}
static void md_emit_v(uint32_t v,uint32_t n){
    uint32_t i;for(i=0;i<n;i++)ed_b(v>>(7u*i));
}
static void md_pack7(const uint8_t *b,uint32_t n){
    uint32_t j,k,msb;
    while(n){k=n>7u?7u:n;msb=0;
        for(j=0;j<k;j++)msb|=(uint32_t)(b[j]>>7u)<<j;
        ed_b(msb);
        for(j=0;j<k;j++)ed_b(b[j]);
        n-=k;b+=k;
    }
}
static int md_id_ok(uint32_t t,uint32_t id){
    if(!id)return 0;
    if(t==AB16_PAT)return id<=AB16_PATTERN_ID_MAX;
    if(t==AB16_SONG)return id<=AB18_SONG_MAX;
    if(t==AB16_ALBUM)return id<=AB18_ALBUM_MAX;
    if(t==AB16_MBOX)return id<=AB_M_SLOTS;
    if(t==AB16_MCHUNK)return id<=0xFFFFFFFFu;
    return 0; /* no unvalidated arbitrary flash or legacy preset writes */
}
static uint32_t md_crc_raw(const uint8_t *b,uint32_t n){return ~ab16_crc_add(0xFFFFFFFFu,b,n);}
static int md_fill(void *ctx,uint32_t off,void *dst,uint32_t n){
    memcpy(dst,(const uint8_t*)ctx+off,n);return 0;
}
static int md_idle(void){
    return !md_up.active && !ab_m_playing && !ab_md_playlist_active &&
           !song.playing && !transport_busy() && !ab15_cache.active &&
           !ab15_cache.dirty && !ab_md.song_dirty && !ab_md.album_dirty &&
           !gu.save && !ui.uboot;
}
/* For an M manifest the serialized chunk offsets are physical FM-1 offsets.
 * Resolve them here, AFTER all immutable chunks are CRC-verified. This is
 * what makes exported M songs portable between different MUSIC DISKs. */
static int md_fix_mbox(ab16_fs *fs,uint8_t *m,uint16_t n){
    uint32_t i,total=0,crc=0xFFFFFFFFu,token,k;
    ab16_file ent;
    if(n<AB_M_HEADER_BYTES||m[0]!='M'||m[1]!='B'||m[2]!='X'||m[3]!='1'||
       !m[20]||m[20]>AB_M_MAX_CHUNKS||n!=AB_M_HEADER_BYTES+4u*m[20]||
       m[21]<1u||m[21]>32u||m[54]!=1u||
       ab_m_ld16(m+4u)<40u||ab_m_ld16(m+4u)>300u||
       ab_m_ld16(m+6u)<24u||ab_m_ld16(m+6u)>960u)return AB16_EINVAL;
    token=ab_m_ld32(m+16u);
    if(!token || token>=0x01000000u)return AB16_EINVAL;
    for(i=0;i<m[20];i++){
        int rc=ab16_lookup(fs,AB16_MCHUNK,(token<<8u)|i,&ent);
        if(rc || !ent.length || ent.length>AB_M_CHUNK_BYTES || ent.length%6u)return AB16_ECRC;
        /* M chunks are short: use existing song scratch, NOT an added buffer. */
        rc=ab16_read(fs,AB16_MCHUNK,(token<<8u)|i,0,ab18_song_wire,ent.length);
        if(rc)return rc;
        for(k=0;k<ent.length;k+=6u){
            uint8_t *e=ab18_song_wire+k,op=e[2];
            if(e[3]>15u || !((op==0x80u||op==0x90u||op==0xA0u||op==0xB0u||
                op==0xC0u||op==0xD0u||op==0xE0u||op==0xF1u||op==0xF2u)))return AB16_ECRC;
        }
        crc=ab16_crc_add(crc,ab18_song_wire,ent.length);
        total+=ent.length/6u;
        ab16_st32(m+AB_M_HEADER_BYTES+4u*i,ent.offset);
    }
    if(total!=ab_m_ld32(m+8u) || (~crc)!=ab_m_ld32(m+12u))return AB16_ECRC;
    return 0;
}
static int md_validate(uint32_t t,uint8_t *b,uint16_t n){
    if(t==AB16_PAT)return ab13_pattern_parse(&ab_md.next_pattern,b,n)?AB16_ECRC:0;
    if(t==AB16_SONG){
        if(n>sizeof ab18_song_wire)return AB16_EINVAL;
        if(ab13_song_parse(&ab_md.next_song,b,n))return AB16_ECRC;
        return 0;
    }
    if(t==AB16_ALBUM)return ab13_album_parse(&ab_md.queued_album,b,n)?AB16_ECRC:0;
    if(t==AB16_MCHUNK){
        uint32_t i;if(!n||n>AB_M_CHUNK_BYTES||n%6u)return AB16_EINVAL;
        for(i=0;i<n;i+=6u){
            uint8_t op=b[i+2u];if(b[i+3u]>15u ||
                !((op>=0x80u&&op<=0xE0u&&(op&15u)==0u)||op==0xF1u||op==0xF2u))return AB16_EINVAL;
        }
    }
    return 0;
}
static void ab_md_editor_cmd(const uint8_t *a,uint32_t n){
    uint32_t op,t,id,off,size,k;ab16_fs fs;ab16_file entry;int rc;
    if(!n){ed_b(127u);ed_b(1u);return;}
    op=a[0];ed_b(op);
    if(op==MD_OP_ABORT){md_up.active=0u;ed_b(0u);return;}
    if(op==MD_OP_INFO){
        rc=ab18_mount(&fs,0);ed_b(rc?2u:0u);
        if(!rc){md_emit_v(AB16_SIZE,3u);md_emit_v(fs.blank_sectors,2u);md_emit_v(fs.next_seq,5u);}
        return;
    }
    if(op==MD_OP_LIST){
        uint16_t bits=0;uint32_t first;
        if(n!=3u || md_up.active || !md_id_ok(a[1],1u) || a[1]==AB16_MCHUNK){ed_b(1u);return;}
        t=a[1];first=(uint32_t)a[2]*16u+1u;
        rc=ab18_index_ensure();if(rc){ed_b(2u);return;}
        for(k=0;k<16u;k++){
            id=first+k;size=ab18_ix_get((uint16_t)t,id);
            if(size && !(size & AB18_IDX_DELETED) && !(t==AB16_PAT && (size&AB18_IDX_FACTORY_EMPTY)))bits|=(uint16_t)(1u<<k);
        }
        ed_b(0u);ed_b(t);ed_b(a[2]);md_emit_v(bits,3u);return;
    }
    if(op==MD_OP_GET){
        if(n!=10u || md_up.active){ed_b(1u);return;}
        t=a[1];id=md_v(a+2,5u);off=md_v(a+7,2u);size=a[9];
        if(!md_id_ok(t,id) || !size || size>MD_RAW_STEP){ed_b(1u);return;}
        rc=ab18_mount(&fs,0);if(!rc)rc=ab16_lookup(&fs,(uint16_t)t,id,&entry);
        if(rc){ed_b(3u);return;}
        if(off>=entry.length){ed_b(1u);return;}
        if(size>entry.length-off)size=entry.length-off;
        rc=ab16_read(&fs,(uint16_t)t,id,off,ab18_pattern_wire,size);
        if(rc){ed_b(5u);return;}
        ed_b(0u);ed_b(t);md_emit_v(id,5u);md_emit_v(off,2u);
        md_emit_v(entry.length,2u);ed_b(size);md_pack7(ab18_pattern_wire,size);return;
    }
    if(op==MD_OP_BEGIN){
        if(n!=10u || !md_idle()){ed_b(2u);return;}
        t=a[1];id=md_v(a+2u,5u);size=md_v(a+7u,2u);
        if(!md_id_ok(t,id) || !size || size>sizeof ab18_pattern_wire || a[9]>1u ||
           (t==AB16_MCHUNK && size>AB_M_CHUNK_BYTES) ||
           (t==AB16_MBOX && size>AB_M_HEADER_BYTES+AB_M_MAX_CHUNKS*4u)){
            ed_b(1u);return;
        }
        rc=ab18_mount(&fs,0);if(rc){ed_b(3u);return;}
        rc=ab16_lookup(&fs,(uint16_t)t,id,&entry);
        if(!rc && !a[9]){ed_b(4u);return;}
        if(rc!=0 && rc!=AB16_ENOTFOUND){ed_b(3u);return;}
        md_up.id=id;md_up.type=(uint8_t)t;md_up.size=(uint16_t)size;
        md_up.used=0u;md_up.overwrite=a[9];md_up.active=1u;
        ed_b(0u);return;
    }
    if(op==MD_OP_DATA){
        uint32_t len;
        if(n<4u || !md_up.active){ed_b(2u);return;}
        off=md_v(a+1u,2u);
        if(off!=md_up.used || off>=md_up.size){ed_b(1u);return;}
        len=ed_unpack7(a+3u,n-3u,ab18_pattern_wire+off,md_up.size-off);
        if(!len || len>MD_RAW_STEP){ed_b(1u);return;}
        md_up.used=(uint16_t)(md_up.used+len);ed_b(0u);
        md_emit_v(md_up.used,2u);return;
    }
    if(op==MD_OP_COMMIT){
        if(n!=6u || !md_up.active || md_up.used!=md_up.size){ed_b(2u);return;}
        if(md_crc_raw(ab18_pattern_wire,md_up.size)!=md_v(a+1u,5u)){
            ed_b(5u);return;
        }
        if(ab_m_playing || song.playing || transport_busy() || ab15_cache.active || gu.save){ed_b(2u);return;}
        rc=md_validate(md_up.type,ab18_pattern_wire,md_up.size);
        if(rc){ed_b(5u);return;}
        rc=ab18_mount(&fs,1);if(rc){ed_b(3u);return;}
        if(md_up.type==AB16_MBOX)rc=md_fix_mbox(&fs,ab18_pattern_wire,md_up.size);
        if(rc){ed_b(5u);return;}
        /* Re-check existence on COMMIT: another writer must not bypass the
         * no-overwrite promise. The FS appends atomically and never erases. */
        if(!md_up.overwrite){ab16_file existing;
            int exists=ab16_lookup(&fs,md_up.type,md_up.id,&existing);
            if(!exists){ed_b(4u);return;}
            if(exists!=AB16_ENOTFOUND){ed_b(3u);return;}
        }
        rc=ab16_put(&fs,md_up.type,md_up.id,md_up.size,md_fill,ab18_pattern_wire);
        if(rc){ed_b(rc==AB16_ENOSPACE?6u:3u);return;}
        md_up.active=0u;ab18_ix.valid=0u;
        ed_b(0u);return;
    }
    if(op==MD_OP_DELETE){
        if(n!=7u || !md_idle()){ed_b(2u);return;}
        t=a[1];id=md_v(a+2u,5u);
        if(!md_id_ok(t,id) || t==AB16_MCHUNK){ed_b(1u);return;}
        rc=ab18_mount(&fs,1);if(!rc)rc=ab16_remove(&fs,(uint16_t)t,id);
        if(rc){ed_b(rc==AB16_ENOTFOUND?3u:5u);return;}
        ab18_ix.valid=0u;ed_b(0u);return;
    }
    ed_b(1u);
}
