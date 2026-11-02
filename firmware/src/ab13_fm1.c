/* Stage13: bank-free codecs and new dedicated 292 KiB MD13 geometry.
 * Compile-time + CDC read-only preflight. IMPORTANT: ZERO on-device NOR WRITES.
 * Stage14 will connect the new player/editor and guarded initial formatting.
 * Do not instantiate full banks in RAM: only the 384-entry directory lives in POOL.
 */
#include "ab13_bankfree.c"
#if FELUCCA_CDC  /* obsolete MD13 preflight, never part of Web-MIDI build */
#define AB13_UNITY 1
#include "ab13_disk.c"
#define AB13_FLASH_BASE 0x97000u
#define AB13_FLASH_TOP 0xE0000u
_Static_assert(MD13_SIZE == AB13_FLASH_TOP - AB13_FLASH_BASE, "disk physical map mismatch");
_Static_assert(MD13_MAX_ENTRIES >= AB13_PATTERNS + AB13_SONGS + AB13_ALBUMS + AB13_PRESETS,
               "bankfree directory too small");
_Static_assert(MD13_PAGE + MD13_MAX_ENTRIES * 12u <= MD13_BANK_SIZE,
               "manifest would overflow its own flash bank");
static md13_disk ab13_directory __attribute__((section(".pool")));
static int ab13_flash_read(void *ignored, uint32_t off, void *dst, uint32_t n)
{
    (void)ignored;
    if (off > MD13_SIZE || n > MD13_SIZE - off) return -1;
    return st_read(AB13_FLASH_BASE + off, dst, n);
}
static int ab13_forbid_erase(void *ignored, uint32_t off)
{
    (void)ignored; (void)off; return -1;
}
static int ab13_forbid_program(void *ignored, uint32_t off, const void *data, uint32_t n)
{
    (void)ignored; (void)off; (void)data; (void)n; return -1;
}
static int ab13_probe(void)
{
    md13_io io;
    if (!flash_ok || song.playing || transport_busy() || gu.save || gu.dz_edit_active)
        return -90;
    io.ctx=0;
    io.read=ab13_flash_read;
    io.erase4k=ab13_forbid_erase;
    io.program=ab13_forbid_program;
    return md13_mount(&ab13_directory,io);
}
static int32_t ab13_file_count(void){return (int32_t)ab13_directory.count;}
static int32_t ab13_disk_generation(void){return (int32_t)ab13_directory.generation;}
static int ab13_selfcheck(void)
{
    return sizeof(ab13_step)==5u && sizeof(ab13_row)==8u &&
           sizeof(ab13_pattern)==642u &&
           AB13_PATTERN_MAX_WIRE==648u &&
           AB13_SONG_MAX_WIRE==2056u &&
           MD13_SIZE==299008u && MD13_MAX_ENTRIES==384u ? 0 : -1;
}

#endif /* FELUCCA_CDC: obsolete MD13 preflight */
