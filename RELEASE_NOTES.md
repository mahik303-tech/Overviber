# Release Notes: Overviber VST3 & Standalone

---

## Version 0.10.0 – Klang, Timing, Mod Matrix und AFX
**Datum:** September 2026

#### Klang (Presets klingen teils anders)
- Stimmen und ConsoleX-Summenbus laufen mit doppelter Abtastrate (unter 100 kHz), mit einem Halbband-Filter zurück auf die Ausgaberate: deutlich weniger Aliasing (Summenbus −62 → −80 dB, VCA −35 → −74 dB, SST-Ladder −27 → −115 dB), die Filter öffnen wieder ganz (SEM bei 15 kHz −7 → −0,1 dB).
- Oszillatoren wie auf der Hardware bis 64 kHz.
- VCA-Sättigung ohne Knick, SST-Ladder neu skaliert, Panning mit konstanter Leistung, geglättete Fader, Panorama und Cutoff (keine Zipper-Geräusche).
- Die doppelte Rate kostet Rechenzeit: +17 bis +53 % bei Wavetable-Stimmen.

#### Timing
- Arpeggiator mit Host-Sync: Bei manchen Blockgrößen spielte er gar nicht oder verlor Schritte, der Schritt am Transport-Start fehlte. Jetzt sitzt jeder Schritt auf dem nächstliegenden Sample, auch bei Loops, Stopp und Neustart.
- Swing und Gate so, wie der Regler es anzeigt (75 % Swing spielte 58 %, ein 10-%-Gate 17 %).
- Interne Clock: Der erste Arp-Ton kommt sofort beim Tastendruck.
- Noten starten und enden auf ihrem MIDI-Sample (vorher bis 0,25 ms Schwankung).
- Glide in achtmal feineren Stufen bei gleicher Zeit, Controller (Modwheel, Pitch Bend, Aftertouch, CC 74, Breath, Expression) geglättet.

#### MOD MATRIX
- Neues Tab: Übersicht der 8 Slots (anklicken, Tiefe ziehen, per Tastatur), Editor des gewählten Slots und Schnellzuweisungen.
- Rechtsklick auf einen Regler mit Modulationsziel: Modulationsquelle hinzufügen oder zur Matrix springen; modulierte Regler zeigen ihre Tiefe als Bogen.
- Host-Parameter heißen jetzt „Mod n Source/Destination/Via/Depth/On“ (automatisierbar, IDs unverändert).

#### AFX (ein Sound pro Taste)
- Neues Tab: AFX-Schalter, 16 Pads (leuchten, solange ihr Sound spielt), Sound-Auswahl mit Vor/Zurück, Pegel, „bearbeiteten Sound auf dieses Pad kopieren“, Klaviatur zum Zuweisen der Tasten, Kit speichern und laden.
- AFX bleibt an, wenn ein Preset geladen wird; der Name von Pad 1 folgt dem geladenen Preset.
- Stimmenzahl und Notenpriorität stehen jetzt im Tab FILTER / VCA (TUNING & VOICES). Der Editor für Split/Layer-Routing entfällt.

#### Bedienung
- Fokusrahmen nur bei Tastaturbedienung; das Wertfeld eines Drehreglers öffnet leer (Strg+Z holt den alten Wert zurück); die SETTINGS-Seite scrollt mit Scrollleiste und Mausrad.

#### Pakete
- Windows x64: VST3, Standalone, Skin Designer, Presets (`disk`)
- macOS Universal (Apple Silicon & Intel): VST3, AU, Standalone
- Linux x64: VST3, Standalone, Presets, Installationsskript

---

## 🌟 Version 0.9.0 – SST Vintage Moog Ladder, ConsoleX Mixing Desk & AFX Workstation Release
**Datum:** September 2026  
**Plattform:** Windows 64-Bit (VST3, Standalone)  
**Entwicklungsumgebung:** JUCE 7.0.12 / C++17 / MSVC (Visual Studio 2026)  

### 📋 Zusammenfassung des Release 0.9.0
Build 0.9.0 bringt wegweisende architektonische und klangliche Erweiterungen für Overviber:
1. **4. Filtermodell – SST Vintage Moog Ladder Filter (`sst-filters`)**:
   - Authentische analoge 4-Pol Transistorkaskade aus dem renommierten Surge XT Ökosystem (Surge Synthesizer Team / Paul Walker, GPL-3.0).
   - Modellierung der echten Differenzverstärker-Nichtlinearität mit stetiger $\tanh()$-Sättigung im Feedback-Zweig und Resonanzkompensation.
   - 2-faches internes Oversampling zur Vermeidung von Nyquist-Verzerrungen und Frequenzstauchung in den hohen Oktaven.
   - 4 wählbare Flankensteilheiten / Polabgriffe: **LP4** (24 dB/Okt), **LP3** (18 dB/Okt), **LP2** (12 dB/Okt) und **LP1** (6 dB/Okt).
   - Hervorragende Echtzeit-Performance mit **34,3-facher Echtzeit-Geschwindigkeit** im Multithread-Benchmark.
   - Volle Integration in den interaktiven `FilterCurveComponent`-Visualisierer mit dynamischer Badge- und Kurvendarstellung.

