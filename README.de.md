# analogBEAST v1.1 — Dokumentation (Deutsch)

**Alternative Synthesizer- und Sequencer-Firmware für den M-VAVE FM-1**

[Homepage](https://dzwerg.github.io/analogBEAST/) · [Synth Studio](https://dzwerg.github.io/analogBEAST/webapp/editor/) · [Emulator](https://dzwerg.github.io/analogBEAST/emu/) · [Web-Installer](https://dzwerg.github.io/analogBEAST/webapp/installer/) · [Release v1.1](https://github.com/dzwerg/analogBEAST/releases/tag/v1.1) · [English](README.en.md)

## Ein eigenständiges Instrument statt nur eines Synth-Presets

analogBEAST erweitert den **M-VAVE FM-1** zu einem kompakten Synthesizer, Hardware-Step-Sequencer und Song-/Album-Player. Der Schwerpunkt liegt auf zwei musikalisch unterschiedlichen Klangerzeugungswegen, der direkten Bedienung am Gerät und der Möglichkeit, **komplette Musikstücke und ganze Abspielfolgen** ohne Computer wiederzugeben.

Die Software basiert auf den Open-Source-Projekten **Felucca** und **Golana**; originaler Urhebervermerk, Lizenzen und kompatible Update-Kennungen bleiben erhalten.

## Zwei unterschiedliche Synth-Routings

In der GOLANA-Engine kann zwischen **MIKROKORG** und **BRUTE** gewechselt werden. Das sind **verschiedene DSP-Signalwege innerhalb derselben Engine**, nicht zwei separat ladbare Synthesizer.

### MIKROKORG — klassisch virtuell-analog

- Zwei Oszillatoren mit mehreren Wellenformen und Mischmöglichkeiten.
- Oszillatorsynchronisation und Ringmodulation.
- Filter-Cutoff, Resonanz und Filter-Hüllkurve.
- Amp-Hüllkurve, LFOs und Modulationszuweisungen.
- Unison-/Glide-/Performance-Einstellungen sowie Delay, Reverb und Modulationseffekte.

Dieser Signalweg eignet sich für klassische Bässe, Leads, Pads und bewegte VA-Sounds.

### BRUTE — anderer Oszillator- und Filterpfad

Im BRUTE-Modus wird die Signalerzeugung umgeschaltet. Zum Klangarsenal gehören:

- **SAW / ULTRASAW:** zusätzliche, gegeneinander verstimmte Sägezahnanteile.
- **PULSE / PWM:** Pulsbreiten-Modulation.
- **METAL:** metallisch klingendes Triangle-Wavefolding.
- **SUB / OVERTONES:** zusätzlicher tiefer oder hoher Oszillatoranteil.
- **BRUTE FACTOR:** dosierbare interne Rückkopplung im Synthesizer-Signalweg.
- **Steiner-Parker-inspirierter LP/BP/HP-Filter:** eigene nichtlineare Filterstrecke.

### Steiner-Parker-Filter — das klangliche Herzstück

Der BRUTE-Modus implementiert einen **digitalen, Steiner-Parker-inspirierten Zweipolfilter** mit **Low-Pass, Band-Pass und High-Pass**. Seine integrierte Sättigung und nichtlineare Rückkopplung erzeugen einen anderen Charakter als der MIKROKORG-Filterpfad. Das ist eine **digitale Interpretation des Prinzips**, keine Bauteil-für-Bauteil-Emulation der analogen Schaltung.

Besonders reizvoll sind druckvolle Bässe, aggressive Leads, Resonanzfahrten, Sequenzen mit starker Filterbewegung und Klangexperimente mit dem Brute Factor.

## 32 Instrumente / Soundprogramme

Der FM-1 bietet **P01–P32**. P01–P16 sind mit den GOLANA-Werksklängen vorbelegt, solange dort keine Benutzersounds gespeichert wurden. P17–P32 dienen als zusätzliche INIT- beziehungsweise Benutzerplätze. **MIDI Program Change 0–31** wählt die Programme P01–P32 an.

Änderungen am Sound lassen sich am Gerät und im Webeditor sofort anhören. **Dauerhaftes Speichern eines Klangprogramms erfolgt über SAVE am FM-1.** Der Webeditor soll Programmspeicherplätze nicht stillschweigend überschreiben.

## Step-Sequencer: acht Bänke, 128 Patterns

| Bank | Patternplätze | Maximale Patternlänge |
|---|---|---|
| **A** | A01–A16 | **64 Steps** |
| **B** | B01–B16 | **64 Steps** |
| **C** | C01–C16 | **64 Steps** |
| **D** | D01–D16 | **64 Steps** |
| **E** | E01–E16 | **16 Steps** |
| **F** | F01–F16 | **16 Steps** |
| **G** | G01–G16 | **16 Steps** |
| **H** | H01–H16 | **16 Steps** |

Das macht **8 × 16 = 128 Patternplätze**. Die längeren Patterns sind ideal für Melodien oder komplette Phrasen, die kürzeren für Grooves, Variationen und Übergänge.

Der Step-Editor ermöglicht unter anderem **Note, Velocity, Gate, Swing, Auflösung sowie Step-Typ und Flags**. Das Gerät besitzt eigene REC-Seiten für die Bearbeitung. Im Browser steht zusätzlich eine grafische Bearbeitung zur Verfügung.

## Neu in Version 1.1 — SONG-Kommandos und Live-Spiel

- **OCTAVE (−3 bis +3 Oktaven)** und **TRANSPOSE (−24 bis +24 Halbtöne)** ändern die Wiedergabe, nicht die gespeicherten Noten.
- **REPEAT (1–64), JUMP (1–64), ENDSTEP (1–64)** steuern Anzahl und Step-Ausschnitt des jeweils nächsten Patterns.
- **BEGIN** wiederholt bei `END` den SONG selbst bei ONCE, nicht aber das ALBUM.
- Im **SEQ-PLAY** wählen **8 schwarze Tasten** die Bank A–H und **16 weiße Tasten** Pattern 01–16. Während PATTERN PLAY erfolgt der NEXT-Wechsel erst am Pattern-Ende.
- **PRESET- und TEMPO-Wechsel während PLAY**, OCT−/OCT+ für Live-Oktaven, leuchtende Notentasten.
- **Web-Piano-Roll:** Transpose-/Octave-Knöpfe mit UNDO; Python-SysEx-Tool versteht die neuen Befehle.

**[Ausführliche Anleitung mit SONG-Beispielen und Bedienung](docs/ANLEITUNG-v1.1.de.md)** · [Release Notes](docs/RELEASE_NOTES-v1.1.md). Die bestehende v1.0-Dateistruktur bleibt kompatibel.

## SONG MODE: 32 vollständige Songs

Der SONG MODE kombiniert Pattern- und Steuerbefehle zu einem musikalischen Ablauf:

- **32 Songs** (S01–S32).
- **Bis zu 64 Einträge/Befehle pro Song**.
- **PATTERN:** ausgewählte Bank/Pattern spielen.
- **TEMPO:** Tempo innerhalb eines Songs ändern.
- **PRESET:** das Klangprogramm wechseln.
- **REST:** eine Pause in den Ablauf einfügen.
- **ARP:** Arpeggiator-Steuerung.
- **END:** das Songende definieren.
- Wiedergabe **ONCE** oder **REPEAT/LOOP** entsprechend der gewählten Betriebsart.

Die Reihenfolge ist frei programmierbar: Intro, Strophe, Refrain, Zwischenteil und Schluss lassen sich auf der FM-1-Hardware nacheinander abspielen. Patternwechsel werden an den vorgesehenen Abschnittsgrenzen ausgeführt.

## ALBUM MODE: 4 Albums mit je 32 Einträgen

Der ALBUM MODE macht aus Einzelstücken ein zusammenhängendes Set:

- **4 Albumplätze**.
- **Bis zu 32 Song-Einträge pro Album**.
- Songs in einer festgelegten Reihenfolge abspielen.
- Albumende und Wiederholung steuern.
- Geeignet für Live-Sets, Vorführungen, Liedersammlungen oder komplette Kinderlieder-Albums.

Die Einheit bleibt ein Standalone-Instrument: Für die Wiedergabe bereits gespeicherter Abläufe wird kein laufender PC benötigt.

## Arpeggiator und Performance

Die ARP-Funktionen umfassen unter anderem Laufmuster, Oktavbereich, zeitliche Auflösung und aktivierbare Steps. SONG- und PATTERN-Wiedergabe können zusammen mit den Performance-Einstellungen verwendet werden. Die Sound- und ARP-Bearbeitung erfolgt direkt am Gerät oder in der passenden Web-Oberfläche.

## USB MIDI, Webeditor und MIDI-Import

Über **USB MIDI** ist analogBEAST mit anderen MIDI-Geräten und Anwendungen verbindbar. Zusätzliche Projektwerkzeuge übertragen Pattern- und Songdaten. Der grafische Browser-Editor unterstützt die Verarbeitung von **Standard-MIDI-Dateien**, inklusive Tempo/Noten und Aufteilung in 16-/64-Step-Patterns.

**[Synth Studio öffnen](https://dzwerg.github.io/analogBEAST/webapp/editor/)** — SOUND mit P01–P32, MIKROKORG/BRUTE-Parameter, ARP, grafischer PATTERN-/SONG-/ALBUM-Editor und MIDI-Song-Import.

**[Web-Emulator starten](https://dzwerg.github.io/analogBEAST/emu/)** — Die Firmware läuft in einer WebAssembly-basierten FM-1-Geräteumgebung direkt im Browser.

**[Web-Installer öffnen](https://dzwerg.github.io/analogBEAST/webapp/installer/)** — Installation über USB auf einem echten FM-1. Erforderlich sind ein geeigneter Browser (z. B. Chrome/Edge), Web-MIDI-/SysEx-Berechtigungen und ein funktionierendes USB-Datenkabel. **Unbedingt vorher eigene Daten sichern und die Geräteerkennung prüfen.**

## Download, Kompatibilität und Installation

- [GitHub Release v1.1](https://github.com/dzwerg/analogBEAST/releases/tag/v1.1)
- [Vorheriges v1.0 Release](https://github.com/dzwerg/analogBEAST/releases/tag/v1.0)
- [Firmware-Paket `analogBEAST-v1.1.fwsc`](https://github.com/dzwerg/analogBEAST/releases/download/v1.1/analogBEAST-v1.1.fwsc)
- [Bisheriger Web-Emulator ZIP (v1.0)](https://github.com/dzwerg/analogBEAST/releases/download/v1.0/analogBEAST-v1.0-webemu.zip)

Die Firmware verwendet weiterhin die Paketidentität **`FM-1_901`** und für den Update-Bootloader den Namen **`Felucca Update`**. Diese Namen haben technische Kompatibilitätsgründe. Die Firmware nicht mit einem geänderten Produktcode paketieren und vor dem Flashen Benutzerdaten sichern.

## Lizenz und Herkunft

**analogBEAST ist GPL-3.0-Software** und basiert auf [Felucca](https://github.com/hugelton/Felucca) (Leo Kuroshita / Hügelton Instruments) sowie [Golana](https://github.com/Gadlele/Golana). Weitere Komponenten und Lizenzinformationen stehen in [LICENSING.md](LICENSING.md) und [LICENSES/](LICENSES/). Urheberrechte, Lizenzhinweise und Credits der Ausgangsprojekte bleiben bestehen.

analogBEAST ist kein offizielles Produkt von M-VAVE, Arturia oder Korg. Markennamen dienen nur der Beschreibung der Klangkonzepte. Es gibt keine Garantie auf fehlerfreien Betrieb oder Hardwareverträglichkeit.
