/* analogBEAST v1.2 Stage 4: strictly read-only MD12 verification.
 * No dynamic allocation and no external dependencies; 256B streaming buffer.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "ab12_probe_ro.h"
#include <stddef.h>

#define MD12_MAGIC 0x3231444Du  /* MD12 */
#define D12R_MAGIC 0x52323144u  /* D12R */
#define MD12_COMMIT 0x1AB2C3D4u

static uint32_t ab12_get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint16_t ab12_get16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8)); }
static uint32_t ab12_crc_feed(uint32_t c,const uint8_t *p,uint32_t n) {
    uint32_t i;
    for(i=0;i<n;i++) { unsigned b; c^=p[i]; for(b=0;b<8;b++) c=(c>>1)^(0xEDB88320u & (0u-(c&1u))); }
    return c;
}
static uint32_t ab12_crc32(const uint8_t *p,uint32_t n) { return ~ab12_crc_feed(0xFFFFFFFFu,p,n); }
static int ab12_read_checked(ab12_ro_read f,void *ctx,uint32_t pos,uint8_t *b,uint32_t n) {
    if(pos>AB12_RO_SIZE||n>AB12_RO_SIZE-pos||!f)return -1;
    return f(ctx,pos,b,n)?-1:0;
}
static int ab12_ismagic(const uint8_t *p,const char *magic) {
    return p[0]==(uint8_t)magic[0]&&p[1]==(uint8_t)magic[1]&&p[2]==(uint8_t)magic[2]&&p[3]==(uint8_t)magic[3];
}
static int ab12_verify_bank(ab12_ro_read f,void *ctx,uint32_t bank,ab12_ro_report *report) {
    uint8_t h[32],ent[12],rec[16],buffer[256];
    uint32_t gen,number,table=0xFFFFFFFFu,count,j,bytes,physical=0;
    uint32_t base=bank*AB12_RO_MANIFEST_SIZE;
    if(ab12_read_checked(f,ctx,base,h,32))return -1;
    if(ab12_get32(h)!=MD12_MAGIC||ab12_get32(h+4)!=1u||!(gen=ab12_get32(h+8))||
       ab12_get32(h+28)!=MD12_COMMIT||ab12_crc32(h,24)!=ab12_get32(h+24))return 0;
    count=ab12_get32(h+12);
    if(count>AB12_RO_MAX_ENTRIES||ab12_get32(h+16)!=count*12u)return 0;
    bytes=0;
    for(j=0;j<count;j++) {
        uint32_t at,len,rc=0xFFFFFFFFu,k;
        if(ab12_read_checked(f,ctx,base+256u+j*12u,ent,12))return -1;
        table=ab12_crc_feed(table,ent,12);
        at=ab12_get32(ent+4);len=ab12_get32(ent+8);number=ab12_get16(ent+2);
        if(ent[0]<1u||ent[0]>4u||!number||at<AB12_RO_DATA_BASE||(at&15u)||
           at>AB12_RO_SIZE||len>AB12_RO_SIZE-at-((AB12_RO_SIZE-at>=16u)?16u:0u)||
           AB12_RO_SIZE-at<16u||((16u+len+15u)&~15u)>AB12_RO_SIZE-at)return 0;
        if(ab12_read_checked(f,ctx,at,rec,16))return -1;
        if(ab12_get32(rec)!=D12R_MAGIC||rec[4]!=ent[0]||rec[5]!=ent[1]||
           ab12_get16(rec+6)!=number||ab12_get32(rec+8)!=len)return 0;
        for(k=0;k<len;) {
            uint32_t n=len-k;if(n>sizeof buffer)n=sizeof buffer;
            if(ab12_read_checked(f,ctx,at+16u+k,buffer,n))return -1;
            rc=ab12_crc_feed(rc,buffer,n);k+=n;
        }
        if((~rc)!=ab12_get32(rec+12))return 0;
        if(bytes>0xFFFFFFFFu-len)return 0;
        bytes+=len;
        physical+=(16u+len+15u)&~15u;
    }
    if((~table)!=ab12_get32(h+20))return 0;
    report->status=AB12_RO_VALID;report->active_bank=(uint8_t)bank;
    report->files=(uint16_t)count;report->generation=gen;
    report->user_bytes=bytes;report->physical_bytes=physical;
    return 1;
}
static int ab12_newer(uint32_t a,uint32_t b) { uint32_t diff=a-b;return diff!=0u&&diff<0x80000000u; }

void ab12_ro_probe(ab12_ro_read f,void *ctx,ab12_ro_report*out) {
    ab12_ro_report first={0},second={0};
    uint8_t tmp[256],marker[4];
    uint32_t p,k;
    int a,b,blank=1,legacy=0,md12_seen=0;
    if(!out)return;
    out->status=AB12_RO_IO_ERROR;out->active_bank=0;
    out->files=0;out->generation=0;out->user_bytes=0;out->physical_bytes=0;
    if(!f)return;
    /* Avoid any destructive action on pre-existing ABST or AB5S areas. */
    {
        static const uint32_t legacy_at[]={0x1000u,0x15000u,0x29000u,0x2D000u};
        for(k=0;k<4;k++) {
            if(ab12_read_checked(f,ctx,legacy_at[k],marker,4))return;
            if(ab12_ismagic(marker,"ABST")||ab12_ismagic(marker,"AB5S"))legacy=1;
        }
    }
    a=ab12_verify_bank(f,ctx,0u,&first);
    b=ab12_verify_bank(f,ctx,1u,&second);
    if(a<0||b<0)return;
    if(a>0||b>0) {
        *out=(a>0&&(b<=0||!ab12_newer(second.generation,first.generation)))?first:second;
        return;
    }
    if(ab12_read_checked(f,ctx,0,marker,4))return;
    md12_seen=ab12_ismagic(marker,"MD12");
    if(ab12_read_checked(f,ctx,AB12_RO_MANIFEST_SIZE,marker,4))return;
    md12_seen|=ab12_ismagic(marker,"MD12");
    if(md12_seen){out->status=AB12_RO_DAMAGED;return;}
    if(legacy){out->status=AB12_RO_LEGACY;return;}
    for(p=0;p<AB12_RO_SIZE;p+=sizeof tmp) {
        uint32_t n=AB12_RO_SIZE-p;if(n>sizeof tmp)n=sizeof tmp;
        if(ab12_read_checked(f,ctx,p,tmp,n))return;
        for(k=0;k<n;k++)if(tmp[k]!=255u){blank=0;break;}
        if(!blank)break;
    }
    out->status=blank?AB12_RO_EMPTY:AB12_RO_FOREIGN;
}
const char *ab12_ro_status_name(uint8_t s) {
    switch(s) {
    case AB12_RO_EMPTY:return "EMPTY";
    case AB12_RO_LEGACY:return "LEGACY ABST/AB5S";
    case AB12_RO_VALID:return "MD12 VALID";
    case AB12_RO_FOREIGN:return "FOREIGN DATA";
    case AB12_RO_DAMAGED:return "MD12 DAMAGED";
    default:return "READ ERROR";
    }
}