2. **Mischpult-Transformation & Airwindows ConsoleX Integration (`voiceMeterPanel`)**:
   - `voiceMeterPanel` wurde in ein voll ausgestattetes analoges 7-Kanal-Mischpult verwandelt: 6 polyphone Stimmenzüge + 1 Master-Bus.
   - Vertikale Mischpult-Schieberegler mit geschliffenen Metall-Faderkappen, beleuchteter Positionskerbe und versenkten Führungsbahnen.
   - 16-Segment vertikale LED-Aussteuerungsketten pro Stimme mit präziser dBFS-Skalierung.
   - **Airwindows ConsoleX Summing Bus**: Jede Stimme durchläuft vor der Summierung eine nichtlineare $\Phi$-Kanalphasenkodierung. Höhere Faderstellungen sättigen die Stimme warm in den Golden Ratio Summenbus.
   - Umstellung von `consoleModelCombo` auf vertikale Radio-Toggles (`consoleModelToggles[0..2]`: Clean/Linear, Symmetrical Soft, Asymmetrical Hard / Mackity).
   - Umstellung von `engineModeCombo` auf horizontal angeordnete Radio-Toggles (`engineModeToggles[0..2]`: Single, Dual, AFX).
   - Vollständige Entfernung des alten `afxSlotCombo`.

3. **Neuer TAB AFX – Dedizierte Multitimbrale Sound-Kit Workstation**:
   - Neuer Reiter **AFX** an 5. Position (zwischen `LFO / ARP` und `MOD MATRIX`), insgesamt 7 Haupt-Tabs.
   - **Verlagerung der Stimmenzuordnung**: `voiceCountSlider` (1 bis 6 Stimmen) und die Assigner-Prioritäts-Toggles (`assignerPrioToggles[0..2]`: Last, Lowest, Highest) wurden ergonomisch aus der `vcaCard` in den TAB AFX verlegt.
   - **16 Sound-Slots**: Jeder Slot speichert ein vollwertiges Synth-Patch-Profil (Filtermodell, Cutoff, Resonanz, Wellenformen, Hüllkurven).
   - **Interaktiver 128-Tasten-Keyboard-Zoneneditor (`AfxKeyboardZoneComponent`)**: Direkte grafische Zuweisung von Tastaturbereichen zu Sound-Slots per Mausklick.
   - **Quick-Mapping-Modi**: *Octave Zones* (8 Oktaven), *Chromatic 16* (Halbtonleiter), *All to Selected* (alle Tasten auf aktiven Slot) und *Copy Current Preset to Slot*.

4. **Vergrößerte Bauhöhe der ersten Modulreihe**:
   - Die Kartenhöhe der ersten Reihe (`cardTopH`) für `filterCard`, `vcaCard` und `mixerCard` wurde von 285 px auf **328 px** vergrößert, was eine entspannte Anordnung aller 4 Filtermodelle, der ConsoleX-Toggles und der horizontalen Engine-Mode-Taster ermöglicht.

5. **Aktualisierte rechtliche Hinweise & Danksagungen**:
   - Offizielle Dokumentation und Lizenzwürdigung von **Surge Synthesizer Team / Paul Walker** (`sst-filters`, GPL-3.0) sowie **Chris Johnson / Airwindows** (*ConsoleX*, MIT License).

---

## 🌟 Version 0.8.0 – Studio-Grade Hybrid Synth Release
**Datum:** September 2026  
**Plattform:** Windows 64-Bit (VST3, Standalone)  
**Entwicklungsumgebung:** JUCE 7.0.12 / C++20 / MSVC (Visual Studio 2026)  

### 📋 Zusammenfassung des Release 0.8.0
Version 0.8.0 markiert den offiziellen Release von **Overviber** als eigenständigen, modernen Studio-Synthesizer mit umfassenden klanglichen und visuellen Erweiterungen:
- **Offizieller Projektname Overviber**: Vollständige Etablierung des neuen Namens und konsistente VST3- und Standalone-Builds (`Overviber.vst3`, `Overviber.exe`).
- **Erweiterte Analog-Filter-Modelle**:
  - Sound Semiconductor SSI2144 24dB Ladder Lowpass (Zero-Delay Feedback).
  - Mutable Instruments Ripples (Liquid 24dB/12dB Lowpass & Bandpass).
  - Mutable Instruments Shelves (4-Band Parametric Console EQ & 12dB State-Variable Filter).
- **Airwindows Mackity Insert**: Legendäre analoge Mischpult-Preamp-Sättigung und Slew-Limiter direkt auf dem Master-Bus.
- **Modernes GUI & Skin Designer**:
  - 8 augenfreundliche kuratierte Farbpaletten und integrierter HSV-Palettengenerator.
  - Eingebettete D-DIN Industrie-Typografie.
  - Vollständig skalierbare Benutzeroberfläche mit Corner-Dragging und Preset-Skalierung.
  - Professioneller Modern Preset Manager mit Echtzeit-Suche, Kategorie-Chips und Vorhörfunktion.
- **Erweiterte DAW-Automation & MIDI-Steuerung**: 13 logisch gegliederte Parametergruppen in Ableton Live, Cubase, Bitwig und Reaper.

---

## 🌟 Version 0.4.0 (Build 0.4) – Modern Studio GUI & English Localization Release
**Datum:** 22. September 2026  
**Plattform:** Windows 64-Bit (VST3, Standalone)  
**Entwicklungsumgebung:** JUCE 7.0.12 / C++20 / MSVC (Visual Studio 2026)  

