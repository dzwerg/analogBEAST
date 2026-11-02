#!/usr/bin/env python3
"""analogBEAST MD16 pattern importer over USB CDC-ACM (NOT OTA)."""
import argparse
import json
import sys
from pathlib import Path

MAX_ID = 512


def pattern(length, resolution=2):
    if not 8 <= length <= 128 or not 0 <= resolution <= 9:
        raise ValueError('length must be 8..128 and resolution 0..9')
    return {'length': length, 'resolution': resolution,
            'steps': [[0, 0, 96, 64, 0] for _ in range(length)]}


def build_json(path, start):
    obj = json.loads(Path(path).read_text(encoding='utf-8'))
    if 'patterns' not in obj or not isinstance(obj['patterns'], list):
        raise ValueError('expected {"patterns": [...]}')
    result = []
    for index, spec in enumerate(obj['patterns']):
        pid = int(spec.get('id', start + index))
        pat = pattern(int(spec.get('length', 16)), int(spec.get('resolution', 2)))
        for st in spec.get('steps', []):
            at = int(st['step'])
            if not 1 <= at <= pat['length']:
                raise ValueError(f'P{pid}: step {at} outside 1..{pat["length"]}')
            flags = int(st.get('flags', 0x80 if 'note' in st else 0))
            note = int(st.get('note', 0))
            vel = int(st.get('velocity', 96))
            gate = int(st.get('gate', 64))
            swing = int(st.get('swing', 0))
            pat['steps'][at - 1] = [note, flags, vel, gate, swing]
        result.append((pid, pat))
    return result


def build_midi(path, pid, length, resolution):
    try:
        import mido
    except ImportError as e:
        raise RuntimeError('pip install mido (for .mid files)') from e
    midi = mido.MidiFile(path)
    pat = pattern(length, resolution)
    pulses = midi.ticks_per_beat / 4.0
    if not pulses:
        raise ValueError('invalid MIDI PPQN')
    tick = 0
    active = {}
    notes = []
    for msg in mido.merge_tracks(midi.tracks):
        tick += msg.time
        if msg.type == 'note_on' and msg.velocity:
            key = (msg.channel, msg.note)
            if key in active:
                raise ValueError(f'retriggered note without note_off: {key}')
            active[key] = (tick, msg.velocity)
        elif msg.type == 'note_off' or (msg.type == 'note_on' and not msg.velocity):
            key = (msg.channel, msg.note)
            if key in active:
                when, vel = active.pop(key)
                notes.append((when, tick - when, msg.note, vel))
    if active:
        raise ValueError('MIDI contains hanging notes without note_off')
    occupied = set()
    for when, duration, pitch, vel in notes:
        step = int(when / pulses + 0.5)
        if not 0 <= step < length:
            raise ValueError(f'note at MIDI tick {when} is outside {length} steps')
        if step in occupied:
            raise ValueError(f'polyphony / multiple notes at step {step + 1}: cannot silently discard notes')
        occupied.add(step)
        gate = min(127, max(1, int(64 * duration / pulses + 0.5)))
        pat['steps'][step] = [pitch, 0x80, vel, gate, 0]
    if not notes:
        raise ValueError('MIDI contains no complete note pairs')
    return [(pid, pat)]


def wire(pat):
    b = bytearray(b'P13!')
    b.extend((pat['length'], pat['resolution'], 0, 0))
    for st in pat['steps']:
        if len(st) != 5:
            raise ValueError('each step must have five fields')
        n, f, v, g, sw = st
        if not (0 <= n <= 127 and (f == 4 or 0 <= f <= 3 or 128 <= f <= 131)
                and 0 <= v <= 127 and 0 <= g <= 127 and 0 <= sw <= 100):
            raise ValueError('invalid step note/flags/velocity/gate/swing')
        if f & 0x80 and (v == 0 or g == 0):
            raise ValueError('a sounding note needs positive velocity and gate')
        b.extend(st)
    return bytes(b)


def fnv32(data):
    hash_ = 2166136261
    for value in data:
        hash_ = ((hash_ ^ value) * 16777619) & 0xffffffff
    return f'{hash_:08X}'


