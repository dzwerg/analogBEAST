/* analogBEAST 1.2 / Stage10: one-time shadow migration into *only* 35
 * previously-empty, explicitly enumerated 4KiB sectors. Source data is
 * locked to a SHA256-confirmed original 1MiB backup; the old ABST/AB5S
 * regions are never programmed or erased by these routines.
 * SPDX-License-Identifier: GPL-3.0-only
 * C99 freestanding; this is included AFTER storage.c and ab12_map_fm1.c.
 */
#include "ab12_s10_payload.h"
#define AB12_S10_BAD_FLASH (-1)
#define AB12_S10_FOREIGN (-2)
#define AB12_S10_IO (-3)
#define AB12_S10_PARTIAL (-4)
#define AB12_S10_NOTFOUND (-5)
#define AB12_S10_OK 0

static int ab12_s10_page_phys(uint32_t logical_page, uint32_t *p) {
    uint32_t base;
    if (logical_page >= AB12_S6_SAFE_SECTORS*16u ||
        ab12_s6_sector(logical_page/16u,&base)) return AB12_S10_BAD_FLASH;
    *p=base+(logical_page%16u)*256u;
    return 0;
}
static const uint8_t *ab12_s10_wanted(uint32_t page) {
    uint32_t j;
    for(j=0;j<AB12_S10_PAGE_COUNT;j++)
        if(ab12_s10_pages[j].page==page)return ab12_s10_pages[j].bytes;
    return 0;
}
static uint8_t ab12_s10_equal(const uint8_t *a,const uint8_t *b,uint32_t n) {
    uint32_t i; for(i=0;i<n;i++) if(a[i]!=b[i])return 0; return 1;
}
/* CRC of all storage-sector bytes that belong to the original layout,
 * excluding every one of the 35 empty sectors reserved for shadow MD12. */
static int ab12_s10_legacy_crc(uint32_t *out) {
    uint8_t buf[256]; uint32_t p,j,off,c=0xFFFFFFFFu;
    for(p=0x97000u;p<0xE0000u;p+=4096u){
        uint32_t ignored, skip=0;
        for(j=0;j<AB12_S6_SAFE_SECTORS;j++)
            if(!ab12_s6_sector(j,&ignored) && ignored==p){skip=1;break;}
        if(skip)continue;
        for(off=0;off<4096u;off+=256u){
            if(st_read(p+off,buf,256u))return AB12_S10_IO;
            c=st_crc32_add(c,buf,256u);
        }
    }
    *out=~c;return 0;
}
/* Reads 140KiB: an unknown/occupied sector blocks ALL program operations.
 * For target pages, accept FF->data or an interrupted earlier programming
 * operation whose NOR bits can still be completed with the desired image. */
static int ab12_s10_preflight(uint32_t *already, uint32_t *missing) {
    uint8_t have[256];uint32_t page,phys,i,done=0,todo=0,c;
    if(!already||!missing)return AB12_S10_BAD_FLASH;
    if(ab12_s10_legacy_crc(&c)||c!=AB12_S10_EXPECT_PROTECTED_CRC)
        return AB12_S10_FOREIGN;
    for(page=0;page<AB12_S6_SAFE_SECTORS*16u;page++) {
        const uint8_t *want=ab12_s10_wanted(page);
        if(ab12_s10_page_phys(page,&phys)||st_read(phys,have,256u))return AB12_S10_IO;
        if(!want){for(i=0;i<256u;i++)if(have[i]!=255u)return AB12_S10_FOREIGN;}
        else {
            if(ab12_s10_equal(have,want,256u))done++;
            else {
                /* Safety: a physical 0 bit not present in the desired image
                 * means this is NOT one of our interrupted staged pages. */
                for(i=0;i<256u;i++)if((have[i]&want[i])!=want[i])return AB12_S10_FOREIGN;
                todo++;
            }
        }
    }
    *already=done;*missing=todo;
    return 0;
}
static int ab12_s10_verify(uint32_t *complete) {
    uint8_t have[256];uint32_t page,phys,i,matched=0;
    if(!complete)return AB12_S10_BAD_FLASH;
    *complete=0;
    for(page=0;page<AB12_S6_SAFE_SECTORS*16u;page++){
        const uint8_t *want=ab12_s10_wanted(page);
        if(ab12_s10_page_phys(page,&phys)||st_read(phys,have,256u))return AB12_S10_IO;
        if(want){if(!ab12_s10_equal(have,want,256u))return AB12_S10_PARTIAL;matched++;}
        else for(i=0;i<256u;i++)if(have[i]!=255u)return AB12_S10_FOREIGN;
    }
    if(matched!=AB12_S10_PAGE_COUNT)return AB12_S10_BAD_FLASH;
    *complete=1u;
    return 0;
}
/* The payload table is ordered so the MD12 bank-0 commit page (logical 0)
 * is literally the final write. No erase is ever necessary/allowed here. */