### 📋 Zusammenfassung des Release 0.4
Build 0.4 bringt eine vollständige Überarbeitung der modernen Studio-Benutzeroberfläche (**Modern GUI**), vollständige englische Lokalisierung und dynamische Hardware-Elemente:
- **Vollständige englische Lokalisierung**: Alle Benutzeroberflächenelemente, Tabs, Modulüberschriften, Parameter-Labels, Auswahllisten und Hilfetexte wurden auf professionelle, international standardisierte englische Synthesizer-Terminologie umgestellt.
- **Thematisch sortierte & klar bezeichnete Parameter in DAWs (Ableton Live, Cubase, Bitwig, Reaper)**:
  - Statt kryptischer Firmware-Kürzel wie `cpAFreq`, `cpAVol`, `cpCutoff`, `spLFOShape` werden in Ableton Live (Spur-Automation, MIDI-Mapping, Device-Parameter-Liste) nun selbsterklärende englische Klarnamen mit Modul-Präfix angezeigt (z. B. `Osc A: Coarse Pitch`, `Filter: Cutoff`, `Filter Env: Attack`, `LFO 1: Waveform`, `Voice: Polyphony Count`, `Perf: Glide Time`).
  - Alle Parameter sind in 13 logische thematische Funktionsgruppen gegliedert (`AudioProcessorParameterGroup`: *Oscillator A, Oscillator B, Oscillator Sync, Filter, Filter Env, Amp Env, WaveMod Env, LFO 1, LFO 2, Voice, Master, Performance, Legacy*) und erscheinen in DAWs in sauberer sequenzieller Reihenfolge statt verstreut.
  - Die internen APVTS-Parameter-IDs bleiben vollständig abwärtskompatibel, sodass bestehende DAW-Projekte und Preset-Dateien nahtlos funktionieren.
- **Thematische Gruppierung mit `ModernSectionCard`-Modulen**: Strukturierung aller Funktionsbereiche in scharfkantige Karten mit dunklem metallischem Hintergrund, Konturrahmen, Cyan-Akzentlinie, Modul-Badges (`[MASTER]`, `[SLAVE]`, `[VCF]`, `[OUTPUT]`, `[ARP]` etc.) und internen Trennlinien.
- **`ModernKnob` mit echten musikalischen Einheiten**: Drehregler zeigen physikalische Einheiten (`Hz/kHz`, `ms/s`, `st`, `ct`, `%`) statt 0–999-Rohwerte an. Bipolare Parameter sweepen von der 12-Uhr-Position.
- **`ModernLedToggle`**: Industrielle Schalter mit scharfen quadratischen LED-Indikatoren in Cyan (keine runden JUCE-Checkboxen).
- **6-Voice LM13700 VCA Gain Monitor**: Echtes Telemetrie-Rack in Tab 2 mit Gate-LEDs, 16-Segment-LED-Ketten und prozentualem VCA-Aussteuerungspegel.
- **LFO-Echtzeit-Oszilloskope**: Animierte Kurvenanzeigen mit zyklisch mitlaufendem Phasenpunkt für LFO 1 und LFO 2 in Tab 4.
- **Behebung von Klicks in der Attack-Phase**: Kontinuierliche Tiefpass-Glättung der VCA-Steuerspannung im LM13700 OTA mit RC-Zeitkonstante $\tau \approx 0{,}6$ ms und latenzfreie Noten-Initialisierung.
- **Konsequentes Industriedesign ohne abgerundete Ecken**: Alle UI-Elemente besitzen scharfe Kanten. Das Plugin-Fenster zeigt das Versions-Badge **v0.4**.

---

## 🌟 Version 0.3.0 (Build 0.3) – Modern Studio GUI & Audio Optimization Release
**Datum:** 21. September 2026  
**Plattform:** Windows 64-Bit (VST3, Standalone)  
**Entwicklungsumgebung:** JUCE 7.0.12 / C++20 / MSVC (Visual Studio 2026)  

### 📋 Zusammenfassung des Release 0.3
Build 0.3 führt eine vollständige, moderne Studio-Benutzeroberfläche (**Modern GUI**) als neuen Standard beim Laden des Plugins ein. Die neue GUI bietet direkten Zugriff auf alle Klangformungsparameter der originalen Firmware (`ui_pages.h`), ohne durch Menüs blättern zu müssen. Über einen Klick auf `[ UI: CLASSIC ]` kann nahtlos auf die Hardware-Frontplatte mit LCD gewechselt werden. Zudem wird die aktuelle Versionsnummer (**v0.3**) prominent in der Kopfzeile dargestellt.

