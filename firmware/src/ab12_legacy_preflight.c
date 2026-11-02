/* analogBEAST v1.2 Stage 5 - validate legacy snapshots before migration.
 * SPDX-License-Identifier: GPL-3.0-only
 * Standalone C11; no dynamic allocation, no flash writes or firmware dependencies.
 */
#include "ab12_legacy_preflight.h"

#define ABST_MAGIC 0x54534241u
#define AB5S_MAGIC 0x53354241u
#define AB5S_COMMIT 0x5AA55AA5u
#define LG_CHUNK 256u
static uint32_t ab12_lg_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
/* Historical analogBEAST-v4 nonstandard nibble CRC table. NEVER replace by zlib. */
static const uint32_t abst_t[16] = {
    0x00000000u,0x1DB71064u,0x3B6E20C8u,0x26D930ACu,
    0x76DC4190u,0x4DB26158u,0x5005713Cu,0xEDB88320u,
    0xF00F9344u,0xD6D6A3E8u,0xCB61B38Cu,0xA00AE278u,
    0xBDBDF21Cu,0x6B6B51F4u,0xB1D29AFAu,0x2C6B0A9Eu
};
static uint32_t ab12_lg_abst_crc_feed(uint32_t c, const uint8_t *p, uint32_t n) {
    uint32_t i;
    for(i=0;i<n;i++) { c^=p[i]; c=(c>>4)^abst_t[c&15u];c=(c>>4)^abst_t[c&15u]; }
    return c;
}
static uint32_t ab12_lg_std_crc_feed(uint32_t c, const uint8_t *p, uint32_t n) {
    uint32_t i;
    for(i=0;i<n;i++) { unsigned b;c^=p[i];for(b=0;b<8;b++) c=(c>>1)^(0xEDB88320u&(0u-(c&1u))); }
    return c;
}
static int ab12_lg_safe_read(ab12_lg_reader rd,void *ctx,uint32_t off,void *dst,uint32_t n) {
    if(!rd||off>AB12_LG_AREA_SIZE||n>AB12_LG_AREA_SIZE-off) return -1;
    return rd(ctx,off,dst,n)?-1:0;
}
static int ab12_lg_is_newer(uint32_t a,uint32_t b){ uint32_t d=a-b;return d&&d<0x80000000u; }
static int8_t ab12_lg_winner(const ab12_lg_copy c[2]) {
    if(c[0].valid && c[1].valid) return (int8_t)(ab12_lg_is_newer(c[1].generation,c[0].generation)?1:0);
    return c[0].valid?0:c[1].valid?1:-1;
}
static int ab12_lg_verify(ab12_lg_reader rd,void *ctx,uint32_t at,uint32_t body,
                  uint32_t magic,uint32_t required_size,int old_crc,
                  ab12_lg_copy *out) {
    uint8_t h[32],b[LG_CHUNK];
    uint32_t i,crc=0xFFFFFFFFu,length;
    if(ab12_lg_safe_read(rd,ctx,at,h,sizeof h))return -1;
    out->present=(uint8_t)(ab12_lg_u32(h)==magic);
    out->valid=0;
    out->generation=out->present?ab12_lg_u32(h+8):0u;
    if(!out->present)return 0;
    length=ab12_lg_u32(h+12);
    if(ab12_lg_u32(h+4)!=1u || out->generation==0u || length!=required_size ||
       body>AB12_LG_AREA_SIZE || length>AB12_LG_AREA_SIZE-body)return 0;
    if(old_crc) {
        if((~ab12_lg_abst_crc_feed(0xFFFFFFFFu,h,28))!=ab12_lg_u32(h+28)) return 0;
    } else {
        if(ab12_lg_u32(h+28)!=AB5S_COMMIT || (~ab12_lg_std_crc_feed(0xFFFFFFFFu,h,24))!=ab12_lg_u32(h+24))return 0;
    }
    for(i=0;i<length;) {
        uint32_t k=length-i;
        if(k>sizeof b)k=sizeof b;
        if(ab12_lg_safe_read(rd,ctx,body+i,b,k))return -1;
        crc=old_crc?ab12_lg_abst_crc_feed(crc,b,k):ab12_lg_std_crc_feed(crc,b,k);
        i+=k;
    }
    out->valid=(uint8_t)((~crc)==ab12_lg_u32(h+16));
    return 0;
}
void ab12_lg_preflight(ab12_lg_reader rd,void *ctx,ab12_lg_report *out) {
    uint32_t i,j;
    uint8_t b[LG_CHUNK];
    if(!out)return;
    { uint32_t j; uint8_t *z=(uint8_t *)out; for(j=0;j<sizeof *out;j++) z[j]=0; }
    out->selected_abst=out->selected_ab5s=-1;
    out->status=AB12_LG_READ_ERROR;
    if(ab12_lg_verify(rd,ctx,0x01000u,0x02000u,ABST_MAGIC,21648u,1,&out->abst[0]) ||
       ab12_lg_verify(rd,ctx,0x15000u,0x16000u,ABST_MAGIC,21648u,1,&out->abst[1]) ||
       ab12_lg_verify(rd,ctx,0x29000u,0x29100u,AB5S_MAGIC,13648u,0,&out->ab5s[0]) ||
       ab12_lg_verify(rd,ctx,0x2D000u,0x2D100u,AB5S_MAGIC,13648u,0,&out->ab5s[1]))return;
    out->selected_abst=ab12_lg_winner(out->abst);
    out->selected_ab5s=ab12_lg_winner(out->ab5s);
    for(i=0;i<AB12_LG_SECTORS;i++) {
        int blank=1;
        for(j=0;j<4096u;j+=sizeof b) {
            uint32_t k;
            if(ab12_lg_safe_read(rd,ctx,i*4096u+j,b,sizeof b))return;
            for(k=0;k<sizeof b;k++) if(b[k]!=0xFFu) {blank=0;break;}
            if(!blank)break;
        }
        if(blank){
            out->blank_sectors++;
            if((i>=8u&&i<20u)||(i>=28u&&i<40u))out->erased_96k++;
            if(i>=49u&&i<60u)out->erased_tail44k++;
        }
    }
    out->status=(out->selected_abst>=0 && out->selected_ab5s>=0)?AB12_LG_GOOD:AB12_LG_BAD;
}
