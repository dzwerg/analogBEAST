#!/usr/bin/env python3
"""analogBEAST FM-1 USB-CDC importer: true P1-P512 or independent M1-x.
No OTA, no formatting, no factory ROM import. Only works with MBOX USB firmware.
M1 v1 = name + PPQN + tempo + raw ordered MIDI event stream, NOT P-patterns.
"""
import argparse
import json
import sys
import time
import zlib
from pathlib import Path

TPQ = 96
MAX_M_EVENTS = 96 * 80


def b6(delta, op, chan, a, b):
    if not all(0 <= v <= 255 for v in (op, chan, a, b)) or not 0 <= delta <= 65535:
        raise ValueError('event out of range')
    return delta.to_bytes(2, 'little') + bytes((op, chan, a, b))


def mbox_from_json(path):
    raw = json.loads(Path(path).read_text(encoding='utf-8'))
    if raw.get('type') != 'M1':
        raise ValueError('expect {"type":"M1", "name":..., "bpm":..., "events": [...]}')
    name = raw['name'].encode('utf-8')
    bpm = int(raw.get('bpm', 120))
    tpq = int(raw.get('tpq', TPQ))
    if not 1 <= len(name) <= 32 or not 40 <= bpm <= 300 or not 24 <= tpq <= 960:
        raise ValueError('bad M1 name (1..32 UTF-8 bytes), tempo or TPQ')
    events = bytearray()
    for ev in raw['events']:
        events.extend(b6(int(ev['delta']), int(ev['op'], 0) if isinstance(ev['op'], str) else int(ev['op']),
                         int(ev.get('channel', 0)), int(ev['a']), int(ev.get('b', 0))))
    return name, bpm, tpq, bytes(events)