Neu hinzugekommen sind in dieser Version:
- **Interaktiver Wellenform-Editor & Visualisierer (Tab 1)**: Direkte Freihand-Mauszeichnung für Oszillator A und B in Echtzeit mit Invertier-, Glättungs- und Normalisierungsfunktionen.
- **Interaktive SSI2144 4-Pol Filterkurve (Tab 2)**: 20 Hz – 20 kHz Frequenzgang mit interaktivem Fadenkreuz zur Cutoff- und Resonanzsteuerung samt Frequenz- und Resonanzauslese.
- **Dedizierte Beschriftung & 3 interaktive ADSR-Hüllkurven (Tab 3)**: Klare zweisprachige Kennzeichnung (*Filter-Hüllkurve / VCF ADSR*, *Lautstärke-Hüllkurve / VCA ADSR*, *Wellenform-Hüllkurve / WaveMod ADSR*) und 3 grafische Hüllkurven-Displays mit ziehbaren Knotenpunkten für A, D, S, R.
- **Vollständige MIDI- und Automationskontrolle in Ableton Live**: Bidirektionale Host-Automation über JUCE APVTS Listener und Unterstützung aller Standard-DAW-CCs (CC 74 Cutoff, CC 71 Reso, CC 73 Attack, CC 72 Release, CC 7 Volume, CC 5 Glide, CC 64 Sustain) sowie der Hardware-CCs des Overcycler.
- **Klares Industriedesign ohne abgerundete Ecken**: Alle Schaltflächen, Dropdowns, Drehregler, Chassis-Elemente und Kurvengraphen sind konsequent im modernen, scharfkantigen Rack-Design gestaltet.

---

### 🎛️ Wichtigste Neuerungen & Fehlerbehebungen in Version 0.3

#### 1. Behebung von Preset 12 ("Choir Voices") & Portamento / Glide-Algorithmus
- **Fehlerbild**: Preset 12 ("Choir Voices") und Preset 13 ("Analog Lead") gaben chaotisches Frequenz-Rauschen, quietschende Tonsprünge oder scheinbar gar keinen musikalischen Ton aus.
- **Fehlerursachen**:
  1. **Mathematische Instabilität in der Portamento-Glättung**:
     - Sowohl Preset 12 (`cpGlide = 848`) als auch Preset 13 (`cpGlide = 520`) nutzen Portamento/Glide (als einzige Werks-Presets).
     - In `SynthEngine::tickTimerEvent` war zuvor eine instabile Formel implementiert:
       `oscANoteCV[v] += (int16_t)(((int32_t)oscATargetCV[v] - oscANoteCV[v]) * 256 / glideAmount);`
     - Bei `cpGlide = 848` ergab die exponentielle Kurve `glideAmount = 13`. Der Faktor `256 / 13 = 19.69` führte zu einer massiven positiven Rückkopplung (Eigenwert $\approx -19$). Der Tonhöhenfehler verzwanzigfachte sich bei jedem 500-Hz-Tick und kippte im Vorzeichen. Dies erzeugte chaotische Tonhöhensprünge über hunderte Oktaven und extremes Audio-Rauschen.
  2. **Uninitialisierte Note-CVs beim Start**:
     - Nach Programmstart oder Preset-Laden stand `oscANoteCV` auf 0 (Note 0 = 8 Hz Sub-Audio). Bei aktivem Glide benötigten Stimmen ohne Noten-Snap mehrere Sekunden, um vom Sub-Bassbereich auf die gespielte Taste hochzugleiten.
  3. **Unisono-Muster-Aktivierung (`VoiceAssigner`)**:
     - `assigner.setPattern(currentPreset.voicePattern, currentPreset.steppedParams[spUnison])` wurde in `SynthEngine::applyPreset()` synchronisiert, sodass im Unisono-Modus alle 6 Stimmen mit Panorama-Spreizung und Detune sauber erklingen.
  4. **Fehlender `cpAmpLevel`-Wert in Legacy-Patches**:
     - `cpAmpLevel` in `preset_0012.conf` wurde auf 999 nachgetragen und der Default-Wert in `PresetData::setDefaults()` auf `UINT16_MAX` (100 %) gesetzt.
- **Lösung**:
  - **Exakte Firmware-Slew-Logik (`computeGlide`)**: Vollständige Portierung des originalen Firmware-Ratenbegrenzers:
    `out += std::min(amount, diff);` bzw. `out -= std::min(amount, diff);`
    Damit gleitet die Tonhöhe linear, gleichmäßig und stabil mit exakt der gewünschten Geschwindigkeit.
  - **Sauberer Notenstart & Preset-Reset**: Wenn noch keine Note gespielt wurde (`oscANoteCV[v] == 0`), setzt die erste Note direkt und unverzögert auf der Zieltonhöhe an. Beim Laden eines neuen Presets werden die Stimmenspeicher bereinigt.
  - Sowohl "Choir Voices" als auch "Analog Lead" klingen nun glasklar, warm und gleiten bei Legato seidenweich ins Ziel.

#### 2. Behebung von Audio-Noise bei Preset "Analog Lead" (Wavetable-Resampling)
- **Fehlerursache**: Preset 13 ("Analog Lead") lädt 600-Sample Single-Cycle-WAV-Dateien (`AKWF_saw.wav`, `AKWF_squ.wav`) von der Festplatte. Die interne Resampling-Funktion `resampleWave` in `OvercyclerTypes.h` shifttete bei jedem Zielschritt fälschlicherweise Verlaufszeiger (`p3=p2; p2=p1; p1=c;`), selbst wenn der fraktionale Zähler den Quellschritt noch nicht gewechselt hatte. Zudem war `herp` für rückwärtslaufende Phasenakkumulatoren ausgelegt. Dies erzeugte bei disk-geladenen Wellenformen alle 4 Samples schwere Stufen- und Rückwärtssprünge, die sich als digitales Summen/Rauschen bemerkbar machten.
- **Lösung**: Vollständige Ersetzung durch eine mathematisch exakte, periodische **Catmull-Rom kubische Spline-Interpolation** (4-Punkt $C^1$-stetig). Alle 600-Sample- und externen AKWF-Wellenformen klingen nun absolut sauber, analog-warm und frei von Resampling-Artefakten.

