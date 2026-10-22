/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v5 Stage 4a: bounded UI edits to the RAM-only v5 model.
 * This is NOT a flash or transport interface.
 */
#ifndef ANALOGBEAST_V5_CONTROLS_H
#define ANALOGBEAST_V5_CONTROLS_H
#include "analogbeast_v5_extra.h"

enum {
    AB5_UI_MODE = 0, AB5_UI_BANK, AB5_UI_PATTERN, AB5_UI_SELECT,
    AB5_UI_SONG, AB5_UI_ROW, AB5_UI_KIND, AB5_UI_VALUE,
    AB5_UI_STEPS, AB5_UI_SONG_LOOP,
    AB5_UI_ALBUM, AB5_UI_SLOT, AB5_UI_ALBUM_SONG, AB5_UI_ALBUM_LOOP,
    AB5_UI_EDIT_STEP, AB5_UI_EDIT_NOTE, AB5_UI_EDIT_TYPE, AB5_UI_EDIT_FLAGS,
    AB5_UI_EDIT_VELO, AB5_UI_EDIT_GATE, AB5_UI_EDIT_SWING,
    AB5_UI_EDIT_LENGTH, AB5_UI_EDIT_RES,
    AB5_UI_NPARAMS
};
/* Return 0 on success; -1 on invalid inputs. Lo/hi are inclusive. */
int ab5_ui_range(const ab5_extra_t *m,unsigned id,int *lo,int *hi);
int ab5_ui_get(const ab5_extra_t *m,unsigned id);
/* Return: 0=unchanged, 1=changed navigation/selection, 2=edited content,
 * -1=invalid action. Never writes flash or touches the v4 snapshot. */
int ab5_ui_set(ab5_extra_t *m,unsigned id,int value);
/* SONG/END on album step: END explicitly truncates this playlist from the cursor. */
int ab5_album_set_kind(ab5_extra_t *m,int end);
#endif
