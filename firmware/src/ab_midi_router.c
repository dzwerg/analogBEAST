/* SPDX-License-Identifier: GPL-3.0-only
 * analogBEAST v1.3 router, step 1: TRS -> USB MIDI real thru.
 * Included AFTER usb.c, BEFORE seq.c in Felucca's existing unity build.
 * Runs in the audio ISR alongside seq's MIDI input consumer. Only that ISR
 * writes midi_out_event(), preserving the existing SPSC queue ownership.
 * BLE and USB HOST drivers are not enabled by this step. */
#include "ab_midi_route_core.h"

/* Safe default: only input-only TRS is thru-routed to USB.
 * USB MIDI still feeds KORG/BRUTE through the existing midi_event() path.
 * BLE destination rows intentionally disabled until hardware output drivers exist. */
static ab_route_matrix_t ab_router = { .route = {
    [ABR_TRS-1u] = { [ABR_OUT_USB] = { 0xFFFFu, ABR_T_ALL, 1u } },
}};
static uint32_t ab_router_sent_usb, ab_router_dropped;
static void ab_router_audio_input(uint32_t packet, uint32_t src)
{
    uint32_t targets = ab_route_targets(&ab_router, src, packet);
    if (targets & (1u << ABR_OUT_USB)) {
        if (usb_midi_out_ready() && mo_w - mo_r < MQ) {
            midi_out_event(packet);
            ab_router_sent_usb++;
        } else ab_router_dropped++;
    }
}
