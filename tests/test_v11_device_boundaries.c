/* QA3 integration: reuse the 19/19 verified QA2 FM-1 simulation fixture.
 * All hardware is represented by web/emu/felucca_web.c; flash is in RAM.
 */
#define QA3_EMBED 1
#define main qa2_controls_main
#include "test_v11_controls.c"
#undef main

static int qa3_checks, qa3_fails;
#define Q3(cond, title) do { \
    int ok = !!(cond); ++qa3_checks; \
    printf("%s QA3: %s\n", ok ? "PASS" : "FAIL", title); \
    if (!ok) ++qa3_fails; \
} while(0)

int main(void)
{
    int qa2_result=qa2_controls_main();
    track_t *t;
    dz_pattern_t *target;
    uint32_t req;
    int changed;
    if(qa2_result) {
        puts("FAIL QA3: original QA2 fixture did not pass; aborting stateful checks");
        return 1;
    }
    t=gu_t();
    target=&dz_model.patterns[dz_seq_pat_index(1u,2u)];
    target->length=16u;
    target->step[0].note=72u;
    target->step[0].velocity=96u;
    target->step[0].gate=64u;

    ab5_play.pattern_loop=1u; /* QA2 set a synthetic transport: enable its LOOP policy explicitly */
    req=AB5_REQ(AB5_REQ_PATTERN,1u,2u);
    Q3(ab5_switch_request==req && ab5_play_active && song.playing,
       "B03 queued while PATTERN transport is running");
    Q3(ab5_play.bank==1u && ab5_play.pattern==0u,
       "playing B01 not replaced before next pattern boundary");

    /* Simulate exactly the ISR callback that runs on a completed pattern.
     * seq_release() emits NOTE OFF for every sounding sequence note, then
     * gu_dz_play_pattern() installs the newly selected pattern. */
    t->seq_n=1u;
    t->seq_notes[0]=60u;
    t->seq_hold=1u;
    changed=gu_song_next();
    Q3(changed==1 && ab5_play.bank==1u && ab5_play.pattern==2u,
       "boundary callback actually switches playing B01 -> B03");
    Q3(ab5_switch_request==0u,
       "accepted NEXT request cleared after boundary");
    Q3(t->seq_n==0u && t->seq_hold==0u,
       "boundary releases all previously sounding sequencer notes");
    Q3(t->p[P_SLEN]==16 && t->step[0].time==ST_NOTE &&
       t->step[0].note[0]==72u,
       "new B03 notes and length loaded into active playback buffer");
    Q3(song.playing==1u && ab5_play.running,
       "pattern switch does not stop transport");
    Q3(gu_song_next() && ab5_play.bank==1u && ab5_play.pattern==2u,
       "following boundary stays B03 when NEXT queue is empty");

    /* Stop/transport completion must never leak a pending NEXT or pitch. */
    ab5_switch_request=AB5_REQ(AB5_REQ_PATTERN,0u,1u);
    ab5_live_transpose=3;
    ab5_song_transpose=2;
    ab5_song_octave=1;
    gu_dz_playback_end();
    Q3(!dz_play_has_backup && !ab5_play_active && !ab5_play.running,
       "STOP clears active V5 transport");
    Q3(ab5_switch_request==0u && ab5_live_transpose==0 &&
       ab5_song_transpose==0 && ab5_song_octave==0,
       "STOP clears pending NEXT and all song/live pitch offsets");

    printf("QA3 DEVICE: %d/%d passed, %d failed (plus QA2 19/19)\n",
           qa3_checks-qa3_fails,qa3_checks,qa3_fails);
    return qa3_fails?1:0;
}
