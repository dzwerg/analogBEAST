/* analogBEAST v1.2 Stage13: bank-free MUSIC DISK object codecs.
 * MIDI pitch 0 is valid: bit7 of flags marks NOTE PRESENT.
 * Compact disk codec, not an in-RAM bank mirror. */
#ifndef AB13_BANKFREE_H
#define AB13_BANKFREE_H
#include <stdint.h>
#define AB13_PATTERNS 256u
#define AB13_SONGS 64u
#define AB13_ALBUMS 4u
#define AB13_PRESETS 32u
#define AB13_MIN_STEPS 8u
#define AB13_MAX_STEPS 128u
#define AB13_MAX_ROWS 256u
#define AB13_ALBUM_SLOTS 32u
#define AB13_STEP_PRESENT 0x80u
#define AB13_PATTERN_MAX_WIRE (8u+AB13_MAX_STEPS*5u)
#define AB13_SONG_MAX_WIRE (8u+AB13_MAX_ROWS*8u)

typedef struct {uint8_t note,flags,velocity,gate,swing;} ab13_step;
typedef struct {uint8_t length,resolution;ab13_step steps[AB13_MAX_STEPS];} ab13_pattern;
typedef struct {uint16_t pattern_id,steps;int16_t value;uint8_t op,flags;} ab13_row;
typedef struct {uint16_t count;uint8_t loop;ab13_row row[AB13_MAX_ROWS];} ab13_song;
typedef struct {uint8_t count,loop;uint16_t song_id[AB13_ALBUM_SLOTS];} ab13_album;
/* op 0: play direct pattern ID; 1: END; 2: tempo; 3: rest;
 * 4: preset; 5: arp; 6: transpose. Song rows have NO bank field. */
#define AB13_OP_PATTERN 0u
#define AB13_OP_END 1u
#define AB13_OP_TEMPO 2u
#define AB13_OP_REST 3u
#define AB13_OP_PRESET 4u
#define AB13_OP_ARP 5u
#define AB13_OP_TRANSPOSE 6u
#define AB13_OP_BEGIN 7u /* loop marker; SONG only */
/* Reuse the existing A13 16-bit album step. Old S1..S32 and M1..M128 retain their bytes. */
#define AB13_ALBUM_PRESET_TAG 0x4000u
#define AB13_ALBUM_PRESET_VALUE_MASK 0x3FFFu
#define AB13_ALBUM_IS_PRESET(ref) (((ref)&0xC000u)==AB13_ALBUM_PRESET_TAG)
#define AB13_ALBUM_PRESET_REF(p) (AB13_ALBUM_PRESET_TAG|(uint16_t)(p))
void ab13_pattern_init(ab13_pattern *p,uint8_t len,uint8_t resolution);
int ab13_pattern_check(const ab13_pattern *p);
int ab13_song_check(const ab13_song *p);
int ab13_album_check(const ab13_album *p);
uint16_t ab13_song_loop_row(const ab13_song *s); /* after final valid BEGIN, else 0 */
uint32_t ab13_pattern_size(const ab13_pattern *p);
uint32_t ab13_song_size(const ab13_song *s);
uint32_t ab13_album_size(const ab13_album *a);
int ab13_pattern_emit(void *ctx,uint32_t off,void *dst,uint32_t n);
int ab13_song_emit(void *ctx,uint32_t off,void *dst,uint32_t n);
int ab13_album_emit(void *ctx,uint32_t off,void *dst,uint32_t n);
int ab13_pattern_parse(ab13_pattern *p,const uint8_t *src,uint32_t size);
int ab13_song_parse(ab13_song *p,const uint8_t *src,uint32_t size);
int ab13_album_parse(ab13_album *p,const uint8_t *src,uint32_t size);
#endif
