/* analogBEAST v1.2 Stage 6 -- no writes, no standard library required.
 * Logical sectors 0..34 cover ONLY sectors that were blank in the verified
 * analogBEAST backup. These three disjoint holes are NOT a flash partition yet.
 * All ABST/AB5S snapshot sectors remain unaddressable by this adapter.
 */
#include "ab12_map.h"
int ab12_s6_sector(uint32_t i, uint32_t *phys) {
    if (!phys) return AB12_S6_EINVAL;
    if (i < 12u) *phys = 0xA8000u + i * AB12_S6_SECTOR;
    else if (i < 24u) *phys = 0xBC000u + (i - 12u) * AB12_S6_SECTOR;
    else if (i < 35u) *phys = 0xD1000u + (i - 24u) * AB12_S6_SECTOR;
    else return AB12_S6_EINVAL;
    return AB12_S6_OK;
}
int ab12_s6_read(ab12_s6_reader reader, void *ctx, uint32_t at, void *dst, uint32_t n) {
    uint8_t *out = (uint8_t *)dst;
    if (!reader || (!dst && n) || at > AB12_S6_TOTAL_BYTES ||
        n > AB12_S6_TOTAL_BYTES - at) return AB12_S6_EINVAL;
    while (n) {
        uint32_t phys, inner = at % AB12_S6_SECTOR;
        uint32_t k = AB12_S6_SECTOR - inner;
        if (k > n) k = n;
        if (ab12_s6_sector(at / AB12_S6_SECTOR, &phys)) return AB12_S6_EINVAL;
        if (reader(ctx, phys + inner, out, k)) return AB12_S6_EIO;
        out += k; at += k; n -= k;
    }
    return AB12_S6_OK;
}
int ab12_s6_preflight(ab12_s6_reader reader, void *ctx, ab12_s6_report *r) {
    uint8_t buf[256];
    uint32_t i, off, j, phys;
    if (!reader || !r) return AB12_S6_EINVAL;
    r->valid = 0u; r->blanks = 0u; r->occupied = 0u;
    r->unreadable = 0u; r->capacity = AB12_S6_TOTAL_BYTES;
    for (i = 0; i < AB12_S6_SAFE_SECTORS; i++) {
        uint8_t blank = 1u;
        if (ab12_s6_sector(i, &phys)) return AB12_S6_EINVAL;
        for (off = 0; off < AB12_S6_SECTOR; off += sizeof buf) {
            if (reader(ctx, phys + off, buf, sizeof buf)) {
                r->unreadable++;
                return AB12_S6_EIO;
            }
            for (j = 0; j < sizeof buf; j++) if (buf[j] != 255u) blank = 0u;
        }
        if (blank) r->blanks++;
        else r->occupied++;
    }
    r->valid = (uint8_t)(r->occupied == 0u && r->unreadable == 0u);
    return r->valid ? AB12_S6_OK : AB12_S6_OCCUPIED;
}
