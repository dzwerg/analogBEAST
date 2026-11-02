/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The firmware side of storage.c (FELUCCA_FLASH): flash reads in short IRQ-off windows so the
 * audio keeps up, erases and writes only inside the storage areas, the audio buffer silenced
 * first. The host tests (storage_test.c) provide their own st_* on a simulated NOR. */
static uint8_t flash_ok;                 /* JEDEC id matched at boot (persist_boot) */
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void audio_silence(void)                 /* IRQs off: the DMA would loop stale audio (buzz) */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}
static uint8_t ab12_s10_migration_active;
static uint8_t ab12_s12_modify_active; /* ONLY manual Stage12 transaction */
static int ab12_s10_reserved(uint32_t off,uint32_t n) {
    if(!n || off>0x100000u || n>0x100000u-off)return 0;
    return (off<0xB4000u && off+n>0xA8000u) ||
           (off<0xC8000u && off+n>0xBC000u) ||
           (off<0xDC000u && off+n>0xD1000u);
}
static int st_erase(uint32_t off)
{
    uint32_t took, f;
    int rc;
    if (ab12_s10_reserved(off, 4096u) &&
        !(ab12_s12_modify_active && (off == 0xA8000u || off == 0xA9000u ||
                                     off == 0xAA000u || off == 0xAB000u)))
        return -9; /* only active MD12 manifest may be erased by transaction */
    if (ab18_disk_owned && FL_IN(off,0x1000u,FL_DATA_LO,FL_DATA_HI)) return -8;
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    f = irq_save();
    audio_silence();
    rc = FL_FAR(fl_erase4k_ram)(off, &took);
    irq_restore(f);
    return rc;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (ab12_s10_reserved(off,n) &&
        !(ab12_s10_migration_active && n==256u && !(off&255u) &&
          ((off>=0xA8000u && off<=0xB3F00u) ||
           (off>=0xBC000u && off<=0xC7F00u) ||
           (off>=0xD1000u && off<=0xDBF00u))) &&
        !(ab12_s12_modify_active && n<=256u && !(off & 0xFF000000u) &&
          ((off>=0xA8000u && off+n<=0xB4000u) ||
           (off>=0xBC000u && off+n<=0xC8000u) ||
           (off>=0xD1000u && off+n<=0xDC000u)))) return -9;
    if (ab18_disk_owned && FL_IN(off,n,FL_DATA_LO,FL_DATA_HI)) return -8;
    if (!FL_STORE_OK(off, n))                   /* only inside the storage areas */
        return -8;
    return fl_write(off, src, n);
}
