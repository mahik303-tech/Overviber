# Overviber Modern Skin — GUI Design Guide & Spezifikation (v3.0)

Dieses Dokument ist die verbindliche Referenz für das Design, das Layout und alle UI-Komponenten der modernen Benutzeroberfläche (**Modern Skin**) des **Overviber**-Synthesizers.

Ziel ist ein kompromisslos konsistentes, ergonomisches Interface im Stil **industrieller Studio-Hardware** (Dieter Rams / High-End Pro Audio), bei dem identische Funktionen modulübergreifend exakt gleich gestaltet, skaliert und bedient werden.

---

## 1. Kernprinzipien & Design-Philosophie

1. **Strikte 90°-Hardware-Kanten (`Corner Radius = 0.0px`)**
   Keine abgerundeten Ecken im gesamten Interface – alle Cards, Buttons, Dialoge, Toggles, Overlays und Displays besitzen messerscharfe Kanten.
2. **5px-Hardware-Raster (`Base Grid = 5px`)**
   Sämtliche Abstände, Margins, Innenabstände (Paddings) und Komponenten-Dimensionen sind Vielfache von 5px.
3. **1.0px Kantenstärke (`Border Width = 1.0px`)**
   Einheitliche Konturen für Modulrahmen, Schienen, Trennlinien und Steuerelemente.
4. **Einheitliche 55px-Drehreglergröße (`Standard = 55px`)**
   Ausnahmslos alle Drehregler im gesamten Synthesizer (Osc, Filter, VCA, Envelopes, LFOs, Arp, Settings) nutzen die einheitliche Hardwaregröße von **55px** ($11 \times 5\text{px}$). Sondergrößen (Small, Hero) sind eliminiert.
5. **1.5x-Label-Breitenregel**
   Parameterbeschriftungen überschreiten die Reglerbreite um maximal 50 % ($W_{\text{label}} \le 1.5 \times 55\text{px} = 82.5\text{px}$).
6. **Deterministisches 7-Rollen-Farbsystem**
   Jedes grafische Element bezieht seine Farbe aus einer von 7 semantischen Theme-Rollen.

---

## 2. Design-Tokens & Geometrie-Konstanten

Definiert im C++ Header `vst/Source/ui/theme/ModernTheme.h`:

```cpp
namespace ComponentTokens {
    constexpr float cornerRadius              = 0.0f;  // Keine Rundungen
    constexpr int   grid                      = 5;     // 5px Basis-Raster
    constexpr float borderWidth               = 1.0f;  // 1px Standard-Kanten
    constexpr int   cardGap                   = 5;     // Spalten- und Zeilenabstand
    constexpr int   headerHeightMin           = 20;    // Minimale Header-Höhe
    constexpr int   headerHeightMax           = 25;    // Maximale Header-Höhe (Std: 22px)
    constexpr int   protectedVoiceMonitorWidth = 236;  // Geschützte Voice-Monitor-Zone
    constexpr float maxLabelWidthRatio        = 1.5f;  // Max. Label-Breite = 1.5x Knob-Breite

    namespace KnobSizes {
        constexpr int Standard = 55; // 11x5: Verbindlicher Standard für ALLE Drehregler
        constexpr int Large    = 55; // Alias
        constexpr int XL       = 55; // Alias
    }
}
```

---

## 3. Semantisches 7-Rollen Farbsystem (`ModernTheme`)

Jedes Theme im Modern Skin basiert auf 7 Rollen:

| Rolle | ID | UI-Bezeichnung | Standard (Cyber Cyan) | Verwendung |
| :---: | :-: | :--- | :--- | :--- |
| `RoleAccent` | **0** | **ACCENT** | `#18B5C9` | Aktive Bögen, Slider-Füllung, LEDs, Tabs, Badges, Visualizer-Kurven |
| `RoleWindowBg` | **1** | **CHASSIS** | `#0D0F14` | Hauptfenster-Hintergrund, Metall-Chassis |
| `RoleCardBg` | **2** | **PANELS** | `#13161C` | Modulkarten-Fläche (`cardBg`), Listen, Text-Editoren |
| `RoleCardHeader` | **3** | **HEADER** | `#1A1E26` | Kopfzeilen der Cards, Button-Körper, Tooltips, Modale Dialoge |
| `RoleCardBorder` | **4** | **BORDERS** | `#292F3C` | Alle 1.0px Rahmen, Divider-Linien, Schienen |
| `RoleKnobs` | **5** | **DIALS** | `#2A2E38` | Reglerkörper-Oberseite, LED-Gehäuse, Swatches |
| `RoleText` | **6** | **TEXT** | `#E0F7FA` | Titelzeilen, Werteanzeigen, aktive Beschriftungen |

