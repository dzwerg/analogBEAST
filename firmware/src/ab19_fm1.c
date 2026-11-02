/* Stage20: safe HAL sector erase + 128 native starter pattern converter.
 * Factory seed code is run from the foreground first boot, NOT from OTA.
 */
#if FELUCCA_FLASH && GOLANA
static int ab19_nor_erase(void*ctx,uint32_t off){
    uint32_t took=0u,f;int rc;
    (void)ctx;
    if(!flash_ok || off%AB16_SECTOR || off>AB16_SIZE-AB16_SECTOR ||
       !FL_IN(AB16_BASE+off,AB16_SECTOR,FL_DATA_LO,FL_DATA_HI))return -1;
    fm1_wdt_feed();
    f=irq_save();
    audio_silence();
    rc=FL_FAR(fl_erase4k_ram)(AB16_BASE+off,&took);
    irq_restore(f);
    fm1_wdt_feed();
    return rc;
}
#endif
