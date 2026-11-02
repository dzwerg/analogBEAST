/* SPDX-License-Identifier: GPL-3.0-only */
#include "analogbeast_v5_nor.h"
_Static_assert(AB5_COPY_BYTES == 4u * AB5_NOR_SECTOR_BYTES, "expected four 4K sectors per copy");
_Static_assert(AB5_NOR_DATA_OFFSET + AB5_NOR_PAYLOAD_BYTES <= AB5_COPY_BYTES,
               "v5 store payload exceeds 16KiB copy");
_Static_assert(AB5_NOR_HEADER_BYTES <= AB5_NOR_DATA_OFFSET, "header overlaps data");
_Static_assert(AB5_STORE_REGION == 0xC9000u && AB5_STORE_END == 0xD1000u,
               "v5 A/B must stay in unused third sample slot, away from PDZ6 and v4");

static uint32_t ab5_copy_base(unsigned copy) { return AB5_STORE_REGION + (uint32_t)copy * AB5_COPY_BYTES; }
static uint32_t ab5_read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8u) | ((uint32_t)p[2]<<16u) | ((uint32_t)p[3]<<24u);
}
static void ab5_write32(uint8_t *p, uint32_t n) {
    p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8u);p[2]=(uint8_t)(n>>16u);p[3]=(uint8_t)(n>>24u);
}
static uint32_t ab5_crc_update(uint32_t crc,const uint8_t *p,uint32_t n) {
    static const uint32_t t[16] = {
        0x00000000u,0x1DB71064u,0x3B6E20C8u,0x26D930ACu,
        0x76DC4190u,0x6B6B51F4u,0x4DB26158u,0x5005713Cu,
        0xEDB88320u,0xF00F9344u,0xD6D6A3E8u,0xCB61B38Cu,
        0x9B64C2B0u,0x86D3D2D4u,0xA00AE278u,0xBDBDF21Cu
    };
    while(n--) { crc ^= *p++; crc=(crc>>4u)^t[crc&15u]; crc=(crc>>4u)^t[crc&15u]; }
    return crc;
}
static uint32_t ab5_crc32(const uint8_t *p,uint32_t n) { return ~ab5_crc_update(0xFFFFFFFFu,p,n); }
static void ab5_feed(const ab_nor_io_t *io){if(io->feed_watchdog)io->feed_watchdog(io->ctx);}
static int ab5_io_ready(const ab_nor_io_t *io){return io&&io->read;}
static int ab5_is_blank(const ab_nor_io_t *io, uint32_t off, uint32_t n) {
    uint8_t buf[AB5_NOR_PAGE_BYTES];uint32_t i,k;
    while(n) {
        k=n>sizeof buf?sizeof buf:n;
        if(io->read(io->ctx,off,buf,k))return -1;
        for(i=0;i<k;i++)if(buf[i]!=0xFFu)return 0;
        off+=k;n-=k;ab5_feed(io);
    }
    return 1;
}
/* The third sample slot is reserved only while its FSMP header sector and
 * all tail sectors outside the v5 A/B partition remain BLANK. If a different
 * firmware or user populated them, never claim/erase anything in this slot. */
static int ab5_outer_blank(const ab_nor_io_t *io)
{
    int h=ab5_is_blank(io,AB5_STORE_REGION-AB5_NOR_SECTOR_BYTES,AB5_NOR_SECTOR_BYTES);
    int t=ab5_is_blank(io,AB5_STORE_END,
                      (AB_SAMPLE_START+AB_SAMPLE_COUNT*AB_SAMPLE_STRIDE)-AB5_STORE_END);
    if(h<0 || t<0)return -1;
    return h && t;
}
static int ab5_payload_crc(const ab_nor_io_t *io,unsigned copy,uint32_t *result){
    uint8_t buf[AB5_NOR_PAGE_BYTES];uint32_t off,n,crc=0xFFFFFFFFu;
    for(off=0;off<AB5_NOR_PAYLOAD_BYTES;off+=n){
        n=AB5_NOR_PAYLOAD_BYTES-off;
        if(n>sizeof buf)n=sizeof buf;
        if(io->read(io->ctx,ab5_copy_base(copy)+AB5_NOR_DATA_OFFSET+off,buf,n))return -1;
        crc=ab5_crc_update(crc,buf,n);ab5_feed(io);
    }
    *result=~crc;return 0;
}
/* Returns copy kind or -1 on IO failure. A magic match alone does NOT
 * authorize a v5 commit: CRC, format, length and final commit all matter. */
