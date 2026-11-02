/* analogBEAST v1.2 / Stage11: optional read-only MD12 -> legacy RAM
 * playback bridge. Included AFTER golana_ui.c. NO FLASH WRITES.
 * Only IDs 1..128 patterns, 1..32 songs, 1..4 albums; legacy
 * 256-entry arranger #33 and existing PRE1 records remain on MD12.
 */
/* Legal MD12 max: sparse 64-step pattern = 6+64*6 = 390 B;
 * sparse 64-row song = 6+64*5 = 326 B; album = 34 B.
 * Reject any non-conforming oversized record rather than reserving 1100 B. */
#define AB12_S11_BMAX 400u
_Static_assert(AB12_S11_BMAX>=6u+64u*6u,"legacy pattern buffer too small");
static uint8_t ab12_s11_body[AB12_S11_BMAX] __attribute__((section(".pool")));
static ab_step_t ab12_s11_steps[64] __attribute__((section(".pool")));
static uint8_t ab12_s11_mounted;
static int ab12_s11_read(uint8_t typ, uint16_t id, uint32_t *length) {
    uint32_t file,n;
    int rc=ab12_s12_lookup(typ,id,&file,&n);
    if(rc || n>AB12_S11_BMAX)return -20;
    if(ab12_s12_file_read(typ,id,0u,ab12_s11_body,n))return -21;
    *length=n;
    return 0;
}
static int ab12_s11_pattern(uint16_t id,uint32_t apply){
    uint32_t n,i,j,cnt,cap,codec,expected;
    uint8_t *b=ab12_s11_body;
    int rc=ab12_s11_read(1u,id,&n);
    if(rc)return rc;
    if(n<6u)return -22;
    cap=b[2];codec=b[3];cnt=ab12_s10_le16(b+4);
    expected=id<=64u?64u:16u;
    if(cap!=expected||b[0]<1u||b[0]>cap||b[1]>9u)return -23;
    if(codec==0u){if(cnt!=cap||n!=6u+cap*5u)return -24;}
    else if(codec==1u){if(cnt>cap||n!=6u+cnt*6u)return -25;}
    else return -26;
    for(i=0;i<cap;i++){
        uint8_t *p=(uint8_t *)&ab12_s11_steps[i];
        p[0]=0u;p[1]=0u;p[2]=96u;p[3]=64u;p[4]=0u;
    }
    if(codec==0u){for(i=0;i<cap;i++){
        uint8_t *p=(uint8_t *)&ab12_s11_steps[i];
        for(j=0;j<5u;j++)p[j]=b[6u+5u*i+j];
    }}else{
        uint32_t used[4]={0,0,0,0};
        for(i=0;i<cnt;i++){
            uint32_t at=b[6u+i*6u];
            uint8_t *p;
            if(at>=cap||(used[at>>5]&(1u<<(at&31u))))return -27;
            used[at>>5]|=1u<<(at&31u);
            p=(uint8_t *)&ab12_s11_steps[at];
            for(j=0;j<5u;j++)p[j]=b[7u+i*6u+j];
        }
    }
    for(i=0;i<cap;i++){
        const uint8_t *p=(const uint8_t *)&ab12_s11_steps[i];
        if(p[0]>127u||p[1]>31u||!p[2]||p[2]>127u||!p[3]||p[3]>127u||p[4]>100u)return -28;
    }
    if(apply){
        if(id<=64u){
            ab_pattern_t *p=&dz_model.patterns[id-1u];
            p->length=b[0];p->resolution=b[1];
            for(i=0;i<cap;i++)p->step[i]=ab12_s11_steps[i];
        }else{
            ab5_pattern_t *p=&ab5_model.extra_patterns[id-65u];
            p->length=b[0];p->resolution=b[1];
            for(i=0;i<cap;i++)p->steps[i]=ab12_s11_steps[i];
        }
    }
    return 0;
}
static int ab12_s11_song(uint16_t id,uint32_t apply){
    uint32_t n,i,j,k=0,cnt,cap,codec,run;
    uint8_t rows[64u*4u];
    uint8_t *b=ab12_s11_body;
    int rc=ab12_s11_read(2u,id,&n);
    if(rc)return rc;
    if(n<6u)return -31;
    cnt=ab12_s10_le16(b);cap=ab12_s10_le16(b+4);codec=b[3];
    if(cap!=64u||cnt<1u||cnt>64u||b[2]>1u)return -32;
    if(codec==0u){
        if(n!=6u+4u*cap)return -33;
        for(i=0;i<4u*cap;i++)rows[i]=b[6u+i];
    }else if(codec==1u){
        if((n-6u)%5u)return -34;
        for(i=6u;i<n;i+=5u){
            run=b[i];if(!run||run>cap-k)return -35;
            for(j=0;j<run;j++){
                uint32_t x;for(x=0;x<4u;x++)rows[(k+j)*4u+x]=b[i+1u+x];
            }
            k+=run;
        }
        if(k!=cap)return -36;
    }else return -37;
    /* Validate structural bounds before touching the legacy RAM model. */
    for(i=0;i<cap;i++){
        uint8_t bank=rows[4u*i],pat=rows[4u*i+1u],steps=rows[4u*i+2u];
        if(!steps||steps>64u)return -38;
        if(bank<8u){if(pat>=16u|| (bank>=4u&&steps>16u))return -39;}
        else if(bank==8u){if(pat!=16u)return -40;}
        /* Other command variants are validated by ab5_extra_validate
         * after the model copy (host test uses source-validated records). */
    }
    if(apply){
        ab5_song_t *s=&ab5_model.songs[id-1u];
        s->count=(uint8_t)cnt;s->loop=b[2];
        for(i=0;i<cap;i++){
            uint8_t *p=(uint8_t *)&s->rows[i];
            for(j=0;j<4u;j++)p[j]=rows[4u*i+j];
        }
    }
    return 0;
}
static int ab12_s11_album(uint16_t id,uint32_t apply){
    uint32_t n,i;
    int rc=ab12_s11_read(3u,id,&n);
    if(rc)return rc;
    if(n!=34u||ab12_s11_body[0]>32u||ab12_s11_body[1]>1u)return -42;
    for(i=0;i<32u;i++){
        uint8_t v=ab12_s11_body[2u+i];
        if(i<ab12_s11_body[0]){if(v>=32u)return -43;}
        else if(v!=255u)return -44;
    }
    if(apply){
        uint8_t *p=(uint8_t *)&ab5_model.albums[id-1u];
        for(i=0;i<n;i++)p[i]=ab12_s11_body[i];
    }
    return 0;
}
/* Two complete passes: malformed source MUST NOT cause half-imported RAM.
 * All disk cells are checked once against the Stage10 committed source.
 * Editing the preview is RAM ONLY (legacy store explicitly disabled).
 */
static int ab12_s11_mount(uint32_t apply){
    uint32_t i,pass,verified=0;
    int rc;
    if(!flash_ok||song.playing||transport_busy()||gu.save||gu.dz_edit_active)return -50;
    if((rc=ab12_s12_verify(&verified))||!verified)return -51;
    for(pass=0;pass<(apply?2u:1u);pass++){
        uint32_t install=apply&&pass==1u;
        for(i=1;i<=128u;i++){
            if((rc=ab12_s11_pattern((uint16_t)i,install)))return rc;
            fm1_wdt_feed();
        }
        for(i=1;i<=32u;i++){
            if((rc=ab12_s11_song((uint16_t)i,install)))return rc;
            fm1_wdt_feed();
        }
        for(i=1;i<=4u;i++){
            if((rc=ab12_s11_album((uint16_t)i,install)))return rc;
            fm1_wdt_feed();
        }
    }
    if(apply){
        dz_store_enabled=0u;ab5_store_enabled=0u;
        ab5_dirty=0u;ab5_prepared_valid=0u;
        ab12_s11_mounted=1u;
        gu_say("MUSIC DISK RAM",0);
    }
    return 0;
}