#### 3. Korrektur & Erweiterung des numerischen Keypads im Classic GUI
- **Problem**: Im Classic GUI waren die Tasten des Keypads rein mit Ziffern (`1` bis `0`) ohne Funktionsbezeichnung beschriftet und in Telefon-Reihenfolge (1-2-3 oben) angeordnet.
- **Lösung**:
  - **Duales Beschriftungsdesign**: Jede Taste zeigt nun sowohl das Tastenkürzel (`7`, `1`, `A`, `*` etc.) in großer Schrift als auch die exakte Synthesizer-Funktion (`ARP`, `SEQ`, `MISC`, `AMP`, `LFO1`, `LFO2`, `OSC`, `WMOD`, `FIL`, `PRST`, `SYNC`, `UNIS`, `PREV`, `NEXT`, `PANIC`, `TRSP`) in farblich akzentuierter Schrift.
  - **Umschaltbares Layout**: Über einen Button oben auf dem Keypad-Modul kann zwischen **PC Numpad** (Ziffernblock 7 8 9 oben – Standard) und originaler **Hardware-Matrix** (1 2 3 oben) gewechselt werden.
  - **Aktive Seiten-Hervorhebung**: Die Taste der aktuell geöffneten Parameterseite leuchtet mit einem edlen Cyan-Glow auf.
  - **Vollständige Seitenunterstützung**: Tasten `8` (Sequencer / `PageSeq`) und `9` (Miscellaneous / `PageMisc`) sowie `#` (Transpose) sind nun vollständig implementiert und steuern die entsprechenden Parameter auf dem LCD an.

#### 4. Modern Studio GUI als Standard & Ableton Live Stabilitäts-Fix
- **Modern GUI als Standardansicht**: Das Plugin öffnet sich ab Werk direkt in der modernen Studio-Ansicht (`ModernEditorView`).
- **Behebung von Ableton Live Abstürzen (0xc0000005)**:
  - *Ursache*: Ableton Live lehnt dynamische Host-Fenstergrößenänderungen (`IPlugFrame::resizeView`) bei VST3-Plugins ohne freie Skalierung oft ab oder gerät in asynchrone Reentrancy-Konflikte. Gleichzeitig führten JUCE-interne `Label::attachToComponent`-Listener auf noch nicht sichtbaren Tab-Containern zu ungültigen Geometrieberechnungen (`setBounds` mit Nullzeiger-Dereferenzierung auf Offset 0x40).
  - *Lösung*: 
    1. **Einheitliche Canvas-Dimension**: Beide Oberflächen (Modern und Classic) wurden harmonisch auf eine feste Größe von **$960 \times 620$ Pixeln** abgestimmt (`setResizable(false, false)`). Der Umschalter blendet die Komponenten sauber ein und aus – **vollständig ohne Host-Resize-Aufrufe**.
    2. **Deterministisches Komponenten-Layout**: Vollständige Ablösung von `attachToComponent` durch direkte Kindkomponenten-Platzierung in den jeweiligen Tabs mit robuster `layoutKnob`-Geometrie.
- **Preset-Synchronisation**: Wellenform- und Bankauswahlmenüs (Oszillator A und B) synchronisieren sich beim Laden von Presets und bei Preset-Wechseln nun vollautomatisch mit den Preset-Daten der Synthesizer-Engine.

#### 5. Behebung von Preset 32 ("UK House Chord ST") & Zero-Delay Feedback (ZDF) Filterarchitektur
- **Fehlerbild**: Bei Preset 32 ("UK House Chord ST") klang der Sound ab Cutoff 348 und Resonance 601 aufsteigend zunehmend unharmonisch, schrill und dissonant.
- **Fehlerursachen**:
  1. **Cutoff-Skalierungsfehler**: In `Ssi2144Filter::setCV` wurde die Grenzfrequenz zuvor über `note = cvCutoff / 256.0f` skaliert. Dies bildete den 16-Bit-Potiwert fälschlicherweise auf 256 Halbtöne (bis zu 21 MHz) ab. Bei Potistellung 348 lag die Grenzfrequenz dadurch bereits bei 1.411 Hz statt der auf der Benutzeroberfläche angezeigten 222 Hz. Bereits ab Potistellung 526 schlug die Grenzfrequenz an die 20-kHz-Obergrenze an.
  2. **1-Sample-Feedback-Verzögerung ($z^{-1}$)**: Die 4-Pol-Ladder-Struktur nutzte eine klassische Rückkopplungsschleife mit einer Sample-Verzögerung (`fb = res * fastTanh(y[3])`). Bei hohen Grenzfrequenzen bewirkt diese Verzögerung eine drastische Phasenverschiebung im Rückkopplungspfad. Die Stabilitätsschwelle sank von $k = 4{,}0$ auf unter $k = 2{,}2$. Infolgedessen geriet der Filter bei Resonance 601 ($k \approx 2{,}46$) selbst ohne Tastenanschlag in eine heftige, parasitäre Eigenschwingung (~9,7 kHz).
  3. **Intermodulationsverzerrung**: Da Preset 32 einen 5-stimmigen Moll-Akkord ohne Filter-Keyboard-Tracking spielt, mischte sich die unharmonische 9,7-kHz-Selbsterregung über die nachfolgenden Sättigungsstufen (`fastTanh`) mit allen 5 Stimmen. Dies erzeugte chaotische Summen- und Differenztöne (Phantom-Harmonische) quer durch das hörbare Spektrum.
  4. **Fehlende Detune-Anbindung**: Die Parameter `cpDetune` und `cpMasterTune` wurden in `SynthEngine.cpp` für Osc A und B noch nicht ausgewertet.
