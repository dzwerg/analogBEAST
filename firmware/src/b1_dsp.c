/* Original B1-style approximation for FM-1 development; NOT a circuit-exact emulator.
   Controls are Q15-ish 0..32767. No libm, no heap, no global DSP state. */
#include "b1_dsp.h"
#include <string.h>
static int32_t clamp16(int32_t x) {return x>32767?32767:x< -32768?-32768:x;}
void ab_b1_reset(ab_b1_state *s) {memset(s,0,sizeof(*s));}
int16_t ab_b1_tick(ab_b1_state *s, const ab_b1_params *p) {
  if(p->slide) {int32_t delta=(int32_t)(s->slide_target-s->inc); s->inc+=(uint32_t)(delta/64);}
  else s->inc=s->slide_target;
  s->phase+=s->inc;
  int32_t osc=p->square ? ((s->phase&0x80000000u)?-22000:22000)
                         : (((int32_t)(s->phase>>16)-32768)*2/3);
  int32_t target=p->gate ? (p->accent?32767:24576):0;
  int32_t step=p->decay+64;
  s->env+=(target-s->env)/(int32_t)(1+((32767-step)>>7));
  int32_t x=(osc*s->env)>>15;
  /* Two-integrator resonant low pass; coefficients conservative for stability. */
  int32_t f=256+(p->cutoff>>5);
  int32_t r=512+(p->resonance>>5);
  s->filter_bp += ((f*(x-s->filter_lp))>>13)-((r*s->filter_bp)>>13);
  s->filter_bp=clamp16(s->filter_bp);
  s->filter_lp+= (f*s->filter_bp)>>13;
  s->filter_lp=clamp16(s->filter_lp);
  x=s->filter_lp;
  /* bounded soft saturation */
  x=clamp16(x+((int32_t)p->drive*x>>16));
  x=(x*32768)/(32768+(x<0?-x:x));
  int32_t delayed=s->delay[s->delay_pos];
  int32_t wet=(delayed*p->delay_mix)>>15;
  int32_t fb=(delayed*p->feedback)>>15;
  s->delay[s->delay_pos]=clamp16(x+fb);
  s->delay_pos=(uint16_t)((s->delay_pos+1u)&255u);
  return (int16_t)clamp16(x+wet);
}
