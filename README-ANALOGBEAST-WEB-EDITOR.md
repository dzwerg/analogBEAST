# analogBEAST v1.0 – Webeditor für SEQ / SONG / ALBUM

Ergänzung des **vorhandenen** `web/editor.html`, ohne dessen Felucca-Synth-/Preset-Editor zu ersetzen.

## Umfang

- **Patterns:** 8 Bänke A–H, 16 Patterns pro Bank; A–D je **64 Steps** (vier 16er-Seiten), E–H je **16 Steps**. Note, Flags (roh, 0–31), Velocity, Gate und Swing; Patternlänge und Auflösung.
- **Song:** 32 Songs, je 64 Einträge (vier 16er-Seiten). Befehle PATTERN (Bank/Nummer/Schrittlänge), END, TEMPO (40–240), PRESET (0–31), REST (1–64 Steps), ARP (0/1), Song-Länge und LOOP/ONCE.
- **Album:** 4 Alben, je 32 Song-Slots, LOOP/ONCE.
- **MIDI/WebMIDI:** dieselben USB-SysEx-96-Operationen wie `tools/analogbeast_transfer.py` (Pattern A–D: 14–17; E–H: 21–22; Song: 23–24/27–28; Album: 25–26; Autosave: 6/30).
- JSON-Import (einzeln oder Sammlung), Export (aktuelles Objekt und vollständiger Gerätesatz: 128 Patterns + 32 Songs + 4 Alben).
- **Schreiben nur nach Geräte-Backup, expliziter Bestätigung, vollständigem Rücklesen und Autosave-Kontrolle.** Keine stillschweigende Dateispeicherung / Änderung der Hardware-Firmware.

## Installation (PowerShell, C:\FM1\analogBEAST)

ZIP im Downloads-Ordner speichern, anschließend **nur** die folgenden Codeblöcke ausführen:

```powershell
cd C:\FM1\analogBEAST
Expand-Archive "$env:USERPROFILE\Downloads\analogBEAST-v1.0-WEBEDITOR-SEQ-SONG-ALBUM.zip" -DestinationPath . -Force
py tools\install_analogbeast_seq_webeditor.py --check-only
```

Nur wenn `PASS`:

```powershell
py tools\install_analogbeast_seq_webeditor.py
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && node tests/test_analogbeast_seq_webeditor.js'
py tests\test_install_seq_editor.py
```

Keine WASM-Neukompilierung nötig, weil die Editorerweiterung unabhängig vom bereits funktionierenden Webemulator läuft.

### Lokal starten

```powershell
cd C:\FM1\analogBEAST\web
py -m http.server 8000
```

Browser: **http://localhost:8000/editor.html** (Chrome/Edge, WebMIDI + SysEx freigeben).
Oben rechts öffnet **`analogBEAST · SEQ`** den zusätzlichen Bereich.

- Erst **MIDI verbinden**; die analogen MIDI-Ports werden bevorzugt.
- **STOP** am FM-1 und **REC verlassen**. Möglichst alle DAWs/anderen MIDI-Tools schließen.
- Bank/Pattern, Song oder Album auswählen und **Vom Gerät LESEN**.
- Änderungen zunächst nur im Browser, dann **Mit BACKUP schreiben**.
- Die Sicherungsdatei wirklich speichern, bevor der Schreibdialog bestätigt wird.
- Im Zweifel **erst JSON exportieren** und Sicherungsdatei kontrollieren.
- Bei Stromausfall ist ein Sammelimport nicht atomar; zum Wiederherstellen das Backup importieren.

### Veröffentlichung

Der Installer erweitert `web/make_site.py`, sodass `analogbeast-seq.js` mit nach `webapp/editor/` kopiert wird. Nach dem Erstellen der GitHub-Pages-Site prüfen, dass `webapp/editor/index.html` die Datei `webapp/editor/analogbeast-seq.js` erfolgreich lädt. Der bereits funktionierende Emulator bleibt in `build/emu/` separat.

### Grenzen

- Es gibt **keine automatische gemeinsame Speicherung zwischen Webemulator und angeschlossenem FM-1**: Der Webeditor verwendet echtes WebMIDI-SysEx, der WASM-Emulator bildet USB-MIDI nicht nach.
- Die Webseite benötigt **localhost oder HTTPS** und einen WebMIDI-/SysEx-fähigen Browser.
- Bestehende Song-, Pattern- und Albuminhalte werden beim Installieren **nicht** verändert.
- Vor einer öffentlichen Release sind Tests am tatsächlichen Gerät nötig (MIDI lesen, lokale Bearbeitung, Backup, gezieltes Schreiben, Rücklesen, Autosave, Reconnect).
