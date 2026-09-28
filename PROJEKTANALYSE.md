# Projektanalyse Overviber

Stand: 28.09.2026. Statische Prüfung von Quellcode, CMake, CI und Git-Stand.
Es wurde nichts gebaut, getestet oder in einer DAW geprüft. Die vorherige
Analyse vom 26.09.2026 ist durch diese Fassung ersetzt.

## Aufbau

- JUCE-Umsetzung der GliGli-Overcycler-Firmware als VST3 und Standalone,
  unter macOS zusätzlich AU. Dazu der separate ModernSkinDesigner.
- C++17, CMake ab 3.22, JUCE 7.0.12 als Submodul. Projektversion 0.9.0.
- `vst/Source/PluginProcessor.*`: Host-Anbindung, Parameter, MIDI, Zustand.
- `vst/Source/dsp/`: sechsstimmige Engine, Wavetable- und Elements-Synthese,
  vier Filtermodelle (SSI2144, Ripples, Shelves, SST-Ladder), LM13700-VCA,
  ADSR/LFO, Arpeggiator, Mod-Matrix, MPE, Console/Mackity, 16 AFX-Parts.
- `vst/Source/ui/`: Modern- und Classic-Oberfläche, Tabs, Kurven- und Welleneditoren.
- `vst/Source/data/`: Presets, Wavetables, Session-Format `.ovm`, Speicherpfade.
- `disk/`: 50 Werkspresets (0000–0049) und AKWF-Wavetables.
- `firmware_17xx/`, `hardware/`, `enclosure/`, `manual/`, `doc/`, `m4l/`:
  Original-Firmware, KiCad-Hardware, Gehäuse, Datenblätter, Max for Live.
- 16 CTest-Tests; GitHub-Workflow für Windows, macOS und Linux.
- Git: fünf eigene Commits auf Basis von `gligli/overcycler`.

## Befunde der Analyse vom 26.09.2026: behoben

1. Build: `SynthEngine.h` bindet `MackityProcessor.h` ein.
2. MIDI-Timing: `processBlock` rendert bis zur Sample-Position jedes Ereignisses.
3. Audiothread: kein Mutex mehr; Zustände kommen über eine SPSC-Queue fester
   Größe. Programmwechsel laden Dateien auf dem Message-Thread.
4. AFX-Zustand: Der Host-Zustand speichert alle 16 Parts mit Waveframes.
5. Tests und CI: alle Tests bei CTest registriert, Assertions auch in Release,
   Testdaten aus diesem Checkout, CI mit Bash und `ctest`.

## Befunde vom 28.09.2026

| Nr. | Befund | Gewicht | Status |
|---|---|---|---|
| 1 | macOS-Mindestversion 10.13, aber `std::filesystem` braucht 10.15 | hoch | behoben |
| 2 | Windows-Pfade mit Umlauten über `std::string`/`fs::path` | mittel | offen |
| 3 | Git-Remote zeigt nur auf `gligli/overcycler`; kein eigenes Backup | mittel | offen |
| 4 | Plugin-Kopie nach Program Files standardmäßig an | niedrig | behoben |
| 5 | Versionen und Dokumentation widersprüchlich | niedrig | behoben |
| 6 | Zustandsübergabe kopiert und kodiert unnötig viel | niedrig–mittel | offen |
| 7 | Kleinigkeiten | niedrig | offen |

### 1. macOS-Mindestversion

`PresetManager.cpp` und `WaveManager.cpp` nutzen `std::filesystem`. Apple stellt
es erst ab macOS 10.15 bereit; mit 10.13 bricht der Build ab.
Umsetzung: `CMakeLists.txt`, CI und `MULTIPLATFORM_GUIDE.md` verwenden jetzt 10.15.
Ein vorhandener Build-Ordner behält seinen Cache-Wert; neu konfigurieren oder
`-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15` angeben.

### 2. Windows-Pfade mit Umlauten (offen)

