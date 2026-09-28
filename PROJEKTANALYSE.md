# Projektanalyse Overviber

Stand: 26.09.2026. Lokale Quellcodeprüfung und gezielte Build-/Testprüfung; kein vollständiger DAW-, GUI- oder Plattformtest.

## Kopie

Quelle: das Original-Overcycler-Repository (`gligli/overcycler`)

Ziel: dieses Overviber-Repository

Der Zielordner war leer. Robocopy kopierte 9.845 Dateien, rund 904 MiB, einschließlich versteckter Git-Daten, JUCE und vorhandener Build-Artefakte. Ergebnis: keine Fehler, keine Abweichungen laut Kopierprotokoll; Exitcode 1 bedeutet erfolgreich kopierte Dateien. Verzeichnisverknüpfungen wurden durch `/XJ` ausgeschlossen. Keine Änderungen am Quellprojekt vorgenommen. Die Kopie wurde nicht zusätzlich per Hash verglichen.

## Aufbau

- C++17, CMake ab 3.22, JUCE; Projektversion 0.8.0.
- Ziele: VST3 und Standalone; zusätzlich AU unter macOS. Separater ModernSkinDesigner.
- `vst/Source/PluginProcessor.cpp`: Host-Anbindung, Parameter, MIDI, Zustandsverwaltung.
- `vst/Source/dsp/`: sechsstimmige Engine, Wavetable-Oszillatoren, Elements/Hybrid-Synthese, Filtermodelle, ADSR/LFO, Arpeggiator, Modulationsmatrix, MPE, ConsoleX/Mackity und AFX-Kits.
- `vst/Source/ui/`: moderne und klassische Oberfläche, Kurven-/Welleneditoren, Theme-System.
- `vst/Source/data/`: Presets, Wavetables, plattformabhängige Speicherpfade.
- `disk/`: Werksdaten; 68 Preset-Dateien vorhanden.
- `firmware_17xx/`, `hardware/`, `enclosure/`, `manual/`, `doc/`, `m4l/`: ursprüngliche Firmware, Hardware, Dokumentation und ergänzende Integration.
- Neun Scenario-Test-Ziele in CMake; GitHub-Workflow für Windows, macOS und Linux.

## Wichtigste Befunde

### 1. Aktueller Quellstand baut nicht vollständig

Ein frischer Build von `ElementsVoiceScenarioTest` mit MSVC 19.51 / Visual Studio 18 2026 schlägt fehl: `SynthEngine.h:164` und `:201` verwenden `MackityProcessor`, ohne dessen Header einzubinden. Compilerfehler C2143, C4430 und C3646 sowie Folgefehler für `mackity`. Das getrennte Elements-Ziel baut erfolgreich.

Beleg: `build-analysis/compile-debug.log`. Zuerst Header-Abhängigkeit korrigieren, anschließend Plugin und Tests frisch bauen. Mitkopierte EXE-Dateien sind kein Nachweis für die Kompilierbarkeit dieses Quellstands.

### 2. Eingehendes MIDI verliert die Sample-Zeitposition

`PluginProcessor.cpp:785` verarbeitet alle MIDI-Ereignisse vor dem einzigen `renderBlock`-Aufruf bei Zeile 834. `metadata.samplePosition` wird nicht verwendet. Ereignisse innerhalb eines Audioblocks wirken dadurch bereits vor dessen Rendering; Note-on und Note-off im selben Block können falsch wiedergegeben werden. Abhilfe: Rendering an Ereignispositionen unterteilen.

### 3. Blockierende Arbeit im Audiothread

`SynthEngine.cpp:1261` sperrt `engineMutex` über das Rendering. Parameter-/Preset-Operationen verwenden denselben Mutex. MIDI Program Change ruft im Audiocallback `setCurrentProgram` und damit `loadPreset` auf; der Preset-Manager liest Dateien, und `applyPreset` lädt Wellenformen. Zusätzlich werden dynamische MIDI-Vektoren verwendet. Daraus entsteht ein plausibles Risiko für Audioaussetzer; eine konkrete Aussetzerquote wurde nicht gemessen.

### 4. AFX-Kits fehlen im Host-Zustand

`PluginProcessor.cpp:873` speichert ausschließlich `getCurrentPreset()` über `serializePresetToString`. Die 16 AFX-Slots und die Notenzuordnung liegen separat in `AfxKit` und werden dabei nicht serialisiert. Individuelle AFX-Kits werden beim Speichern und erneuten Öffnen eines DAW-Projekts deshalb nicht durch diesen Zustand wiederhergestellt.