### 8 Integrierte Werks-Paletten
1. **Cyber Cyan (Default)**: Avionics-Cyan mit tiefem Schiefer-Chassis.
2. **Solar Amber**: Vintage Röhren-Gold mit Wolfram-Bronze-Gehäuse.
3. **Acid Neon Green**: Phosphor-Oszilloskop-Grün auf Obsidian-Schwarz.
4. **Nordic Frost**: Eloxiertes Polar-Blau mit Platin-Schriftzug.
5. **Synthwave 80s**: Magenta-Rose mit nächtlichem Violett-Chassis.
6. **Industrial Slate**: Sicherheits-Orange auf Mattgrau (Dieter-Rams-Stil).
7. **Obsidian Crimson**: Taktisches Radar-Rot auf Stealth-Carbon.
8. **Monochrome Stealth**: Entspiegeltes Platin-Weiß auf Titan-Schwarz.

*Zusätzlich: Dynamischer HSV-Generator (`ModernTheme::createCustomTheme`) & Speichern benutzerdefinierter Paletten in `UserPalettes.xml`.*

---

## 4. Typografie-Hierarchie (`ModernFontManager`)

Die primäre Schriftart ist **D-DIN** (integrierte TrueType-Vektorschrift) mit Fallbacks auf robuste Systemschriften. Die globale Skalierung ist im Settings-Tab von **0.7x bis 1.5x** anpassbar.

| UI-Element | Schreibweise | Schriftgröße | Schriftschnitt | Farbrolle |
| :--- | :--- | :---: | :---: | :--- |
| **Card Header Title** | ALL CAPS | `11.0f` | **Bold** | `theme.textTitle` |
| **Card Header Badge** | ALL CAPS | `9.0f` | **Bold** | `theme.accent` |
| **Section Dividers** | ALL CAPS | `9.0f` | **Bold** | `theme.textMuted` |
| **Parameter Labels** | ALL CAPS | `9.0f` | **Bold** | `theme.textMuted` |
| **Navigation Tabs** | ALL CAPS | `10.5f` | **Bold** | `theme.textTitle` / `textMuted` |
| **Action & Header Buttons** | ALL CAPS | `10.5f` | **Bold** | `theme.textTitle` |
| **Auswahl-Buttons** | ALL CAPS | `10.5f` | **Bold** | `theme.textTitle` / `textMuted` |
| **Numeric Readouts** | Tabellarisch | `10.0f` | **Bold** | `theme.textTitle` |
| **Debug Tooltips** | ALL CAPS / Monospace | `15.0f` | **Bold** | `#FFFFFF` |

---

## 5. UI-Komponenten & Spezifikationen

### 5.1 Section Card (`ModernSectionCard`)
Container zur visuellen Bündelung von Modulen.
- **Kanten**: `0.0px` Radius, `1.0px` Rahmen in `theme.cardBorder`, Hintergrund `theme.cardBg`.
- **Header**: Höhe `22px`, Hintergrund `theme.cardHeader`, 1px Trennlinie unten.
- **Titel**: Links `8px` eingerückt, `11.0f bold`, ALL CAPS.
- **Badge**: Rechts im Header, `9.0f bold`, Farbe `theme.accent` (z. B. `CORE`, `VCF`, `AMP`, `LFO`, `ARP`).
- **Akzentbalken (`setAccentBar`)**: Optionaler 3px breiter vertikaler Akzentstreifen am linken Rand.
- **Trennlinien (`addDivider`, `addVerticalDivider`)**: 1px Linien in `theme.cardBorder` mit zentriertem ALL CAPS Text (`9.0f bold`, `theme.textMuted`). Sicherheitsabstand zu Headern: $y \ge 30\text{px}$.

