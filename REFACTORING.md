# Audio refactoring

## Decisions

- Establish a freshly compiled, isolated baseline before changing the sound.
- Keep persisted parameter IDs and numeric enum values stable. Deprecated names
  can remain aliases; migration must precede removal.
- Treat the current ConsoleXProcessor as a custom console inspired by Airwindows,
  not an exact implementation of upstream Console X.
- Calibrate oscillator/filter/headroom levels from measurements. Neither a fixed
  0.28 voice gain nor a fixed 4x filter gain is an accepted clipping guarantee.
- Separate file-format compatibility from preservation of the legacy sound.
- No file access, dynamic allocation or blocking locks in the eventual audio
  callback. Replacing locks requires an explicit state ownership/queue design.

## First stage: reproducible build and tests

- Repair the missing MackityProcessor include required by SynthEngine.
- Compile common scenario DSP into one static library.
- Register all nine scenarios with CTest, including assertions in Release.
- Resolve test fixtures from this checkout or an explicit command-line path;
  fail on missing fixtures instead of using another checkout.
- Give each test a build-local output directory.
- Seed storage tests into an isolated directory, never the user's folders.
- Run the complete CTest suite in CI; use Bash for CMake's multiline configure.

Windows example (choose an installed generator):

```powershell
cmake -S . -B build-analysis -G "Visual Studio 18 2026" -A x64
cmake --build build-analysis --config Release --parallel 4
ctest --test-dir build-analysis -C Release --output-on-failure
```

Use `-DBUILD_TESTING=OFF` for a product-only build. Existing `build/` artifacts
copied from another checkout are not a validation baseline.

## Completed implementation and validation

The six work packages are implemented in this checkout. The current results,
compatibility decisions, usage, measurements and remaining host limitations are
recorded in [AUDIO_REFACTORING_REPORT.md](AUDIO_REFACTORING_REPORT.md).

Windows Release builds the VST3, Standalone and Skin Designer. All eleven CTest
scenarios pass. The 424-case reference matrix contains no non-finite samples;
repeated smoke renders produce identical SHA256 hashes.

The engine allocation test does not establish allocation freedom of the host's
JUCE MIDI output buffer. File-backed program changes are asynchronous. Legacy
Clean/Mackity choices and persisted IDs remain for compatibility. The local
console is not upstream Airwindows Console X. DAW and other-platform validation
remain outside the locally verified result.

## Modern skin UI refactoring

`ModernEditorView` is reduced to the editor shell: tab bar, theme and
typography, debug inspector and the modal dialogs. Every tab in
`vst/Source/ui/tabs/` derives from `ModernTabModule`, owns its controls and
implements `setup()`, `updateFromEngine()` and `resized()`:

| Module | Owns |
|---|---|
| `OscillatorTab` | oscillators A/B and their wave editors |
| `FilterVcaTab` | filter model/mode/envelope, response curve with Shelves EQ, amplifier, master console, mixer & tuning, voice meter |
| `EnvelopeTab` | envelopes and their curve displays |
| `LfoArpTab` | LFOs, arpeggiator and pattern sequencer |
| `ModMatrixTab` | 8-slot modulation matrix, bender/modwheel/pressure |
| `AfxTab` | voice allocation, AFX kit, routing, setup files |
| `SettingsTab` | MPE, skin & palette, typography, window scale, `skin_config.conf` / `user_palettes.conf`, debug inspector switch |

- `ModernTabContext` provides the shared services. Parameter writes go
  through the editor, so processor and APVTS observe them; `refreshFromEngine`
  resynchronises all tabs after a setup file is loaded.
- `SettingsTab::Host` is the explicit interface for editor-level services
  (theme, fonts, window scale, skin switch, debug overlay, dialogs).
- Removed: `ParallelTab` with its legacy/module surfaces, the parallel
  `ModernSkinModule` state, the adopt/state-mirror/fallback callbacks of the
  former filter tab and the transitional prototype hooks of the context.
- Safety net: `ModernSkinScenarioTest` records layout (`layout.txt`) and
  control bindings (`bindings.txt`) of every tab for the scenarios `default`,
  `ripples` and `vintage-6db`. `--pixels <dir>` compares against local pixel
  snapshots, which are not committed; `--update` rewrites the fixtures. The test
  redirects the configuration directory via
  `OverviberPaths::setAppConfigDirectoryOverride` and never touches the user's
  files. The only fixture changes of the migration are the removed transitional
  container surfaces in the OSC and FILTER/VCA summaries.

## Elements envelope on heap storage

