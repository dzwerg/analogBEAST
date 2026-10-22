# analogBEAST v1.0 — Full Documentation (English)

**Custom synthesizer and sequencer firmware for the M-VAVE FM-1**

[Home](https://dzwerg.github.io/analogBEAST/) · [Synth Studio](https://dzwerg.github.io/analogBEAST/webapp/editor/) · [Web Emulator](https://dzwerg.github.io/analogBEAST/emu/) · [Web Installer](https://dzwerg.github.io/analogBEAST/webapp/installer/) · [Release v1.0](https://github.com/dzwerg/analogBEAST/releases/tag/v1.0) · [Deutsch](README.de.md)

## Beyond a single synth patch

**analogBEAST** turns the M-VAVE FM-1 into a compact **synthesizer, hardware sequencer and song/album performance player**. It focuses on characterful sound design, direct on-device operation, and the ability to arrange **complete pieces of music — even entire playlists — without a running computer**.

It builds on the open-source **Felucca** and **Golana** projects. Upstream copyright notices, licenses, and compatibility identifiers are preserved.

## Two distinct synthesis routings

The GOLANA synth engine offers two distinctly different **DSP signal paths**: **MIKROKORG** and **BRUTE**. These are not merely preset banks, but they are also **not two separately installed synth engines**. They are selectable architectures within the same GOLANA engine.

### MIKROKORG — classic virtual-analog direction

- Two oscillators with selectable waveforms and mixing.
- Oscillator sync and ring modulation.
- Cutoff, resonance and filter envelope.
- Amp envelope, LFOs and modulation assignments.
- Unison, glide and performance features.
- Delay, reverb and modulation effects.

This path is suited to classic virtual-analog basses, leads, pads and animated textures.

### BRUTE — a different oscillator and filter path

The BRUTE path switches to a different synthesis structure featuring:

- **SAW / ULTRASAW:** additional slightly detuned sawtooth components.
- **PULSE / PWM:** variable pulse width.
- **METAL:** metallic triangle wavefolding.
- **SUB / OVERTONES:** an additional lower or higher oscillator contribution.
- **BRUTE FACTOR:** controllable internal synthesis feedback.
- **Steiner-Parker-inspired filter:** nonlinear digital LP/BP/HP processing.

### The Steiner-Parker-inspired filter

A signature part of BRUTE is the **digital two-pole Steiner-Parker-inspired filter** with **low-pass, band-pass and high-pass** responses. It uses deliberately nonlinear feedback and saturation for a lively, assertive sound distinct from the MIKROKORG filter path. This is a **digital interpretation of the filter concept**, not a transistor- or diode-accurate circuit emulation.

Combine the filter with Brute Factor, ULTRASAW, PWM and the metallic wavefolder for driven basses, cutting leads, intense sweeps and rhythmic sound experiments.

## 32 sound programs

The device has **P01–P32** program slots. P01–P16 initially load GOLANA factory sounds unless overwritten with user sounds; P17–P32 are additional INIT/user program positions. **MIDI Program Change 0–31** selects P01–P32.

Parameters are editable on the hardware and through the Web Studio. **Permanent writing of a sound program is initiated with SAVE on the FM-1**; live editor changes should not silently overwrite a stored program.

## Eight pattern banks — 128 patterns

| Bank | Slots | Maximum steps |
|---|---|---|
| **A** | A01–A16 | **64** |
| **B** | B01–B16 | **64** |
| **C** | C01–C16 | **64** |
| **D** | D01–D16 | **64** |
| **E** | E01–E16 | **16** |
| **F** | F01–F16 | **16** |
| **G** | G01–G16 | **16** |
| **H** | H01–H16 | **16** |

**Eight banks × 16 slots = 128 patterns.** Use the longer banks for full melodic phrases and the shorter banks for grooves, accents, fills and variations. Editing includes individual notes, velocity, gate, swing, step types/flags and rhythmic resolution, with hardware REC pages and a graphical browser editor.

## SONG MODE — 32 full arrangements

Compose **up to 32 songs**, each with **up to 64 entries/commands**. The SONG data model supports:

- **PATTERN:** play the chosen bank/pattern.
- **TEMPO:** change tempo inside a song.
- **PRESET:** switch sound programs.
- **REST:** insert a pause.
- **ARP:** control the arpeggiator.
- **END:** mark the song's end.
- **ONCE / REPEAT:** single-run or repeating playback modes.

Arrange intros, verses, choruses, transitions and endings as an ordered sequence. Song mode is designed to perform a complete piece rather than repeatedly playing just one pattern.

## ALBUM MODE — playlists on the instrument

**Four album slots** can each contain **up to 32 song entries**. Put songs in a chosen order and control the end/loop behavior. This is useful for live sets, demonstrations, song collections and prepared performances. Once stored on the hardware, playback does not require a PC to stay connected.

## Arpeggiator and performance

The arpeggiator provides multiple patterns, octave range, rhythmic subdivision and step masks. Synth Studio exposes sound-related ARP settings, while the hardware also offers the relevant performance pages.

## USB MIDI and MIDI song import

USB MIDI integrates analogBEAST with computers and other MIDI-capable devices. Transfer tools support working with pattern and song data. The graphical web editor can import **Standard MIDI Files**, preserving note and tempo information and dividing material into supported 16- and 64-step patterns.

**[Synth Studio / graphical editor](https://dzwerg.github.io/analogBEAST/webapp/editor/)** — SOUND with P01–P32, synth and ARP controls, graphical pattern/SONG/ALBUM editing and MIDI-song import.

**[WebAssembly emulator](https://dzwerg.github.io/analogBEAST/emu/)** — Run the simulated FM-1 firmware in a modern browser without connected hardware.

**[USB web installer](https://dzwerg.github.io/analogBEAST/webapp/installer/)** — Install firmware using a compatible Web MIDI/SysEx browser such as Chrome or Edge. **Back up all personal device content and confirm device detection before flashing.**

## Download, identity and safety

- [GitHub Release v1.0](https://github.com/dzwerg/analogBEAST/releases/tag/v1.0)
- [Firmware package `.fwsc`](https://github.com/dzwerg/analogBEAST/releases/download/v1.0/analogBEAST-v1.0.fwsc)
- [Web emulator ZIP](https://github.com/dzwerg/analogBEAST/releases/download/v1.0/analogBEAST-v1.0-webemu.zip)

The firmware intentionally retains package identity **`FM-1_901`** and the compatible **`Felucca Update`** bootloader label. Do not change either just to rebrand the project. Flashing custom firmware is done at your own risk; keep backups of user presets, patterns, songs and projects.

## Credits, licenses and trademarks

**GPL-3.0.** Based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments and [Golana](https://github.com/Gadlele/Golana). See [LICENSING.md](LICENSING.md) and [LICENSES/](LICENSES/) for additional component attributions. Original copyright notices and license information are preserved.

This is an independent community project, not affiliated with M-VAVE, Arturia or Korg. Product and filter names describe sonic inspiration only; they do not claim compatibility with, or exact digital cloning of, analog hardware circuits. No warranty is provided.