### 5.2 Drehregler (`OvercyclerKnob` / Rotary Slider)
Zentrales Bedienelement für alle kontinuierlichen Parameter.
- **Verbindliche Größe**: Einheitlich **`55 x 55px`** ($11 \times 5\text{px}$).
- **12-Uhr-Indexmarkierung**: Fester Referenzstrich zur Nullpunkt-Orientierung.
- **Wertbögen**: 3.5px Linienstärke. Unipolar: Start bei 7 Uhr (210°). Bipolar: Start bei 12 Uhr (0°) nach links (-) bzw. rechts (+).
- **Reglerkörper**: Radialer Metall-Farbverlauf (`knobBodyTop` $\rightarrow$ `knobBodyBot`).
- **Nadel**: Scharfkantige Rechteck-Nadel (`2.2px` in `theme.knobNeedle`).
- **Label**: Mittig unter dem Regler, ALL CAPS, `9.0f bold`, max. 82.5px Breite. Mindestens **16px** Sicherheitsabstand zu nachfolgenden Linien/Kanten.

### 5.3 Linearer Schieberegler (`LinearSlider`) und Wertebalken (`LinearBar`)
Für Pegel, Mix und Filter-Resonanzkurven.
- **Schiene (Track)**: `2.0px` Breite in `theme.cardBorder`, Bipolar-Markierung in der Mitte.
- **Thumb**: Scharfes Rechteck (`14 x 8px` horizontal / `8 x 14px` vertikal) in `theme.accent` mit 1px Rahmen.
- **Wertebalken** (`juce::Slider::LinearBar`, z. B. NOISE auf OSC, Stimmenzahl, Matrix-Tiefe, Split-Tastenbereich): Feld wie ein Button (`theme.buttonBg`), von links (bipolar: von der Mitte) halbtransparent in `theme.accent` gefüllt, 2px Akzentlinie am Wert, Name und Wert als Text im Balken. Eine Modulation der Matrix zeigt sich als Linie am unteren Rand.

### 5.4 LED-Taster & Toggles (`juce::ToggleButton`)
Nur für Ein/Aus-Schalter (Sync, Loop, Unison, Hard Sync, Latch, Anzeige-Optionen); eine Wahl zwischen mehreren Werten ist eine Button-Reihe (5.6).
- **Gehäuse**: Quadratisch `13 x 13px`, `0.0px` Radius, Hintergrund `theme.knobBodyTop`, Rahmen `1.0px`.
- **LED-Kern**: Quadratisch `7 x 7px` in `theme.accent` (ON) mit zentralem weißen `3 x 3px` Glanzkern. Im OFF-Zustand abgedunkelt (`opacity = 0.35f`).
- **Beschriftung**: ALL CAPS, `9.0f bold`, `theme.textTitle` (aktiv) bzw. `theme.textMuted` (inaktiv).

### 5.5 Action- & Header-Buttons (`ModernHeaderButton` / `TextButton`)
- **Höhe**: Standard `26px` (Bereich: 20–28px), `0.0px` Radius, `1.0px` Rahmen.
- **Zustände**: Default (`theme.cardHeader`), Hover (+12 % Helligkeit), Active (`theme.accentDark` mit 2px Akzentbalken unten).
- **Flash-Animation**: Automatische optische Erfolgsrückmeldung (z. B. `SAVED!` für 1200ms).

### 5.6 Auswahl als Button-Reihe (`ModernChoiceButtons`) statt Dropdown
Der Modern Skin hat keine Dropdowns: Jede Wahl zwischen wenigen Werten ist eine Reihe oder ein Raster von Buttons (eine Radio-Gruppe), alle Optionen bleiben sichtbar, ein Klick wählt.
- **Aufbau**: `ModernChoiceButtons(labels, columns)`; `columns = 0` legt alle Optionen in eine Reihe. Gewählte Option im Stil des aktiven Buttons (5.5).
- **IDs**: Die Buttons heißen `<prefix>[i]`, die Gruppe `<prefix>Choices` (`setIdPrefix`).
- **Lange Listen**: Presets (AFX-Pad) sind eine Liste (`juce::ListBox`) mit Nummer und Name; die Wellen (Hunderte, in Bänken) wählt das Menü bzw. der Browser am Waveform-Editor.
- **Verteilung in der Karte**: Button-Reihen nicht alle oben stapeln; oben, in der Mitte (zwischen den Reglerreihen) und unten verteilen (z. B. FILTER: Familie oben, Regler, Filtertyp unten; LFO: Form oben, Bereich/Trigger zwischen den Reglern, Pitch-Ziel unten).