JUCE liefert Pfade per `toStdString()` als UTF-8. `fs::path(std::string)`,
`std::ifstream` und `path.string()` verwenden unter Windows die ANSI-Codepage.
Liegt der Dokumente-Ordner z. B. unter `C:\Users\Jürgen`, werden Presets und
Waves nicht gefunden oder es entstehen Ausnahmen.
Empfehlung: Dateizugriffe in `PresetManager` und `WaveManager` auf `juce::File`
umstellen oder durchgehend `std::u8string`/`fs::u8path` verwenden.

### 3. Git und Veröffentlichung (offen)

Einziges Remote ist `upstream` (gligli), `master` folgt diesem Remote. Die
eigenen Commits sind nirgends gesichert. Empfehlung: eigenes GitHub-Repository
als `origin` anlegen, `master` darauf umstellen und pushen. Den Platzhalter
`your-username` im README danach ersetzen.

### 4. Plugin-Kopie nach dem Build

`JUCE_COPY_PLUGIN_AFTER_BUILD` war standardmäßig `ON`. Ohne Administratorrechte
scheitert dann der Build unter Windows.
Umsetzung: Standard ist jetzt `OFF`. Wer die automatische Installation möchte,
konfiguriert mit `-DJUCE_COPY_PLUGIN_AFTER_BUILD=ON`. Ein vorhandener
Build-Ordner behält seinen bisherigen Cache-Wert (bisher `ON`).

### 5. Versionen und Dokumentation

Umgesetzt:
- Projektversion in CMake, CI-Paketnamen, `install_linux.sh` und
  `GITHUB_PUBLISHING_GUIDE.md` auf 0.9.0 (wie in den Release Notes).
- Release Notes 0.9.0: C++17 statt C++20.
- README: Test-Badge und Testabschnitt auf die 16 CTest-Tests umgestellt,
  Verzeichnisstruktur korrigiert, macOS 10.15 und die Kopieroption dokumentiert.
- `MULTIPLATFORM_GUIDE.md`: Tests über `ctest`, macOS 10.15, Kopieroption.
- Diese Analyse ersetzt die veraltete Fassung vom 26.09.

Die datierten Berichte (`AUDIO_FIXES_2026-09-27.md`, `PRESET_0023_CLICK_FIX.md`,
`AUDIO_REFACTORING_REPORT.md`) bleiben als Momentaufnahmen unverändert; ihre
Testzahlen (11, 12, 13) beziehen sich auf den jeweiligen Tag.

### 6. Zustandsübergabe (offen)

Ein `PreparedState` umfasst rund 300 KB (16 Parts × 4 Waves × 2400 Samples).
Bei jeder Änderung, auch durch Automation, wird er bis zu 30-mal pro Sekunde
erfasst, verglichen und zusätzlich die gesamte Session als JSON mit Base64
kodiert. Im Audiothread werden pro Block bis zu zwei Zustände verglichen.
Das funktioniert, ist aber aufwendig. Empfehlung: Änderungsmarker je Part und
Wave; Session-Text erst bei `getStateInformation` oder gedrosselt erzeugen.

### 7. Kleinigkeiten (offen)

- Programmwechsel wirken verzögert über den 30-Hz-Timer, nicht samplegenau
  (bekannt und dokumentiert).
- `getTailLengthSeconds()` liefert 0, obwohl Release-Phasen nachklingen.
- `MidiClickScenarioTest` wird ohne `/UNDEBUG` bzw. `-UNDEBUG` gebaut.
- Ob die Layout-Fixtures von `ModernSkinScenarioTest` unter macOS passen, ist
  ungeprüft. Unter Linux ohne X-Display wird der Test übersprungen.

## Empfohlene nächste Schritte

1. Eigenes Remote anlegen und die Änderungen pushen.
2. CI auf allen drei Plattformen laufen lassen (prüft auch macOS 10.15).
3. Dateizugriffe auf Unicode-sichere Pfade umstellen (Befund 2).
4. Zustandsübergabe mit Änderungsmarkern verschlanken (Befund 6).
5. Vor einer Veröffentlichung Hör- und DAW-Tests mit verschiedenen Blockgrößen.
