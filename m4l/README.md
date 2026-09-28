# Overviber Performance Companion (Max for Live MIDI Device)

Ein hochentwickeltes **Max for Live MIDI Device** (`.amxd` & `.maxpat`), maßgeschneidert für den **Overviber (GliGli Overcycler) 6-Voice Wavetable Synthesizer**. Es schöpft das volle klangliche und ausdrucksstarke Potenzial des Synthesizers mit komplexen Voicings, dynamischen Akkordprogressionen, Strumming-Humanisierung und einer mehrdimensionalen CC-/Modulations-Engine aus.

---

## 🌟 Hauptfunktionen im Überblick

### 1. 🎹 6-Voice Evolving Chord & Voicing Engine
- **4 kuratierte Akkord-Bänke** mit jeweils 8 komplexen, 6-stimmigen Voicings:
  - **Bank 1 (Neo-Soul Velvet)**: Maj9, m11, 13sus4, Maj7#11, m9, 7alt, 9#11, EbMaj13.
  - **Bank 2 (Ambient & Cinematic Horizons)**: Weit gefächerte Cluster über 3 Oktaven (z.B. Sus2add9, m11/9, Open Lydian), perfekt für morphende Wavetable-Pads.
  - **Bank 3 (Cyberwave & Analog Nostalgia)**: Blade-Runner-Sus4-Akkorde, Vangelis CS-80 Brass, Tape-Drift-Voicings.
  - **Bank 4 (Jazz Fusion & Harmonic Exploration)**: Maj7(9,13), 7#11, m(Maj7,9), AbMaj7#5#11.
- **Voicing Spread-Modi**: *Tight*, *Drop-2* (untere Mitten oktaviert für analoge Wärme) und *Wide Shimmer* (hohe Obertöne).
- **Strumming & Humanize**: Einstellbare Anschlagsverzögerung (0–150 ms) in den Richtungen *Up*, *Down*, *Alternate* oder *Random* inklusive Mikro-Timing- und Velocity-Jitter für absolut lebendige, organische Pads.
- **Spielmodi**:
  - `Direct`: Regulärer Keyboard-Pass-Through mit voller Modulation.
  - `Key>Chord`: Jeder Tastenanschlag triggert ein erweitertes 6-stimmiges Voicing.
  - `Progression`: Weiterschalten harmonischer Progressionen bei jedem Anschlag.

---

### 2. 🎛️ Multi-Axis Modulation & Macro Controller
Das Device sendet kontinuierliche MIDI-CCs und Aftertouch direkt an die **Overviber Polyphonic Modulation Matrix**:
- **X/Y Controller**:
  - **X-Achse -> CC 1 (Mod Wheel)**: Morpht das VCF-Filter (Cutoff) und öffnet Obertöne.
  - **Y-Achse -> CC 74 (Timbre / Slide)**: Steuert die WaveMod-Tiefe (Wavefolder / Crossover-Morphing).
- **CC 11 (Expression)**: Dynamische Lautstärken- und Hüllkurven-Artikulation.
- **CC 2 (Breath)**: Steuerung des Airwindows Mackity Console Drive & Bandsättigung.
- **Aftertouch / Channel Pressure**: Direkter Anschub für Vibrato und Resonanz.

---

### 3. 🌊 Dual Synchronized Modulation LFOs
- **LFO 1 (Filter & Waveform Movement)**:
  - Wellenformen: Sinus, Dreieck, Sägezahn, Random Sample & Hold.
  - Ziele: `CC 1 Cutoff`, `CC 74 WaveMod`, `CC 11 Expression`.
  - Rate: 0.05 Hz bis 10 Hz | Depth: 0–100%.
- **LFO 2 (Organic Drift & Aftertouch Swell)**:
  - Wellenformen: Sinus, Dreieck, Golden-Ratio Organic Drift.
  - Ziele: `Aftertouch (Mod Matrix Slot 2)`, `CC 2 Breath`, `Pitch Wow/Drift`.
  - Rate: 0.02 Hz bis 5 Hz | Depth: 0–100%.

---

## 🚀 Schnellstart in Ableton Live

1. **Overviber VST3 auf einer MIDI-Spur laden**.
2. Ziehe das Device **`Overviber_Performance_Companion.amxd`** aus dem Ordner `m4l/` direkt **vor** das Overviber VST3 Plugin auf derselben MIDI-Spur.
3. Wähle im Overviber Synthesizer ein Preset, zum Beispiel einen Pad-Sound.
4. Schalte im M4L-Device auf **`Key>Chord`** oder klicke auf die **Chord-Pads 1–8**.
5. Bewege die X/Y-Regler oder aktiviere die LFOs. Wie stark die CCs wirken, hängt von der Modulationsmatrix des gewählten Presets ab.