### 5.6.1 Symbole (`ModernGlyphs.h`)
Wo ein Symbol die Option zeigt, steht es auf dem Button, gezeichnet als Linie in der Textfarbe; meist mit kurzem Text daneben (Name und voller Text im Tooltip).
- **LFO-Formen**: nur Symbol.
- **Filtertyp** (Tiefpass/Bandpass/Hochpass/Notch mit Flankensteilheit) und **EQ-Band** (Shelf/Peak): Frequenzgang + Text.
- **Hüllkurventyp**: exponentiell oder linear + `FAST`/`SLOW X4`.
- **WaveMod-Typ** (GRIT, PWM, FM, MORPH, FOLD, CRUSH): was der Typ aus einem Sinus macht + Name (`modernglyphs::SymbolButton`).
- **Paletten**: Farbchip (Akzent und Kartenfläche) + Name.
- Ohne eindeutiges Symbol bleibt Text: Arp-Modi, MIDI-Kanäle, Prozentwerte, Namen.

### 5.7 Interaktive Grafik-Visualizer
- **Waveform-Editor (`WaveformEditorComponent`)**:
  - Pixelgenaues Zeichnen von Wavetable-Frames per Maus.
  - Header mit Wellenform-Display-Button (Klick öffnet Drawer) und bedarfsgesteuertem Diskettensymbol (`SaveDisketteButton`) bei ungespeicherten Änderungen.
  - Toolbar mit `SMOOTH` (Multiplikator-Slider), `INVERT` und `NORMALIZE` (Gain-Target-Slider).
- **Filter-Frequenzgang (`FilterCurveComponent`)**:
  - Interaktive Cutoff- und Resonanz-Nodes mit Mausrad-Q-Faktor-Steuerung.
- **ADSR-Kurven (`AdsrCurveComponent`)**:
  - Drag-fähige Kontrollpunkte für Attack, Decay/Sustain und Release mit Hüllkurven-Füllung.
- **LFO-Wellenform-Vorschau (`LfoWavePreviewComponent`)**:
  - Phasen-animierte Wellenformanzeige (Sinus, Dreieck, Sägezahn, Rechteck, S&H).
- **16-Step Arpeggiator-Matrix (`ArpVisualizerComponent`)**:
  - Echtzeit-Synchronisation mit DSP-Engine (`arp.getPattern()`).
  - 16 Steps $\times$ 8 Tonhöhenspuren, Notennamen-Telemetrie (z. B. `C4`), animierter Playhead und Gate-Pulse-Meter.

### 5.8 Modale Dialoge & Overlays
- **Backdrop**: Halbtransparentes Schwarz (`rgba(0, 0, 0, 0.6)`), fängt alle Hintergrund-Klicks ab.
- **Dialogfenster**: `0.0px` Radius, `1.0px` Rahmen in `theme.accent`, Header in `theme.cardHeader`.
- **Schließen**: Escape-Taste oder Klick außerhalb.
- **Color Picker Modal (`ModernColorPickerModal`)**: Großzügige `600 x 490px` Bounding-Box, Farbrad, RGB/HSV-Slider, Live-Vorschau, `APPLY` und `CANCEL`.
- **Preset Browser Overlay (`ModernPresetBrowserOverlay`)**: Vollflächiger Drawer mit Dual-Modus (`PATCH PRESETS` / `WAVEFORMS & TABLES`), Live-Textsuche und Kategorie-Chips.

---

## 6. Layout-Architektur & Tab-Struktur

### 6.1 Feste Header-Leiste (Top-Bar, 50px)
- **Links**: Logo (`brandLogoLabel`), Subtitel (`subtitleLabel`) und Versions-Badge (`versionBadgeLabel`).
- **Mitte**: Modern Preset Bar (`<`, `ModernPresetDisplayButton`, `>`, `SAVE`, `INIT`) – vertikal zentriert bei $y = 12\text{px}$, Höhe $26\text{px}$.
- **Rechts**: 6-Stimmen Voice-Monitor in der **geschützten Zone** ($W \ge 236\text{px}$).

### 6.2 Die 7 Haupt-Tabs

