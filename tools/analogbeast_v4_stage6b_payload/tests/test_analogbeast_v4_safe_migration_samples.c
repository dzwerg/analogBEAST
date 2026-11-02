/* Host integration test: real NOR driver, legacy PDZ6 importer and MIDI gate.
 * No FM-1 is connected and there are no physical flash writes.
 * cc -std=c11 -O1 -Ifirmware/src tests/test_analogbeast_v4_safe_migration.c
 *    firmware/src/analogbeast_snapshot.c -o build/host/ab_safe_migration_test
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "golana_pattern_model.h"
#define CHECK(v) do {if(!(v)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v);exit(1);}}while(0)
#define ST_MAGIC 0x554C4546u
#define ST_SECTOR 4096u
#define ST_PAYLOAD_OFF 256u
#define OBJ_DZ_PAGE0 10u /* real storage.c enum when GOLANA=1 */
#define FL_SIZE 0x100000u
typedef struct {
    uint32_t magic;
    uint16_t type,slot;
    uint32_t seq,len,crc,rsv[2];
    uint32_t hcrc;
} st_hdr_t;
static uint8_t st_buf[3840];
static uint8_t flash[FL_SIZE];
static uint32_t fm1_ms;
static uint8_t flash_ok=1u;
static unsigned erases,programs;
static int fail_next_erase;
static int busy;
static void fm1_wdt_feed(void) {}
static int transport_busy(void){return busy;}
static int st_read(uint32_t off,void *dst,uint32_t n){if(off>FL_SIZE||n>FL_SIZE-off)return -1;memcpy(dst,flash+off,n);return 0;}
static int st_erase(uint32_t off){if(off%4096||off>FL_SIZE-4096)return -1;if(fail_next_erase){fail_next_erase=0;return -1;}memset(flash+off,255,4096);++erases;return 0;}
static int st_prog(uint32_t off,const void *src,uint32_t n){const uint8_t *p=src;uint32_t i;if(off>FL_SIZE||n>FL_SIZE-off||n>256)return -1;for(i=0;i<n;i++){if((~flash[off+i])&p[i])return -1;flash[off+i]&=p[i];}++programs;return 0;}
static uint32_t st_crc32_add(uint32_t c,const void *p,uint32_t n){const uint8_t *b=p;uint32_t k;while(n--){c^=*b++;for(k=0;k<8;k++)c=(c>>1)^(0xEDB88320u & -(c&1u));}return c;}
static uint32_t st_crc32(const void*p,uint32_t n){return ~st_crc32_add(0xFFFFFFFFu,p,n);}
#include "../firmware/src/golana_dz_store.c"
/* SysEx requests execute the same firmware handler (read and write commands). */
static dz_seq_model_t dz_model;
static struct {uint8_t ready,dz_edit_active,save;} gu={1,0,0};
static uint8_t dz_play_has_backup;
#include "../firmware/src/golana_dz_midi.c"

