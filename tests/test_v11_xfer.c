/* QA4: real analogBEAST 1.1 SONG/ALBUM SysEx 96 payload parser.
 * No hardware, no flash writes: source under test is included verbatim. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "analogbeast_v5_extra.h"
#include "../firmware/src/analogbeast_v5_transfer.c"
/* v4 migration is not part of this parser test. */
int ab_snapshot_validate(const ab_snapshot_t *p) { return p ? 0 : -1; }
static int tests;
#define T(c,msg) do { ++tests; if(!(c)){fprintf(stderr,"FAIL %s line %d\n",msg,__LINE__);return 1;}else printf("PASS %s\n",msg); }while(0)
static ab5_extra_t m,old;
static uint8_t payload[128],reply[128],data[128];
static void encode(unsigned i,unsigned b,unsigned p,unsigned st,unsigned val){
  unsigned j=3u+5u*i;
  payload[j]=(uint8_t)b;payload[j+1]=(uint8_t)p;payload[j+2]=(uint8_t)st;
  payload[j+3]=(uint8_t)(val&127u);payload[j+4]=(uint8_t)(val>>7);
}
int main(void){
  unsigned k;int rc;
  const unsigned bc[16]={0,8,9,10,11,12,13,14,15,16,17,18,0,9,13,14};
  const unsigned va[16]={0,0,200,31,0,1,6,48,5,12,13,0,0,40,0,0};
  ab5_extra_init(&m);
  T(sizeof(ab_song_row_t)==4,"v1.0 four-byte SONG records retained");
  T(sizeof(ab5_extra_t)==13648,"v1.0 13648-byte backup layout retained");
  T(ab5_extra_validate(&m)==0,"default model valid");
  m.extra_patterns[1].steps[0].note=91u;
  memcpy(&old,&m,sizeof m);
  payload[0]=0;payload[1]=0;payload[2]=16;
  for(k=0;k<16;k++)encode(k,bc[k],bc[k]==8?16:0,bc[k]==0?16:1,va[k]);
  rc=ab5_xfer_exec(&m,23,payload,83,reply);
  T(rc==3 && reply[0]==0 && reply[1]==0 && reply[2]==16,"op23 writes mixed old/new 1.1 rows");
  T(m.songs[0].rows[2].value==200 && m.songs[0].rows[7].value==48,"8-bit command values retained");
  T(m.extra_patterns[1].steps[0].note==91,"SONG transfer leaves E-H patterns alone");
  T(ab5_extra_validate(&m)==0,"written 1.1 SONG validates");
  rc=ab5_xfer_exec(&m,24,payload,3,reply);
  T(rc==83 && !memcmp(reply,payload,83),"op24 exact readback all 16 rows including 200 BPM");
  T(reply[3+2*5+3]==72 && reply[3+2*5+4]==1,"8-bit TEMPO 200 encoded as 7-bit 72/1");
  data[0]=0;data[1]=16;data[2]=1;
  T(ab5_xfer_exec(&m,27,data,3,reply)==3,"op27 SONG metadata write");
  T(ab5_xfer_exec(&m,28,data,1,reply)==3 && reply[1]==16 && reply[2]==1,"op28 SONG metadata readback");
  /* Reject a late invalid row: entire chunk must remain unchanged. */
  memcpy(&old,&m,sizeof m);
  encode(15,16,0,1,0); /* JUMP=0 invalid */
  T(ab5_xfer_exec(&m,23,payload,83,reply)<0,"JUMP zero rejected by real parser");
  T(!memcmp(&old,&m,sizeof m),"invalid last row causes zero partial mutations");
  encode(15,14,0,1,0); /* restore valid */
  payload[3+15*5+4]=2; /* high byte invalid; val=256 */
  T(ab5_xfer_exec(&m,23,payload,83,reply)<0,"more-than-eight-bit value rejected");
  T(!memcmp(&old,&m,sizeof m),"invalid 8-bit encoding does not mutate model");
  encode(15,14,0,1,0);
  /* Destination copy, then self-overlap: requires memmove semantics. */
  data[0]=0;data[1]=0;data[2]=1;data[3]=0;data[4]=16;
  T(ab5_xfer_exec(&m,29,data,5,reply)==5,"op29 SONG copy 1 -> 2");
  T(!memcmp(m.songs[0].rows,m.songs[1].rows,16*sizeof(ab_song_row_t)),"SONG copy preserves 1.1 opcodes");
  data[0]=1;data[1]=0;data[2]=1;data[3]=2;data[4]=14;
  T(ab5_xfer_exec(&m,29,data,5,reply)==5,"op29 overlapping SONG copy accepted");
  T(!memcmp(m.songs[1].rows+2,m.songs[0].rows,14*sizeof(ab_song_row_t)),"overlapping SONG copy is exact");
  /* ALBUM operations preserve 127 wire marker for empty slots. */
  data[0]=0;data[1]=2;data[2]=1;
  for(k=0;k<32;k++)data[3+k]=(uint8_t)(k<2?k:127);
  T(ab5_xfer_exec(&m,25,data,35,reply)==3,"op25 ALBUM write");
  T(ab5_xfer_exec(&m,26,data,1,reply)==35 && !memcmp(reply,data,35),"op26 ALBUM exact readback");
  T(ab5_extra_validate(&m)==0,"model valid after mixed SONG/ALBUM transfer");
  printf("QA4 C parser: %d checks PASSED\n",tests);
  return 0;
}
