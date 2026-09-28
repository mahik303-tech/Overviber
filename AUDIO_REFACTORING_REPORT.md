# Audio-Refactoring: Abschlussstand vom 26.09.2026

Nachfolgende Fehlerkorrekturen und Installation vom 27.09.2026:
[Performance, Pegel, Filterwechsel und Presetanzeige](AUDIO_FIXES_2026-09-27.md).

> **Stand 28.09.2026:** Der Legacy-Gain-Modus (Schalter „Calibrated Gain“,
> Resonanz-Vorverstärkung, ursprünglicher Console-Encoder) und das Laden einer
> einzelnen `.conf` als Setup sind entfernt. Es gibt nur noch das kalibrierte
> Gain-Staging, und Setups sind ausschließlich `.ovm`. Da es noch kein Release
> gibt, werden keine Altzustände migriert. Die Abschnitte unten beschreiben den
> damaligen Stand; Details in [REFACTORING.md](REFACTORING.md).

Arbeitskopie: dieses Overviber-Repository. Die ursprüngliche Overcycler-Kopie
wurde nicht verändert.

## Die sechs Arbeitsschritte

1. **Pegelanalyse und Elements:** Diagnosepunkte für Oszillator, Mixer, Filter,
   VCA, Bus, Console und Ausgang erfassen Peak, RMS, ungültige Samples und
   Überschreitungen von ±1. Nicht initialisierte Phasen sowie Damping/Feedback
   in Ominous wurden initialisiert. Der globale Resonanz-Vorverstärkungsfaktor
   entfällt im kalibrierten Betrieb; Legacy-Presets behalten ihn.
2. **Referenzrenderings:** `AudioReferenceRender` erzeugt unveränderte Float32-
   Stereo-WAVs, Preset-Snapshots, CSV-Messwerte und ein Manifest. Vorhandene
   Zielordner werden nicht überschrieben. Die Matrix umfasst 68 Factory-Presets
   mit einer/sechs Stimmen und 288 Elements-Fälle über vier Modelle, vier Filter,
   drei Resonanzwerte und 44,1/48/96 kHz.
3. **MIDI-Timing:** Audioblöcke werden an den Sample-Positionen eingehender
   Ereignisse geteilt. Ausgehende Arpeggiator-Ereignisse behalten ihre Position
   im Hostblock. Der Prozessortest vergleicht einen Note-On bei Sample 128 mit
   zwei entsprechend geteilten Blöcken und prüft Stille vor dem Note-On.
4. **Audio-Zustandsbesitz:** Editor und Audio besitzen getrennte Engines.
   Vorbereitete numerische Zustände einschließlich Waveframes wechseln über
   eine feste SPSC-Queue; höchstens zwei Queue-Zustände werden je Block gelesen.
   Dateien, JSON und Speicherfreigaben liegen außerhalb der Rendering-Funktion.
   Arpeggiator und interne MIDI-Ausgabe verwenden feste Puffer. Der getestete
   Enginepfad einschließlich Zustandsübernahme und aller Arp-Modi alloziert nicht.
5. **Kalibrierung und Console:** Beim Vorbereiten wird die Kleinsignal-
   Durchlassverstärkung der vier Filter mit 1 kHz gemessen. Neue Sounds verwenden
   diese Korrektur, einen weichen Encoder-Knick und eine Ausgangskennlinie ab
   |0,9| mit Grenzwert 0,98. Das ist bewusste Sättigung, keine Garantie für
   unverzerrten oder aliasfreien Klang. Die lokale Console ist eine eigene,
   von Airwindows inspirierte Implementierung. GUI und Host-Auswahl zeigen jetzt
   die tatsächliche numerische Reihenfolge Console=0, Clean=1, Mackity=2.
6. **Parts, Speicherung und UI:** Alle Routingarten verwenden dieselben 16 Parts
   und denselben Pool von sechs Stimmen. Part 1 ist zugleich das Main-Preset.
   Custom Routing bietet MIDI-Kanal/Any, Notenbereich und Aktivierung je Part;
   überlappende Bereiche bilden polyphone Layer. MPE-Member-Kanäle routen über
   Kanal 1; Ausdrucksdaten erreichen alle zugeordneten Layer. Arp-Ausgabe routet
   über Kanal 1. Das versionierte `.ovm`-JSON speichert Parts, exakte 16-Bit-
   Parameter, aktive Waveframes, Mapping, Fader und Sequencer. Laden erfolgt
   zunächst in einem temporären Zustand. Unbekannte Versionen und fehlerhafte
   numerische Presets werden abgewiesen.

## Bedienung und Kompatibilität

Im AFX/Parts-Bereich einen der 16 Parts auswählen und **Custom Routing**
aktivieren. Kanal und Low/High bestimmen dessen Eingangsbereich. **Calibrated
Gain** schaltet das neue Gain-Staging. **Save Setup / Load Setup** speichern und
laden komplette `.ovm`-Setups. Die Standalone-Anwendung bietet dieselbe Funktion;
im reinen Skin Designer sind diese zwei Dateifunktionen deaktiviert.

