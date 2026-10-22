# Contributing to analogBEAST / Mitmachen

Thank you for your interest in **analogBEAST**, open-source custom firmware and browser tools for the M-VAVE FM-1.

**Repository:** https://github.com/dzwerg/analogBEAST
**Issues:** https://github.com/dzwerg/analogBEAST/issues

## Report a bug / Fehler melden

Please include: (1) the firmware version, (2) hardware/MIDI/browser setup, (3) exact steps to reproduce, (4) expected vs. actual behavior, and (5) the relevant log or screenshot if available. State whether the problem occurs on **real hardware**, **the web emulator**, or **the web editor**.

Bitte immer Version, Hardware-/MIDI-Konfiguration, Reproduktionsschritte, erwartetes und tatsächliches Verhalten sowie relevante Logs angeben. Wichtig: Tritt das Problem auf echter Hardware, im Emulator oder im Browser-Editor auf?

## Feature requests / Ideen

Requests concerning **MIKROKORG / BRUTE synthesis**, the **Steiner-Parker-inspired filter**, pattern editing, **SONG MODE**, **ALBUM MODE**, USB MIDI, and the web apps are welcome. Explain the intended musical workflow and how the requested feature would operate on the physical FM-1 controls.

## Pull requests

- Keep changes focused and explain their behavior.
- Check firmware memory/flash limits and real-time constraints when touching synthesis or transport code.
- Run the applicable Python/Node/host tests and a firmware build where the toolchain is available.
- Do not include personal NOR backups, private songs/presets, device dumps, `.fwsc` binaries generated during development, or local build artifacts unless they are explicitly required for a reviewed release.
- Preserve original SPDX copyright lines, third-party licenses, and attribution.
- New code must be compatible with the project's **GPL-3.0** licensing.
- Do **not** change firmware package identity `FM-1_901` or the compatible loader name `Felucca Update` solely for branding.

**Upstream:** [Felucca](https://github.com/hugelton/Felucca) (Leo Kuroshita / Hügelton Instruments) and [Golana](https://github.com/Gadlele/Golana).
