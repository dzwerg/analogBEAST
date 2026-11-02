/* Stage16 FM-1 glue: READ-ONLY diagnostic mount. NEVER erases or programs NOR.
 * Console function lives after console.c so con_kv/con_puts are available.
 */
#if FELUCCA_FLASH
static int ab16_fm1_read(void*ctx,uint32_t off,void*dst,uint32_t n){
    int rc;(void)ctx;
    if(off>AB16_SIZE||n>AB16_SIZE-off||!flash_ok)return -1;
    fm1_wdt_feed();
    rc=st_read(AB16_BASE+off,dst,n);
    fm1_wdt_feed();
    return rc;
}
#if FELUCCA_CDC
static void ab16_console_disk(void){
    ab16_fs fs;ab16_io io;int rc;
    if(song.playing){con_puts("STOP playback before musicfs diagnostic\r\n");return;}
    if(!flash_ok){con_puts("MUSIC FS: flash unavailable\r\n");return;}
    io.ctx=0;io.read=ab16_fm1_read;
    io.prog=0;io.erase=0; /* HARD DISABLE ALL DEVICE WRITES IN DEV16 */
    rc=ab16_mount(&fs,io);
    con_kv("musicfs_mount_rc",rc);
    if(rc==AB16_ENOFS)con_puts("MUSIC FS: not initialized; existing contents untouched\r\n");
    else if(rc==AB16_EFOREIGN)con_puts("MUSIC FS: foreign/older format; no writes\r\n");
    else if(!rc){
        con_kv("musicfs_volume_bytes",AB16_SIZE);
        con_kv("musicfs_records",(int32_t)fs.records);
        con_kv("musicfs_next_seq",(int32_t)fs.next_seq);
        con_kv("musicfs_blank_sectors",(int32_t)fs.blank_sectors);
        con_kv("musicfs_blocked_sectors",(int32_t)fs.blocked_sectors);
        con_puts("MUSIC FS: diagnostic read-only (dev18 mounted writes)\r\n");
    }
}
#endif /* FELUCCA_CDC */
#endif /* FELUCCA_FLASH */