static int ab12_s10_migrate(void) {
    uint8_t have[256],ram_page[256];
    uint32_t ready,todo,j,k,phys,complete;
    int rc=ab12_s10_preflight(&ready,&todo);
    if(rc)return rc;
    if(!todo)return ab12_s10_verify(&complete);
    for(j=0;j<AB12_S10_PAGE_COUNT;j++){
        const uint8_t *want=ab12_s10_pages[j].bytes;
        if(ab12_s10_page_phys(ab12_s10_pages[j].page,&phys) || st_read(phys,have,256u))return AB12_S10_IO;
        if(ab12_s10_equal(have,want,256u))continue;
        for(k=0;k<256u;k++){
            if((have[k]&want[k])!=want[k])return AB12_S10_FOREIGN;
            ram_page[k]=want[k];
        }
        if(st_prog(phys,ram_page,256u))return AB12_S10_IO;
        if(st_read(phys,have,256u)||!ab12_s10_equal(have,want,256u))return AB12_S10_IO;
        fm1_wdt_feed();
    }
    rc=ab12_s10_verify(&complete);
    return rc ? rc : (complete?0:AB12_S10_PARTIAL);
}
/* Read *committed* real MD12 file by type (1 pattern, 2 song, 3 album,
 * 4 preset) and id. Never reads the source image from firmware ROM.
 * Caller sees nothing until the entire shadow disk matches its final
 * committed image (we validate manifest + contents before file lookup). */
static uint32_t ab12_s10_le32(const uint8_t *b) {
    return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
}
static uint16_t ab12_s10_le16(const uint8_t *b) {
    return (uint16_t)((uint16_t)b[0]|((uint16_t)b[1]<<8));
}
static int ab12_s10_logical_read(uint32_t off, void *out, uint32_t n) {
    uint8_t *p=(uint8_t*)out;
    if(off>AB12_S6_TOTAL_BYTES||n>AB12_S6_TOTAL_BYTES-off)return AB12_S10_BAD_FLASH;
    while(n){uint32_t phys,skip=off%4096u,k=4096u-skip;
        if(k>n)k=n;
        if(ab12_s6_sector(off/4096u,&phys)||st_read(phys+skip,p,k))return AB12_S10_IO;
        off+=k;p+=k;n-=k;
    }
    return 0;
}
static int ab12_s10_lookup(uint8_t typ,uint16_t id,uint32_t *file,uint32_t *len) {
    uint8_t h[32],e[12],rh[16];uint32_t i,cnt,at,n;
    int rc;
    if(!file||!len||typ<1u||typ>4u||!id)return AB12_S10_BAD_FLASH;
    rc=ab12_s10_logical_read(0u,h,32u);if(rc)return rc;
    if(ab12_s10_le32(h)!=0x3231444Du||ab12_s10_le32(h+28)!=0x1AB2C3D4u||
       ab12_s10_le32(h+4)!=1u||ab12_s10_le32(h+12)!=AB12_S10_EXPECT_FILES)
        return AB12_S10_PARTIAL;
    cnt=ab12_s10_le32(h+12);
    for(i=0;i<cnt;i++){
        rc=ab12_s10_logical_read(256u+i*12u,e,12u);if(rc)return rc;
        if(e[0]!=typ||ab12_s10_le16(e+2)!=id)continue;
        at=ab12_s10_le32(e+4);n=ab12_s10_le32(e+8);
        if((at&15u)||at<16384u||at>AB12_S6_TOTAL_BYTES-16u||n>AB12_S6_TOTAL_BYTES-at-16u)
            return AB12_S10_BAD_FLASH;
        rc=ab12_s10_logical_read(at,rh,16u);if(rc)return rc;
        if(ab12_s10_le32(rh)!=0x52323144u||rh[4]!=typ||
            ab12_s10_le16(rh+6)!=id||ab12_s10_le32(rh+8)!=n)return AB12_S10_FOREIGN;
        *file=at+16u;*len=n;
        return 0;
    }
    return AB12_S10_NOTFOUND;
}
static int ab12_s10_file_read(uint8_t typ,uint16_t id,uint32_t off,void *out,uint32_t n){
    uint32_t at,len;
    int rc=ab12_s10_lookup(typ,id,&at,&len);
    if(rc)return rc;
    if(off>len||n>len-off)return AB12_S10_BAD_FLASH;
    return ab12_s10_logical_read(at+off,out,n);
}
