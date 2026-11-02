#include "ab13_disk.h"
#ifndef AB13_UNITY
#include <string.h>
#endif
/* String functions provided by containing unity build or host. */
/* V3: full bank-free 0x97000..0xDFFFF (292 KiB) MUSIC DISK. */

#define M_COMMIT 0x1AB2C3D4u
#define M_HDR 32u
#define M_REC 16u
#define M_ENT 12u
#define M_MAGIC 0x3331444Du  /* 'MD13' LE */
#define R_MAGIC 0x52333144u  /* 'D12R' LE */
static uint32_t s13_rd32(const uint8_t *b){return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);}
static uint16_t s13_rd16(const uint8_t *b){return (uint16_t)(b[0] | ((uint16_t)b[1]<<8));}
static void s13_wr32(uint8_t *b,uint32_t n){b[0]=(uint8_t)n;b[1]=(uint8_t)(n>>8);b[2]=(uint8_t)(n>>16);b[3]=(uint8_t)(n>>24);}
static void s13_wr16(uint8_t *b,uint16_t n){b[0]=(uint8_t)n;b[1]=(uint8_t)(n>>8);}
static uint32_t s13_align16(uint32_t v){return (v+15u)&~15u;}
static uint32_t s13_crc_step(uint32_t c, const uint8_t *p,uint32_t n){while(n--){int j;c^=*p++;for(j=0;j<8;j++)c=(c>>1)^(0xEDB88320u & (0u-(c&1u)));}return c;}
static uint32_t s13_crc(const uint8_t *p,uint32_t n){return ~s13_crc_step(0xFFFFFFFFu,p,n);}
static int s13_io_read(const md13_disk*d,uint32_t p,void *dst,uint32_t n){if(p>MD13_SIZE || n>MD13_SIZE-p || !d->io.read)return MD13_EINVAL;return d->io.read(d->io.ctx,p,dst,n)?MD13_EIO:0;}
static int s13_io_erase(md13_disk*d,uint32_t p){if(p%MD13_SECTOR || p>MD13_SIZE-MD13_SECTOR || !d->io.erase4k)return MD13_EINVAL;return d->io.erase4k(d->io.ctx,p)?MD13_EIO:0;}
static int s13_io_program(md13_disk*d,uint32_t p,const void*src,uint32_t n){if(!n || n>MD13_PAGE || p>MD13_SIZE-n || p/MD13_PAGE!=(p+n-1)/MD13_PAGE || !d->io.program)return MD13_EINVAL;return d->io.program(d->io.ctx,p,src,n)?MD13_EIO:0;}
static int s13_write_bytes(md13_disk*d,uint32_t p,const uint8_t*src,uint32_t n){while(n){uint32_t k=MD13_PAGE-p%MD13_PAGE;if(k>n)k=n;if(s13_io_program(d,p,src,k))return MD13_EIO;p+=k;src+=k;n-=k;}return 0;}
static int s13_valid_type(uint8_t type){return type>=MD13_PAT1 && type<=MD13_PRE1;}
static int s13_key_eq(const md13_entry*e,uint8_t t,uint16_t n){return e->type==t && e->number==n;}
static int s13_active_index(const md13_disk*d,uint8_t t,uint16_t n){uint32_t i;for(i=0;i<d->count;i++)if(s13_key_eq(&d->active[i],t,n))return (int)i;return -1;}
static int s13_shadow_index(const md13_disk*d,uint32_t n,uint8_t t,uint16_t id){uint32_t i;for(i=0;i<n;i++)if(s13_key_eq(&d->candidate[i],t,id))return (int)i;return -1;}
static int s13_descriptor_ok(const md13_entry*e){uint32_t required;
    if(!s13_valid_type(e->type)||!e->number||e->offset<MD13_DATA_BASE||e->offset%16||e->length>MD13_SIZE-MD13_DATA_BASE-M_REC)return 0;
    required=s13_align16(M_REC+e->length);
    return e->offset<=MD13_SIZE && required<=MD13_SIZE-e->offset;
}
static uint32_t s13_ent_end(const md13_entry*e){return e->offset+s13_align16(M_REC+e->length);}
static int s13_verify_record(md13_disk*d,const md13_entry*e){uint8_t hdr[16],buf[256];uint32_t c=0xFFFFFFFFu,pos=0,n;
    if(!s13_descriptor_ok(e)||s13_io_read(d,e->offset,hdr,16))return MD13_ECORRUPT;
    if(s13_rd32(hdr)!=R_MAGIC||hdr[4]!=e->type||hdr[5]!=e->flags||s13_rd16(hdr+6)!=e->number||s13_rd32(hdr+8)!=e->length)return MD13_ECORRUPT;
    while(pos<e->length){n=e->length-pos;if(n>sizeof buf)n=sizeof buf;if(s13_io_read(d,e->offset+16+pos,buf,n))return MD13_EIO;c=s13_crc_step(c,buf,n);pos+=n;}
    return (~c)==s13_rd32(hdr+12) ? MD13_OK : MD13_ECORRUPT;
}
static int s13_bank_read(md13_disk*d,uint32_t bank,md13_entry*arr,uint16_t*nout,uint32_t*gen){
    uint32_t i,j,tcrc=0xFFFFFFFFu,cnt,bytes;uint8_t h[32],b[12];
    if(s13_io_read(d,bank*MD13_BANK_SIZE,h,32))return MD13_EIO;
    if(s13_rd32(h)!=M_MAGIC||s13_rd32(h+4)!=1u||s13_rd32(h+8)==0||s13_rd32(h+28)!=M_COMMIT)return MD13_ECORRUPT;
    cnt=s13_rd32(h+12);bytes=s13_rd32(h+16);
    if(cnt>MD13_MAX_ENTRIES||bytes!=cnt*M_ENT||s13_crc(h,24)!=s13_rd32(h+24))return MD13_ECORRUPT;
    for(i=0;i<cnt;i++){
        if(s13_io_read(d,bank*MD13_BANK_SIZE+MD13_PAGE+i*M_ENT,b,M_ENT))return MD13_EIO;
        tcrc=s13_crc_step(tcrc,b,M_ENT);
        arr[i].type=b[0];arr[i].flags=b[1];arr[i].number=s13_rd16(b+2);
        arr[i].offset=s13_rd32(b+4);arr[i].length=s13_rd32(b+8);
        if(!s13_descriptor_ok(&arr[i])||s13_verify_record(d,&arr[i]))return MD13_ECORRUPT;
        for(j=0;j<i;j++){
            const md13_entry*a=&arr[j],*z=&arr[i];
            if(s13_key_eq(a,z->type,z->number))return MD13_ECORRUPT;
            if(a->offset<s13_ent_end(z)&&z->offset<s13_ent_end(a))return MD13_ECORRUPT;
        }
    }
    if((~tcrc)!=s13_rd32(h+20))return MD13_ECORRUPT;
    *nout=(uint16_t)cnt;*gen=s13_rd32(h+8);return MD13_OK;
}
static int s13_newer(uint32_t a,uint32_t b){uint32_t diff=a-b;return diff && diff<0x80000000u;}
static int s13_blank_region(md13_disk*d,uint32_t start,uint32_t n){uint8_t b[256];uint32_t i;
    while(n){uint32_t k=n>256?256:n;if(s13_io_read(d,start,b,k))return MD13_EIO;
        for(i=0;i<k;i++)if(b[i]!=0xFF)return 0;
        start+=k;n-=k;
    }return 1;
}
int md13_mount(md13_disk*d,md13_io io){uint16_t n0=0,n1=0;uint32_t g0=0,g1=0,i;int a,b,blank;
    if(!d||!io.read||!io.erase4k||!io.program)return MD13_EINVAL;
    /* IMPORTANT: this is a development prototype; caller must provide scratch memory. */
    memset(d,0,sizeof *d);d->io=io;d->active_bank=-1;d->cursor=MD13_DATA_BASE;
    a=s13_bank_read(d,0,d->active,&n0,&g0);
    b=s13_bank_read(d,1,d->candidate,&n1,&g1);
    if(a==0 && (b!=0||!s13_newer(g1,g0))){d->count=n0;d->generation=g0;d->active_bank=0;}
    else if(b==0){memcpy(d->active,d->candidate,n1*sizeof(md13_entry));d->count=n1;d->generation=g1;d->active_bank=1;}
    else {
        /* Refuse legacy ABST, torn/foreign layouts, corrupt manifests. An empty
         * disk MUST be physically all 0xFF before first use. */
        blank=s13_blank_region(d,0,MD13_SIZE);
        if(blank<0)return MD13_EIO;
        return blank?MD13_OK:MD13_EFOREIGN;
    }
    for(i=0;i<d->count;i++)if(s13_ent_end(&d->active[i])>d->cursor)d->cursor=s13_ent_end(&d->active[i]);
    if(d->cursor>=MD13_SIZE)d->cursor=MD13_DATA_BASE;
    return MD13_OK;
}
int md13_lookup(const md13_disk*d,uint8_t t,uint16_t num,md13_entry*out){int i;if(!d||!out)return MD13_EINVAL;i=s13_active_index(d,t,num);if(i<0)return MD13_ENOTFOUND;*out=d->active[i];return 0;}
int md13_read(const md13_disk*d,uint8_t t,uint16_t num,uint32_t offset,void*out,uint32_t len){md13_entry e;int rc;
    if(!d||(!out&&len))return MD13_EINVAL;
    rc=md13_lookup(d,t,num,&e);if(rc)return rc;
    if(offset>e.length||len>e.length-offset)return MD13_EINVAL;
    return s13_io_read(d,e.offset+M_REC+offset,out,len);
}
static int s13_overlaps(uint32_t a,uint32_t ae,uint32_t b,uint32_t be){return a<be&&b<ae;}
static int s13_owned(const md13_disk*d,uint32_t p,uint32_t end,uint32_t n){uint32_t j;
    for(j=0;j<d->count;j++)if(s13_overlaps(p,end,d->active[j].offset,s13_ent_end(&d->active[j])))return 1;
    for(j=0;j<n;j++)if(s13_overlaps(p,end,d->candidate[j].offset,s13_ent_end(&d->candidate[j])))return 1;
    return 0;
}
static int s13_allocate(md13_disk*d,uint32_t size,uint32_t candidate_count,uint32_t*out){uint32_t passes,p,end,start,sizea;int is_blank;
    if(size>MD13_SIZE-MD13_DATA_BASE-15)return MD13_ENOSPACE;
    sizea=s13_align16(size);start=d->cursor;
    if(start<MD13_DATA_BASE||start>=MD13_SIZE)start=MD13_DATA_BASE;
    for(passes=0;passes<2;passes++){
        p=passes?MD13_DATA_BASE:start;end=passes?start:MD13_SIZE;
        for(;p+sizea<=end;p+=16){
            if(s13_owned(d,p,p+sizea,candidate_count))continue;
            is_blank=s13_blank_region(d,p,sizea);
            if(is_blank<0)return MD13_EIO;
            if(is_blank){*out=p;d->cursor=p+sizea;if(d->cursor>=MD13_SIZE)d->cursor=MD13_DATA_BASE;return 0;}
        }
    }
    return MD13_ENOSPACE;
}
static int s13_make_record(md13_disk*d,const md13_change*c,uint32_t n,md13_entry*out){uint8_t chunk[256],hdr[16];uint32_t offset,at,crc_run=0xFFFFFFFFu,len;int rc;
    rc=s13_allocate(d,M_REC+c->length,n,&offset);if(rc)return rc;
    for(at=0;at<c->length;at+=len){
        len=c->length-at;if(len>MD13_PAGE)len=MD13_PAGE;
        /* Offset+16 may straddle a program page: s13_write_bytes chunks correctly. */
        if(c->fill(c->ctx,at,chunk,len))return MD13_EIO;
        crc_run=s13_crc_step(crc_run,chunk,len);
        if(s13_write_bytes(d,offset+M_REC+at,chunk,len))return MD13_EIO;
    }
    s13_wr32(hdr,R_MAGIC);hdr[4]=c->type;hdr[5]=c->flags;s13_wr16(hdr+6,c->number);s13_wr32(hdr+8,c->length);s13_wr32(hdr+12,~crc_run);
    if(s13_write_bytes(d,offset,hdr,16))return MD13_EIO;
    out->offset=offset;out->length=c->length;out->number=c->number;out->type=c->type;out->flags=c->flags;
    return s13_verify_record(d,out);
}
int md13_transaction(md13_disk*d,const md13_change*c,uint32_t changes){uint32_t n,i,j,target,base,tablecrc=0xFFFFFFFFu,gen;int pos,rc;uint8_t b[32];
    if(!d||(!c&&changes)||changes>MD13_MAX_ENTRIES)return MD13_EINVAL;
    n=d->count;memcpy(d->candidate,d->active,n*sizeof(md13_entry));
    /* Validate FIRST, before any programming or erasure. */
    for(i=0;i<changes;i++){
        if(!s13_valid_type(c[i].type)||!c[i].number||(!c[i].deleted&&!c[i].fill)||c[i].length>MD13_SIZE-MD13_DATA_BASE-M_REC)return MD13_EINVAL;
        for(j=0;j<i;j++)if(c[i].type==c[j].type&&c[i].number==c[j].number)return MD13_EINVAL;
        
    }
    for(i=0;i<changes;i++){
        pos=s13_shadow_index(d,n,c[i].type,c[i].number);
        if(c[i].deleted){if(pos>=0){d->candidate[pos]=d->candidate[--n];}continue;}
        int fresh=0;
        if(pos<0){if(n>=MD13_MAX_ENTRIES)return MD13_ENOSPACE;pos=(int)n++;fresh=1;}
        /* Keep old record s13_owned until manifest commits. On replacing one key,
         * the active[] array still owns the original. The fresh record only
         * becomes candidate[] after fully verified. */
        rc=s13_make_record(d,&c[i],fresh?n-1u:n,&d->candidate[pos]);
        if(rc)return rc;
    }
    target=d->active_bank==0?1u:0u;base=target*MD13_BANK_SIZE;
    if(s13_io_erase(d,base)||s13_io_erase(d,base+MD13_SECTOR))return MD13_EIO;
    for(i=0;i<n;i++){
        const md13_entry*e=&d->candidate[i];
        b[0]=e->type;b[1]=e->flags;s13_wr16(b+2,e->number);s13_wr32(b+4,e->offset);s13_wr32(b+8,e->length);
        tablecrc=s13_crc_step(tablecrc,b,12);
        if(s13_write_bytes(d,base+MD13_PAGE+i*M_ENT,b,M_ENT))return MD13_EIO;
    }
    gen=d->generation+1u;if(!gen)gen=1u;
    s13_wr32(b,M_MAGIC);s13_wr32(b+4,1);s13_wr32(b+8,gen);s13_wr32(b+12,n);s13_wr32(b+16,n*M_ENT);s13_wr32(b+20,~tablecrc);
    s13_wr32(b+24,s13_crc(b,24));s13_wr32(b+28,M_COMMIT);
    if(s13_write_bytes(d,base,b,32))return MD13_EIO;
    /* Read/CRC all newly committed records before changing active state. */
    {uint16_t read_count=0;uint32_t read_gen=0;
     if(s13_bank_read(d,target,d->candidate,&read_count,&read_gen)||read_count!=n||read_gen!=gen)return MD13_EVERIFY;}
    memcpy(d->active,d->candidate,n*sizeof(md13_entry));d->count=(uint16_t)n;d->generation=gen;d->active_bank=(int8_t)target;
    return MD13_OK;
}
int md13_sweep_dead(md13_disk*d,uint32_t*sectors_erased){
    uint32_t sec,i,erased=0,other_gen=0;
    uint16_t other_count=0;
    int referenced,blank,keep_other=0;
    if(!d||!sectors_erased)return MD13_EINVAL;
    /* The OTHER (older) committed manifest is still a valid recovery root.
     * Protect its records too. Never erase data needed for CRC fallback. */
    if(d->active_bank>=0 && s13_bank_read(d,1u-(uint32_t)d->active_bank,
                                       d->candidate,&other_count,&other_gen)==0)
        keep_other=1;
    for(sec=MD13_DATA_BASE/MD13_SECTOR;sec<MD13_SIZE/MD13_SECTOR;sec++){
        referenced=0;
        for(i=0;i<d->count;i++)
            if(s13_overlaps(sec*MD13_SECTOR,(sec+1)*MD13_SECTOR,
                        d->active[i].offset,s13_ent_end(&d->active[i])))referenced=1;
        if(keep_other)for(i=0;i<other_count;i++)
            if(s13_overlaps(sec*MD13_SECTOR,(sec+1)*MD13_SECTOR,
                        d->candidate[i].offset,s13_ent_end(&d->candidate[i])))referenced=1;
        if(referenced)continue;
        blank=s13_blank_region(d,sec*MD13_SECTOR,MD13_SECTOR);
        if(blank<0)return MD13_EIO;
        if(!blank){if(s13_io_erase(d,sec*MD13_SECTOR))return MD13_EIO;erased++;}
    }
    *sectors_erased=erased;return 0;
}
const char *md13_error(int e){switch(e){case 0:return "OK";case MD13_EIO:return "I/O or power-cut";case MD13_ECORRUPT:return "corrupt";case MD13_EINVAL:return "invalid input";case MD13_ENOSPACE:return "no room";case MD13_EFOREIGN:return "foreign/legacy or uncommitted disk";case MD13_ENOTFOUND:return "not found";case MD13_EVERIFY:return "commit verify failed";default:return "unknown error";}}
