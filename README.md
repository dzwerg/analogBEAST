# analogBEAST v1.0

**Two synthesis routings. One powerful sequencer. Complete songs and albums.**
**Zwei Synth-Routings. Ein leistungsfähiger Sequencer. Komplette Songs und Albums.**

Alternative open-source synthesizer and sequencer firmware for the **M-VAVE FM-1**.

[Website](https://dzwerg.github.io/analogBEAST/) · [Synth Studio](https://dzwerg.github.io/analogBEAST/webapp/editor/) · [Web Emulator](https://dzwerg.github.io/analogBEAST/emu/) · [Web Installer](https://dzwerg.github.io/analogBEAST/webapp/installer/) · [Download v1.0](https://github.com/dzwerg/analogBEAST/releases/tag/v1.0)

**Documentation:** [English](README.en.md) · [Deutsch](README.de.md) · [Release notes](docs/RELEASE_NOTES-v1.0.md)

## At a glance / Auf einen Blick

| Feature / Merkmal | analogBEAST v1.0 |
|---|---|
| Synthesis / Synthese | **MIKROKORG** and **BRUTE** — two distinct DSP signal paths within GOLANA |
| BRUTE filter / Filter | **Digital Steiner-Parker-inspired**, low-pass / band-pass / high-pass, nonlinear saturation |
| Sound programs / Klangprogramme | **32** (P01–P32) |
| Pattern banks / Pattern-Bänke | **8** (A–H), 16 patterns per bank = **128 patterns** |
| Banks A–D / Bänke A–D | Up to **64 steps** per pattern |
| Banks E–H / Bänke E–H | Up to **16 steps** per pattern |
| SONG MODE | **32 songs**, up to **64 rows/commands** each |
| ALBUM MODE | **4 albums**, up to **32 song entries** each |
| Song commands / Befehle | PATTERN, TEMPO, PRESET, REST, ARP, END |
| Browser tools / Browser-Werkzeuge | Synth Studio, graphical SONG/ALBUM editor, MIDI-song import, WebAssembly emulator, USB web installer |
| MIDI | USB MIDI; Program Change P01–P32; pattern/song transfer tools |
| Firmware package identity | `FM-1_901` (kept for updater compatibility) |

## English — what makes analogBEAST different?

analogBEAST turns the FM-1 into a small **instrument and song-performance workstation**. Its **MIKROKORG** routing offers a familiar virtual-analog structure with oscillator sync/ring modulation, filters and modulation. Switch to **BRUTE** for a genuinely different oscillator-mixing and filter path: ULTRASAW, PWM, metallic triangle wavefolding, a sub/overtone oscillator, **Brute Factor** feedback and a **nonlinear digital Steiner-Parker-inspired LP/BP/HP filter**. These are two different routings **inside the same GOLANA engine**, not two separate plug-in engines.

The sequencer goes far beyond a short loop. **Eight banks and 128 pattern slots** provide both 64-step phrases and compact 16-step parts. **SONG MODE** chains patterns and commands into complete pieces; **ALBUM MODE** plays prepared groups of songs. A browser-based **Synth Studio** supplies sound and arpeggiator controls, graphical pattern/song/album editing and MIDI import.

**[Read the complete English documentation →](README.en.md)**

## Deutsch — mehr als ein Pattern-Looper

analogBEAST macht aus dem FM-1 einen eigenständigen **Synthesizer, Hardware-Sequencer und Song-Player**. Das **MIKROKORG-Routing** liefert klassische virtuell-analoge Klanggestaltung. Das **BRUTE-Routing** verwendet einen anderen Oszillator-Mix und einen eigenen Filter-Signalweg mit **ULTRASAW, PWM, METAL-artigem Wavefolding, Sub/Obertönen, Brute Factor und einem digital modellierten Steiner-Parker-inspirierten LP/BP/HP-Filter**. Beide Routings gehören zur **gleichen GOLANA-Engine**.

Der Sequencer bietet **acht Bänke mit insgesamt 128 Pattern-Plätzen**. Lange 64-Step-Phrasen und kurze 16-Step-Patterns lassen sich im **SONG MODE** zu vollständigen Musikstücken kombinieren. Mit dem **ALBUM MODE** werden daraus ganze Abspielfolgen. Für Sound, ARP, Pattern, Songs, Albums und MIDI-Import gibt es einen grafischen Browser-Editor.

**[Vollständige deutsche Dokumentation lesen →](README.de.md)**

## Try it in your browser / Direkt im Browser

- **[Synth Studio / Webeditor](https://dzwerg.github.io/analogBEAST/webapp/editor/):** sound programs P01–P32, BRUTE/MIKROKORG parameters, ARP, graphical pattern/SONG/ALBUM editing and MIDI import.
- **[Web Emulator](https://dzwerg.github.io/analogBEAST/emu/):** WebAssembly FM-1 simulation; no hardware required.
- **[Web Installer](https://dzwerg.github.io/analogBEAST/webapp/installer/):** USB firmware update interface; requires a compatible Chrome/Edge browser and MIDI/SysEx permissions. **Back up device data first.** Verify device detection before writing.
- **[GitHub v1.0 Release](https://github.com/dzwerg/analogBEAST/releases/tag/v1.0):** verified `.fwsc` package and emulator ZIP.

## Build and safety

The firmware uses the original **`FM-1_901`** package identity and the compatible `Felucca Update` loader name. Do not change either solely for branding. The original firmware sources, third-party credits, and licensing notices are retained. The included firmware package is for M-VAVE FM-1 hardware; installation is at your own risk. Preserve backups of any personal presets, patterns, songs and projects.

**License:** GPL-3.0. Based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments and [Golana](https://github.com/Gadlele/Golana). Other third-party license details are in [LICENSING.md](LICENSING.md) and [LICENSES/](LICENSES/). analogBEAST is not affiliated with or endorsed by M-VAVE, Arturia, or Korg. "Steiner-Parker", "microKORG", and "Brute" are descriptive references, not claims of hardware or circuit-level equivalence.
