# analogBEAST v1.1 — Verification record (23 October 2026)

| Stage | Result | Scope |
|---|---|---|
| Build | PASS | app 426,168 B; `.data + .bss` 64,404 / 73,728 B; pool 351,244 / 368,640 B; package 610,086 B |
| QA2 | **19/19 PASS** | simulated controls, pattern queue, live preset/tempo, note LEDs |
| QA3 interpreter | **90 checks PASS** | repeat, segment boundaries, SONG/ALBUM, read/write semantics |
| QA3 device callback | **10/10 PASS** | B01→B03 at boundary, note release, STOP cleanup |
| QA3 NOR | PASS | CRC, A/B commits, interrupted saves, corrupt copy fallback, legacy area protection |
| QA4 Python↔C | **225 checks PASS** | SysEx 96 dispatcher, SONG/ALBUM write/read/restore, busy write refusal |
| QA4 browser syntax | PASS | JavaScript syntax and whitespace |
| Hardware | Maintainer confirmation | v1.1 installed and described as functioning on real FM-1; no independent instrumented trace supplied |

**Not yet fully green:** legacy `tests/golana_sim.c` UI regression cases predate the current analogBEAST interface (previous run: 40 failed expectations). They remain to be adapted; do not claim the entire upstream legacy test suite passes.

**Not covered by the host checks:** all real USB/MIDI-host combinations, long-duration endurance, every sound/program and every external routing topology.

**Reproducibility:** `tests/run_v11_qa.sh`, `tests/run_v11_qa3.sh`, and `tests/run_v11_qa4.sh` exercise the corresponding host checks. The package in the v1.1 GitHub Release is a copied/renamed exact binary from the tested developer build, not a rebuild with changed OTA identity.
