/* QA4: byte-framed SysEx 96 <-> actual firmware dispatcher over stdio.
 * Runs ONLY as native host simulation; simulated NOR/RAM, no USB hardware.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "../web/emu/felucca_web.c"
static uint8_t wire_in[256],wire_out[256];
static void send_error(void){puts("ERR");fflush(stdout);}
int main(void){
    char line[4096];
    unsigned size=0;
    web_nor_erase();
    web_boot();
    /* Explicit QA-only gate: simulated flash does not contain the user's
     * actual first-boot v5 header. Nothing here reaches the physical FM-1. */
    gu.ready=1u;
    ab5_store_enabled=1u;
    ab5_boot_status=AB5_BOOT_V5_VALID;
    ab5_state.valid=1u;
    puts("READY QA4_SIMULATED_RAM_ONLY");fflush(stdout);
    while(fgets(line,sizeof line,stdin)){
        const char *p=line;size=0;
        if(!strncmp(line,"EXIT",4))break;
        if(!strncmp(line,"BUSY ",5)){
            song.playing=(uint8_t)(line[5]=='1');
            puts("OK");fflush(stdout);continue;
        }
        while(*p && *p!='\n' && size<sizeof wire_in){
            char *end; unsigned long value;
            while(*p==' '||*p=='\t')p++;
            if(!*p||*p=='\n')break;
            value=strtoul(p,&end,16);
            if(end==p||value>255u){size=0;break;}
            wire_in[size++]=(uint8_t)value;p=end;
        }
        if(size<8u||wire_in[0]!=0xF0u||wire_in[size-1u]!=0xF7u||
           wire_in[1]!=0x7Du||wire_in[2]!=0x46u||wire_in[3]!=0x4Cu||wire_in[4]!=96u){
            send_error();continue;
        }
        /* Sysex framing is checked above. Inside, use the actual FM-1
         * command dispatcher, including STOP/REC/busy/write gates. */
        {
            const uint32_t nr=dz_midi_request(wire_in+5u,size-6u,wire_out);
            unsigned j;
            if(!nr||nr>192u){send_error();continue;}
            printf("F0 7D 46 4C 60");
            for(j=0;j<nr;j++)printf(" %02X",wire_out[j]);
            puts(" F7");fflush(stdout);
        }
    }
    return 0;
}
