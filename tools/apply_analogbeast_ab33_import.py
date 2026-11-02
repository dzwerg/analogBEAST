#!/usr/bin/env python3
"""analogBEAST ab3.2 -> ab3.3: MIDI song transfer and tied note gate repair.
--check is read-only, --apply makes backups, --verify checks exact installed content.
"""
from pathlib import Path
import argparse, hashlib, sys

ROOT = Path(__file__).resolve().parent.parent
SRC = Path(__file__).resolve().with_name('golana_dz_midi_v3.c')
RELS = ('firmware/src/golana_dz_midi.c','firmware/src/seq.c')
# The expected ab3.2 source is pinned by SHA-256 after normalizing line endings.
BASE_HASH = {
    'firmware/src/golana_dz_midi.c': '070ead41f55e70975ba2ea697380a64f6f680483d4c461f174e98c489e6bc2a7',
    'firmware/src/seq.c': '9dc5a64288e3b74965c5bcb723650067b1dddbe705872fc50bcc130eca8e93d7',
}
INSERT_BEFORE = '''#define SEQ_REP (1u << 16)'''
INSERT_DECL = '''#if GOLANA
/* A playing DZ song uses the note-duration aware TIE gate below. */
static uint8_t dz_play_has_backup;
#endif
'''
TIE_OLD = '''    if (s->time == ST_TIE) {
        if (t->seq_n) {
            t->seq_off = gate + period / 2u;
            t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie;   /* chains hold at any GATE / swing */
        }
        return;
    }
'''
TIE_NEW = '''    if (s->time == ST_TIE) {
        if (t->seq_n) {
#if GOLANA
            if (t == &trk[0] && dz_play_has_backup && !next_tie) {
                /* A NOTE followed by TIEs has a gate over its ENTIRE
                 * duration. The final tie must release at the correct
                 * fraction of the final 1/16 step, not 1.5 steps late. */
                uint32_t run = 1u, at = idx;
                while (run < len) {
                    at = (at + len - 1u) % len;
                    if (seq_steps(t)[at].time == ST_TIE) { run++; continue; }
                    if (seq_steps(t)[at].time == ST_NOTE) run++;
                    break;
                }
                {
                    uint32_t duration = run * period * (uint32_t)t->p[P_SGATE] / 128u;
                    uint32_t elapsed = (run - 1u) * period;
                    t->seq_off = duration > elapsed ? duration - elapsed : 1u;
                }
                t->seq_hold = 0u;
            } else
#endif
            {
                t->seq_off = gate + period / 2u;
                t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie;
            }
        }
        return;
    }
'''

def norm(b): return b.replace(b'\r\n',b'\n').replace(b'\r',b'\n')
def sha(b): return hashlib.sha256(norm(b)).hexdigest()
def convert(b, lineend): return b.replace(b'\n', b'\r\n') if lineend == b'\r\n' else b

def expected():
    midi_v3=SRC.read_bytes()
    seq=norm((ROOT/RELS[1]).read_bytes())
    orig=seq
    if INSERT_DECL.encode() in seq and TIE_NEW.encode() in seq:
        return {RELS[0]:norm(midi_v3),RELS[1]:seq}
    if seq.count(INSERT_BEFORE.encode()) != 1 or seq.count(TIE_OLD.encode()) != 1:
        raise RuntimeError('seq.c passt nicht zur ab3.2-Basis; keine Änderungen')
    seq=seq.replace(INSERT_BEFORE.encode(), (INSERT_DECL+INSERT_BEFORE).encode(),1)
    seq=seq.replace(TIE_OLD.encode(),TIE_NEW.encode(),1)
    assert seq!=orig
    return {RELS[0]:norm(midi_v3),RELS[1]:seq}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    x=parser.add_mutually_exclusive_group(required=True)
    x.add_argument('--check',action='store_true');x.add_argument('--apply',action='store_true');x.add_argument('--verify',action='store_true')
    args=parser.parse_args()
    eng=ROOT/'firmware/src/eng_golana.c'
    bak=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not eng.exists() or not bak.exists() or eng.read_bytes()!=bak.read_bytes():
        raise RuntimeError('BRUTE ist nicht byte-identisch mit dev5; STOP')
    print('PASS: BRUTE DSP dev5 byte-identisch')
    originals={k:(ROOT/k).read_bytes() for k in RELS}
    targets=expected()
    for k in RELS:
        now=originals[k]
        if norm(now)==targets[k]:
            print('ALREADY',k)
        elif sha(now)==BASE_HASH[k]:
            print('READY ',k)
        else:
            print('EXPECTED SHA:',BASE_HASH[k]); print('ACTUAL SHA:  ',sha(now))
            raise RuntimeError(f'unbekannte Quellversion {k}; kein Patch')
    if args.verify:
        if not all(norm(originals[k])==targets[k] for k in RELS):
            raise RuntimeError('ab3.3 noch nicht installiert')
        print('PASS: ab3.3 installiert'); return
    if args.check:
        print('CHECK ONLY: nichts verändert'); return
    backups=[]
    for k in RELS:
        now=originals[k]
        if norm(now)==targets[k]: continue
        dest=ROOT/k
        backup=dest.with_name(dest.name+'.before-ab33-import')
        if backup.exists(): raise RuntimeError('Backup bereits vorhanden: '+str(backup))
        backups.append((dest,backup,now,targets[k]))
    for dest,backup,now,new in backups:
        backup.write_bytes(now)
        crlf=b'\r\n' if b'\r\n' in now else b'\n'
        dest.write_bytes(convert(new,crlf));print('PATCHED:',dest.relative_to(ROOT))
    print('PASS: ab3.3-Importer-Patch angewendet; Tests und Build stehen noch aus')

if __name__=='__main__':
    try: main()
    except Exception as e: sys.exit('ERROR: '+str(e))