static void legacy_fixture(uint32_t half,uint32_t slot,uint32_t copy){
    dz_old_page_t p;
    st_hdr_t h;
    uint32_t addr=0x97000u+slot*2u*ST_SECTOR+copy*ST_SECTOR;
    uint32_t j,k,n=0;
    memset(&p,0,sizeof p);
    p.magic=DZ_OLD_MAGIC; p.version=2;p.half=(uint8_t)half;p.generation=12;
    p.bank=0;p.pattern=0;p.song_mode=1;p.loop_mode=1;p.song_count=128;
    for(j=0;j<DZ_OLD_HALF;j++){
       p.length[j]=64;
       for(k=0;k<64;k+=2,n+=3){
          uint16_t x=(uint16_t)(30+((half*32+j)%64)) | (uint16_t)(k==0?0:1)<<7;
          uint16_t y=(uint16_t)(30+((half*32+j)%64)) | (uint16_t)(k==62?0:1)<<7;
          p.step[n]=(uint8_t)x;
          p.step[n+1]=(uint8_t)((x>>8)&15u)|(uint8_t)((y&15u)<<4);
          p.step[n+2]=(uint8_t)(y>>4);
       }
    }
    memset(&h,0,sizeof h);
    h.magic=ST_MAGIC;h.type=(uint16_t)(OBJ_DZ_PAGE0+half);h.slot=(uint16_t)copy;
    h.seq=12;h.len=sizeof p;h.crc=st_crc32(&p,sizeof p);h.hcrc=st_crc32(&h,sizeof h-4u);
    memcpy(flash+addr,&h,sizeof h);
    memcpy(flash+addr+ST_PAYLOAD_OFF,&p,sizeof p);
}
static int req(uint8_t op,const uint8_t *args,unsigned na,uint8_t *out){uint8_t in[160];unsigned j;in[0]=op;in[1]=11u;for(j=0;j<na;j++)in[2+j]=args[j];return (int)dz_midi_request(in,na+2u,out);}
int main(void){
    static uint8_t flash_orig[8*4096];
    uint8_t out[200],wrong[9]={'C','O','M','M','I','T',' ','V','5'};
    uint8_t correct[9]={'C','O','M','M','I','T',' ','V','4'};
    uint8_t step_req[4]={0,0,64,2};
    uint32_t i;
    /* Reproduce the physical first boot: legacy banks exist, but a user
     * sample slot already contains data. Import MUST happen in RAM and
     * NOR writes MUST stay blocked despite timer, MIDI and manual commit. */
    memset(flash,0xff,sizeof flash);
    legacy_fixture(0,0,0);legacy_fixture(1,1,0);
    memcpy(flash_orig,flash+0x97000,sizeof flash_orig);
    flash[AB_SAMPLE_START] = 'F'; /* occupied Felucca sample header */
    dz_seq_model_init(&dz_model);
    dz_store_boot(&dz_model);
    CHECK(dz_store_write_locked && dz_store_import_pending && !dz_store_enabled);
    CHECK(dz_model.patterns[0].step[0].note == 30 && dz_model.patterns[63].step[0].note == 93);
    CHECK(!dz_store_dirty && !dz_store_error && !dz_store_have_commit);
    CHECK(req(18,0,0,out)==9 && out[2]==0 && out[3]==1 && out[4]==1 && out[5]==0);
    dz_store_mark_dirty(); fm1_ms=90000;
    for(i=0;i<50;i++) dz_store_poll(&dz_model,1);
    CHECK(erases==0 && programs==0);
    CHECK(req(19,correct,9,out)==3 && out[2]==3);
    CHECK(memcmp(flash_orig,flash+0x97000,sizeof flash_orig)==0);
    CHECK(flash[AB_SAMPLE_START]=='F');
    puts("PASS Stage6b: occupied FSMP slot still imports 64 patterns RAM-only; never writes");
    /* A foreign copy header is also an independent reason to prohibit NOR. */
    flash[AB_SAMPLE_START]=0xFF;
    flash[AB_SAMPLE_START+AB_RECORD_SECTOR_OFFSET]=0x23;
    dz_seq_model_init(&dz_model);dz_store_boot(&dz_model);
    CHECK(dz_store_import_pending && !dz_store_enabled && dz_store_write_locked);
    CHECK(dz_store_nor.foreign_copy[0] && dz_model.patterns[0].step[0].note==30);
    dz_store_mark_dirty();fm1_ms+=7000;dz_store_poll(&dz_model,1);
    CHECK(erases==0 && programs==0);
    CHECK(memcmp(flash_orig,flash+0x97000,sizeof flash_orig)==0);
    puts("PASS Stage6b: foreign NOR copy preserves RAM import and blocks commit");
    /* Restore fully blank NOR for the original Stage6 commit tests. */
    flash[AB_SAMPLE_START+AB_RECORD_SECTOR_OFFSET]=0xFF;
    dz_seq_model_init(&dz_model);
    dz_store_boot(&dz_model);
    CHECK(dz_store_enabled && dz_store_write_locked && dz_store_import_pending && !dz_store_have_commit);
    CHECK(!dz_store_dirty);
    for(i=0;i<64;i++){
       CHECK(dz_model.patterns[i].step[0].note == 30+i);
       CHECK(dz_model.patterns[i].step[0].velocity==96);
       CHECK(dz_model.patterns[i].step[0].gate==64);
    }
    CHECK(req(18,0,0,out)==9 && out[2]==0 && out[3]==1 && out[4]==1 && out[7]==0);
    CHECK(req(6,0,0,out)==3 && out[2]==3); /* legacy save polling blocked */
    dz_store_mark_dirty();fm1_ms=90000;
    for(i=0;i<100;i++)dz_store_poll(&dz_model,1);
    CHECK(erases==0 && programs==0);
    /* Changing patterns by MIDI and legacy RESET must never bypass the gate. */
    CHECK(req(16,step_req,4,out)==7 && out[2]==0);
    { const uint8_t reset_key[5]={'R','E','S','E','T'};
      CHECK(req(9,reset_key,5,out)==3 && out[2]==0);
      CHECK(dz_store_write_locked && erases==0 && programs==0);
      dz_seq_model_init(&dz_model);
      dz_store_boot(&dz_model); /* A reboot still recovers original PDZ6 banks */
      CHECK(dz_store_import_pending && dz_model.patterns[0].step[0].note==30);
    }
    CHECK(req(19,wrong,9,out)==3 && out[2]==1 && erases==0);
    CHECK(req(19,correct,8,out)==3 && out[2]==1 && erases==0);
    busy=1;CHECK(req(19,correct,9,out)==3 && out[2]==2 && erases==0);
    busy=0;gu.dz_edit_active=1;
    CHECK(req(19,correct,9,out)==3 && out[2]==2 && erases==0);
    gu.dz_edit_active=0;
    /* A power cut during the explicit save must not unlock autosave. */
    fail_next_erase=1;
    CHECK(req(19,correct,9,out)==3 && out[2]==4);
    CHECK(dz_store_write_locked && dz_store_import_pending && !dz_store_have_commit);
    {unsigned before=erases;fm1_ms+=7000;dz_store_poll(&dz_model,1);CHECK(erases==before);}
    dz_store_error=0;
    /* Owner-confirmed COMMIT V4 is the only entry point that writes a snapshot. */
    CHECK(req(19,correct,9,out)==5 && out[2]==0 && out[3]==1 && out[4]==0);
    CHECK(erases>0 && programs>0 && dz_store_have_commit && !dz_store_write_locked && !dz_store_import_pending);
    CHECK(memcmp(flash_orig,flash+0x97000,sizeof flash_orig)==0);
    CHECK(flash[0xA0000]==255 && flash[0xB4000]==255 && flash[0xC8000]==255);
    CHECK(req(19,correct,9,out)==3 && out[2]==3); /* replay cannot commit twice */
    dz_store_mark_dirty();fm1_ms+=5000;
    {unsigned before=erases;dz_store_poll(&dz_model,1);CHECK(erases>before);} /* normal autosave now unlocked */
    dz_seq_model_init(&dz_model);dz_store_boot(&dz_model);
    CHECK(!dz_store_write_locked && dz_store_have_commit && !dz_store_import_pending);
    for(i=0;i<64;i++)CHECK(dz_model.patterns[i].step[0].note==30+i);
    printf("PASS Stage6: 64 legacy patterns RAM-only; timer/MIDI/RESET blocked; explicit one-shot commit; legacy flash intact; reboot and autosave OK\n");
    return 0;
}
