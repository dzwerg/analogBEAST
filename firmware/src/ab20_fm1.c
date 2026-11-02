/* analogBEAST Stage20: preserve stock Web-OTA. One-time, foreground, on-device
 * first boot creates MD16 + 128 normal mutable starter patterns from app ROM.
 * Subsequent app updates MUST NOT seed or format an existing valid volume.
 * Maintenance clear leaves an EMPTY, VALID MD16 volume so reboot does not
 * create new patterns before a user flashes another firmware.
 * NO flash operations in an audio ISR. Boot/load and maintenance are foreground.
 */
#if FELUCCA_FLASH && GOLANA
static int ab20_boot_rc;
static void ab20_claim(void){
    ab18_disk_owned=1u;
    dz_store_enabled=0u;
}
/* Existing MUSIC DISK only.  The old Donner/legacy first-boot data and
 * migration/marker writers are intentionally removed from firmware.
 * NEVER erase, format, seed, or rewrite a volume during an OTA reboot.
 * Users can create empty pattern IDs in REC and save those themselves. */
static int ab20_first_boot(void){
    ab16_fs disk;ab16_io io;int rc;
    if(!flash_ok){ab20_boot_rc=AB16_EIO;return ab20_boot_rc;}
    /* Even if the volume is damaged/unformatted, do not hand these former
     * data sectors back to old automatic writers. No flash writes on boot. */
    ab20_claim();
    io.ctx=0;io.read=ab16_fm1_read;io.prog=0;io.erase=0;
    rc=ab16_mount(&disk,io);
    ab20_boot_rc=rc;
    return rc;
}
/* Explicit five CCW detents in hidden GLO menu only. This deliberately does
 * NOT reseed or return all bytes to 0xFF: a single valid MD16 filesystem marker
 * remains in the former project region (first sector), while sample and preset
 * sectors are completely erased. Empty volume survives an analogBEAST restart.
 */
static int ab20_clear_for_other_firmware(void){
    ab16_io io;ab16_fs disk;uint32_t sector;int rc;
    /* UI-only guards (save/editor) are enforced by the caller. Keep the
     * low-level destructive API usable before/after the Golana UI include. */
    if(!flash_ok||song.playing||transport_busy()||seq_counting()||
       ab15_cache.active||ui.uboot)return AB16_ESTATE;
    ab20_claim();
    io.ctx=0;io.read=ab16_fm1_read;io.prog=ab18_fm1_prog;
    io.erase=ab19_nor_erase;
    for(sector=0;sector<AB16_NSECTORS;sector++){
        if(io.erase(io.ctx,sector*AB16_SECTOR))return AB16_EIO;
    }
    rc=ab16_create_erased(&disk,io);
    if(rc)return rc;
    if(disk.records!=1u)return AB16_ESTATE;
    /* No legacy sample reads after MD16 volume creation. */
    ab15_clear(&ab15_cache);
    ab17_init(&ab17_ui_delete,0u);
    return AB16_OK;
}
#endif
