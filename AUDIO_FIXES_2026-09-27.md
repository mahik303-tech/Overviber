# Korrekturen für Performance, Pegel, Filterwechsel und Presetanzeige

Nachfolgende MIDI-spezifische Korrektur:
[Note-Off-Klicks bei Preset 0023](PRESET_0023_CLICK_FIX.md).

Gemeldete Umgebung: Ableton Live 12 (laufender Prozess: Live 12 Beta),
44,1 kHz, 512 Samples, UMC1820 ASIO.

## Änderungen

- Filtertypen und Filtermodi wechseln bei aktiven Stimmen über je 10 ms
  Aus-/Einblendung mit glatter Kennlinie. Der neue Filter wird bei stummem
  Übergang initialisiert und erhält sofort die aktuellen CV-Werte. Rasche
  Änderungen übernehmen das zuletzt gewählte Ziel. Es werden keine zwei
  vollständigen Filterpfade dauerhaft parallel berechnet.
- Im kalibrierten Betrieb erhalten alle Filter 12 dB Eingangsreserve; die
  Verstärkung wird danach wiederhergestellt. Damit fällt der SST bei nominalem
  Eingang nicht mehr durch vorzeitige Sättigung deutlich gegenüber den anderen
  Modellen ab. Die vorhandene Kleinsignalkorrektur bleibt bestehen.
- Preset-Browsing deaktiviert die gewählte Kalibrierung nicht länger. Explizite
  Legacy-Imports und gespeicherte Setups behalten ihren Gain-Modus.
- Unveränderte Filter-CVs werden nicht erneut berechnet. Unveränderte Waveframes
  werden nicht erneut kopiert. Vorbereitete Part-Zustände konfigurieren nur
  betroffene Parts neu; veraltete Main-Part-Snapshots überschreiben nicht mehr
  zwischenzeitlich eingegangene Host-Parameter.
- Der kalibrierte Console-Encoder nutzt eine vorab erzeugte Kennlinientabelle.
  Ein dichter Sweep prüft die Abweichung zur ursprünglichen Formel, einschließlich
  des weichen Knicks. Legacy-Encoder und Oversampling der Filter bleiben erhalten.
  Shelves berechnet unveränderte Gain-Kennwerte nicht erneut.
- `presetDisplayBtn` zeigt den Namen aus dem tatsächlich geladenen Zustand,
  einschließlich Init und `.ovm`. Der Editor aktualisiert die Anzeige nach
  verzögerten Programmwechseln. Gleicher Inhalt verursacht kein erneutes Repaint.
  Init synchronisiert die Host-Parameter; reine Namensänderungen werden gespeichert.

## Nachweise

- Vollständiger Windows-Release-Build erfolgreich, **12/12 CTest-Tests bestanden**.
- Regressionsprüfung der Filterpegel: sechs Stimmen, 44,1 kHz, Block 512,
  Cutoff offen, Resonanz null, Oszillatorlevel etwa 24 %. RMS der vier Filter
  im Clean-Bus: 0,06803 / 0,06803 / 0,07308 / 0,06843, Spannweite etwa 0,62 dB.
  Vor der Headroom-Korrektur lag SST im vergleichbaren 48-kHz-Test bei 0,04444
  gegenüber 0,06735 beim SSI. Diese Referenz beweist keine Lautheitsgleichheit
  bei beliebigen Resonanzen, Filtermodi oder unterschiedlichen Presets.
- Console/SSI-Benchmark bei 44,1 kHz und 512 Samples: etwa 68,8 auf 52,7 ms
  für 98.304 Samples mit sechs Stimmen, rund 23 % weniger Rechenzeit.
  Liquid und Shelves bleiben erheblich teurer; die gleiche Einsparung gilt
  nicht für jeden Filter. Messungen sind lokal und können schwanken.
- Größter Encoderfehler im Sweep: **2,80 × 10⁻⁷**.
- Filterwechselmatrix mit allen Modellpaaren bei 44,1/48/96 kHz: endlich;
  Sample-Sprünge bleiben innerhalb 110 % der gemessenen natürlichen Signalflanke
  plus 0,0002. Die anfängliche absolute Grenze 0,01 wurde schon vom unveränderten
  Testsignal überschritten und deshalb durch diese Vergleichsprüfung ersetzt.
- Echter Plugin-Prozessor, 44,1 kHz/512 Samples, sechs Stimmen, wiederholte
  Filterwechsel: Mittel **1,02 ms**, Maximum **1,92 ms**, Blockbudget **11,61 ms**.
  Dies ist kein ASIO-/Ableton-End-to-End-Test und keine Worst-Case-Garantie.
- Erneut **424 Referenzfälle**, keine nichtendlichen Ausgangssamples.
- Automatischer UI-Test: verzögert geladenes Preset, anschließender Init-Zustand
  und Erhalt der Kalibrierung. Zusätzlicher Test verhindert das Zurücksetzen
  aktueller Host-Parameter durch ältere Editorzustände.

Logs: `build-analysis/followup-build.log`, `followup-tests.log`,
`Testing/Temporary/LastTest.log`, `performance-44100-before-console.log` und
`reference-followup-20260927/metrics.csv` unter `build-analysis`.

## Installation und nächster Start

Installiert nach Schließen von Live:
`C:\Program Files\Common Files\VST3\Overviber.vst3`.
Die installierte Binärdatei stimmt per SHA256 mit dem geprüften Build überein:
`159346B485DCFA73A4790DDFA9BC73DB33E0AD869087010015BBAF772376097F`.

Sicherung der vorherigen Version:
`build-analysis/installed-backups/Overviber-before-20260927-fixes.vst3`.

Live neu starten und Overviber laden. In bereits gespeicherten Live-Sets kann
**Calibrated Gain** noch ausgeschaltet sein; für das neue Gain-Staging im
AFX/Parts-Bereich einschalten und das Set speichern. Bestehende Zustände werden
nicht stillschweigend auf eine andere Klangcharakteristik migriert.
