/* analogBEAST v1.2: M1-x Musicbox on MD16, not pattern/song objects.
 * CDC M-Song importer removed; legacy metadata/chunks remain readable.
 * Append-only immutable chunks, manifest published LAST. Never format/erase.
 * Buffer sharing: ab18_song_wire is used only while upload owns the console.
 */
#define AB_M_META 6u
#define AB_M_CHUNK 7u
#define AB_M_SLOTS 128u
#define AB_M_CHUNK_BYTES 480u
#define AB_M_MAX_CHUNKS 96u
#define AB_M_HEADER_BYTES 56u
#define AB_M_MAX_EVENTS (AB_M_MAX_CHUNKS*(AB_M_CHUNK_BYTES/6u))
#define AB_M_MAGIC0 'M'
#define AB_M_MAGIC1 'B'
#define AB_M_MAGIC2 'X'
#define AB_M_MAGIC3 '1'
/* Persistent player contains no entire M1 file in RAM. Playback is streamed
 * from verified chunk offsets in the manifest, in the foreground only. */
typedef struct {
    uint32_t total, tick_events, token, due_ms, tick_fraction, pause_at;
    uint8_t paused;
    uint16_t id, bpm, tpq, pos, chunk_len;
    uint8_t chunks, current_chunk, active, have_next, repeat;
    uint8_t next[6];
    uint32_t chunk_offset, manifest_offset;
    ab16_fs fs;
} ab_m_player_t;
static ab_m_player_t ab_m_player __attribute__((section(".pool")));
/* The flag is forward-declared in felucca.c for UI/storage gating. */
static uint16_t ab_m_ld16(const uint8_t *b){return (uint16_t)((uint16_t)b[0]|((uint16_t)b[1]<<8));}
static uint32_t ab_m_ld32(const uint8_t *b){return (uint32_t)b[0]|((uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24);}
/* Collect committed chunk offsets with one linear scan. The manifest contains
 * these offsets; playback never needs to scan the full 292KiB disk per note. */
static int ab_m_open(uint16_t id,uint8_t *meta,uint32_t *length){
    ab16_fs fs;ab16_file entry;int rc;
    if(!id||id>AB_M_SLOTS)return AB16_EINVAL;
    rc=ab18_mount(&fs,0);if(rc)return rc;
    rc=ab16_lookup(&fs,AB_M_META,id,&entry);if(rc)return rc;
    if(entry.length<AB_M_HEADER_BYTES||entry.length>sizeof ab18_song_wire)return AB16_ECRC;
    rc=ab16_read(&fs,AB_M_META,id,0u,meta,entry.length);if(rc)return rc;
    if(meta[0]!='M'||meta[1]!='B'||meta[2]!='X'||meta[3]!='1'||meta[54]!=1u||
       !meta[20]||meta[20]>AB_M_MAX_CHUNKS||entry.length!=AB_M_HEADER_BYTES+4u*meta[20]||
       !ab_m_ld32(meta+8)||!meta[21]||meta[21]>32u)return AB16_ECRC;
    if(length)*length=entry.length;
    return 0;
}
static int ab_m_exists(uint16_t id){
    uint32_t size=0;
    return ab_m_open(id,ab18_song_wire,&size);
}
/* Foreground M player: streams to USB-MIDI and the internal synth's
 * existing MIDI input queue. Album sequencing remains in the UI thread. */
static void ab_m_silence(void){
    uint32_t ch;
    memset(ab_m_led_notes,0,sizeof ab_m_led_notes);
    for(ch=0u;ch<16u;ch++){
        uint32_t pkt=0x0Bu | (0xB0u|ch)<<8u | 123u<<16u;
        uint32_t flags=irq_save();
        (void)midi_enqueue(pkt,1u); /* internal KORG/BRUTE audio */
        midi_out_event(pkt);         /* original external USB MIDI */
        irq_restore(flags);
    }
}
static void ab_m_stop(void){
    ab_m_player.active=0u;ab_m_player.have_next=0u;ab_m_playing=0u;
    ab_m_silence();
}
static int ab_m_start(uint16_t id){
    uint8_t *m=ab18_song_wire;int rc;uint32_t n;
    /* Re-entering from the M-LOOP must NOT reset a live TEMPO adjustment. */
    uint8_t restarting=(uint8_t)(ab_m_player.active && ab_m_player.repeat &&
                                   ab_m_player.id==id);
    if(song.playing||transport_busy()||ab15_cache.active||ab15_cache.dirty)return AB16_ESTATE;
    rc=ab_m_open(id,m,&n);if(rc)return rc;
    (void)n;
    memset(&ab_m_player,0,sizeof ab_m_player);
    ab_m_player.id=id;ab_m_player.bpm=ab_m_ld16(m+4u);ab_m_player.tpq=ab_m_ld16(m+6u);
    ab_m_player.total=ab_m_ld32(m+8u);ab_m_player.token=ab_m_ld32(m+16u);
    ab_m_player.chunks=m[20];
    {ab16_file f;rc=ab18_mount(&ab_m_player.fs,0);if(rc)return rc;
     rc=ab16_lookup(&ab_m_player.fs,AB_M_META,id,&f);if(rc)return rc;
     ab_m_player.manifest_offset=f.offset;}
    /* M1's saved BPM is an INITIAL value only, not a locked player clock.
     * A saved BPM 0 is ignored (including an otherwise valid empty tempo).
     * A restart from LOOP preserves whatever was selected live. */
    if((ab_m_player.bpm && (ab_m_player.bpm<40u || ab_m_player.bpm>300u)) ||
       !ab_m_player.tpq||ab_m_player.total>AB_M_MAX_EVENTS)return AB16_ECRC;
    if(!restarting && ab_m_player.bpm && song.g[G_BPM]!=0)
        song.g[G_BPM]=(int16_t)(ab_m_player.bpm>240u?240u:ab_m_player.bpm);
    ab_m_player.bpm=(uint16_t)(song.g[G_BPM]>0 ? song.g[G_BPM] : 120u);
    ab_m_player.due_ms=fm1_ms; /* anchor AFTER metadata lookup, not before flash I/O */
    memset(ab_m_led_notes,0,sizeof ab_m_led_notes);
    ab_m_player.active=1u;ab_m_playing=1u;return 0;
}
static void ab_m_repeat(uint8_t repeat){ab_m_player.repeat=repeat?1u:0u;}
static void ab_m_play_service(void){
    uint8_t raw[AB16_HDR];ab16_ahdr hdr;uint32_t off,seq;int rc;
    if(!ab_m_player.active)return;
    /* Pausing freezes the due time of the next already-prefetched event.
     * Resuming never catches up a backlog of notes. During live tempo edits,
     * scale the remaining delay by old/new BPM, preserving its phase. */
    if(song.g[G_BPM]<=0){
        if(!ab_m_player.paused){
            ab_m_player.paused=1u;ab_m_player.pause_at=fm1_ms;
            ab_m_silence(); /* Note Off once, player state remains active */
        }
        return;
    }
    if(ab_m_player.paused){
        ab_m_player.due_ms+=(uint32_t)(fm1_ms-ab_m_player.pause_at);
        ab_m_player.paused=0u;
    }
    if(ab_m_player.bpm!=(uint16_t)song.g[G_BPM]){
        if(ab_m_player.have_next){
            int32_t remaining=(int32_t)(ab_m_player.due_ms-fm1_ms);
            if(remaining>0){
                uint64_t scaled=(uint64_t)(uint32_t)remaining*ab_m_player.bpm;
                ab_m_player.due_ms=fm1_ms+(uint32_t)(scaled/(uint32_t)song.g[G_BPM]);
            }
        }
        ab_m_player.bpm=(uint16_t)song.g[G_BPM];
        ab_m_player.tick_fraction=0u;
    }
    if(!ab_m_player.have_next){
        if(ab_m_player.tick_events>=ab_m_player.total){
            uint16_t id=ab_m_player.id;uint8_t repeat=ab_m_player.repeat;
            /* A looping M file already contains its Note Off events. Do not
             * inject 16 channel-wide All-Notes-Off packets between repeats:
             * these can cut the first notes of the new iteration. */
            if(repeat && !ab_md_playlist_active){
                if(!ab_m_start(id)){ab_m_player.repeat=1u;return;}
            }
            ab_m_stop();return;
        }
        if(!ab_m_player.chunk_len||ab_m_player.pos>=ab_m_player.chunk_len){
            if(ab_m_player.current_chunk>=ab_m_player.chunks){ab_m_stop();return;}
            rc=ab16_rd(&ab_m_player.fs,ab_m_player.manifest_offset+AB16_HDR+
                AB_M_HEADER_BYTES+4u*ab_m_player.current_chunk,raw,4u);
            if(rc){ab_m_stop();return;}
            off=ab_m_ld32(raw);
            rc=ab16_rd(&ab_m_player.fs,off,raw,sizeof raw);if(rc||!ab16_decode(raw,&hdr)||
                hdr.type!=AB_M_CHUNK||hdr.id!=((ab_m_player.token<<8u)|ab_m_player.current_chunk)||
                hdr.flags||hdr.length%6u||!hdr.length){ab_m_stop();return;}
            /* Verify payload CRC before sending any event from the chunk. */
            {ab16_file f={0};f.offset=off;f.length=hdr.length;
             if(ab16_rawread(&ab_m_player.fs,&f,0,0,0)){ab_m_stop();return;}}
            ab_m_player.chunk_offset=off;ab_m_player.chunk_len=hdr.length;
            ab_m_player.pos=0u;ab_m_player.current_chunk++;
        }
        rc=ab16_rd(&ab_m_player.fs,ab_m_player.chunk_offset+AB16_HDR+ab_m_player.pos,ab_m_player.next,6u);
        if(rc){ab_m_stop();return;}
        ab_m_player.pos=(uint16_t)(ab_m_player.pos+6u);
        seq=ab_m_ld16(ab_m_player.next);
        /* Fractional millisecond carries between MIDI ticks; without this,
         * 960-PPQN imports drift badly at high event densities. */
        {uint32_t den=(uint32_t)ab_m_player.bpm*ab_m_player.tpq;
         uint64_t elapsed=(uint64_t)seq*60000ull+ab_m_player.tick_fraction;
         ab_m_player.due_ms+=(uint32_t)(elapsed/den);
         ab_m_player.tick_fraction=(uint32_t)(elapsed%den);}
        ab_m_player.have_next=1u;
    }
    if((int32_t)(fm1_ms-ab_m_player.due_ms)<0)return;
    {uint8_t *e=ab_m_player.next;uint32_t pkt=0,st=e[2],ch=e[3];
     if(st==0x80u||st==0x90u){pkt=(st==0x90u?0x09u:0x08u)| (st|ch)<<8u|
                                    (uint32_t)e[4]<<16u|(uint32_t)e[5]<<24u;}
     else if(st==0xB0u||st==0xE0u||st==0xA0u){
         pkt=(st==0xB0u?0x0Bu:st==0xE0u?0x0Eu:0x0Au)|(st|ch)<<8u|
                 (uint32_t)e[4]<<16u|(uint32_t)e[5]<<24u;
     }else if(st==0xD0u||st==0xC0u){
         pkt=(st==0xC0u?0x0Cu:0x0Du)|(st|ch)<<8u|(uint32_t)e[4]<<16u;
     }else if(st==0xF1u){
         /* MIDI-file tempo metadata is not a second player clock.
          * It must never override the live knob, especially if the value is 0. */
     }
     if(pkt){uint32_t flags=irq_save();
         if(((pkt>>8u)&0xF0u)!=0xC0u && (mi_w-mi_r)>=MQ-1u){
             irq_restore(flags);return; /* wait for audio ISR to drain MIDI input */
         }
         /* Preserve the user's selected synth preset; imported MIDI files may
          * carry their own Program Change meant for an external MIDI module. */
         if(((pkt>>8u)&0xF0u)!=0xC0u && !midi_enqueue(pkt,1u)){
             /* Do not discard this already-decoded event when the synth's
              * ring is busy. Retry on the next main-loop pass. */
             irq_restore(flags);return;
         }
         midi_out_event(pkt);irq_restore(flags);
     }
     if(pkt)ab_m_led_note_event(st,ch,e[4],e[5]);
     ab_m_player.tick_events++;ab_m_player.have_next=0u;
    }
}
