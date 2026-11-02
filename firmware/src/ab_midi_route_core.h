/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v1.3 early router core.
 * No queue, malloc, hardware access or flash; host-testable.
 * Source IDs match Felucca midi_in_source: USB=1, TRS=2; BLE-P=3, BLE-C=4 reserved.
 * MIDI-SysEx never goes through this channel-event router: OTA/editor USB path untouched. */
#ifndef AB_MIDI_ROUTE_CORE_H
#define AB_MIDI_ROUTE_CORE_H
#include <stdint.h>

enum { ABR_USB = 1, ABR_TRS = 2, ABR_BLE_P = 3, ABR_BLE_C = 4 };
enum { ABR_OUT_USB = 0, ABR_OUT_BLE_P = 1, ABR_OUT_BLE_C = 2, ABR_OUT_COUNT = 3 };
enum { ABR_T_NOTE = 1u<<0, ABR_T_CC = 1u<<1, ABR_T_PROGRAM = 1u<<2,
       ABR_T_PRESSURE = 1u<<3, ABR_T_BEND = 1u<<4, ABR_T_REALTIME = 1u<<5,
       ABR_T_OTHER = 1u<<6, ABR_T_ALL = 127u };
typedef struct { uint16_t channels; uint8_t types; uint8_t enabled; } ab_route_t;
typedef struct { ab_route_t route[4][ABR_OUT_COUNT]; } ab_route_matrix_t;

static uint32_t ab_route_type(uint32_t packet)
{
    uint32_t st = (packet >> 8) & 255u;
    if (st >= 0xF8u) return ABR_T_REALTIME;
    if (st >= 0xF0u) return ABR_T_OTHER;
    if (st < 0x80u) return 0;
    switch (st & 0xF0u) {
    case 0x80u: case 0x90u: return ABR_T_NOTE;
    case 0xB0u: return ABR_T_CC;
    case 0xC0u: return ABR_T_PROGRAM;
    case 0xA0u: case 0xD0u: return ABR_T_PRESSURE;
    case 0xE0u: return ABR_T_BEND;
    default: return ABR_T_OTHER;
    }
}
static uint32_t ab_route_targets(const ab_route_matrix_t *m, uint32_t src, uint32_t packet)
{
    uint32_t status, typ, ch, out, targets = 0;
    if (!m || src < ABR_USB || src > ABR_BLE_C) return 0;
    status = (packet >> 8) & 255u;
    typ = ab_route_type(packet);
    if (!typ) return 0;
    ch = status < 0xF0u ? status & 15u : 16u; /* system messages: no MIDI channel */
    for (out = 0; out < ABR_OUT_COUNT; out++) {
        const ab_route_t *r = &m->route[src - 1u][out];
        if (!r->enabled || !(r->types & typ) || (ch != 16u && !(r->channels & (1u << ch))))
            continue;
        /* no route back to its own physical connection */
        if ((src == ABR_USB && out == ABR_OUT_USB) ||
            (src == ABR_BLE_P && out == ABR_OUT_BLE_P) ||
            (src == ABR_BLE_C && out == ABR_OUT_BLE_C)) continue;
        targets |= 1u << out;
    }
    return targets;
}
#endif
