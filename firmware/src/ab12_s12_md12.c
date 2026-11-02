#include "ab12_s12_md12.h"
/* String functions provided by containing unity build or host. */

#define M_COMMIT 0x1AB2C3D4u
#define M_HDR 32u
#define M_REC 16u
#define M_ENT 12u
#define M_MAGIC 0x3231444Du  /* 'MD12' LE */
#define R_MAGIC 0x52323144u  /* 'D12R' LE */
static uint32_t s12_rd32(const uint8_t *b){return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);}
static uint16_t s12_rd16(const uint8_t *b){return (uint16_t)(b[0] | ((uint16_t)b[1]<<8));}
static void s12_wr32(uint8_t *b,uint32_t n){b[0]=(uint8_t)n;b[1]=(uint8_t)(n>>8);b[2]=(uint8_t)(n>>16);b[3]=(uint8_t)(n>>24);}
static void s12_wr16(uint8_t *b,uint16_t n){b[0]=(uint8_t)n;b[1]=(uint8_t)(n>>8);}
static uint32_t s12_align16(uint32_t v){return (v+15u)&~15u;}
static uint32_t s12_crc_step(uint32_t c, const uint8_t *p,uint32_t n){while(n--){int j;c^=*p++;for(j=0;j<8;j++)c=(c>>1)^(0xEDB88320u & (0u-(c&1u)));}return c;}
static uint32_t s12_crc(const uint8_t *p,uint32_t n){return ~s12_crc_step(0xFFFFFFFFu,p,n);}
static int s12_io_read(const md12_disk*d,uint32_t p,void *dst,uint32_t n){if(p>MD12_SIZE || n>MD12_SIZE-p || !d->io.read)return MD12_EINVAL;return d->io.read(d->io.ctx,p,dst,n)?MD12_EIO:0;}
static int s12_io_erase(md12_disk*d,uint32_t p){if(p%MD12_SECTOR || p>MD12_SIZE-MD12_SECTOR || !d->io.erase4k)return MD12_EINVAL;return d->io.erase4k(d->io.ctx,p)?MD12_EIO:0;}
static int s12_io_program(md12_disk*d,uint32_t p,const void*src,uint32_t n){if(!n || n>MD12_PAGE || p>MD12_SIZE-n || p/MD12_PAGE!=(p+n-1)/MD12_PAGE || !d->io.program)return MD12_EINVAL;return d->io.program(d->io.ctx,p,src,n)?MD12_EIO:0;}
static int s12_write_bytes(md12_disk*d,uint32_t p,const uint8_t*src,uint32_t n){while(n){uint32_t k=MD12_PAGE-p%MD12_PAGE;if(k>n)k=n;if(s12_io_program(d,p,src,k))return MD12_EIO;p+=k;src+=k;n-=k;}return 0;}
static int s12_valid_type(uint8_t type){return type>=MD12_PAT1 && type<=MD12_PRE1;}
static int s12_key_eq(const md12_entry*e,uint8_t t,uint16_t n){return e->type==t && e->number==n;}
static int s12_active_index(const md12_disk*d,uint8_t t,uint16_t n){uint32_t i;for(i=0;i<d->count;i++)if(s12_key_eq(&d->active[i],t,n))return (int)i;return -1;}
static int s12_shadow_index(const md12_disk*d,uint32_t n,uint8_t t,uint16_t id){uint32_t i;for(i=0;i<n;i++)if(s12_key_eq(&d->candidate[i],t,id))return (int)i;return -1;}
static int s12_descriptor_ok(const md12_entry*e){uint32_t required;
    if(!s12_valid_type(e->type)||!e->number||e->offset<MD12_DATA_BASE||e->offset%16||e->length>MD12_SIZE-MD12_DATA_BASE-M_REC)return 0;
    required=s12_align16(M_REC+e->length);
    return e->offset<=MD12_SIZE && required<=MD12_SIZE-e->offset;
}
static uint32_t s12_ent_end(const md12_entry*e){return e->offset+s12_align16(M_REC+e->length);}
static int s12_verify_record(md12_disk*d,const md12_entry*e){uint8_t hdr[16],buf[256];uint32_t c=0xFFFFFFFFu,pos=0,n;
    if(!s12_descriptor_ok(e)||s12_io_read(d,e->offset,hdr,16))return MD12_ECORRUPT;
    if(s12_rd32(hdr)!=R_MAGIC||hdr[4]!=e->type||hdr[5]!=e->flags||s12_rd16(hdr+6)!=e->number||s12_rd32(hdr+8)!=e->length)return MD12_ECORRUPT;
    while(pos<e->length){n=e->length-pos;if(n>sizeof buf)n=sizeof buf;if(s12_io_read(d,e->offset+16+pos,buf,n))return MD12_EIO;c=s12_crc_step(c,buf,n);pos+=n;}
    return (~c)==s12_rd32(hdr+12) ? MD12_OK : MD12_ECORRUPT;
}
static int s12_bank_read(md12_disk*d,uint32_t bank,md12_entry*arr,uint16_t*nout,uint32_t*gen){
    uint32_t i,j,tcrc=0xFFFFFFFFu,cnt,bytes;uint8_t h[32],b[12];
    if(s12_io_read(d,bank*MD12_BANK_SIZE,h,32))return MD12_EIO;
    if(s12_rd32(h)!=M_MAGIC||s12_rd32(h+4)!=1u||s12_rd32(h+8)==0||s12_rd32(h+28)!=M_COMMIT)return MD12_ECORRUPT;
    cnt=s12_rd32(h+12);bytes=s12_rd32(h+16);
    if(cnt>MD12_MAX_ENTRIES||bytes!=cnt*M_ENT||s12_crc(h,24)!=s12_rd32(h+24))return MD12_ECORRUPT;
    for(i=0;i<cnt;i++){
        if(s12_io_read(d,bank*MD12_BANK_SIZE+MD12_PAGE+i*M_ENT,b,M_ENT))return MD12_EIO;
        tcrc=s12_crc_step(tcrc,b,M_ENT);
        arr[i].type=b[0];arr[i].flags=b[1];arr[i].number=s12_rd16(b+2);
        arr[i].offset=s12_rd32(b+4);arr[i].length=s12_rd32(b+8);
        if(!s12_descriptor_ok(&arr[i])||s12_verify_record(d,&arr[i]))return MD12_ECORRUPT;
        for(j=0;j<i;j++){
            const md12_entry*a=&arr[j],*z=&arr[i];
            if(s12_key_eq(a,z->type,z->number))return MD12_ECORRUPT;
            if(a->offset<s12_ent_end(z)&&z->offset<s12_ent_end(a))return MD12_ECORRUPT;
        }
    }
    if((~tcrc)!=s12_rd32(h+20))return MD12_ECORRUPT;
    *nout=(uint16_t)cnt;*gen=s12_rd32(h+8);return MD12_OK;
}
static int s12_newer(uint32_t a,uint32_t b){uint32_t diff=a-b;return diff && diff<0x80000000u;}
static int s12_blank_region(md12_disk*d,uint32_t start,uint32_t n){uint8_t b[256];uint32_t i;
    while(n){uint32_t k=n>256?256:n;if(s12_io_read(d,start,b,k))return MD12_EIO;
        for(i=0;i<k;i++)if(b[i]!=0xFF)return 0;
        start+=k;n-=k;
    }return 1;
}
int md12_mount(md12_disk*d,md12_io io){uint16_t n0=0,n1=0;uint32_t g0=0,g1=0,i;int a,b,blank;
    if(!d||!io.read||!io.erase4k||!io.program)return MD12_EINVAL;
    /* IMPORTANT: this is a development prototype; caller must provide scratch memory. */
    memset(d,0,sizeof *d);d->io=io;d->active_bank=-1;d->cursor=MD12_DATA_BASE;
    a=s12_bank_read(d,0,d->active,&n0,&g0);
    b=s12_bank_read(d,1,d->candidate,&n1,&g1);
    if(a==0 && (b!=0||!s12_newer(g1,g0))){d->count=n0;d->generation=g0;d->active_bank=0;}
    else if(b==0){memcpy(d->active,d->candidate,n1*sizeof(md12_entry));d->count=n1;d->generation=g1;d->active_bank=1;}
    else {
        /* Refuse legacy ABST, torn/foreign layouts, corrupt manifests. An empty
         * disk MUST be physically all 0xFF before first use. */
        blank=s12_blank_region(d,0,MD12_SIZE);
        if(blank<0)return MD12_EIO;
        return blank?MD12_OK:MD12_EFOREIGN;
    }
    for(i=0;i<d->count;i++)if(s12_ent_end(&d->active[i])>d->cursor)d->cursor=s12_ent_end(&d->active[i]);
    if(d->cursor>=MD12_SIZE)d->cursor=MD12_DATA_BASE;
    return MD12_OK;
}
int md12_lookup(const md12_disk*d,uint8_t t,uint16_t num,md12_entry*out){int i;if(!d||!out)return MD12_EINVAL;i=s12_active_index(d,t,num);if(i<0)return MD12_ENOTFOUND;*out=d->active[i];return 0;}
int md12_read(const md12_disk*d,uint8_t t,uint16_t num,uint32_t offset,void*out,uint32_t len){md12_entry e;int rc;
    if(!d||(!out&&len))return MD12_EINVAL;
    rc=md12_lookup(d,t,num,&e);if(rc)return rc;
    if(offset>e.length||len>e.length-offset)return MD12_EINVAL;
    return s12_io_read(d,e.offset+M_REC+offset,out,len);
}
static int s12_overlaps(uint32_t a,uint32_t ae,uint32_t b,uint32_t be){return a<be&&b<ae;}
static int s12_owned(const md12_disk*d,uint32_t p,uint32_t end,uint32_t n){uint32_t j;
    for(j=0;j<d->count;j++)if(s12_overlaps(p,end,d->active[j].offset,s12_ent_end(&d->active[j])))return 1;
    for(j=0;j<n;j++)if(s12_overlaps(p,end,d->candidate[j].offset,s12_ent_end(&d->candidate[j])))return 1;
    return 0;
}
static int s12_allocate(md12_disk*d,uint32_t size,uint32_t candidate_count,uint32_t*out){uint32_t passes,p,end,start,sizea;int is_blank;
    if(size>MD12_SIZE-MD12_DATA_BASE-15)return MD12_ENOSPACE;
    sizea=s12_align16(size);start=d->cursor;
    if(start<MD12_DATA_BASE||start>=MD12_SIZE)start=MD12_DATA_BASE;
    for(passes=0;passes<2;passes++){
        p=passes?MD12_DATA_BASE:start;end=passes?start:MD12_SIZE;
        for(;p+sizea<=end;p+=16){
            if(s12_owned(d,p,p+sizea,candidate_count))continue;
            is_blank=s12_blank_region(d,p,sizea);
            if(is_blank<0)return MD12_EIO;
            if(is_blank){*out=p;d->cursor=p+sizea;if(d->cursor>=MD12_SIZE)d->cursor=MD12_DATA_BASE;return 0;}
        }
    }
    return MD12_ENOSPACE;
}
static int s12_make_record(md12_disk*d,const md12_change*c,uint32_t n,md12_entry*out){uint8_t chunk[256],hdr[16];uint32_t offset,at,crc_run=0xFFFFFFFFu,len;int rc;
    rc=s12_allocate(d,M_REC+c->length,n,&offset);if(rc)return rc;
    for(at=0;at<c->length;at+=len){
        len=c->length-at;if(len>MD12_PAGE)len=MD12_PAGE;
        /* Offset+16 may straddle a program page: s12_write_bytes chunks correctly. */
        if(c->fill(c->ctx,at,chunk,len))return MD12_EIO;
        crc_run=s12_crc_step(crc_run,chunk,len);
        if(s12_write_bytes(d,offset+M_REC+at,chunk,len))return MD12_EIO;
    }
    s12_wr32(hdr,R_MAGIC);hdr[4]=c->type;hdr[5]=c->flags;s12_wr16(hdr+6,c->number);s12_wr32(hdr+8,c->length);s12_wr32(hdr+12,~crc_run);
    if(s12_write_bytes(d,offset,hdr,16))return MD12_EIO;
    out->offset=offset;out->length=c->length;out->number=c->number;out->type=c->type;out->flags=c->flags;
    return s12_verify_record(d,out);
}
int md12_transaction(md12_disk*d,const md12_change*c,uint32_t changes){uint32_t n,i,j,target,base,tablecrc=0xFFFFFFFFu,gen;int pos,rc;uint8_t b[32];
    if(!d||(!c&&changes)||changes>MD12_MAX_ENTRIES)return MD12_EINVAL;
    n=d->count;memcpy(d->candidate,d->active,n*sizeof(md12_entry));
    /* Validate FIRST, before any programming or erasure. */
    for(i=0;i<changes;i++){
        if(!s12_valid_type(c[i].type)||!c[i].number||(!c[i].deleted&&!c[i].fill)||c[i].length>MD12_SIZE-MD12_DATA_BASE-M_REC)return MD12_EINVAL;
        for(j=0;j<i;j++)if(c[i].type==c[j].type&&c[i].number==c[j].number)return MD12_EINVAL;
        
    }
    for(i=0;i<changes;i++){
        pos=s12_shadow_index(d,n,c[i].type,c[i].number);
        if(c[i].deleted){if(pos>=0){d->candidate[pos]=d->candidate[--n];}continue;}
        int fresh=0;
        if(pos<0){if(n>=MD12_MAX_ENTRIES)return MD12_ENOSPACE;pos=(int)n++;fresh=1;}
        /* Keep old record s12_owned until manifest commits. On replacing one key,
         * the active[] array still owns the original. The fresh record only
         * becomes candidate[] after fully verified. */
        rc=s12_make_record(d,&c[i],fresh?n-1u:n,&d->candidate[pos]);
        if(rc)return rc;
    }
    target=d->active_bank==0?1u:0u;base=target*MD12_BANK_SIZE;
    if(s12_io_erase(d,base)||s12_io_erase(d,base+MD12_SECTOR))return MD12_EIO;
    for(i=0;i<n;i++){
        const md12_entry*e=&d->candidate[i];
        b[0]=e->type;b[1]=e->flags;s12_wr16(b+2,e->number);s12_wr32(b+4,e->offset);s12_wr32(b+8,e->length);
        tablecrc=s12_crc_step(tablecrc,b,12);
        if(s12_write_bytes(d,base+MD12_PAGE+i*M_ENT,b,M_ENT))return MD12_EIO;
    }
    gen=d->generation+1u;if(!gen)gen=1u;
    s12_wr32(b,M_MAGIC);s12_wr32(b+4,1);s12_wr32(b+8,gen);s12_wr32(b+12,n);s12_wr32(b+16,n*M_ENT);s12_wr32(b+20,~tablecrc);
    s12_wr32(b+24,s12_crc(b,24));s12_wr32(b+28,M_COMMIT);
    if(s12_write_bytes(d,base,b,32))return MD12_EIO;
    /* Read/CRC all newly committed records before changing active state. */
    {uint16_t read_count=0;uint32_t read_gen=0;
     if(s12_bank_read(d,target,d->candidate,&read_count,&read_gen)||read_count!=n||read_gen!=gen)return MD12_EVERIFY;}
    memcpy(d->active,d->candidate,n*sizeof(md12_entry));d->count=(uint16_t)n;d->generation=gen;d->active_bank=(int8_t)target;
    return MD12_OK;
}
int md12_sweep_dead(md12_disk*d,uint32_t*sectors_erased){
    uint32_t sec,i,erased=0,other_gen=0;
    uint16_t other_count=0;
    int referenced,blank,keep_other=0;
    if(!d||!sectors_erased)return MD12_EINVAL;
    /* The OTHER (older) committed manifest is still a valid recovery root.
     * Protect its records too. Never erase data needed for CRC fallback. */
    if(d->active_bank>=0 && s12_bank_read(d,1u-(uint32_t)d->active_bank,
                                       d->candidate,&other_count,&other_gen)==0)
        keep_other=1;
    for(sec=MD12_DATA_BASE/MD12_SECTOR;sec<MD12_SIZE/MD12_SECTOR;sec++){
        referenced=0;
        for(i=0;i<d->count;i++)
            if(s12_overlaps(sec*MD12_SECTOR,(sec+1)*MD12_SECTOR,
                        d->active[i].offset,s12_ent_end(&d->active[i])))referenced=1;
        if(keep_other)for(i=0;i<other_count;i++)
            if(s12_overlaps(sec*MD12_SECTOR,(sec+1)*MD12_SECTOR,
                        d->candidate[i].offset,s12_ent_end(&d->candidate[i])))referenced=1;
        if(referenced)continue;
        blank=s12_blank_region(d,sec*MD12_SECTOR,MD12_SECTOR);
        if(blank<0)return MD12_EIO;
        if(!blank){if(s12_io_erase(d,sec*MD12_SECTOR))return MD12_EIO;erased++;}
    }
    *sectors_erased=erased;return 0;
}
const char *md12_error(int e){switch(e){case 0:return "OK";case MD12_EIO:return "I/O or power-cut";case MD12_ECORRUPT:return "corrupt";case MD12_EINVAL:return "invalid input";case MD12_ENOSPACE:return "no room";case MD12_EFOREIGN:return "foreign/legacy or uncommitted disk";case MD12_ENOTFOUND:return "not found";case MD12_EVERIFY:return "commit verify failed";default:return "unknown error";}}