### 5. Tests und CI vermitteln zu viel Sicherheit

- Mehrere Tests bevorzugen fest codierte Datenpfade zum alten Ordner `GliGli Overcycler`. Ein erfolgreicher Lauf in der Kopie ist daher kein vollständiger Isolationstest.
- `ElementsVoiceScenarioTest.cpp` prüft mit `assert`; Release setzt `NDEBUG`, wodurch diese Prüfungen entfallen.
- Der GitHub-Workflow führt fünf der neun Scenario-Ziele aus; ClassicSkin, ConsoleX/AFX und beide Elements-Ziele fehlen bei den Testschritten.
- Der Windows-Konfigurationsschritt benutzt Backslash-Zeilenfortsetzung ohne Bash-Shell-Angabe. Das passt nicht zur standardmäßigen PowerShell-Ausführung und muss vor einem CI-Lauf korrigiert werden.
- CMake registriert die Scenario-Programme nicht mit `add_test`; ein allgemeiner CTest-Lauf deckt sie nicht automatisch ab.

### 6. Umzug und Versionsverwaltung

- `build/CMakeCache.txt:340` verweist auf den alten Quellordner. Für Overviber neu konfigurieren, den kopierten Cache nicht weiterverwenden.
- Ein separater frischer Build wurde unter `build-analysis/` angelegt. Dieser Ordner ist vom vorhandenen `/build_*/`-Ignore-Muster nicht erfasst.
- Der kopierte Git-Stand enthält zahlreiche bestehende unversionierte Dateien, darunter `vst/`, `CMakeLists.txt` und `.github/`. Diese Arbeit ist in der Kopie erhalten, aber nicht durch den letzten Commit abgesichert.
- Git meldete wegen der durch die Sandbox erzeugten Eigentümerschaft einen Safe-Directory-Konflikt. Die Prüfung nutzte nur eine auf den jeweiligen Aufruf beschränkte Ausnahme; keine globale Git-Konfiguration wurde verändert.

## Durchgeführte Prüfungen

Frische CMake-Konfiguration im Zielordner: erfolgreich.

Vorhandene, mitkopierte Release-Testprogramme:

| Programm | Ergebnis |
|---|---|
| FilterScenarioTest | 917 bestanden, 0 fehlgeschlagen |
| ArpScenarioTest | 103 bestanden, 0 fehlgeschlagen |
| AdvancedMidiScenarioTest | 11 bestanden, 0 fehlgeschlagen |
| ModMatrixScenarioTest | 24 bestanden, 0 fehlgeschlagen |
| ClassicSkinScenarioTest | 54 bestanden, 0 fehlgeschlagen |
| ConsoleXAndAfxScenarioTest | 13 bestanden, 0 fehlgeschlagen |
| ElementsScenarioTest | Exitcode 0 |
| ElementsVoiceScenarioTest | Exitcode 0; Release-Assertions deaktiviert |

Frisch gebauter Debug-ElementsScenarioTest: Exitcode 1; gemeldete Performance von 2,6x Echtzeit unterschreitet die 5x-Grenze. Diese Messung ist Debug-spezifisch und kein Release-Benchmark. Vollständige Ausgabe: `build-analysis/ElementsScenarioTest-debug.log`.

Frischer Debug-ElementsVoiceScenarioTest: Compilerfehler, daher nicht ausgeführt.

StorageScenarioTest wurde nicht ausgeführt, da er die echten Benutzerordner initialisiert und mit Werksdaten befüllt. Kein vollständiger Plugin-Build, DAW-Test, Hörtest oder macOS/Linux-Build durchgeführt. Der Produktquellcode wurde für diese Analyse nicht geändert.

## Empfohlene Reihenfolge

1. Buildfehler beheben und alle Ziele im neuen Pfad frisch bauen.
2. Tests von alten absoluten Pfaden lösen, Release-Prüfungen aktiv halten und CI vervollständigen.
3. MIDI samplegenau verarbeiten und Dateizugriffe/Blockierungen aus dem Audiothread entfernen.
4. Vollständigen AFX-Zustand speichern und Wiederherstellung testen.
5. Danach Plugin in einer DAW mit verschiedenen Blockgrößen, Automation und Presetwechseln prüfen.