static int ab5_classify(const ab_nor_io_t *io,unsigned copy,uint32_t *gen){
    uint8_t h[AB5_NOR_HEADER_BYTES];uint32_t crc;
    int b;
    if(io->read(io->ctx,ab5_copy_base(copy),h,sizeof h))return -1;
    if(ab5_read32(h)==AB5_NOR_MAGIC && ab5_read32(h+4u)==AB5_NOR_FORMAT){
        if(ab5_read32(h+12u)==AB5_NOR_PAYLOAD_BYTES && ab5_read32(h+8u)!=0u &&
           ab5_read32(h+24u)==ab5_crc32(h,24u) && ab5_read32(h+28u)==AB5_NOR_COMMIT){
            if(ab5_payload_crc(io,copy,&crc))return -1;
            if(crc==ab5_read32(h+16u)){
                *gen=ab5_read32(h+8u);return AB5_NOR_COPY_VALID;
            }
        }
        return AB5_NOR_COPY_TORN;
    }
    /* Wholly blank copy can be safely claimed. Header-only blank is NOT enough
     * (a cut during erase could leave foreign payload sectors). */
    b=ab5_is_blank(io,ab5_copy_base(copy),AB5_COPY_BYTES);
    if(b<0)return -1;
    return b?AB5_NOR_COPY_EMPTY:AB5_NOR_COPY_FOREIGN;
}
int ab5_nor_probe(const ab_nor_io_t *io,ab5_nor_state_t *s){
    uint32_t gens[2]={0u,0u};unsigned c;
    int outer;
    if(!ab5_io_ready(io)||!s)return -1;
    outer=ab5_outer_blank(io);
    if(outer<0)return -2;
    if(!outer) {
        s->valid=0u;s->active_copy=0u;s->generation=0u;
        s->copy_kind[0]=AB5_NOR_COPY_FOREIGN;
        s->copy_kind[1]=AB5_NOR_COPY_FOREIGN;
        return 0; /* foreign sample, no writes */
    }
    s->valid=0u;s->active_copy=0u;s->generation=0u;
    for(c=0;c<2u;c++){
        int k=ab5_classify(io,c,&gens[c]);
        if(k<0)return -2;
        s->copy_kind[c]=(uint8_t)k;
    }
    if(s->copy_kind[0]==AB5_NOR_COPY_VALID||s->copy_kind[1]==AB5_NOR_COPY_VALID){
        unsigned winner=(s->copy_kind[0]!=AB5_NOR_COPY_VALID)?1u:
            (s->copy_kind[1]!=AB5_NOR_COPY_VALID)?0u:
            (int32_t)(gens[1]-gens[0])>0?1u:0u;
        s->valid=1u;s->active_copy=(uint8_t)winner;s->generation=gens[winner];
    }
    return 0;
}
int ab5_nor_load(const ab_nor_io_t *io,ab5_nor_state_t *s,
                 ab_write_snapshot_fn put,void *ctx){
    uint8_t buf[AB5_NOR_PAGE_BYTES];uint32_t off,n;
    if(!put||ab5_nor_probe(io,s))return -1;
    if(!s->valid)return 1;
    for(off=0;off<AB5_NOR_PAYLOAD_BYTES;off+=n){
        n=AB5_NOR_PAYLOAD_BYTES-off;
        if(n>sizeof buf)n=sizeof buf;
        if(io->read(io->ctx,ab5_copy_base(s->active_copy)+AB5_NOR_DATA_OFFSET+off,buf,n)||put(ctx,off,buf,n))
            return -2; /* caller must discard any partially loaded model */
        ab5_feed(io);
    }
    return 0;
}
int ab5_nor_save(const ab_nor_io_t *io,ab5_nor_state_t *s,
                 ab_read_snapshot_fn get,void *ctx){
    uint8_t h[AB5_NOR_HEADER_BYTES],buf[AB5_NOR_PAGE_BYTES];
    uint32_t sum=0xFFFFFFFFu,off,n,check,newgen;
    unsigned c;
    if(!ab5_io_ready(io)||!io->erase4k||!io->program||!get||!s)return -1;
    if(ab5_nor_probe(io,s))return -2;
    if(!s->valid){
        /* Never reclaim a legacy PDZ6 page by accident. */
        if(s->copy_kind[0]!=AB5_NOR_COPY_EMPTY||s->copy_kind[1]!=AB5_NOR_COPY_EMPTY)return -3;
        c=0u;newgen=1u;
    }else{
        c=1u-s->active_copy;
        if(s->copy_kind[c]==AB5_NOR_COPY_FOREIGN)return -4;
        newgen=s->generation+1u;if(!newgen)newgen=1u;
    }
    /* Before erasing anything, ensure source snapshot can be read fully. */
    for(off=0;off<AB5_NOR_PAYLOAD_BYTES;off+=n){
        n=AB5_NOR_PAYLOAD_BYTES-off;if(n>sizeof buf)n=sizeof buf;
        if(get(ctx,off,buf,n))return -5;
        sum=ab5_crc_update(sum,buf,n);
    }
    sum=~sum;
    for(off=0;off<AB5_COPY_BYTES;off+=AB5_NOR_SECTOR_BYTES){
        if(io->erase4k(io->ctx,ab5_copy_base(c)+off))return -6;
        ab5_feed(io);
    }
    /* Header is tagged AB5S before payload writing, with commit bit EMPTY.
     * A crash creates a recognizable TORN v5 object, never a valid snapshot. */
    for(off=0;off<sizeof h;off++)h[off]=0xFFu;
    ab5_write32(h,AB5_NOR_MAGIC);ab5_write32(h+4u,AB5_NOR_FORMAT);
    ab5_write32(h+8u,newgen);ab5_write32(h+12u,AB5_NOR_PAYLOAD_BYTES);
    ab5_write32(h+16u,sum);ab5_write32(h+20u,0xFFFFFFFFu);ab5_write32(h+24u,ab5_crc32(h,24u));
    if(io->program(io->ctx,ab5_copy_base(c),h,sizeof h))return -7;
    for(off=0;off<AB5_NOR_PAYLOAD_BYTES;off+=n){
        n=AB5_NOR_PAYLOAD_BYTES-off;if(n>sizeof buf)n=sizeof buf;
        if(get(ctx,off,buf,n))return -8;
        if(io->program(io->ctx,ab5_copy_base(c)+AB5_NOR_DATA_OFFSET+off,buf,n))return -9;
        ab5_feed(io);
    }
    if(ab5_payload_crc(io,c,&check)||check!=sum)return -10;
    /* Final irreversible 1 -> 0 programming of the four-byte commit word. */
    ab5_write32(buf,AB5_NOR_COMMIT);
    if(io->program(io->ctx,ab5_copy_base(c)+28u,buf,4u))return -11;
    /* read-back includes committed flag AND entire payload CRC */
    if(ab5_nor_probe(io,s)||!s->valid||s->active_copy!=c||s->generation!=newgen)return -12;
    return 0;
}
