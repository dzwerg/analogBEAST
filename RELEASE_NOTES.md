# analogBEAST 2.0 RC1 — Pre-Release

Stage 18 · FM-1_901 · Native JieLi build

## Änderungen seit 1.1

## Entwicklung seit analogBEAST 1.1

### MUSIC DISK / MD16

Das MUSIC DISK-System ist eine zentrale Erweiterung von analogBEAST 2.0.

- 292 KiB NOR-Flash-Bereich: 0x97000 bis 0xDFFFF.
- Eigenes MD16-Dateisystem mit variablen Datensatzlängen.
- Pattern-IDs bis 512 im aktuellen Speicherpfad.
- Dateiobjekte für Patterns, Songs und Alben.
- Weitere definierte Datentypen für Presets, VL-1 und Musicbox/M1.
- CRC32-Prüfsummen, Sequenznummern und Commit-Marker.
- Schutz gegen unvollständige Schreibvorgänge.
- Kompakter RAM-Index für schnelle Pattern-Auswahl.
- Logische Löschung über Tombstone-Datensätze.
- Geschützte Speicheroperationen außerhalb des Audio-Interrupts.
- Kein automatisches Formatieren oder Überschreiben fremder Datenträger.
- Keine automatische Neuinitialisierung des Datenträgers nach Firmware-Updates.

MD12 und MD13 waren frühere Entwicklungsstufen mit alternativen
Transaktions- und Speicherverfahren. MD16 ist das neuere Dateisystem.

Wichtig: Garbage Collection ist im Dateisystem implementiert,
im Stage-18-Gerätepfad jedoch deaktiviert. Die Datenfunktionen
benötigen weitere Hardware- und Stromausfalltests.

### Sequencer und Arranger

- Erweiterte Pattern- und Step-Datenmodelle.
- Song- und Album-Datenstrukturen.
- MIDI-Import- und Musicbox-Werkzeuge.
- Bearbeitung und persistente Speicherung von Pattern-Daten.
- Wiedergabe-, Transport-, Loop- und Step-Funktionen.
- Schutz vor unsicheren Schreibvorgängen während der Wiedergabe.
- Pattern-Auswahl anhand tatsächlich vorhandener Dateien.

Nicht alle historischen Entwicklungsfunktionen sind automatisch
Bestandteil der aktiven, hardwaregetesteten RC1-Bedienoberfläche.

### Synthesizer und DSP

- KORG- und BRUTE-Synthesemodi.
- Felucca ANALOG und LOFI.
- Experimentelle B1-Bass-Engine.
- Engine-spezifische Parameterseiten.
- Dynamisch wiederverwendeter DSP-Zustand je Part.
- Fade und Ressourcenfreigabe beim Engine-Wechsel.

Die B1-Engine ist noch keine originalgetreue Donner-B1-Emulation.

### Presets

- 32 bestehende beschreibbare User-Instrumente.
- 12 originale Felucca-ANALOG-Werksklänge.
- 5 originale Felucca-LOFI-Werksklänge.
- Gemeinsame Preset-Auswahl mit vorgesehener Engine-Umschaltung.
- P18 SYNTH KEYBOARD und P19 PETZ MOOG als eingebettete
  Originaldatensätze erhalten.
- Keine Änderung des bestehenden User-Preset-Flash-Layouts.

Die eingebetteten Originale ersetzen kein Backup eventuell
bearbeiteter Presets vom tatsächlichen Gerät.

### USB und MIDI

- USB-Audio und USB-MIDI als Composite-Gerät.
- Umschaltbare USB-Device-, Audio- und MIDI-Host-Profile.
- MIDI-Routing und zusätzliche Importwerkzeuge.
- Getrennte Regelung von USB-Audio-Ausgang und lokalem MASTER.
- Erhalt des USB8-Original-Composite-Quellstands als Rückfallbasis.

### Entwicklungs- und Teststatus

RC1 basiert auf dem nativen Stage-18-Build. Es handelt sich um
ein experimentelles Pre-Release. Insbesondere MUSIC DISK,
USB-Host-Kompatibilität, Preset-Wechsel und B1-Klangverhalten
sind noch nicht vollständig auf Hardware validiert.

## RC1 Build

- Firmware-App: 256536 Bytes
- Statischer RAM: 60388 / 73728 Bytes
- DSP-Pool: 302048 / 368640 Bytes
- FWSC: 610086 Bytes

## Status

Experimenteller Release Candidate. Nicht alle Funktionen wurden
am echten FM-1 vollständig getestet. USB8-Fallback aufbewahren.