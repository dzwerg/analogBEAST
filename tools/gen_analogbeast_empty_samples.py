#!/usr/bin/env python3
"""Make zero ADPCM resources for GOLANA-only analogBEAST firmware.

All sample zones are nonplaying. The old identifiers are temporary ABI
compatibility until the retired sample engine is removed completely.
"""
import os
import sys
from pathlib import Path
if len(sys.argv) != 2:
    raise SystemExit('usage: gen_analogbeast_empty_samples.py OUT_HEADER')
root=Path(__file__).resolve().parents[1]
core=(root/'firmware/src/core.h').read_text()
if '#define GOLANA 1' not in core or os.environ.get('GOLANA') in {'0', 'false', 'False'}:
    raise SystemExit('STOP: minimal samples require GOLANA=1, only KORG/BRUTE')
p=Path(sys.argv[1]);p.parent.mkdir(parents=True,exist_ok=True)
p.write_text('''/* No ADPCM samples in analogBEAST KORG/BRUTE build. */
#pragma once
#include <stdint.h>
static const uint8_t SMP_DATA[1] = {0};
static const smp_zone_t SMP_ZONES[1] = {{0}};
static const smp_set_t SMP_SETS[1] = {{"NONE", 0, 1}};
#define SMP_NSETS 1
#define SMP_NPRESETS 1
static const preset_t SMP_PRESET_TABLE[1] = {{"NONE", {0, 0, 0, 1, 127, 0, 0, 0}, {5, 80, 100, 50}, 0, 0, {0,0,0,0}, 0}};
static const uint8_t SMP_SET_ORIG[1] = {0};
#define SMP_NALIAS 0
#define SMP_SET_NAMES_INIT "NONE"
static const char *const SMP_SET_NAMES[1] = {"NONE"};
/* Legacy SLICE not compiled in GOLANA; keep declaration-only macros. */
#define SLC_BREAK_BPM 120
#define SLC_BREAK_STEPS 16
#define SLC_BREAK_INIT {0}
#define SLC_PIANO_INIT {0}
''')
print('samples: 0 sets, no ADPCM, empty zone compatibility ->',p)