`.conf` bleibt das einzelne Legacy-Presetformat. Der Setup-Import routet es auf
den Main-Part und aktiviert Legacy-Gain. Numerische Parameter-IDs und bestehende
Console-/Routingwerte bleiben erhalten. Clean und Mackity sind aus diesem Grund
als Legacy-Optionen vorhanden. Ein vollständiges Löschen dieser Modi würde alte
Projekte anders wiedergeben. Eingebettet sind die vier aktuell verwendeten
Waveframes jedes Parts, nicht sämtliche Dateien seiner Wave-Bibliothek.

## Messergebnisse und Prüfungen

- Windows Release: VST3, Standalone, Skin Designer und Testprogramme gebaut.
- CTest: **11/11 bestanden**, mit aktiven Assertions auch in Release.
- Endmatrix: **424 Fälle**, keine NaN/Inf an irgendeinem Diagnosepunkt;
  maximaler Ausgangspeak **0,555741**, keine Ausgangssamples über ±1.
- Zwei unabhängige Smoke-Läufe: alle erzeugten Dateien SHA256-identisch.
- Elements-Integration bei 48 kHz: Modal 0,115925; String 0,0430934;
  Chords 0,119497; Ominous 0,00310574. Das kurze, tiefe Ominous-Szenario ist leise.
  Die früheren pauschalen Mindestpegel 0,05/0,02 waren keine kalibrierten
  Referenzen und wurden durch dokumentierte modellbezogene Intervalle ersetzt.
  Die Produktionsverstärkung wurde nicht pro Modell zum Bestehen dieser Tests
  hochgesetzt. Der Sechsstimmen-Modal-Test erreichte etwa 17-fache Echtzeit;
  dies ist eine lokale Messung und keine allgemeine CPU-Zusage.
- Setup-Roundtrip prüft auch bearbeitete Wave-Samples und exakte Parameterwerte.
  Weitere Regressionen prüfen Layer und Note-Off-Trennung nach MIDI-Kanal.

Lokale Artefakte unter `build-analysis/`:

| Datei/Ordner | Inhalt |
| --- | --- |
| `final-build.log`, `final-tests.log` | Build und gesamte Testsuite |
| `reference-before/` | Ursprüngliche Diagnose einschließlich ungültiger Ominous-Samples |
| `reference-stable/` | Vergleich nach Initialisierungsreparatur |
| `reference-after/` | Kalibrierte Endmatrix und Factory-Legacy-Renderings |
| `reference-repeat-a/`, `reference-repeat-b/` | Wiederholbarkeitsvergleich |
| `reproducibility.log` | Ergebnis des SHA256-Vergleichs |

## Verbleibende Grenzen

- Der vom Host bereitgestellte JUCE-MIDI-Ausgabepuffer kann beim Einfügen von
  Ereignissen wachsen. Der Allokationstest deckt die Engine ab; ein vollständig
  allokationsfreier gesamter Plugin-Callback ist damit **nicht** nachgewiesen.
- Programmwechsel laden Dateien auf dem Message-Thread. Sie wirken verzögert
  (30-Hz-Timer plus Ladezeit), nicht am MIDI-Sample; Offline-Hosts ohne laufende
  Message-Schleife benötigen vorab geladene Setups. Die Timing-Zusage gilt für
  die direkt verarbeiteten Noten-/Controller-Ereignisse.
- Editoränderungen gelangen über denselben Timer zur Audio-Engine. Mono/Unison
  behalten ihre bestehende globale Stimmenlogik; Layer sind für Polybetrieb
  getestet. Kalibrierung gleicht nicht die wahrgenommene Lautheit aller Sounds,
  Resonanzen, EQ-Einstellungen oder Filtermodi an.
- DAW-Wiedergabe, visuelle GUI-Abnahme sowie macOS/Linux wurden hier nicht
  ausgeführt. Die Messmatrix ist endlich; sie beweist keine universelle
  Clippingfreiheit oder bitidentische Wiedergabe sämtlicher alter Projekte.

## Projektleitlinien

Parameter-IDs stabil halten, Änderungen am Klang mit Rohdaten belegen und
Hardware-Kompatibilität von Klangidentität unterscheiden. Keine Dateioperationen,
JSON-Verarbeitung oder GUI-Zugriffe in den Rendering-Pfad aufnehmen. Audio-
Zustände besitzen klar definierte Threads; neue Übergaben brauchen begrenzte
Puffer und Überlaufverhalten. Optimierte Builds testen, Assertions aktiv halten,
Testdaten und Ausgaben isolieren. Keine Tests gegen Benutzer-Presetverzeichnisse.
Bestehende Open-Source-Lizenzen und Attributionen erhalten. Hör- und DAW-Tests
ergänzen die automatischen Prüfungen vor einer Veröffentlichung.