```
+---------------------------------------------------------------------------------------------------------+
| [GLIGLI OVERCYCLER]   [ < ] [ 01: INIT LEAD   | SYNTH v ] [ > ]  [ SAVE ]  [ INIT ]   [ 1 2 3 4 5 6 ]   | (Top-Bar 50px)
+---------------------------------------------------------------------------------------------------------+
| [ OSC ]  [ FILTER / VCA ]  [ ENV ]  [ LFO / ARP ]  [ AFX ]  [ MOD MATRIX ]  [ SETTINGS ]               | (Tab-Bar 28px)
+---------------------------------------------------------------------------------------------------------+
|                                                                                                         |
|   TAB-INHALT (5px Grid, 0px Radius, 55px Knobs, 5px Card Gap)                                          |
|                                                                                                         |
+---------------------------------------------------------------------------------------------------------+
```

1. **`OSC` (Quellen)**:
   - Oben: Engine (`DUAL WAVETABLE`, `ELEMENTS MODAL`, `HYBRID`) und der Wertebalken `NOISE`.
   - `OSC A` / `OSC B`: Regler oben, WaveMod-Typen (Symbole) unten, `HARD SYNC TO OSC A` als Schalter; im Elements-Modus sind die dort wirkungslosen Regler abgedunkelt.
   - Darunter die Waveform-Editoren (Badges `OSC A` / `OSC B`) bzw. die Elements-Karte.
2. **`FILTER / VCA`**:
   - Reihe 1: `FILTER` (Familie oben, Regler, Filtertyp bzw. EQ-Band unten) und der Frequenzgang.
   - Reihe 2: `TUNING & VOICES` (Master Tune, Glide, Quantize, Unison, Stimmen, Priorität), `OUTPUT & BUS` (VCA-Pegel, ConsoleX Drive/Air, Mackity Drive) und das Voice-Mischpult.
3. **`ENV`**: `FILTER ENV`, `AMP ENV`, `WAVEMOD ENV` mit je 5 Reglern, ADSR-Kurve und darunter Kurventyp (2 x 2 Symbol-Buttons) und `LOOP ENVELOPE`.
4. **`LFO / ARP`**: `LFO 1`, `LFO 2` (Form oben, Geschwindigkeit, Bereich/Trigger, Ziele, `PITCH TO` unten; `START DELAY` im LFO, den es betrifft) und `ARPEGGIATOR` (Modus oben, Gate/Swing/Tempo, Oktaven und Rate, Latch/Sync unten); darunter die Previews.
5. **`AFX`**: Kit mit 16 Pads, gewähltes Pad (Preset-Liste, Pegel, Tasten), Klaviatur.
6. **`MOD MATRIX`**: Performance-Controller, Übersicht der 8 Slots, Editor (Quelle, Via, Ziel als Button-Raster, Tiefe als Balken), Schnellzuweisungen.
7. **`SETTINGS`** (scrollt): `MIDI & MPE`, `SPLIT / LAYER`, `SKIN & PALETTE` (Paletten, Rollenfarbe, Schrift, Schriftgröße, Fenstergröße), `EDITOR BEHAVIOUR`, `DEVELOPER & DEBUG`.

---

## 7. Entwickler- & Support-Werkzeuge (Debug Mode)

Aktivierbar über den Schalter **DEBUG MODE (SHOW COMPONENT IDS)** im Tab `SETTINGS`:
1. **Sofortiger Weißer Hover-Rahmen (`ModernDebugHighlightOverlay`)**:
   - Beim Überfahren eines beliebigen Elements wird sofort ein scharfer `2.0px` weißer Rahmen mit Eckmarkern eingeblendet.
   - Klicks werden transparent durchgereicht (`setInterceptsMouseClicks(false, false)`).
2. **High-Contrast Tooltip-Fenster (`ModernDebugTooltipWindow`)**:
   - `15.0f Bold` Schriftgröße in D-DIN, tiefschwarzer Hintergrund (`#121316`), `1.5px` Akzent-Rahmen.
   - Zeigt die exakte C++-Komponenten-ID (z. B. `oscAVolKnob`, `filterCard`, `modernPresetBar`).
3. **Automatischer Clipboard-Kopierer**:
   - Nach **2 Sekunden Verweilen** auf einem Element (oder sofort per `Ctrl+C`) wird die ID in die Zwischenablage kopiert und ein Bestätigungs-Toast eingeblendet.

---

## 8. Zustandsmatrix aller interaktiven Komponenten