- **Lösung**:
  - **Zero-Delay Feedback (ZDF / TPT) 4-Pol Topologie**: Vollständiger Umbau des SSI2144-Filters auf eine verzögerungsfreie Topologie mit algebraisch aufgelöstem Rückkopplungspfad ($u = \frac{\text{in} - k S}{1 + k G^4}$). Phasenverschiebungen durch Latenz werden vollständig eliminiert.
  - **Logarithmische Cutoff-Skalierung**: Exakte logarithmische Frequenzabbildung von 20 Hz bis 20.000 Hz, synchron zur grafischen Frequenzgangsanzeige der GUI.
  - **Musikalische Resonanzkurve**: Bei Resonance 601 liefert das Filter nun eine seidige Resonanzüberhöhung ($Q \approx 4\text{--}6$) mit 0 % parasitärer Selbstoszillation. Reine Sinus-Selbstoszillation setzt analoggetreu erst bei Extremwerten (> 85 %) ein.
  - **Detune-Kopplung**: `cpDetune` und `cpMasterTune` werden nun hardwaregetreu auf beide Oszillatoren angewendet.

#### 6. Behebung von Klickgeräuschen in der Attack-Phase (Preset "Accoustic Piano" & VCA-Glättung)
- **Fehlerbild**: Bei Preset 3 ("Accoustic Piano") und anderen Presets mit sehr kurzen Attack-Zeiten (`cpAmpAtt = 0`) traten bei Notenanschlägen störende, hochfrequente Klicks und Knackser auf.
- **Fehlerursachen**:
  1. **Diskontinuierlicher VCA-Verstärkungssprung**: Auf der Original-Hardware wird die Steuerspannung des 4.000-Hz-DACs über ein analoges RC-Glied ($R = 4{,}7\text{ k}\Omega$, $C = 100\text{ nF} - 470\text{ nF}$) an den $I_{abc}$-Eingang des LM13700 geführt. In der VST-Engine fehlte diese Glättung: Bei `cpAmpAtt = 0` sprang `currentGain` innerhalb eines einzigen Samples von $0{,}0$ auf $1{,}0$. Trat dieser Sprung bei einer Phasenlage der Oszillatoren ungleich Null auf, entstand ein stufenförmiger Spannungssprung von bis zu $\Delta = 0{,}08$ in 22 $\mu$s, der sich als Knacken bemerkbar machte.
  2. **Verzögerte CV-Initialisierung (Stale Voice Parameters)**: Wenn der Voice-Assigner eine Note zuteilte (`gateOn`), wurde die Stimme zwar aktiviert, ihre Tonhöhe und Filterwerte wurden jedoch erst bis zu 12 Samples später beim nächsten 4.000-Hz-Tick berechnet. In diesen ersten Samples erklang die Stimme kurzzeitig mit den Parametern der vorherigen Note.
  3. **Fehlende Nullung inaktiver Stimmen**: Inaktive Stimmen wurden in der CV-Schleife übersprungen und behielten ihren vorherigen VCA-Gain-Wert im Speicher.
- **Lösung**:
  - **Hardwaregetreue VCA-Steuerspannungs-Glättung (`Lm13700Vca`)**: Kontinuierliche Tiefpass-Glättung der VCA-Verstärkung pro Sample mit einer analogen Zeitkonstante von $\tau \approx 0{,}6\text{ ms}$. Scharfe Stufen-Diskontinuitäten werden vollständig beseitigt (max. $\Delta$ sinkt von $0{,}074$ auf unter $0{,}003$ – über 96 % Reduktion), während der perkussive Klavieranschlag voll erhalten bleibt.
  - **Sofortige Initialisierung bei Note-On (`updateSingleVoice`)**: Bei jedem Notenanschlag werden Tonhöhe, Grenzfrequenz, Modulation und Hüllkurvenstartwert unverzüglich und latenzfrei ab Sample 0 gesetzt.
  - **Sauberer Nullung & Reset inaktiver Stimmen**: Freigegebene Stimmen werden im VCA explizit auf Pegel 0 gesetzt.