class CDC:
    def __init__(self, name):
        try:
            import serial
        except ImportError as e:
            raise RuntimeError('pip install pyserial') from e
        self.dev = serial.Serial(name, 115200, timeout=2, write_timeout=3)
        self.dev.dtr = True
        self.dev.reset_input_buffer()

    def close(self):
        self.dev.close()

    def call(self, command, okprefix):
        self.dev.write((command + '\n').encode('ascii'))
        # Console echoes inputs and prompts; only an explicit IM* result counts.
        from time import monotonic
        end = monotonic() + 12.0
        while monotonic() < end:
            line = self.dev.readline().decode('ascii', 'replace').strip()
            if line.startswith('IMERR '):
                raise RuntimeError(f'{command.split()[0]} failed: {line}')
            if line.startswith(okprefix):
                return line
        raise TimeoutError(f'no device ACK for {command.split()[0]}. STOP and inspect COM port.')


def write_all(dev, patterns, overwrite):
    for pid, pat in patterns:
        print(f'P{pid}: {pat["length"]} steps, importing ...', flush=True)
        try:
            dev.call(f'imbegin {pid} {pat["length"]} {pat["resolution"]} {int(overwrite)}', f'IMBEGIN OK {pid}')
            for i, step in enumerate(pat['steps'], 1):
                dev.call('imstep ' + str(i) + ' ' + ' '.join(map(str, step)), f'IMSTEP OK {i}')
            result = dev.call('imcommit', f'IMCOMMIT OK {pid} ')
            actual = result.split()[-1]
            expected = fnv32(wire(pat))
            if actual != expected:
                raise RuntimeError(f'P{pid}: flash readback hash {actual} != host {expected}')
            print(f'P{pid}: saved and verified ({actual})')
        except Exception:
            try:
                dev.call('imabort', 'IMABORT OK')
            except Exception:
                pass
            raise


def main(argv=None):
    a = argparse.ArgumentParser(description='USB-CDC MUSIC DISK importer (no OTA, no format)')
    a.add_argument('source', type=Path, help='.musicbox.json or .mid MIDI file')
    a.add_argument('--port', help='Windows COMx or Linux /dev/ttyACMx')
    a.add_argument('--start', type=int, default=129, help='starting pattern ID, default 129')
    a.add_argument('--length', type=int, default=16, help='MIDI steps 8..128 (default 16)')
    a.add_argument('--resolution', type=int, default=2, help='pattern resolution (default 2 = 1/16)')
    a.add_argument('--overwrite', action='store_true', help='explicitly allow replacing an existing pattern')
    a.add_argument('--dry-run', action='store_true', help='validate content, no USB writes')
    a.add_argument('--list-ports', action='store_true', help='list USB-CDC COM ports')
    args = a.parse_args(argv)
    if args.list_ports:
        try:
            from serial.tools import list_ports
        except ImportError as e:
            raise SystemExit('pip install pyserial') from e
        for p in list_ports.comports():
            print(f'{p.device}: {p.description}')
        return 0
    if args.source.suffix.lower() in ('.mid', '.midi'):
        patterns = build_midi(args.source, args.start, args.length, args.resolution)
        print('Note: MIDI tempo is NOT stored in pattern data; choose tempo on device / in Song.')
    else:
        patterns = build_json(args.source, args.start)
    ids = [p for p, _ in patterns]
    if not patterns or len(set(ids)) != len(ids) or any(i < 1 or i > MAX_ID for i in ids):
        raise ValueError('IDs must be unique in P1..P512 and nonempty')
    for pid, p in patterns:
        buf = wire(p)
        print(f'P{pid}: {len(buf)} bytes, {len(p["steps"])} steps, hash {fnv32(buf)}')
    if args.dry_run:
        print('DRY RUN PASS: no USB traffic and no FLASH changes')
        return 0
    if not args.port:
        raise ValueError('explicit --port COMx required. Use --list-ports to find device.')
    dev = CDC(args.port)
    try:
        write_all(dev, patterns, args.overwrite)
    finally:
        dev.close()
    print('IMPORT COMPLETE')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, RuntimeError, TimeoutError) as exc:
        print('ERROR:', exc, file=sys.stderr)
        sys.exit(1)