`FactoryPresetHeadroomScenarioTest` crashed intermittently with an access
violation under CTest in an MI Elements hybrid preset (formerly preset 0067),
while direct runs passed. AddressSanitizer located it deterministically in
`elements::MultistageEnvelope::Process`: a finished envelope reads
`shape_[num_segments_]` and `level_[num_segments_ + 1]`, which `set_adsr()`
never writes. The firmware keeps its voices in zero-initialised static storage;
Overviber allocates them on the heap. The segment arrays are now
zero-initialised, and TEST 8 of `ElementsVoiceScenarioTest` covers
uninitialised storage.

Windows Release builds the VST3, Standalone and Skin Designer; all 16 CTest
scenarios pass.

## Audio engine refactoring (2026-09)

`SynthEngine` (~1500 lines) combines MIDI/MPE input, part routing and voice
allocation, modulation, parameter translation, clock/arpeggiator, the master
bus and preset/file access. The refactoring splits these responsibilities
without changing the sound, except in the step that is explicitly marked as
changing the sound.

### Decision: every voice uses the parameters of its own part

A voice takes all sound parameters from the part it plays. Today a voice of
parts 2–16 takes these values from part 1 (the main preset):

| Parameter | Location today |
|---|---|
| Oscillator A/B base pitch, cutoff and keyboard tracking at note-on | `SynthEngine::assignerEvent` |
| Master tune, amp level, unison detune | `SynthEngine::updateSingleVoice` |
| LFO 1/2 (shape, speed, rate, amount, retrigger) | two global `LfoModule`s, set by `applyControls` |
| Glide time | global `glideAmount` |
| Live edits of cutoff and envelopes | reach only voices that follow the main part |

These stay engine-wide because they describe the instrument, not a sound:
voice pool (count, priority, unison pattern), MPE zone, arpeggiator and clock,
and the master bus (Console, Mackity, output ceiling, preset crossfade).
LFOs become per part (16 × 2 instances), so voices of one part keep a shared
phase. This changes the sound of multi-part setups and gets its own new
reference. No legacy mode is kept (see the rules).

### Rules

- Every step except "Per-part parameters" stays bit-exact against a local
  baseline created before that step (`AudioReferenceCompare`).
- Overviber has no release and no production use yet. Parameter IDs, enum
  values and file formats may change without migration or legacy modes.
- No allocation, file access or blocking locks in `renderBlock`.

### Phase 0: safety net (done)

`AudioReferenceRender` has three modes:

```powershell
# Before a step: baseline of the current engine (the directory must not exist)
build-check\Release\AudioReferenceRender.exe --out build-check\audio-baseline
# After a step: bit-exact comparison (also run by CTest as AudioReferenceCompare)
build-check\Release\AudioReferenceRender.exe --compare build-check\audio-baseline
# Render time per oscillator engine and filter
build-check\Release\AudioReferenceRender.exe --bench
```

- A baseline contains one float32 WAV per case, `hashes.txt` (FNV-1a over the
  sample bits), `metrics.csv` and `manifest.txt`. A comparison lists each
  differing case with its largest deviation and first differing frame.
- CTest registers `AudioReferenceCompare` against `<build>/audio-baseline`
  (cache variable `OVERVIBER_AUDIO_BASELINE`). Hashes depend on compiler and
  platform, so no baseline is committed. The test exits with the skip code
  while no baseline exists.
- 401 cases: all factory presets (1 and 6 voices), the Elements matrix and 13
  new scenarios for paths the matrix did not cover. The scenarios are custom
  routing with three parts plus main-part edits, the AFX kit, mono legato with
  two glide times, parameter automation at unaligned positions (block sizes 64
  and 37, including a walk through all filter models and Shelves bands), LFOs
  with all eight matrix slots and controllers, MPE, the internal-clock arp,
  hybrid oscillators, unison, the fully engaged master bus, and a
  prepared state that arrives while notes sound.
- Checked: a baseline compared with itself gives 401 identical cases. A gain
  change of 0.002 % is detected in all 401 cases. A full comparison takes
  about 18 s.

Baseline benchmark (Windows, MSVC Release, six voices, 44.1 kHz, block 512,
10 s of audio; best of three; test build with diagnostics compiled in):

| Oscillator | SSI2144 | Liquid | Shelves | SST |
|---|---|---|---|---|
| Wavetable | 237 ms (42×) | 1490 ms (6.7×) | 1315 ms (7.6×) | 355 ms (28×) |
| Elements | 601 ms (17×) | 1802 ms (5.5×) | 1628 ms (6.1×) | 748 ms (13×) |
| Hybrid | 501 ms (20×) | 1766 ms (5.7×) | 1593 ms (6.3×) | 635 ms (16×) |

