/* Host-side contract test of exact firmware MIDI opcodes, NOT a hardware test. */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "golana_pattern_model.h"
static dz_seq_model_t dz_model;
static struct { uint8_t ready, dz_edit_active, save; } gu = {1u,0u,0u};
static uint8_t dz_store_enabled=1u,dz_store_dirty,dz_store_error,dz_store_have_commit=1u,dz_play_has_backup;
static uint8_t dz_store_write_locked=1u,dz_store_import_pending=1u;
static uint8_t dz_store_explicit_commit(const dz_seq_model_t*m){(void)m;if(!dz_store_write_locked)return 3;dz_store_write_locked=0;dz_store_import_pending=0;dz_store_have_commit=1;dz_store_dirty=0;return 0;}
static int transport_busy(void) {return 0;}
static void dz_store_mark_dirty(void){dz_store_dirty=1;}
static void dz_store_poll(const dz_seq_model_t*m,int allowed){(void)m;(void)allowed;}
#include "../firmware/src/golana_dz_midi.c"
#define OK(c) do{if(!(c)){fprintf(stderr,"FAIL:%u: %s\n",__LINE__,#c); return 1;}}while(0)
static int send(uint8_t *req,unsigned n,uint8_t *out){memset(out,0xDA,220);return (int)dz_midi_request(req,n,out);}
int main(void)
{
    uint8_t a[150], out[220];
    unsigned i,n;
    dz_seq_model_init(&dz_model);
    a[0]=0;a[1]=1;n=send(a,2,out);
    OK(n==13 && out[2]==0 && out[3]==5 && out[10]==3 && out[11]==0 && out[12]==2);
    OK(out[n]==0xDA); /* no response overflow */
    /* Metadata: maximum 256 (two base-128 bytes), never mapped to 128. */
    a[0]=12;a[1]=2;a[2]=0;a[3]=2;a[4]=1;a[5]=1;
    n=send(a,6,out);OK(n==7 && out[2]==0 && dz_model.song_count==256);
    a[0]=13;n=send(a,2,out);OK(n==7 && out[3]==0 && out[4]==2 && out[5]==1 && out[6]==1);
    /* Write and read rows crossing 128 and the 255/256 limit. */
    a[0]=10;a[1]=3;a[2]=127;a[3]=0;a[4]=3;
    for(i=0;i<3;i++){unsigned off=5+i*5;a[off]=0;a[off+1]=(uint8_t)i;a[off+2]=64;a[off+3]=0;a[off+4]=0;}
    n=send(a,20,out);OK(n==6 && out[2]==0);
    OK(dz_model.song[127].pattern==0 && dz_model.song[128].pattern==1 && dz_model.song[129].pattern==2);
    a[0]=11;n=send(a,5,out);OK(n==21 && out[2]==0 && out[3]==127 && out[4]==0 && out[5]==3);
    OK(out[7]==0 && out[12]==1 && out[17]==2); /* pattern at offsets 9,14,19 */
    a[0]=10;a[2]=127;a[3]=1;a[4]=1;
    a[5]=8;a[6]=0;a[7]=1;a[8]=1;a[9]=0; /* ARP ON at 256th row */
    n=send(a,10,out);OK(out[2]==0 && dz_model.song[255].bank==8 && dz_model.song[255].value==1);
    a[2]=127;a[3]=1;a[4]=2; n=send(a,15,out);
    OK(n==3 && out[2]==1 && dz_model.song[255].bank==8); /* overrun refused atomically */
    a[2]=127;a[3]=1;a[4]=1;a[5]=9;
    n=send(a,10,out);OK(n==3 && out[2]==1 && dz_model.song[255].bank==8);
    /* 16-step transfers; verify all 5 fields and no short/invalid request partial writes. */
    a[0]=14;a[1]=4;a[2]=3;a[3]=15;a[4]=48;a[5]=16;
    for(i=0;i<16;i++){unsigned off=6+i*5;a[off]=60+i;a[off+1]=i%4;
        a[off+2]=1+i;a[off+3]=100+i;a[off+4]=i;}
    n=send(a,86,out);OK(n==7 && out[2]==0 && out[3]==3 && out[4]==15 && out[5]==48 && out[6]==16);
    OK(dz_model.patterns[63].step[63].note==75 && dz_model.patterns[63].step[63].velocity==16);
    a[0]=15;n=send(a,6,out);OK(n==87 && out[2]==0);
    OK(out[3]==3 && out[4]==15 && out[5]==48 && out[6]==16 && out[7]==60 && out[8]==0);
    OK(out[7+15*5]==75 && out[7+15*5+2]==16 && out[7+15*5+3]==115);
    a[0]=14; a[6+5*15+2]=0;n=send(a,86,out);
    OK(n==3 && out[2]==1 && dz_model.patterns[63].step[63].velocity==16);
    a[0]=16;a[4]=32;a[5]=2;n=send(a,6,out);OK(n==7 && out[2]==0);
    a[0]=17;n=send(a,4,out);OK(n==7 && out[2]==0 && out[5]==32 && out[6]==2);
    a[0]=16;a[4]=0;n=send(a,6,out);OK(n==3 && out[2]==1 && dz_model.patterns[63].length==32);
    /* Status read-only; malformed confirmations must never trigger a commit. */
    a[0]=18;a[1]=31;n=send(a,2,out);
    OK(n==9 && out[2]==0 && out[3]==1 && out[4]==1);
    a[0]=19;n=send(a,2,out);OK(n==3 && out[2]==1);
    memcpy(a+2,"COMMIT V5",9);n=send(a,11,out);OK(n==3 && out[2]==1 && dz_store_write_locked);
    memcpy(a+2,"COMMIT V4",9);n=send(a,11,out);
    OK(n==5 && out[2]==0 && out[3]==1 && out[4]==0 && !dz_store_write_locked);
    n=send(a,11,out);OK(n==3 && out[2]==3); /* one-shot, cannot replay */
    /* Deprecated destructive opcode still refused. */
    a[0]=1;n=send(a,2,out);OK(n==3 && out[2]==1);
    /* Legacy v3 read still returns <=128 rows; no silent overflow. */
    a[0]=5;a[2]=127;a[3]=1;n=send(a,4,out);OK(n==10 && out[2]==0);
    a[2]=128;n=send(a,4,out);OK(n==3 && out[2]==1);
    OK(ab_snapshot_validate(&dz_model)==0);
    puts("PASS Stage3: 256-row MIDI, 14-bit offsets, full step data, atomic validation, legacy and overflow guards");
    return 0;
}
