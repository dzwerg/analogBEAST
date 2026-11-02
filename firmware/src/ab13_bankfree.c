#include "ab13_bankfree.h"
/* Freestanding; no heap, C library dependencies, or XIP-unsafe flash calls. */
static uint16_t ab13_rd16(const uint8_t*b){return (uint16_t)((uint16_t)b[0]|((uint16_t)b[1]<<8));}
static int16_t ab13_rd_i16(const uint8_t*b){return (int16_t)ab13_rd16(b);}
static void ab13_zero(void*b,uint32_t len){uint8_t*p=(uint8_t*)b;while(len--)*p++=0u;}
static int ab13_hdr(const uint8_t*b,uint32_t n,uint8_t type){return n>=8u&&b[0]==type&&b[1]=='1'&&b[2]=='3'&&b[3]=='!';}
void ab13_pattern_init(ab13_pattern*p,uint8_t len,uint8_t res){uint32_t i;if(!p)return;ab13_zero(p,sizeof *p);p->length=len;p->resolution=res;for(i=0;i<128u;i++){p->steps[i].velocity=96u;p->steps[i].gate=64u;}}
int ab13_pattern_check(const ab13_pattern*p){uint32_t i;if(!p||p->length<8u||p->length>128u||p->resolution>9u)return -1;
 for(i=0;i<p->length;i++){const ab13_step*s=&p->steps[i];if(s->note>127u||(s->flags&0x60u)||s->velocity>127u||s->gate>127u||s->swing>100u)return -2;
 if((s->flags&AB13_STEP_PRESENT)&&(!s->velocity||!s->gate))return -3;}
 return 0;}
int ab13_song_check(const ab13_song*s){uint32_t i;if(!s||!s->count||s->count>256u||s->loop>1u)return -1;
 for(i=0;i<s->count;i++){const ab13_row*r=&s->row[i];if(r->flags)return -2;switch(r->op){
 case AB13_OP_PATTERN:if(!r->pattern_id||!r->steps||r->steps>128u)return -3;break;
 case AB13_OP_END:if(r->pattern_id||r->steps||r->value)return -4;break;
 case AB13_OP_TEMPO: {
     if(r->pattern_id||r->steps||(r->value!=0 && (r->value<40||r->value>240)))return -5;
     break;} 
 case AB13_OP_REST:if(r->pattern_id||!r->steps||r->steps>128u)return -6;break;
 case AB13_OP_PRESET:if(r->pattern_id||r->steps||r->value<1||r->value>32)return -7;break;
 case AB13_OP_ARP:if(r->pattern_id||r->steps||(r->value!=0&&r->value!=1))return -8;break;
 case AB13_OP_TRANSPOSE:if(r->pattern_id||r->steps||r->value< -24||r->value>24)return -9;break;
 case AB13_OP_BEGIN:if(r->pattern_id||r->steps||r->value)return -11;break;
 default:return -10;}}
 return 0;}
int ab13_album_check(const ab13_album*a){uint32_t i;if(!a||a->count>32u||a->loop>1u)return -1;
 for(i=0;i<a->count;i++){uint16_t ref=a->song_id[i];if(!ref)return -2;
  if(AB13_ALBUM_IS_PRESET(ref) && (!(ref&AB13_ALBUM_PRESET_VALUE_MASK)||(ref&AB13_ALBUM_PRESET_VALUE_MASK)>32u))return -3;}
 return 0;}
/* SONG LOOP re-enters after the last BEGIN before END, but only if playable material follows.
 * Album playback never invokes this rewind: each album loop starts at step 1. */
uint16_t ab13_song_loop_row(const ab13_song*s){
 uint16_t i,begin=0u;uint8_t playable_after=0u;
 if(!s)return 0u;
 for(i=0u;i<s->count && i<AB13_MAX_ROWS;i++){
  uint8_t op=s->row[i].op;
  if(op==AB13_OP_END)break;
  if(op==AB13_OP_BEGIN){begin=(uint16_t)(i+1u);playable_after=0u;}
  else if(op==AB13_OP_PATTERN||op==AB13_OP_REST)playable_after=1u;
 }
 return begin && playable_after?begin:0u;
}
uint32_t ab13_pattern_size(const ab13_pattern*p){return !ab13_pattern_check(p)?8u+5u*p->length:0u;}
uint32_t ab13_song_size(const ab13_song*s){return !ab13_song_check(s)?8u+8u*s->count:0u;}
uint32_t ab13_album_size(const ab13_album*a){return !ab13_album_check(a)?8u+2u*a->count:0u;}
static int ab13_emit_pattern_byte(const ab13_pattern*p,uint32_t at,uint8_t*v){const ab13_step*s;uint32_t i;
 if(at<4){*v=(uint8_t)"P13!"[at];return 0;}if(at==4){*v=p->length;return 0;}if(at==5){*v=p->resolution;return 0;}if(at<8){*v=0u;return 0;}
 at-=8u;s=&p->steps[at/5u];i=at%5u;*v=i==0?s->note:i==1?s->flags:i==2?s->velocity:i==3?s->gate:s->swing;return 0;}
