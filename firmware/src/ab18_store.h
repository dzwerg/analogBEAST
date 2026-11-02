/* analogBEAST Stage18: mounted-MD16 pattern persistence. NO format API. */
#ifndef AB18_STORE_H
#define AB18_STORE_H
#include "ab16_fs.h"
#include "ab13_bankfree.h"
int ab18_save_pattern(ab16_fs *fs,uint32_t id,const ab13_pattern *p);
int ab18_load_pattern(ab16_fs *fs,uint32_t id,ab13_pattern *p);
int ab18_delete_pattern(ab16_fs *fs,uint32_t id);
int ab18_next_pattern(ab16_fs *fs,uint32_t current,int dir,uint32_t *next);
#endif