| Komponente | DEFAULT | HOVER | ACTIVE / PRESSED | FOCUS | DISABLED |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Drehregler (55px)** | Track + Accent-Bogen | +10% Helligkeit + Glow | Nadel folgt Maus | 1px Accent-Ring | 45% Opacity |
| **Linear Slider** | Accent-Thumb | +15% Thumb-Glow | Thumb gedrückt | 1px Accent-Rahmen | 45% Opacity |
| **Push / Action Button** | CardHeader Bg | +12% Aufhellung | AccentDark + 2px Bar | 1px Accent-Rahmen | 45% Opacity |
| **LED Toggle** | Off/On LED-Status | Gehäuse heller | Status wechselt | 1px Gehäusering | 45% Opacity |
| **Auswahl-Button** | ButtonBg + 1px Border | +12% Aufhellung | gewählt: AccentDark | 1px Accent-Rahmen | 45% Opacity |
| **TextEditor** | CardBg + 1px Border | Rand in textMuted | Text markiert / Cursor | 1px Accent-Rahmen | 45% Opacity |

FOCUS gilt nur bei Tastaturnavigation: Tab zeigt den Rahmen, der nächste Mausklick blendet ihn aus. Ein angeklicktes Element behält seinen normalen Rahmen.

---

## 9. Performance & Qualitätssicherung

- **Zero-Allocation**: Keine dynamischen Speicherallokationen im Audio-Thread oder in `paint()`-Methoden.
- **60 FPS Begrenzung**: Bildwiederholrate für alle animierten Visualizer (LFO, Arp, Voice Meters) ist auf 60 Hz getaktet.
- **Automatisierte Testsuiten**:
  - `FilterScenarioTest.exe`: **477 / 477 Tests bestanden (100 %)**.
  - `ArpScenarioTest.exe`: **76 / 76 Tests bestanden (100 %)**.
  - `ModMatrixScenarioTest.exe` & `AdvancedMidiScenarioTest.exe`: Vollständig verifiziert.

---

## 10. Code-Mapping-Übersicht

| Spezifikationsbereich | C++ Quellcodedatei | Wichtigste Klassen & Methoden |
| :--- | :--- | :--- |
| **Design-Tokens & Theme** | `vst/Source/ui/theme/ModernTheme.h` | `namespace ComponentTokens`, `struct ModernTheme`, `namespace KnobSizes` |
| **Typografie & Schriftarten** | `vst/Source/ui/theme/ModernFontManager.h` | `class ModernFontManager` (D-DIN TTF Loader & Cache) |
| **LookAndFeel & Drawing** | `vst/Source/ui/ModernEditorView.cpp` | `ModernLookAndFeel` (`drawRotarySlider`, `drawLinearSlider`, `drawToggleButton`...) |
| **Section Cards & Divider** | `vst/Source/ui/ModernEditorView.h/.cpp` | `ModernSectionCard` (`addDivider`, `addVerticalDivider`, `setAccentBar`) |
| **Preset Bar & Browser Drawer** | `vst/Source/ui/ModernPresetManager.h/.cpp` | `ModernPresetBar`, `ModernPresetDisplayButton`, `ModernPresetBrowserOverlay`, `ModernSaveAsModal` |
| **Interaktive Visualizer** | `vst/Source/ui/ModernEditorView.h/.cpp` | `WaveformEditorComponent`, `FilterCurveComponent`, `AdsrCurveComponent`, `ArpVisualizerComponent` |
| **Farbwähler Modal** | `vst/Source/ui/ModernEditorView.h/.cpp` | `ModernColorPickerModal`, `ColorSwatchButton`, `PaletteSwatchStrip` |
| **Debug Inspector & Overlay** | `vst/Source/ui/ModernEditorView.h/.cpp` | `ModernDebugTooltipWindow`, `ModernDebugHighlightOverlay`, `setupComponentIDs()` |
| **Top-Bar & Main Editor** | `vst/Source/ui/PluginEditor.h/.cpp` | `OvercyclerAudioProcessorEditor`, `brandLogoLabel`, `versionBadgeLabel` |
| **Modern Skin Designer (Tool)** | `vst/Source/ui/designer/ModernSkinDesignerMain.cpp` | Eigenständiges Standalone-Tool zur interaktiven Theme-Entwicklung |
