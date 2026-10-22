# Releasing analogBEAST / Veröffentlichungen

## Published v1.0

- Repository: https://github.com/dzwerg/analogBEAST
- GitHub release: https://github.com/dzwerg/analogBEAST/releases/tag/v1.0
- Website: https://dzwerg.github.io/analogBEAST/
- Firmware package identity: `FM-1_901`

The original v1.0 Git tag and `.fwsc` / emulator release assets were published before this documentation update. **Documentation-only updates must not move the existing `v1.0` tag or rebuild/replace release binaries.** The existing `.github/workflows/release.yml` originated upstream; review and adapt its tags, website configuration, and source references before relying on it for analogBEAST releases.

## Source and website layout

- `main`: firmware sources, web sources, documentation and tests.
- `gh-pages`: generated static website (`index.html`, `emu/`, `webapp/editor/`, `webapp/installer/`, `firmware/`).
- `build/pages-site`: local, separate Git working directory for the `gh-pages` branch, *not* the firmware source repository.

## Future release checklist

1. Validate current firmware source, emulator source and browser assets.
2. Run tests and perform a fresh build with the supported toolchain.
3. Verify memory limits, firmware identity `FM-1_901`, loader compatibility, and SHA-256 sums.
4. Back up hardware presets, patterns, songs and projects before flashing.
5. Test a real FM-1: synth modes, program selection, bank A–H playback/editing, SONG, ALBUM, USB MIDI and persistent storage.
6. Publish verified binaries in a GitHub Release, with versioned changelog and license notices.
7. Deploy corresponding browser assets to `gh-pages`; test HTTPS, Web MIDI, SysEx and WebAssembly behavior in a supported browser.
8. Preserve all upstream authors, licenses and third-party attribution.

## Documentation publication

Use the accompanying **`publish.ps1`** from the documentation ZIP. It validates local branches/remotes, backs up files, stages only the intended documentation and homepage files, commits/pushes `main` and `gh-pages`, updates GitHub About, then edits the existing release notes. It **never flashes the FM-1, edits firmware source, moves the release tag or replaces release binaries**.
