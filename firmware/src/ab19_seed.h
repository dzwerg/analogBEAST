/* analogBEAST v1.2 Stage19: one-time MD16 initialization.
 * Only explicit calling code may erase; never automatically format on boot.
 * The 128 starter patterns are normal mutable files after initialization. */
#ifndef AB19_SEED_H
#define AB19_SEED_H
#include "ab16_fs.h"
#include "ab13_bankfree.h"
#define AB19_STARTER_COUNT 128u
#define AB19_FACTORY_STEPS 16u
typedef int (*ab19_factory_fn)(void *ctx,uint16_t id,ab13_pattern *p);
/* Destructive: ERASE ENTIRE 0x97000..0xDFFFF region, ONLY when NOT mounted.
 * Requires caller's separate explicit confirmation and a full offline backup.
 * Never call from IRQ or from automatic startup. */
int ab19_erase_for_init(ab16_io io);
/* Requires entire region freshly erased. Writes factory files FIRST,
 * publishes the sole AB16 marker LAST. Never erase or retry automatically. */
int ab19_seed_blank(ab16_io io,ab19_factory_fn factory,void *ctx);
#endif
