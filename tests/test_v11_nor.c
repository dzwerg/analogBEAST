/* QA3: reuse complete v5 NOR regression, then run v1.1-opcode persistence.
 * Only simulated RAM NOR. No physical flash writes.
 */
#define main qa3_prior_nor_main
#include "test_analogbeast_v5_nor.c"
#undef main

int main(void)
{
    unsigned i;
    const uint8_t commands[] = {AB5_CMD_OCTAVE, AB5_CMD_TRANSPOSE,
        AB5_CMD_REPEAT, AB5_CMD_JUMP, AB5_CMD_ENDSTEP, AB5_CMD_BEGIN};
    const uint8_t vals[] = {4, 20, 5, 12, 13, 0};
    int rc = qa3_prior_nor_main();
    CHECK(rc == 0);
    fresh();
    model.songs[0].count = 8;
    model.songs[0].rows[0] = (ab_song_row_t){0,0,64,0};
    for(i=0;i<6;i++){
        model.songs[0].rows[i+1] = (ab_song_row_t){commands[i],0,1,vals[i]};
    }
    model.songs[0].rows[7] = (ab_song_row_t){AB5_CMD_END,AB5_CMD_END_PATTERN,1,0};
    CHECK(ab5_extra_validate(&model)==0);
    CHECK(ab5_nor_save(&io,&s,model_get,&model)==0);
    must_load_match();
    puts("PASS QA3: v1.1 SONG commands persist and reload over simulated NOR A");
    model.songs[0].rows[1].value = 2;
    model.songs[0].rows[3].value = 7;
    CHECK(ab5_nor_save(&io,&s,model_get,&model)==0);
    must_load_match();
    puts("PASS QA3: v1.1 SONG command edits persist across A/B generation switch");
    CHECK(flashmem[0xC8000u] == 0xFFu);
    puts("PASS QA3: third user sample header not overwritten");
    return 0;
}
