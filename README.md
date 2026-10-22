# analogBEAST v1.0 — Instrumentenwahl P01–P32 (V2)

Quellcode: `golana_prog.c`, `golana_factory.c`, `golana_ui.c`, `midi_control.c`, `core.h`.

Die Felucca-Engine-Liste **ANALOG/FM6/PHASE/…/GOLANA** ist keine analogBEAST-Instrumentenliste. In analogBEAST ist **GOLANA Engine 13**; der Klangauswahldrehknopf am Gerät lädt ein **Programm P01–P32**. P01–P16 sind werkseitige GOLANA-Sounds (sofern dort kein eigener Sound gespeichert ist); P17–P32 sind INIT oder eigene Programme. BRUTE / MIKROKORG sind Soundmodi.

Dieser Fix baut den sichtbaren **INSTRUMENT · P01–P32**-Wähler fest in SOUND ein. Die alten Engine-/Preset-Dropdowns bleiben nur unsichtbar zur Abwärtskompatibilität erhalten. Die Namen überschreibender Benutzerklänge werden aus den bestehenden 32 User-Slots via UP_LIST gelesen. Ein Klick auf ein Instrument sendet MIDI Program Change (P01=0 bis P32=31) und liest die Klangparameter erneut. Wie in der Firmware werden die Programmwechsel beim laufenden SONG oder beim REC abgewiesen. Das bisherige Editorprotokoll verrät nicht, welches `gu.prog` am Gerät ausgewählt ist; vor der ersten manuellen Auswahl wird deshalb kein ausgewähltes Programm behauptet.

## Installation (Windows PowerShell)

ZIP nach Downloads speichern:

```powershell
cd C:\FM1\analogBEAST
Expand-Archive "$env:USERPROFILE\Downloads\analogBEAST-v1.0-INSTRUMENT-P01-P32-V2.zip" -DestinationPath . -Force
py tools\fix_analogbeast_instrument_ui.py --check-only
py tools\fix_analogbeast_instrument_ui.py
```

Mit `Strg+Shift+R` den Editor neu laden. Oben auf SOUND sollte **INSTRUMENT · P01–P32** statt Engine/Presets stehen. STOP aktivieren, `Programmnamen lesen`, dann P01/P02 testen.

Die Änderungen betreffen nur `web/editor.html`. SONGs, ARP, Studio-Regler, Sequencer und MIDI-Song-Importer bleiben unverändert.

Rückbau: `py tools\fix_analogbeast_instrument_ui.py --restore`. Das Backup ist `web/editor.html.before-ab-instrument-v2`.

Wenn die Vorprüfung `REFUSED` meldet, bitte die genaue Ausgabe zeigen. Nicht weiter patchen; die Datei weicht dann vom erwarteten Quellstand ab.