static int ab13_emit_song_byte(const ab13_song*p,uint32_t at,uint8_t*v){const ab13_row*r;uint32_t k;
 if(at<4){*v=(uint8_t)"S13!"[at];return 0;}if(at==4||at==5){*v=(uint8_t)(p->count>>(8u*(at-4u)));return 0;}if(at==6){*v=p->loop;return 0;}if(at==7){*v=0;return 0;}
 at-=8u;r=&p->row[at/8u];k=at%8u;*v=k==0?(uint8_t)r->pattern_id:k==1?(uint8_t)(r->pattern_id>>8):k==2?(uint8_t)r->steps:k==3?(uint8_t)(r->steps>>8):k==4?(uint8_t)r->value:k==5?(uint8_t)((uint16_t)r->value>>8):k==6?r->op:r->flags;return 0;}
static int ab13_emit_album_byte(const ab13_album*p,uint32_t at,uint8_t*v){uint16_t x;
 if(at<4){*v=(uint8_t)"A13!"[at];return 0;}if(at==4){*v=p->count;return 0;}if(at==5){*v=p->loop;return 0;}if(at<8){*v=0;return 0;}
 at-=8u;x=p->song_id[at/2u];*v=(uint8_t)(x>>(8u*(at&1u)));return 0;}
int ab13_pattern_emit(void*ctx,uint32_t off,void*dst,uint32_t n){const ab13_pattern*p=(const ab13_pattern*)ctx;uint32_t i,size=ab13_pattern_size(p);uint8_t*b=(uint8_t*)dst;if(!size||!dst||off>size||n>size-off)return -1;for(i=0;i<n;i++)ab13_emit_pattern_byte(p,off+i,b+i);return 0;}
int ab13_song_emit(void*ctx,uint32_t off,void*dst,uint32_t n){const ab13_song*p=(const ab13_song*)ctx;uint32_t i,size=ab13_song_size(p);uint8_t*b=(uint8_t*)dst;if(!size||!dst||off>size||n>size-off)return -1;for(i=0;i<n;i++)ab13_emit_song_byte(p,off+i,b+i);return 0;}
int ab13_album_emit(void*ctx,uint32_t off,void*dst,uint32_t n){const ab13_album*p=(const ab13_album*)ctx;uint32_t i,size=ab13_album_size(p);uint8_t*b=(uint8_t*)dst;if(!size||!dst||off>size||n>size-off)return -1;for(i=0;i<n;i++)ab13_emit_album_byte(p,off+i,b+i);return 0;}
int ab13_pattern_parse(ab13_pattern*p,const uint8_t*b,uint32_t n){uint32_t i;if(!p||!b||!ab13_hdr(b,n,'P')||b[4]<8u||b[4]>128u||b[5]>9u||b[6]||b[7]||n!=8u+5u*b[4])return -1;
 ab13_pattern_init(p,b[4],b[5]);for(i=0;i<p->length;i++){ab13_step*s=&p->steps[i];const uint8_t*c=b+8u+5u*i;s->note=c[0];s->flags=c[1];s->velocity=c[2];s->gate=c[3];s->swing=c[4];}return ab13_pattern_check(p);}
int ab13_song_parse(ab13_song*p,const uint8_t*b,uint32_t n){uint32_t i;uint16_t count;if(!p||!b||!ab13_hdr(b,n,'S'))return -1;count=ab13_rd16(b+4);if(!count||count>256u||b[6]>1u||b[7]||n!=8u+8u*count)return -2;
 ab13_zero(p,sizeof *p);p->count=count;p->loop=b[6];for(i=0;i<count;i++){const uint8_t*c=b+8u+8u*i;ab13_row*r=&p->row[i];r->pattern_id=ab13_rd16(c);r->steps=ab13_rd16(c+2);r->value=ab13_rd_i16(c+4);r->op=c[6];r->flags=c[7];}return ab13_song_check(p);}
int ab13_album_parse(ab13_album*p,const uint8_t*b,uint32_t n){uint32_t i;if(!p||!b||!ab13_hdr(b,n,'A')||b[4]>32u||b[5]>1u||b[6]||b[7]||n!=8u+2u*b[4])return -1;
 ab13_zero(p,sizeof *p);p->count=b[4];p->loop=b[5];for(i=0;i<p->count;i++)p->song_id[i]=ab13_rd16(b+8u+2u*i);return ab13_album_check(p);}
