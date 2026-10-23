/* analogBEAST 1.1 runtime acceptance tests. Run only on host, never on the device.
 * CC example:
 * cc -std=c11 -O2 -Ifirmware/src \
 *   tests/test_v11_runtime.c firmware/src/analogbeast_v5_extra.c \
 *   firmware/src/analogbeast_v5_controls.c firmware/src/analogbeast_v5_runtime.c \
 *   -o build/test_v11_runtime && build/test_v11_runtime
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "analogbeast_v5_runtime.h"
#include "analogbeast_v5_controls.h"

/* Boot/flash are intentionally NOT used; host tests provide harmless stubs. */
int ab_snapshot_validate(const ab_snapshot_t *m) { return m ? 0 : -1; }
int ab5_nor_load(const ab_nor_io_t *io,ab5_nor_state_t *state,
                 ab_write_snapshot_fn put,void *ctx)
{ (void)io; (void)state; (void)put; (void)ctx; return -1; }
static ab_snapshot_t v4;
static ab5_extra_t m;
static ab5_play_t play;

static ab_song_row_t row(unsigned kind,unsigned pat,unsigned steps,unsigned value)
{
    ab_song_row_t r;
    r.bank=(uint8_t)kind;r.pattern=(uint8_t)pat;
    r.play_steps=(uint8_t)steps;r.value=(uint8_t)value;
    return r;
}
static void prep(void)
{
    unsigned p;
    memset(&v4,0,sizeof v4);
    ab5_extra_init(&m);
    for(p=0;p<AB_PATTERNS;p++)v4.patterns[p].length=64;
    m.play_mode=AB5_MODE_SONG;m.selected_song=0;
}
static void assert_pattern(unsigned pattern,unsigned start,unsigned steps)
{
    assert(play.running && !play.rest);
    assert(play.bank==0 && play.pattern==pattern);
    assert(play.step_start==start && play.play_steps==steps);
}
static void test_slice_repeat(unsigned order)
{
    unsigned i;
    prep();
    m.songs[0].count=7;
    m.songs[0].rows[0]=row(0,0,64,0);         /* A01 */
    if(order==0){
        m.songs[0].rows[1]=row(AB5_CMD_REPEAT,0,1,5);
        m.songs[0].rows[2]=row(AB5_CMD_JUMP,0,1,12);
        m.songs[0].rows[3]=row(AB5_CMD_ENDSTEP,0,1,13);
    }else{
        m.songs[0].rows[1]=row(AB5_CMD_JUMP,0,1,12);
        m.songs[0].rows[2]=row(AB5_CMD_ENDSTEP,0,1,13);
        m.songs[0].rows[3]=row(AB5_CMD_REPEAT,0,1,5);
    }
    m.songs[0].rows[4]=row(0,1,64,0);       /* A02 */
    m.songs[0].rows[5]=row(0,2,64,0);       /* A03 */
    m.songs[0].rows[6]=row(AB5_CMD_END,AB5_CMD_END_PATTERN,1,0);
    assert(ab5_extra_validate(&m)==0);
    assert(ab5_play_start(&m,&v4,&play));
    assert_pattern(0,0,64);
    for(i=0;i<5;i++){
        assert(ab5_play_next(&m,&v4,&play));
        assert_pattern(1,11,2);             /* A02, Steps 12 and 13 */
    }
    assert(ab5_play_next(&m,&v4,&play));
    assert_pattern(2,0,64);                 /* A03 complete */
    assert(!ab5_play_next(&m,&v4,&play));
}
static void test_begin_loop(void)
{
    prep();
    m.songs[0].loop=0; /* BEGIN loops even when SONG ONCE */
    m.songs[0].count=7;
    m.songs[0].rows[0]=row(AB5_CMD_PRESET,0,1,3);
    m.songs[0].rows[1]=row(AB5_CMD_TEMPO,0,1,120);
    m.songs[0].rows[2]=row(AB5_CMD_BEGIN,0,1,0);
    m.songs[0].rows[3]=row(AB5_CMD_PRESET,0,1,8);
    m.songs[0].rows[4]=row(AB5_CMD_TEMPO,0,1,130);
    m.songs[0].rows[5]=row(0,0,64,0);
    m.songs[0].rows[6]=row(AB5_CMD_END,AB5_CMD_END_PATTERN,1,0);
    assert(ab5_extra_validate(&m)==0);
    assert(ab5_play_start(&m,&v4,&play));
    assert_pattern(0,0,64);
    assert(play.tempo==130 && play.preset==8);
    assert(play.tempo_changed && play.preset_changed);
    assert(ab5_play_next(&m,&v4,&play));
    assert_pattern(0,0,64);
    assert(play.begin_repeat);
    assert(play.tempo==130 && play.preset==8);
    assert(!play.tempo_changed && !play.preset_changed);
    assert(ab5_play_next(&m,&v4,&play));
}
static void test_commands_ui(void)
{
    prep();
    assert(ab5_ui_set(&m,AB5_UI_KIND,AB5_CMD_TRANSPOSE)==2);
    assert(ab5_ui_set(&m,AB5_UI_VALUE,-24)==2);
    assert(m.songs[0].rows[0].value==0);
    assert(ab5_ui_get(&m,AB5_UI_VALUE)==-24);
    assert(ab5_ui_set(&m,AB5_UI_VALUE,+12)==2);
    assert(m.songs[0].rows[0].value==36);
    assert(ab5_ui_set(&m,AB5_UI_KIND,AB5_CMD_OCTAVE)==2);
    assert(ab5_ui_set(&m,AB5_UI_VALUE,-2)==2);
    assert(m.songs[0].rows[0].value==1);
    assert(ab5_ui_set(&m,AB5_UI_KIND,AB5_CMD_REPEAT)==2);
    assert(ab5_ui_set(&m,AB5_UI_VALUE,5)==2);
    assert(m.songs[0].rows[0].value==5);
}
static void test_octave_transpose(void)
{
    prep();
    m.songs[0].count=4;
    m.songs[0].rows[0]=row(AB5_CMD_OCTAVE,0,1,4);    /* +1 octave */
    m.songs[0].rows[1]=row(AB5_CMD_TRANSPOSE,0,1,22); /* -2 semitones */
    m.songs[0].rows[2]=row(0,0,64,0);
    m.songs[0].rows[3]=row(AB5_CMD_END,AB5_CMD_END_PATTERN,1,0);
    assert(ab5_extra_validate(&m)==0);
    assert(ab5_play_start(&m,&v4,&play));
    assert_pattern(0,0,64);
    assert(play.octave==1 && play.transpose==-2); /* +10 total */
}
static void test_album_does_not_begin_loop(void)
{
    prep();
    m.play_mode=AB5_MODE_ALBUM;
    m.albums[0].count=1;m.albums[0].song_ids[0]=0;
    m.songs[0].count=3;
    m.songs[0].rows[0]=row(AB5_CMD_BEGIN,0,1,0);
    m.songs[0].rows[1]=row(0,0,64,0);
    m.songs[0].rows[2]=row(AB5_CMD_END,AB5_CMD_END_PATTERN,1,0);
    assert(ab5_extra_validate(&m)==0);
    assert(ab5_play_start(&m,&v4,&play));
    assert_pattern(0,0,64);
    assert(!ab5_play_next(&m,&v4,&play));
}
int main(void)
{
    test_slice_repeat(0);
    test_slice_repeat(1);
    test_begin_loop();
    test_commands_ui();
    test_octave_transpose();
    test_album_does_not_begin_loop();
    puts("PASS analogBEAST 1.1: REPEAT/JUMP/ENDSTEP, BEGIN, signed values, ALBUM separation");
    return 0;
}