#### 7. Modern Studio GUI Overhaul: Vollständige englische Lokalisierung & dynamische Rack-Module
- **Dateien**: [`ModernEditorView.h`](file:///d:/Google%20Gemini/GliGli%20Overcycler/vst/Source/ui/ModernEditorView.h) & [`ModernEditorView.cpp`](file:///d:/Google%20Gemini/GliGli%20Overcycler/vst/Source/ui/ModernEditorView.cpp)
- **Vollständige englische Lokalisierung**:
  - Alle Benutzeroberflächenelemente, Tabs, Modulüberschriften, Parameter-Labels, Auswahllisten und Hilfetexte wurden auf professionelle, international standardisierte englische Synthesizer-Terminologie umgestellt.
  - Kryptische Firmware-Kürzel (`cpFilKbdAmt`, `cpWModAEnv`, `spAWModType`, `cpAmpAtt` etc.) wurden durch intuitive Audiobegriffe (`KEY TRACKING`, `ENV DEPTH`, `MOD DEPTH`, `ATTACK`, `COARSE PITCH`, `FINE DETUNE` etc.) ersetzt.
- **Thematische Gruppierung mit scharfkantigen `ModernSectionCard`-Modulen**:
  - Optische Zusammenfassung aller Funktionsbereiche (*OSCILLATOR A*, *OSCILLATOR B*, *MASTER MIXER & TUNING*, *SSI2144 ANALOG FILTER*, *AMPLIFIER & MASTER OUTPUT*, *ENVELOPES*, *LFOS*, *PERFORMANCE & ARP*).
  - Jede Karte verfügt über einen dunklen Industrie-Hintergrund, scharfe Konturlinien, ein Header-Banner mit cyanfarbener Akzentleiste, modulbezogene Badges (`[MASTER]`, `[SLAVE]`, `[VCF]`, `[24 dB/OCT]`, `[OUTPUT]`, `[ARP]` etc.) und strukturierende interne Trennlinien mit thematischen Unterbeschriftungen.
- **Dynamische Hardware-Komponenten**:
  - **`ModernKnob` mit physischen Audio-Einheiten**: Drehregler zeigen nun echte musikalische Werte direkt anstelle von Rohwerten 0–999 an (z. B. `20 Hz – 20.0 kHz` für Filter-Cutoff, `ms / s` für Hüllkurven und Glide, `st` für Halbtöne, `ct` für Cent-Detuning, `%` für Lautstärken, Resonanz und Modulationstiefen).
  - **Bipolare Sweep-Arcs**: Bei bipolaren Reglern (Detune, Tune, Pitch, Mod-Tiefen) sweepet der cyanfarbene LED-Kranz dynamisch von der 12-Uhr-Mittelposition nach links oder rechts.
  - **Scharfgekantete `ModernLedToggle`-Schalter**: Vollständiger Ersatz von Standard-Checkboxen durch industrielle Kipp- und Druckschalter mit leuchtenden, rechteckigen Cyan-LEDs.
  - **Scharfgekantete Dropdown-Menüs & Popups**: Dunkles Rack-Design mit Cyan-Fokusumrandung und scharfer Winkelpfeil-Grafik.
- **Echtzeit-Hardware-Telemetrie & Oszilloskope**:
  - **6-Voice LM13700 VCA Gain Monitor**: Ein dediziertes Telemetrie-Rack in Tab 2 visualisiert alle 6 Stimmen mit scharfen Gate-LEDs, 16-Segment-LED-Ketten in Cyan/Weiß und prozentualem VCA-Aussteuerungspegel.
  - **LFO-Echtzeit-Oszilloskope**: In Tab 4 visualisieren zwei animierte Oszilloskop-Displays die aktuelle LFO-Wellenform samt phasenstetig mitlaufendem Trace-Marker.
  - **Scharfkantige Stimmen-LEDs in der Kopfzeile**: Die oberen Stimmenanzeigen wurden von runden Kreisen auf scharfkantige LED-Blöcke umgestellt.
- **Konsequenter Verzicht auf abgerundete Ecken**: 100 % scharfes, professionelles Industrie- und Hardware-Rack-Design im gesamten Plugin.

---

### 🎨 Die 5 Reiter des Modern Studio Editors (`ModernEditorView`)
Die moderne Oberfläche ist in 5 logische Reiter strukturiert und deckt **alle Firmware-Optionen** vollständig ab:

- **1. Oscillators Tab**:
  - **Dual-Oszillatoren A & B**: Unabhängige Auswahl von Bank und Wellenform (über 4.400 Single-Cycle-Waves), Pegel, Detune, Sub-Oszillator, Oktav-, Halbton- und Cent-Transponierung.
  - **7 WaveMod-Modi**: Vollständige Parameterkontrolle über *Off*, *Aliasing* (Grit), *Width* (PWM), *CrossOver* (Wave-Morphing), *Folder* (Wavefolding), *BitCrush* (Sample-/Bitrate) und *Frequency* (FM) inklusive Zweitwellenform-Auswahl und Modulationsstärke.
  - **Hard-Sync**: Direkte Master/Slave-Phasensynchronisation per Klick aktivierbar.
  - **Grafischer Wellenform-Display & Freihand-Zeicheneditor**:
    - Echtzeit-Oszilloskop-Display mit Phasenteilern und Nulldurchgang.
    - Freihändiges Zeichnen von Wellenformen mit der Maus direkt in den Wave-Puffer der Synthesizer-Engine.
    - Schnelle Werkzeuge: `[OSC A]` / `[OSC B]` Umschalter, `[INVERT]` (Phasenumkehr), `[SMOOTH]` (Gleitender Mittelwert) und `[NORM]` (Peak-Normalisierung).

- **2. Filter & VCA Tab**:
  - **SSI2144 4-Pol 24 dB/Okt Kaskadenfilter**: Cutoff, Resonanz (mit Lautstärkekompensation), Filter-Hüllkurventiefe (bipolar), Velocity-Empfindlichkeit und Key-Tracking (Key Follow).
  - **VCA & Noise**: Rauschgenerator mit hardwaregetreuem Pseudo-Zufall (LFSR) und Master-Lautstärke.
  - **Interaktiver Filter-Frequenzgang**:
    - Grafische Darstellung des 20 Hz – 20 kHz Frequenzgangs mit 24 dB/Okt-Flankensteilheit und Resonanzüberhöhung ($Q$).
    - Interaktives Fadenkreuz-Node: Horizontales Ziehen regelt Cutoff, vertikales Ziehen regelt Resonanz.
    - Präzise Textauslese von Grenzfrequenz (Hz / kHz) und Resonanz (in %).

- **3. Envelopes Tab**:
  - **Klare zweisprachige Kennzeichnung**:
    - **Filter-Hüllkurve (VCF ADSR)**
    - **Lautstärke-Hüllkurve (VCA ADSR)**
    - **Wellenform-Hüllkurve (WaveMod ADSR)**
  - **3 interaktive ADSR-Kurven**:
    - Eigene grafische Kurvendarstellung für jede der drei Hüllkurven mit schattiertem Farbverlauf und Phasenabschnitten (A, D, S, R).
    - Draggbare Rechteck-Knotenpunkte:
      - Attack-Peak: Ziehen nach links/rechts passt Attack-Zeit an.
      - Decay/Sustain-Knoten: Ziehen nach links/rechts passt Decay-Zeit an, Ziehen nach oben/unten passt Sustain-Pegel an.
      - Release-Knoten: Ziehen nach links/rechts passt Release-Zeit an.
    - Volle bidirektionale Kopplung mit den Drehreglern und der Synth-Engine.
  - **Erweiterte Parameter pro Hüllkurve**:
    - Umschaltbar zwischen **Fast** und **Slow** Zeitbasis.
    - Umschaltbar zwischen **Exponential** und **Linear** Verlauf.
    - Bipolare **Velocity**-Steuerung.
    - **Loop-Modus**: Hüllkurve wiederholt sich zyklisch als komplexer LFO, solange eine Taste gehalten wird.

- **4. LFOs Tab**:
  - **Dual LFO 1 & LFO 2**: 7 umschaltbare Wellenformen (*Pulse, Triangle, Random/S&H, Sine, Noise, Saw, InvSaw*).
  - **Timing**: Frequenzregelung mit Tempo-Multiplikatoren und schaltbarem Tasten-Retrigger.
  - **Modulations-Matrix**: Direkte Zuweisung und Stärkenregelung auf *Pitch A/B*, *WaveMod A/B*, *Cutoff*, *Resonanz* und *VCA Amp*.

- **5. Performance & Arpeggiator Tab**:
  - **Voice Assigner**: Polyphonie- und Unisono-Modus mit Stereo-Detune-Spreizung, Notenpriorität (*Last, Low, High*), Legato-Modus und exponentielles Portamento/Glide.
  - **Arpeggiator**: Modi *Up, Down, Up/Down, Random, Assign* mit Taktteilern und interner 500-Hz-Ticker-Synchronisation.
  - **Pitchbend-Range**: Einstellbar von 1 bis 24 Halbtönen.

- **Echtzeit Voice-Visualisierung**:
  - Sowohl in der Classic- als auch in der Modern-Ansicht zeigen **6 aktive LED-Meter** den aktuellen Status, die Stimmenverteilung und die Hüllkurvenauslastung der 6 Stimmen in Echtzeit an.

---

## 🛠️ Historie: Version 0.2.0 (DSP Stabilization & Performance Release)
*(Zuvor geführt als Version 1.0.1 / 1.1)*

- **Wavetable-Oszillator (`WtOsc`)**: Dynamische Anpassung an die DAW-Samplerate (44,1 kHz bis 192 kHz). Behebung von Unterläufen des Akkumulators und Begrenzung des Hermite-Interpolationsfaktors $\alpha \in [0, 4096]$ zur vollständigen Beseitigung digitaler Spikes und Knackser.
- **VCA-Stufe (`Lm13700Vca`)**: Beseitigung des 46 %-DC-Offsets durch Ersetzung der Hardware-Diodentabelle durch eine stetige Kennlinie mit hyperbolischer Tangens-Sättigung ($\tanh$).
- **Analog-Filter (`Ssi2144Filter`)**: Begrenzung der Resonanzrückkopplung auf den musikalischen Bereich $k \in [0{,}0, 4{,}1]$ und Schutz vor Überläufen der internen Integratorstufen.
- **Noise-Pegel**: Harmonische Balance des Rauschpegels auf Werkspresets.
- **Puffer-Initialisierung**: Zuverlässiges `buffer.clear()` in jedem Audio-Block.

---

## 📂 Installationsverzeichnisse

| Komponente | Dateipfad |
|---|---|
| **VST3 Plugin (64-Bit)** | `C:\Program Files\Common Files\VST3\Overviber.vst3` |
| **Standalone Anwendung** | `build\Overviber_artefacts\Release\Standalone\Overviber.exe` (im Repository) |
| **Presets & Wellenformen** | `C:\Users\<DeinNutzer>\Documents\Overviber\` |
| **Quellcode** | Wurzelverzeichnis dieses Repositorys |

---

## 🎛️ DAW-Einrichtung

1. **DAW öffnen** (z. B. Ableton Live, FL Studio, Reaper, Cubase, Studio One, Bitwig).
2. **VST3-Plugin-Scan** ausführen (sofern nicht automatisch erkannt).
3. **Overviber** laden.
4. Über die Kopfzeile zwischen **[ UI: MODERN ]** und **[ UI: CLASSIC ]** umschalten!