def mbox_from_mid(path):
    try:
        import mido
    except ImportError as exc:
        raise RuntimeError('Install MIDI reader: py -m pip install mido') from exc
    mid = mido.MidiFile(str(path))
    if not mid.ticks_per_beat:
        raise ValueError('MIDI TPQ is zero')
    # Preserve original MIDI tick timing whenever the source PPQN fits M1.
    # Higher-PPQN files are resampled to 960 ticks/quarter, never to 16 steps.
    output_tpq = mid.ticks_per_beat if 24 <= mid.ticks_per_beat <= 960 else 960
    merged = mido.merge_tracks(mid.tracks)
    abs_in = 0
    last = 0
    bpm = 120
    out = bytearray()
    unsupported = set()
    for msg in merged:
        abs_in += msg.time
        at = (abs_in * output_tpq + mid.ticks_per_beat // 2) // mid.ticks_per_beat
        op = None
        ch = getattr(msg, 'channel', 0)
        a = b = 0
        if msg.type == 'note_on':
            op, a, b = (0x90 if msg.velocity else 0x80), msg.note, msg.velocity
        elif msg.type == 'note_off':
            op, a, b = 0x80, msg.note, msg.velocity
        elif msg.type == 'program_change':
            op, a = 0xC0, msg.program
        elif msg.type == 'control_change':
            op, a, b = 0xB0, msg.control, msg.value
        elif msg.type == 'pitchwheel':
            op = 0xE0
            bend = int(msg.pitch) + 8192
            a, b = bend & 127, (bend >> 7) & 127
        elif msg.type == 'aftertouch':
            op, a = 0xD0, msg.value
        elif msg.type == 'polytouch':
            op, a, b = 0xA0, msg.note, msg.value
        elif msg.type == 'set_tempo':
            rounded = int(round(mido.tempo2bpm(msg.tempo)))
            if not 40 <= rounded <= 300:
                raise ValueError(f'MIDI tempo {rounded} outside 40..300 BPM')
            if at == 0 and not out:
                bpm = rounded
                continue
            op, ch, a, b = 0xF1, 0, rounded & 255, rounded >> 8
        elif msg.is_meta:
            continue
        else:
            unsupported.add(msg.type)
            continue
        delta = at - last
        if delta < 0:
            raise ValueError('non-monotonic MIDI tick order')
        while delta > 65535:
            out.extend(b6(65535, 0xF2, 0, 0, 0))
            delta -= 65535
        out.extend(b6(delta, op, ch, a, b))
        last = at
    if unsupported:
        raise ValueError('Cannot preserve unsupported MIDI messages: ' + ', '.join(sorted(unsupported)))
    if not out:
        raise ValueError('empty or unsupported MIDI file')
    name = path.stem.encode('utf-8')
    if len(name) > 32:
        raise ValueError('filename exceeds 32 UTF-8 bytes; rename MIDI or use M1 JSON')
    return name, bpm, output_tpq, bytes(out)


def validate_events(data):
    if not data or len(data) % 6 or len(data) // 6 > MAX_M_EVENTS:
        raise ValueError(f'M1 must contain 1..{MAX_M_EVENTS} 6-byte events')
    for i in range(0, len(data), 6):
        op, ch, a, b = data[i+2:i+6]
        if ch > 15:
            raise ValueError(f'bad channel in event #{i//6}')
        valid = ((op == 0x90 and a <= 127 and 1 <= b <= 127) or
                 (op in (0x80, 0xB0, 0xE0, 0xA0) and a <= 127 and b <= 127) or
                 (op in (0xC0, 0xD0) and a <= 127 and b == 0) or
                 (op == 0xF1 and 40 <= a + (b << 8) <= 300) or
                 (op == 0xF2 and ch == a == b == 0))
        if not valid:
            raise ValueError(f'bad event #{i//6}: {data[i:i+6].hex()}')


class Port:
    def __init__(self, name):
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError('Install USB serial: py -m pip install pyserial') from exc
        self.port = serial.Serial(name, 115200, timeout=1.0, write_timeout=3.0)
        self.port.dtr = True
        self.port.reset_input_buffer()

    def close(self):
        self.port.close()

    def send(self, cmd, prefix):
        self.port.write((cmd + '\n').encode('ascii'))
        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline:
            s = self.port.readline().decode('utf-8', 'replace').strip()
            if s.startswith(('MERR ', 'IMERR ')):
                raise RuntimeError(s)
            if s.startswith(prefix):
                return s
        raise TimeoutError('USB-CDC did not acknowledge ' + cmd.split()[0])


def send_m1(port, slot, name, bpm, tpq, data, overwrite):
    validate_events(data)
    crc = zlib.crc32(data)
    port.send(f'mbegin {slot} {bpm} {tpq} {len(data)//6} {crc} {int(overwrite)}', f'MBEGIN OK {slot}')
    try:
        for i in range(0, len(name), 16):
            port.send(f'mname {i} {name[i:i+16].hex()}', 'MNAME OK')
        for i in range(0, len(data), 18):
            block = data[i:i+18]
            port.send('mdata ' + block.hex(), f'MDATA OK {(i+len(block))//6}')
        ack = port.send('mcommit', f'MCOMMIT OK {slot} ')
        if ack.split()[-1].upper() != f'{crc:08X}':
            raise RuntimeError('device CRC disagreement: ' + ack)
    except Exception:
        try:
            port.send('mabort', 'MABORT OK')
        except Exception:
            pass
        raise


def main():
    parser = argparse.ArgumentParser(description='analogBEAST M1 musicbox USB importer (independent of P patterns)')
    parser.add_argument('file', nargs='?', type=Path, help='.mid, .midi, or .musicbox.json')
    parser.add_argument('--m1', type=int, help='M1-x song number, 1..128')
    parser.add_argument('--port', help='Windows COM port, e.g. COM5')
    parser.add_argument('--overwrite', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--list-ports', action='store_true')
    args = parser.parse_args()
    if args.list_ports:
        from serial.tools import list_ports
        for p in list_ports.comports():
            print(p.device, p.description)
        return
    if not args.file or not args.m1 or not 1 <= args.m1 <= 128:
        parser.error('file and --m1 1..128 required')
    name, bpm, tpq, data = (mbox_from_mid(args.file) if args.file.suffix.lower() in ('.mid', '.midi')
                           else mbox_from_json(args.file))
    validate_events(data)
    print(f'M1-{args.m1}: {name.decode("utf-8")!r}, tempo {bpm} BPM, {tpq} PPQN, '
          f'{len(data)//6} events, data {len(data)} bytes, CRC {zlib.crc32(data):08X}')
    print('The MUSIC DISK is not formatted and no pattern P is touched.')
    if args.dry_run:
        print('DRY RUN PASS – no FM-1 writes')
        return
    if not args.port:
        parser.error('--port COMx required (use --list-ports)')
    port = Port(args.port)
    try:
        send_m1(port, args.m1, name, bpm, tpq, data, args.overwrite)
    finally:
        port.close()
    print(f'M1-{args.m1} import committed and acknowledged')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, RuntimeError, OSError, TimeoutError) as exc:
        sys.exit('ERROR: ' + str(exc))