### Phase 1: central parameter translation (done, bit-exact)

- New file `dsp/VoiceConfig.h`: functions that apply a part preset to a voice.
  It covers envelope times, shape, speed and velocity (table-driven through
  `EnvelopeParams`), the filter model and the Shelves bands.
  `configureVoice()` fixes the order: filter model before the Shelves bands.
- `applyControls`, `configureVoicePart`, `setContinuousParam` and
  `setSteppedParam` use these functions. Before, the Shelves conversion
  existed four times and the envelope setup three times.
- `followsMainPart()`, `forEachMainPartVoice()` and `voicePreset()` replace
  the scattered `voiceSlot[v] > 0` / `>= 0` checks.
- `assignerEvent` no longer sets the velocity from the main preset. That
  value was always overwritten by the part's velocity in
  `configureVoicePart`, so the code had no effect.
- Result: `SynthEngine.cpp` is 180 lines shorter (`VoiceConfig.h` adds 90). All 17 CTest tests
  pass, and `AudioReferenceCompare` finds 401 of 401 cases identical.

### Step 1: modulation extracted (done, bit-exact)

- New files `dsp/Modulation.h/.cpp`. `ModulationInputs` collects everything
  the control-rate computation of a voice reads: part and main preset, LFOs,
  envelope outputs, per-note expression, controllers, note CVs and the gain
  mode. `modulation::computeVoiceControls()` returns a `VoiceControls`
  struct, and `modulation::apply()` hands it to the voice.
- The matrix sums into `std::array<float, modDestCount>` instead of 21 local
  variables and a `switch`. The computation is split into
  `resonanceAndMixer`, `pitch`, `cutoff`, `amp`, `waveMod` and `elements`,
  with the arithmetic, types and order of operations unchanged.
- `modulation::source()` replaces the body of `evaluateModSource`, which
  remains as a wrapper. `VoiceExpressionState` moved to `Modulation.h`.
- `updateSingleVoice` shrinks from 275 to 14 lines. `SynthEngine.cpp` now has
  1014 lines (1496 before phase 1).
- Removed 11 matrix destinations the engine never implemented: Shelves
  gain/frequency, LFO 1/2 speed/depth, attack/decay/release, arp gate and
  swing. The enum is renumbered (`modDestCount` 22). `ModMatrixScenarioTest`
  and `RefactoringScenarioTest` use existing destinations instead. The Modern
  skin fixtures change only in the eight destination lists (33 → 22 entries).
- All 17 CTest tests pass, and `AudioReferenceCompare` finds 401 of 401 cases
  identical. Render times are within ±2 % of the baseline.

### Compatibility leftovers removed

- **Legacy gain staging:** the `calibratedGain` flag is gone from `Voice`,
  `Modulation`, `SynthEngine`, `PreparedState` and the `.ovm` setup. The
  resonance-dependent mixer pre-gain and the original ConsoleX encoder curve
  are removed. Unison compensation, the filter input pad with measured
  correction and the output ceiling now always apply. The "CALIBRATED GAIN"
  toggle in the AFX tab is removed; the setup buttons move up into its place.
- **`.conf` as setup:** `SessionState::decode` accepts only versioned JSON
  setups, and "Load Setup" offers only `*.ovm`. Single presets stay `.conf`;
  the preset browser is unchanged.
- **Kept on purpose:** the factory presets use the GliGli Overcycler firmware
  format. Their key aliases (`spSync`, `spABaseWMod`) and the conversion of
  modwheel/pressure/timbre targets into matrix slots are needed to load them.
- Tests: `MidiClickScenarioTest` checks only the remaining gain staging. The
  reference scenario `scenario_legacy_gain_bus` became `scenario_master_bus`
  (same bus settings, standard gain). Against the step 1 baseline, all other
  400 cases stayed bit-exact, because the removed branches were only active in
  legacy mode. The baseline was then recreated. All 17 CTest tests pass.

### Step 2: voice management extracted (done, bit-exact)

- `dsp/MidiInput.h/.cpp`: channel-wide controllers (bend, modwheel,
  pressure, timbre, breath, expression) and the per-note expression of each
  voice, including the bend range conversion and control-rate smoothing. The
  engine's MIDI handlers only decide between channel-wide and MPE member
  messages and which voices a per-note message addresses.
