# analogBEAST v1.1 — Bedienungsanleitung der neuen Funktionen

Diese Anleitung ergänzt die [vollständige v1.0-Dokumentation](../README.de.md). Die bestehenden Synthese-, REC-, SONG- und ALBUM-Seiten bleiben erhalten. Die neuen Funktionen gelten für die v1.1-Firmware des M-VAVE FM-1.

## 1. LIVE PLAY: Bank und Pattern direkt über die Klaviatur

Auf **SEQ → PLAY** lassen sich Patterns ohne das Menü anzuhalten auswählen:

- **Schwarze Tasten:** die ersten acht schwarzen Tasten wählen die Bänke **A bis H**.
- **Weiße Tasten:** die 16 weißen Tasten wählen **Pattern 01 bis 16**.
- Bei **STOP** erfolgt die Auswahl sofort. Während **PATTERN PLAY** wird das neue Pattern als **NEXT** vorgemerkt. Der laufende Abschnitt spielt zu Ende; der Wechsel erfolgt an der nächsten Pattern-Grenze.
- Die Auswahl spielt keine Keyboard-Noten. Die Klaviatur-LEDs zeigen stattdessen die tatsächlich klingenden Tonhöhen.
- In SONG/ALBUM bleiben die bestehenden Wechselmechanismen am Song-/Album-Ende erhalten.

**Beispiel:** Während B01 läuft, Bank B und danach die weiße Taste für Pattern 03 drücken. B01 läuft vollständig bis zum Ende; danach beginnt B03 automatisch. Die alte Sequencer-Note wird vor dem Wechsel freigegeben, der Transport läuft weiter.

## 2. TEMPO, PRESET und Transponierung während PLAY

- **ALGORITHM-Drehregler:** ändert das Tempo auch außerhalb der HOME-Seite.
- **PRESET-Drehregler:** wählt P01–P32, ohne PLAY zu stoppen oder das aktuelle Pattern neu zu starten. Der Synth-Sound ändert sich, der Transport bleibt erhalten.
- **OCT− / OCT+:** Live-Transponierung der Sequenz um jeweils eine Oktave nach unten/oben.
- **STOP:** setzt Live-Transponierung sowie eine ausstehende NEXT-Auswahl zurück.

Bei extrem kurzen Noten kann die LED-Anzeige durch die Aktualisierungsrate des Displays/der LEDs begrenzt sein.

## 3. Die sechs neuen SONG-Befehle

| Befehl | Wert | Wirkung |
|---|---:|---|
| `OCTAVE` | −3 … +3 | Verschiebt alle nachfolgenden Noten um Oktaven |
| `TRANSPOSE` | −24 … +24 | Verschiebt nachfolgende Noten um Halbtöne |
| `REPEAT` | 1 … 64 | Gesamtzahl der Wiedergaben des nächsten Patterns |
| `JUMP` | 1 … 64 | Erster gespielter Step des nächsten Patterns |
| `ENDSTEP` | 1 … 64 | Letzter gespielter Step des nächsten Patterns |
| `BEGIN` | ohne Wert | SONG springt bei `END` wieder zum Anfang |

`JUMP`, `ENDSTEP` und `REPEAT` beziehen sich auf das **nächste Pattern**, nicht auf alle späteren Patterns. `OCTAVE` und `TRANSPOSE` bleiben dagegen aktiv, bis ein neuer entsprechender Befehl ihren Wert ändert. Die gespeicherten Pattern-Noten selbst bleiben unverändert.

### Beispiel: Nur zwei Steps fünfmal spielen

```text
JUMP       12
ENDSTEP    13
REPEAT      5
A02        64
A03        64
END
```

Das Pattern **A02 spielt Step 12 und 13 insgesamt fünfmal**. Danach läuft A03 wieder mit dem normalen Bereich und ohne ererbte fünffache Wiederholung. Bei den kurzen Bänken E–H sind nur ihre vorhandenen 16 Steps nutzbar.

### BEGIN in SONG und ALBUM

`BEGIN` ermöglicht eine fortlaufende SONG-Schleife, selbst wenn **ONCE** eingestellt ist. Es wiederholt **nicht automatisch ein ALBUM**. Bei der durch `BEGIN` ausgelösten Rückkehr bleiben manuelle TEMPO-/PRESET-Anpassungen bestehen; die einleitenden TEMPO-/PRESET-Kommandos werden nicht jedes Mal erneut erzwungen. ALBUM-Wiederholung bleibt durch die Album-Loop-Einstellung gesteuert.

## 4. Web-Piano-Roll

In der v1.1-Version des Webeditors lassen sich Noten im Piano-Roll-Raster mit **TRANSPOSE −1/+1 Halbton** und **OCTAVE −12/+12 Halbtöne** verschieben. UNDO macht die Änderung rückgängig. Die Tonhöhen werden vor dem Schreiben auf gültige MIDI-Werte begrenzt. TIE-Balken verwenden die zuletzt gespielte Note als Höhenbezug.

**Hinweis:** Die öffentlich gehostete Web-App unter GitHub Pages wird erst dann auf v1.1 aktualisiert, wenn die Browserdateien separat bereitgestellt wurden. Die lokal im Git-Repository enthaltene `web/analogbeast-seq.js` ist Teil des v1.1-Quellstands.

## 5. Datenübertragung und Firmware-Installation

SONG-/ALBUM-Backups behalten das Format von v1.0. Die sechs neuen Befehle sind zusätzliche Werte **13–18**; bestehende Dateistrukturen und SysEx-Befehle bleiben gleich. Das aktualisierte Python-Tool `tools/analogbeast_transfer.py` kann diese neuen Zeilen lesen und schreiben.

**Vor einem Update:** FM-1 stoppen, REC verlassen, Geräte-Daten vollständig sichern, nur das gewünschte FM-1 per USB verbinden. Das veröffentlichte Paket behält die Identität **`FM-1_901`** und den Bootloader **`Felucca Update`**. Nicht ohne Sicherung flashen.

## 6. Bewusst unverändert

- **128 Patterns:** A–D mit maximal 64 Steps, E–H mit maximal 16 Steps.
- **32 SONGs** à maximal 64 Befehle; **4 ALBUMs** à maximal 32 Song-Einträge.
- Klang-Engines, Sound-Presets, Song/Album-Datenformat und bisherige MIDI-Befehle.
- Kein automatisches BLE-MIDI, keine Änderung der USB-Paketidentität.
