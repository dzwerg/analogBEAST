/* Stage15: MD13 or ROM pattern -> RAM cache. Only console commands do
 * foreground SPI reads. Playback ISR uses cached steps exclusively.
 * Disk edit/save/format remains DISABLED (no NOR writes in dev15). */
static int ab15_busy(void){
    return song.playing||transport_busy()||gu.save||gu.dz_edit_active||ab15_cache.active;
}
static int ab15_read_disk(uint16_t id){
    return ab18_load_disk(id); /* MD16, only foreground and mounted volume */
}
/* This routine is called ONLY from the existing seq IRQ/boundary callback;
 * no MD13 flash calls and no malloc. A 128-step pattern streams as 64+64. */
static void ab15_audio_page(track_t *t){
    uint32_t i,n=ab15_page_length(&ab15_cache);
    for(i=0;i<NSTEP;i++){
        step_t *s=&t->step[i];const ab13_step*in=i<n?ab15_page_step(&ab15_cache,(uint8_t)i):0;
        memset(s,0,sizeof *s);
        if(ab15_event_type(in)==AB15_TIE)s->time=ST_TIE;
        else if(ab15_event_type(in)==AB15_NOTE){
            s->time=ST_NOTE;s->n=1u;s->note[0]=in->note;s->vel=in->velocity;
            s->flags=(uint8_t)(in->flags&(SF_ACCENT|SF_SLIDE));
        }else s->time=ST_REST;
    }
    t->p[P_SLEN]=(int16_t)n;
    t->p[P_SDIV]=ab15_cache.pattern.resolution;
    t->p[P_SSWING]=0;
}
static uint32_t ab15_audio_gate(uint32_t idx){
    const ab13_step*s=ab15_page_step(&ab15_cache,(uint8_t)idx);
    return s&&s->gate?s->gate:64u;
}
#if FELUCCA_CDC
static uint32_t ab15_value(const char **p,uint32_t min,uint32_t max,int *ok){
    int valid=0;uint32_t n=con_num(p,&valid);
    while(**p==' ')(*p)++;
    *ok=valid&&n>=min&&n<=max&&**p==0;
    return n;
}
static void ab15_console_status(void){
    con_kv("pattern_id",ab15_cache.pattern_id);
    con_kv("pattern_loaded",ab15_cache.loaded);
    con_kv("pattern_length",ab15_cache.loaded?ab15_cache.pattern.length:0);
    con_kv("pattern_page",ab15_cache.page);
    con_kv("pattern_armed",ab15_cache.armed);
    con_kv("pattern_active",ab15_cache.active);
    con_kv("pattern_loop",ab15_cache.loop);
    con_kv("pattern_ram_dirty",ab15_cache.dirty);
    con_puts("READ-ONLY DISK: edits RAM only, no save in dev15\r\n");
}
static void ab15_console_disk(const char*p){
    int ok=0;uint32_t id=ab15_value(&p,1u,65535u,&ok);
    if(!ok){con_puts("usage: patdisk 1..65535\r\n");return;}
    con_kv("patdisk_rc",ab15_read_disk((uint16_t)id));
}
static void ab15_console_len(const char*p){
    int ok=0;uint32_t n=ab15_value(&p,8u,128u,&ok);
    if(!ok||ab15_busy()){con_puts("usage: patlen 8..128 (STOP first)\r\n");return;}
    con_kv("patlen_rc",ab15_set_length(&ab15_cache,(uint8_t)n));
}
static void ab15_console_note(const char*p){
    int a=0,b=0;uint32_t s=con_num(&p,&a),note=con_num(&p,&b);
    while(*p==' ')p++;
    if(!a||!b||*p||s<1u||s>128u||note>127u||ab15_busy()){
        con_puts("usage: patnote STEP(1..128) MIDI(0..127), STOP first\r\n");return;
    }
    con_kv("patnote_rc",ab15_note(&ab15_cache,(uint8_t)(s-1u),(uint8_t)note));
}
static void ab15_console_rest(const char*p){
    int ok=0;uint32_t n=ab15_value(&p,1u,128u,&ok);
    if(!ok||ab15_busy()){con_puts("usage: patrest STEP(1..128), STOP first\r\n");return;}
    con_kv("patrest_rc",ab15_rest(&ab15_cache,(uint8_t)(n-1u)));
}
static void ab15_console_tie(const char*p){
    int ok=0;uint32_t n=ab15_value(&p,1u,128u,&ok);
    if(!ok||ab15_busy()){con_puts("usage: pattie STEP(1..128), STOP first\r\n");return;}
    con_kv("pattie_rc",ab15_tie(&ab15_cache,(uint8_t)(n-1u)));
}
static void ab15_console_loop(const char*p){
    int ok=0;uint32_t n=ab15_value(&p,0u,1u,&ok);
    if(!ok||ab15_busy()){con_puts("usage: patloop 0|1, STOP first\r\n");return;}
    ab15_cache.loop=(uint8_t)n;ab15_cache.armed=ab15_cache.loaded;
    con_kv("patloop",ab15_cache.loop);
}
static void ab15_console_off(void){
    if(ab15_busy()){con_puts("STOP playback before patoff\r\n");return;}
    ab15_cache.armed=0u;con_puts("RAM pattern disarmed\r\n");
}
#endif /* FELUCCA_CDC: legacy CLI-only pattern commands */
