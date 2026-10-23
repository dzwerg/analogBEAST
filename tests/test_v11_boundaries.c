/* analogBEAST 1.1 QA3: playback boundaries and backward-compatible wire model.
 * Host only, no real NOR or hardware. 2026-10-23.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "analogbeast_v5_runtime.h"

#define CK(test, label) do { \
    if (!(test)) { fprintf(stderr, "FAIL [%d]: %s\n", __LINE__, label); exit(1); } \
    ++checks; printf("PASS %s\n", label); \
} while(0)

/* Runtime's preview dependencies are not invoked by these RAM-only tests. */
int ab_snapshot_validate(const ab_snapshot_t *v) { return v ? 0 : -1; }
int ab5_nor_load(const ab_nor_io_t *io, ab5_nor_state_t *st,
                 ab_write_snapshot_fn put, void *ctx)
{ (void)io; (void)st; (void)put; (void)ctx; return -1; }

static int checks;
static ab5_extra_t model, copy;
static ab_snapshot_t legacy;
static ab5_play_t p;
static uint8_t wire[AB5_EXTRA_WIRE_BYTES];

static ab_song_row_t row(unsigned bank, unsigned pattern, unsigned steps, unsigned value)
{
    ab_song_row_t r = {(uint8_t)bank, (uint8_t)pattern, (uint8_t)steps, (uint8_t)value};
    return r;
}
static void init(void)
{
    unsigned i;
    memset(&legacy, 0, sizeof legacy);
    ab5_extra_init(&model);
    for (i=0; i<AB_PATTERNS; ++i) legacy.patterns[i].length = 64;
    legacy.patterns[0].step[0].note = 60;
    legacy.patterns[0].step[0].velocity = 96;
    legacy.patterns[0].step[0].gate = 64;
    legacy.patterns[1 * AB5_PAT_PER_BANK + 2].length = 5;
    legacy.patterns[1 * AB5_PAT_PER_BANK + 2].step[0].note = 72;
}
static void pattern_boundary(void)
{
    uint8_t consumed = 255;
    uint32_t req = AB5_REQ(AB5_REQ_PATTERN,1,2);
    init();
    model.play_mode=AB5_MODE_PATTERN;
    model.bank=0; model.pattern=0;
    CK(ab5_play_start(&model,&legacy,&p), "PATTERN A01 starts");
    CK(p.bank==0 && p.pattern==0 && p.play_steps==64, "A01 length 64 before NEXT");
    CK(p.bank==0 && p.pattern==0 && req!=0, "queued request has no immediate side effects");
    CK(ab5_play_next_request(&model,&legacy,&p,req,&consumed), "boundary accepts NEXT B03");
    CK(consumed==1 && p.bank==1 && p.pattern==2 && p.play_steps==5,
       "actual boundary moves A01 -> B03 with B03 length");
    consumed=255;
    CK(ab5_play_next_request(&model,&legacy,&p,0,&consumed), "next B03 boundary continues");
    CK(consumed==0 && p.bank==1 && p.pattern==2,
       "NEXT is not accidentally replayed on later boundaries");
    /* A nonexistent user-pattern bank/pattern must not consume the queue. */
    model.extra_patterns[0].length=16;
    consumed=255;
    CK(ab5_play_next_request(&model,&legacy,&p,AB5_REQ(AB5_REQ_SONG,0,1),&consumed),
       "wrong request type leaves PATTERN playing");
    CK(consumed==0 && p.pattern==2, "wrong-mode request not consumed");
    p.pattern_loop=0;
    consumed=255;
    CK(!ab5_play_next_request(&model,&legacy,&p,0,&consumed), "PATTERN ONCE stops at boundary");
    CK(p.running==0, "PATTERN ONCE clears running flag");
}
static void song_boundary(void)
{
    uint8_t consumed=255;
    init();
    model.play_mode=AB5_MODE_SONG;
    model.selected_song=0;
    model.songs[0].count=3;
    model.songs[0].rows[0]=row(AB5_CMD_TEMPO,0,1,112);
    model.songs[0].rows[1]=row(0,0,64,0);
    model.songs[0].rows[2]=row(AB5_CMD_END,AB5_CMD_END_PATTERN,1,0);
    model.songs[1].count=3;
    model.songs[1].rows[0]=row(AB5_CMD_PRESET,0,1,7);
    model.songs[1].rows[1]=row(0,1,64,0);
    model.songs[1].rows[2]=row(AB5_CMD_END,AB5_CMD_END_PATTERN,1,0);
    CK(ab5_extra_validate(&model)==0, "SONG 01 and 02 valid");
    CK(ab5_play_start(&model,&legacy,&p), "SONG 01 starts");
    CK(p.song_id==0 && p.pattern==0 && p.tempo==112 && p.tempo_changed,
       "SONG 01 tempo applied before its pattern");
    CK(ab5_play_next_request(&model,&legacy,&p,AB5_REQ(AB5_REQ_SONG,0,1),&consumed),
       "queued NEXT SONG taken at SONG END");
    CK(consumed==1 && p.song_id==1 && p.pattern==1,
       "NEXT SONG starts SONG 02 from its first full pattern");
    CK(p.preset_changed && p.preset==7 && !p.tempo_changed,
       "new SONG processes PRESET, does not inherit old TEMPO command flag");
    CK(!ab5_play_next_request(&model,&legacy,&p,0,&consumed),
       "SONG 02 ONCE ends with no unintended restart");
}
static void album_boundary(void)
{
    uint8_t consumed=255;
    init();
    model.play_mode=AB5_MODE_ALBUM;
    model.albums[0].count=2;
    model.albums[0].song_ids[0]=0;
    model.albums[0].song_ids[1]=1;
    model.songs[0].count=3;
    model.songs[0].rows[0]=row(AB5_CMD_BEGIN,0,1,0);
    model.songs[0].rows[1]=row(0,0,64,0);
    model.songs[0].rows[2]=row(AB5_CMD_END,AB5_CMD_END_PATTERN,1,0);
    model.songs[1].count=2;
    model.songs[1].rows[0]=row(0,1,64,0);
    model.songs[1].rows[1]=row(AB5_CMD_END,AB5_CMD_END_PATTERN,1,0);
    CK(ab5_extra_validate(&model)==0, "ALBUM fixtures valid");
    CK(ab5_play_start(&model,&legacy,&p), "ALBUM starts first song");
    CK(p.song_id==0 && p.pattern==0, "ALBUM slot 1 is SONG 01");
    CK(ab5_play_next_request(&model,&legacy,&p,0,&consumed),
       "ALBUM advances despite BEGIN marker");
    CK(p.song_id==1 && p.pattern==1 && p.album_pos==1,
       "BEGIN loop is disabled in ALBUM; next full song selected");
    CK(!ab5_play_next_request(&model,&legacy,&p,0,&consumed),
       "ALBUM ONCE stops after slot 2");
    model.albums[0].loop=1;
    CK(ab5_play_start(&model,&legacy,&p), "ALBUM LOOP restarts");
    CK(ab5_play_next(&model,&legacy,&p) && p.album_pos==1, "ALBUM LOOP reaches slot 2");
    CK(ab5_play_next(&model,&legacy,&p) && p.album_pos==0 && p.song_id==0,
       "ALBUM LOOP wraps to SONG 01 from beginning");
}
static void old_wire_and_new_wire(void)
{
    static const uint8_t old_cmds[] = {AB5_CMD_END, AB5_CMD_TEMPO, AB5_CMD_PRESET,
                                        AB5_CMD_REST, AB5_CMD_ARP};
    static const uint8_t new_cmds[] = {AB5_CMD_OCTAVE, AB5_CMD_TRANSPOSE,
                                        AB5_CMD_REPEAT, AB5_CMD_JUMP,
                                        AB5_CMD_ENDSTEP, AB5_CMD_BEGIN};
    const uint8_t new_vals[] = {4, 20, 5, 12, 13, 0};
    unsigned i;
    init();
    CK(sizeof(ab_song_row_t)==4 && sizeof(ab5_extra_t)==13648,
       "v1.0 four-byte song row and 13648-byte v5 wire size retained");
    model.songs[0].count=12;
    model.songs[0].rows[0]=row(0,0,64,0);
    for(i=0;i<5;i++) {
        unsigned val=0;
        if(old_cmds[i]==AB5_CMD_TEMPO)val=120;
        if(old_cmds[i]==AB5_CMD_PRESET)val=7;
        if(old_cmds[i]==AB5_CMD_ARP)val=1;
        model.songs[0].rows[i+1]=row(old_cmds[i],
            old_cmds[i]==AB5_CMD_END?AB5_CMD_END_PATTERN:0,1,val);
    }
    for(i=0;i<6;i++)model.songs[0].rows[i+6]=row(new_cmds[i],0,1,new_vals[i]);
    CK(ab5_extra_validate(&model)==0,"old and new opcodes coexist in valid song rows");
    CK(ab5_extra_get(&model,0,wire,sizeof wire)==0, "full 13648-byte wire encoding succeeds");
    CK(wire[0]==model.play_mode && wire[1]==model.bank,
       "existing leading wire fields not shifted");
    memset(&copy,0,sizeof copy);
    /* Deliberately use the same 256-byte chunks as the real NOR stream. */
    for(i=0;i<sizeof wire;i+=256u){
        unsigned n=(sizeof wire-i<256u)?sizeof wire-i:256u;
        CK(ab5_extra_put(&copy,i,wire+i,n)==0, "NOR chunk accepted");
    }
    CK(ab5_extra_validate(&copy)==0 && memcmp(&model,&copy,sizeof model)==0,
       "all old+new song commands survive exact wire roundtrip");
    CK(memcmp(model.songs[0].rows,copy.songs[0].rows,sizeof model.songs[0].rows)==0,
       "every 4-byte song row survives byte-for-byte");
    CK(legacy.patterns[0].step[0].note==60,
       "v4 A-D source pattern was not rewritten by v5 backup roundtrip");
    model.songs[0].rows[6].value=7; /* OCTAVE bias range 0..6 */
    CK(ab5_extra_validate(&model)!=0, "invalid new OCTAVE value rejected");
    model.songs[0].rows[6].value=4;
    model.songs[0].rows[9].value=0; /* JUMP must start at 1 */
    CK(ab5_extra_validate(&model)!=0, "invalid JUMP zero rejected");
}
int main(void)
{
    pattern_boundary();
    song_boundary();
    album_boundary();
    old_wire_and_new_wire();
    printf("QA3 PURE: %d checks passed, 0 failures\n", checks);
    return 0;
}
