/* SPDX-License-Identifier: GPL-3.0-only */
/* Mounted-MD16 pattern persistence: no format, no sector ownership takeover.
 * Only a correctly mounted volume may be modified. All calls foreground-only.
 */
#include "ab18_store.h"
#define AB18_PATTERN_MAX_WIRE AB13_PATTERN_MAX_WIRE
int ab18_save_pattern(ab16_fs *fs,uint32_t id,const ab13_pattern *p){
    uint32_t n;ab16_file got;int rc;
    if(!fs||!fs->mounted||!fs->io.prog||!p||!id||ab13_pattern_check(p))return AB16_ESTATE;
    n=ab13_pattern_size(p);
    if(n<48u||n>AB16_MAX_DATA)return AB16_EINVAL;
    rc=ab16_put(fs,AB16_PAT,id,(uint16_t)n,ab13_pattern_emit,(void *)p);
    if(rc)return rc;
    rc=ab16_lookup(fs,AB16_PAT,id,&got);
    if(rc)return rc;
    return got.length==n?0:AB16_ECRC;
}
/* Static scratch is only accessed in foreground; audio IRQ reads RAM cache.
 * Not reentrant: stage18 only calls this from the foreground UI/CDC path. */
int ab18_load_pattern(ab16_fs *fs,uint32_t id,ab13_pattern *p){
    static uint8_t wire[AB18_PATTERN_MAX_WIRE];ab16_file got;int rc;
    if(!fs||!fs->mounted||!p||!id)return AB16_ESTATE;
    rc=ab16_lookup(fs,AB16_PAT,id,&got);if(rc)return rc;
    if(got.length<48u||got.length>sizeof wire)return AB16_EINVAL;
    rc=ab16_read(fs,AB16_PAT,id,0u,wire,got.length);if(rc)return rc;
    return ab13_pattern_parse(p,wire,got.length)?AB16_ECRC:0;
}
int ab18_delete_pattern(ab16_fs *fs,uint32_t id){
    ab16_file e;int rc;
    if(!fs||!fs->mounted||!fs->io.prog||!id)return AB16_ESTATE;
    rc=ab16_remove(fs,AB16_PAT,id);if(rc)return rc;
    rc=ab16_lookup(fs,AB16_PAT,id,&e);
    return rc==AB16_ENOTFOUND?0:(rc?rc:AB16_ESTATE);
}
int ab18_next_pattern(ab16_fs *fs,uint32_t current,int dir,uint32_t *next){
    return ab16_select_existing(fs,AB16_PAT,current,dir,next);
}
