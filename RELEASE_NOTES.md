# analogBEAST 2.0 RC1 — pre-release

Stage 18 native build, firmware identity `FM-1_901`.

- Synth modes: KORG / BRUTE / ANALOG / LOFI / experimental B1.
- PRESETS UI: P01–P32 user slots + A01–A12 ANALOG + L01–L05 LOFI factory presets; engine auto-switch upon selecting factory sounds is included in code.
- 32 user slots and their flash layout unchanged. P18 SYNTH KEYBOARD / P19 PETZ MOOG protected as embedded source originals (live device state not backed up).
- USB source unchanged from USB8 original-composite fallback.
- Native build: app 256,536 B; static RAM 60,388 / 73,728 B; pool 302,048 / 368,640 B; fwsc 610,086 B.

**Known limitations:** B1 not yet a faithful Donner B1 implementation; the UI, presets and USB combinations need real-hardware validation. Do not treat RC1 as stable or a final release. Preserve your USB8 recovery image.