- `dsp/VoiceAllocator.h/.cpp`: part routes and custom routing, the part of
  each voice, note and target CVs, glide and the filter CV slew. `assign()`
  replaces the three copies of the routing loop (note-on, arp output, and the
  plain assigner call). `partForNewVoice()` holds the part choice by engine
  mode.
- `SynthEngine.cpp`: 852 lines (1014 after step 1, 1496 at the start).
- The assigner and arp keep `std::function` callbacks. The engine's lambdas
  capture only `this`, so they fit the small-buffer storage and do not
  allocate. They run once per note event, not per sample. Replacing them
  would require rewriting the more than 20 capturing lambdas in
  `ArpScenarioTest` and `RefactoringScenarioTest` for no measurable gain.
- All 17 CTest tests pass, and `AudioReferenceCompare` finds 401 of 401 cases
  identical.

### Next steps

1. Done, see step 1 above.
2. Done, see step 2 below.
3. **Per-part parameters:** implement the decision above, after steps 1 and 2
   have extracted the modulation and voice code. `ModulationInputs::main`
   then disappears. New reference, and the differences are documented.
   Changes the sound.
4. **Master bus:** `MasterBus` with named constants (bus headroom 0.45,
   filter input pad 0.25/×4, noise 0.35, ceiling 0.9/0.08). Pan and unison
   gain are computed per block. Bit-exact.
5. **Block rendering:** segments between CV/tick boundaries, each voice
   renders into its own buffer, summed in the same voice order.
   `ConsoleX::encodeVoice` is stateless, so the sum stays bit-exact.
6. **Separate model and engine:** `SynthModel` (presets, waves, file I/O,
   creating the `PreparedState`) for the editor. The audio engine keeps no
   `PresetManager`.

### Analysis of monolithic code blocks

Function lengths (excluding third-party code in `dsp/audible`), with the
proposed treatment:

| Lines | Function | Problem | Proposal |
|---|---|---|---|
| 275 | `SynthEngine::updateSingleVoice` | Modulation, pitch, filter, amp, WaveMod and Elements in one function at 4 kHz × 6 voices | Done in step 1 (14 lines, `Modulation.cpp`) |
| 157 | `SynthEngine::setContinuousParam` | `switch` with per-group side effects | After phase 1, a table of parameter group → apply function; per-part routing in step 3 |
| 124 | `SynthEngine::renderBlock` | Clock, CV rate, voices, console, Mackity, ceiling and crossfade in the sample loop; `getVoicePan()` checks the voice pattern per sample and voice | Steps 4 and 5 |
| 120 | `SynthEngine::setSteppedParam` | Same as above | Same as above |
| 79 | `SynthEngine::assignerEvent` | Note CVs from the main preset, part choice, voice configuration | Step 2, then step 3 |
| 155 | `Arpeggiator::clockTick` | Mode logic of all arp modes in one function | Strategy per mode; `ArpScenarioTest` as a safety net |
| 113 | `VoiceAssigner::assignNote` | Priority, unison, legato and stealing mixed together | Split into `findVoice`/`stealVoice`/`assignUnison` |
| 129 | `OvercyclerAudioProcessor::processBlock` | State takeover, host transport, MIDI dispatch, MIDI output and metering | `dispatchMidi()`, `publishTelemetry()` |
| 404 | `createParameterLayout` | Hand-written list of all parameters | Generate from the parameter tables in `PresetManager` |
| 245–284 | `SettingsTab/FilterVcaTab/LfoArpTab::setup` | UI construction per control | Declarative control tables (outside the audio work) |
| 363 | `FilterCurveComponent::paint` | Response calculation and drawing mixed together | Compute the response separately and cache it (UI) |

Further findings:

- **CPU:** Liquid (Ripples) and Shelves cost about 6× the SSI2144 (see the
  table). Both run the analog model oversampled with `exp2f` per sub-step,
  each voice separately. The most promising gain is processing all six voices
  together in one SIMD pass (`simd::float_4` is already in use). This needs a
  voice-wide filter interface and comes after step 5. Reducing the
  oversampling would change the sound and is not planned.
- **Modulation matrix:** 11 of 32 targets were offered in the UI but ignored
  by the engine. Removed in step 1.
- **State takeover:** a `PreparedState` holds the waves of all 16 parts
  (16 × 4 × 2400 samples ≈ 300 KB). The editor copies and compares it on
  every timer tick. The audio thread compares all waves on every takeover.
  Change counters per part and per wave would reduce this to the changed
  parts.
- `Voice::isActive()` is evaluated twice per sample and voice (in
  `renderBlock` and `processSample`). Step 5 checks it once per segment.
