# analogBEAST 2.0 RC2

Experimental pre-release for M-VAVE FM-1.

## Changes since 1.1

- MUSIC DISK MD16: 292 KiB NOR filesystem, CRC32, append-only
  records and guarded persistent pattern/song/album storage.
- Compact RAM index supporting pattern IDs 1-512.
- Expanded sequencer, arranger, MIDI and musicbox tooling.
- Five synthesis modes: KORG, BRUTE, ANALOG, LOFI and
  experimental B1.
- Preset browser: P01-P32, A01-A12 and L01-L05.
- USB composite audio/MIDI and device/host development.
- Preserved original P18/P19 preset source data.

## RC2 changes

- New firmware identity: FM-1_947.
- Update-loader identity: ota-FM-1_947.
- Hub metadata in fmhub.json.
- Native JieLi build completed successfully.
- Successfully flashed from FM-1_901 on real FM-1.
- Restart and running identity FM-1_947 confirmed.

## Known limitations

- Experimental B1 engine, not a faithful Donner B1 emulation.
- MUSIC DISK and USB combinations require further hardware testing.
- Reinstalling another firmware from RC2 has NOT yet been tested.
- The Firmware Hub's full installation and recovery validation
  requirements have NOT yet been completed.
- Back up user data before changing firmware.
- Device-side MD16 garbage collection is not enabled in Stage 18.

Firmware SHA256:
0BE3BCC8C43F7E801FD5D4F7D3E776CEA425C4EC293D08377C91BC4770E2558C