/* analogBEAST 1.1 device-UI simulator regression tests.
 * Host-only: web/emu/felucca_web.c simulates FM-1 and flash in memory.
 * Compile after ./build.sh with -DGOLANA=1; never writes physical flash.
 */
#include <stdio.h>
#include <string.h>
#include "../web/emu/felucca_web.c"
#ifdef QA3_EMBED
#define main qa2_controls_main
#endif

static int failures, checks;
static void ck(int cond, const char *name)
{
    checks++;
    printf("%s %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}
static void render_ms(unsigned ms)
{
    unsigned n=(unsigned)(ms * 44.1+0.5);
    while(n){
        unsigned m=n>128 ? 128:n;
        web_render(m);
        n-=m;
    }
}
static void key_press(unsigned physical)
{
    web_keys(1u<<physical);
    render_ms(55);
    web_keys(0);
    render_ms(55);
}
static int physical_key(int black, unsigned place)
{
    unsigned k;
    for(k=0;k<27;k++) if(key_black(k)==black && key_place(k)==place) return (int)k;
    return -1;
}
static int play_page(void)
{
    unsigned p;
    for(p=0;p<GU_NPAGE;p++)
        if(GU_PAGES[p].k[0].id==GV_V5_SELECT) return (int)p;
    return -1;
}
int main(void)
{
    int pg, kb, kw;
    track_t *t;
    step_t backup[NSTEP];
    int16_t len,div;
    unsigned prior;

    web_nor_erase();
    web_boot();
    render_ms(1600);
    key_press(12); /* dismiss startup animation, as tests/golana_sim.c */
    render_ms(250);
    ck(gu.ready, "simulator booted");
    if(!gu.ready) return 1;
    pg=play_page();
    kb=physical_key(1,1); /* second black key => Bank B */
    kw=physical_key(0,2); /* third white key => Pattern 03 */
    ck(pg>=0 && kb>=0 && kw>=0, "PLAY page and keyboard positions located");
    if(pg<0 || kb<0 || kw<0)return 1;
    t=gu_t();

    /* A physical keyboard selection is a control input, not a played note.
     * Check the actual audio/render path, not only a source-code string. */
    gu_go((unsigned)pg);
    ck(ab5_live_next_keys!=0, "PLAY keyboard selection enabled");
    ab5_model.bank=0;
    ab5_model.pattern=0;
    ab5_switch_request=0;
    key_press((unsigned)kb);
    ck(ab5_model.bank==1, "black key chooses Bank B");
    ck(!song.playing && t->nheld==0 && t->seq_n==0,
       "bank key does not start transport or leave sounding notes");
    key_press((unsigned)kw);
    ck(ab5_model.pattern==2, "white key chooses Pattern 03");
    ck(ab5_switch_request==0, "when stopped, pattern choice is immediate selection, not NEXT");
    ck(t->nheld==0 && t->seq_n==0, "pattern key does not play a note");
    fm1_in.notes=0;
    gu_leds();
    ck(web_lit_keys()==0, "no bank/pattern selection LEDs");

    /* New global tempo knob must work on a non-HOME page (real input). */
    gu_go(1); /* OSC page, not HOME */
    prior=(unsigned)song.g[G_BPM];
    web_enc(EN_ALGO,+1);
    gu_input();
    ck((unsigned)song.g[G_BPM]>prior, "ALGORITHM changes TEMPO outside HOME");

    /* A live change must preserve the entire sequencer, clock and play flags.
     * Exercise gu_load(), not just direct parameter copying. */
    memcpy(backup,t->step,sizeof backup);
    len=t->p[P_SLEN];div=t->p[P_SDIV];
    prior=(unsigned)gu.prog;
    song.playing=1;
    dz_play_has_backup=1;
    ab5_play_active=1;
    ab5_play.running=1;
    ab5_play.mode=AB5_MODE_PATTERN;
    gu_load((prior+1u)%GL_NPROG);
    ck(gu.prog==(prior+1u)%GL_NPROG,
       "physical PRESET loading is allowed during PLAY");
    ck(song.playing && dz_play_has_backup && ab5_play_active,
       "PRESET change does not restart/stop transport");
    ck(memcmp(backup,t->step,sizeof backup)==0 &&
       t->p[P_SLEN]==len && t->p[P_SDIV]==div,
       "PRESET change preserves pattern and timing");

    /* Queue NEXT through the actual hardware key, without waiting a bar.
     * The ISR consumes the request only at the pattern boundary. */
    gu_go((unsigned)pg);
    ab5_model.bank=1;
    ab5_model.pattern=0;
    ab5_play.bank=1;
    ab5_play.pattern=0;
    ab5_switch_request=0;
    dz_model.patterns[dz_seq_pat_index(1u,2u)].length=16u;
    ck(ab5_pattern_length(&ab5_model,&dz_model,1u,2u)>0,
       "NEXT target B03 has a playable pattern length");
    web_keys(1u<<(unsigned)kw);
    gu_input(); /* UI records the key; do not advance the audio clock */
    ck(ab5_switch_request==AB5_REQ(AB5_REQ_PATTERN,1u,2u),
       "white key queues B03 as NEXT on active PATTERN transport");
    ck(ab5_play.bank==1u && ab5_play.pattern==0u,
       "NEXT queue does not switch the sounding pattern immediately");
    web_keys(0);
    gu_input();

    /* LED test injects sounding sequencer notes, as seen by gu_leds.
     * Display maps MIDI F3 (53) through G5 (79) onto the 27 keys. */
    fm1_in.notes=0;
    t->arp_note=0;
    t->seq_n=1;
    t->seq_notes[0]=60; /* C4 = physical key 7 */
    song.octave=0;
    gu_leds();
    ck((web_lit_keys() & (1u<<7))!=0,
       "sounding C4 illuminates the correct physical key");
    t->seq_notes[0]=72;
    gu_leds();
    ck((web_lit_keys() & (1u<<19))!=0 && (web_lit_keys() & (1u<<7))==0,
       "sounding C5 changes LED pitch instead of leaving old key on");
    t->seq_n=0;
    gu_leds();
    ck(web_lit_keys()==0, "NOTE OFF clears keyboard LEDs");

    printf("analogBEAST 1.1 UI QA: %d/%d passed, %d failed\n",
           checks-failures,checks,failures);
    return failures?1:0;
}
