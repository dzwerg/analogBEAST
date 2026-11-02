/* analogBEAST v1.2 Stage14: 128 equal, editable starter patterns.
 * 13 real built-in phrases, 115 empty 16-step templates.
 * Does not flag data as immutable or factory. */
#ifndef AB14_FACTORY_H
#define AB14_FACTORY_H
#include <stdint.h>
#include "ab13_bankfree.h"
#include "ab13_disk.h"
#define AB14_STARTER_COUNT 128u
#define AB14_STARTER_STEPS 16u
#define AB14_STARTER_WIRE (8u + AB14_STARTER_STEPS * 5u)
/* Return 1 for a ROM phrase, 0 for an empty slot, negative for failure.
 * src() populates note[16], flags[16] for ids 1..128; legacy flags 1 ACC,
 * 2 SLIDE, 4 TIE. Legacy note zero means REST (note zero can be written
 * as PRESENT using a subsequent normal edit). */
typedef int (*ab14_factory_source)(void *user,uint16_t id,uint8_t note[16],uint8_t flags[16]);
int ab14_make_pattern(uint16_t id,ab14_factory_source source,void *user,ab13_pattern *pattern);
#ifndef AB14_DEVICE
/* HOST-OFFLINE preparation of a freshly erased MD13 image ONLY.
 * Never called from firmware; requires ~6 KiB host stack for 128 descriptors.
 * Single all-or-nothing manifest transaction. No format/erase of legacy flash. */
int ab14_seed_fresh(md13_disk *disk,ab14_factory_source source,void *user);
#endif
#endif
