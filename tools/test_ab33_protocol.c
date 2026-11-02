#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "golana_pattern_model.h"
static dz_seq_model_t dz_model;
static struct {int ready, dz_edit_active, save;} gu={1,0,0};
static int dz_play_has_backup;
static int dz_store_enabled=1,dz_store_dirty,dz_store_error,dz_store_have_commit=1;
static int flash_ok=1,gx_blocked,gx_dirty;
static int proj_slot[4];
#define ST_SECTOR 4096u
static int transport_busy(void){return 0;}
static int st_erase(uint32_t x){(void)x;return 0;}
static void fm1_wdt_feed(void){}
static int dz_store_pair_blank(uint8_t a){(void)a;return 1;}
static void gx_boot(void){}
static void dz_store_boot(dz_seq_model_t *m){(void)m;}
static void dz_store_mark_dirty(void){dz_store_dirty=1;}
static void dz_store_poll(dz_seq_model_t *m,int x){(void)m;(void)x;dz_store_dirty=0;}
static void gu_say(const char *a, const char *b){(void)a;(void)b;}
#include "golana_pattern_model.c"
#include "golana_dz_midi_v3.c"
static int fails,checks;
static void ck(int val,const char *name){checks++;printf("%s %s\n",val?"PASS":"FAIL",name);if(!val)fails++;}
static uint32_t cmd(uint8_t *a,uint32_t n,uint8_t *o){return dz_midi_request(a,n,o);}
int main(void){
 uint8_t a[200],out[200];uint32_t n,i;
 dz_seq_model_init(&dz_model);
 a[0]=0;a[1]=1;n=cmd(a,2,out);
 ck(n==10 && out[2]==0 && out[3]==3 && out[8]==64 && out[9]==0,"v3, 64 patterns, 128 song entries");
 a[0]=2;a[1]=2;a[2]=0;a[3]=7;a[4]=64;
 for(i=0;i<64;i++){a[5+2*i]= i%4==0?67:0;a[6+2*i]=i%4==0?0:i%4<3?4:0;}
 n=cmd(a,133,out); ck(n==3 && out[2]==0,"Bank A08 write");
 a[0]=3;a[1]=3;a[2]=0;a[3]=7;n=cmd(a,4,out);
 ck(n==132 && out[2]==0 && out[3]==64 && out[4]==67 && out[6]==0 && out[7]==4,"Bank A08 readback");
 a[0]=4;a[1]=4;a[2]=0;a[3]=5;
 const uint8_t rows[25]={6,0,1,31,0,5,0,1,120,0,0,0,64,0,0,7,0,32,0,0,4,16,1,0,0};
 memcpy(a+4,rows,25); n=cmd(a,29,out);
 ck(n==5 && out[2]==0 && dz_model.song[0].bank==6 && dz_model.song[1].value==120 && dz_model.song[2].play_steps==64 && dz_model.song[3].bank==7 && dz_model.song[4].bank==4,"PRESET TEMPO A01 REST END chunk upload");
 a[0]=5;a[1]=5;a[2]=0;a[3]=5;n=cmd(a,4,out);
 ck(n==30 && out[2]==0 && memcmp(out+5,rows,25)==0,"Song chunk readback");
 a[0]=7;a[1]=6;a[2]=0;a[3]=0;a[4]=1;n=cmd(a,5,out);
 ck(n==6 && out[2]==0 && dz_model.song_count==128,"128 song entries metadata 0=128");
 a[0]=8;a[1]=7;n=cmd(a,2,out);
 ck(n==6 && out[3]==0 && out[4]==0 && out[5]==1,"Metadata readback");
 a[0]=4;a[1]=8;a[2]=5;a[3]=1;a[4]=5;a[5]=0;a[6]=1;a[7]=240&127;a[8]=240>>7;
 n=cmd(a,9,out);ck(n==5 && out[2]==0 && dz_model.song[5].value==240,"Tempo 240 valid in 7-bit transport");
 a[4]=5;a[5]=0;a[6]=0;a[7]=1;a[8]=0;
 n=cmd(a,9,out);ck(n==3 && out[2]==1 && dz_model.song[5].value==240,"Invalid chunk rejected atomically");
 a[0]=6;a[1]=9;n=cmd(a,2,out);
 ck(n==5 && out[2]==0 && out[3]==0 && out[4]==1,"A/B Flash commit acknowledged");
 printf("%d/%d passed (%d errors)\n",checks-fails,checks,fails);return fails?1:0;
}
