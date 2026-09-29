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

A voice takes all sound parameters from the part it plays. Implemented in
step 3. Before, a voice of parts 2–16 took these values from part 1 (the main
preset):

| Parameter | Location before step 3 |
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

### Step 3: every voice uses its own part (done, changes the sound)

- Note CVs: base pitch of both oscillators, cutoff and keyboard tracking come
  from the part preset. `assignerEvent` now decides the part before it
  computes the note CVs.
- Modulation: master tune, amp level and unison detune come from the part.
  `ModulationInputs::main` is removed.
- LFOs: two per part (`partLfos[16][2]`), configured from the part preset.
  A part's LFOs start running with its first note and then run freely;
  part 1 always runs. Retrigger resets the LFOs of the voice's part. The
  editor shows the LFOs of part 1 (`getLfo`).
- Glide: per voice, from the voice's part at note-on. Edits of part 1 reach
  the voices that follow part 1.
- Edits of other parts arrive as prepared state: changed parts reconfigure
  their voices (as before), and a changed cutoff now also retargets the
  filter CV of their sounding voices, like a live edit of part 1.
- Engine-wide on purpose: voice pool (count, priority, unison pattern), MPE
  zone and bend ranges, engine mode, arpeggiator and clock, master bus.
- Verification: against the step 2 baseline, 398 of 401 cases stay
  bit-exact. Only `scenario_multipart_routing`, `scenario_afx_kit` and
  `scenario_prepared_state` change, all finite with similar levels (output
  peak 0.35 → 0.39, 0.54 → 0.51, 0.33 → 0.33). `RefactoringScenarioTest`
  checks that a part 2 voice takes its pitch from part 2 and that part 2's
  LFO runs while part 1's is stopped. The baseline was recreated; all 17
  CTest tests pass; single-part render times are unchanged within noise.

### Step 4: master bus extracted (done, bit-exact)

- New `dsp/MasterBus.h` (header-only, called per sample): console encode per
  voice and bus sum, console decode, bus headroom, Mackity parallel send with
  smoothing, output ceiling and the crossfade after a preset change. It owns
  the ConsoleX and Mackity processors, the send state and the last output.
- Named constants instead of magic numbers: `MasterBus::kBusHeadroom` (0.45),
  `kMackityReturnGain`/`kMackityReturnPadGain`, `kSendSmoothingSeconds`,
  `kCeilingThreshold`/`kCeilingRange` (0.9/0.08), `kPresetCrossfadeSeconds`;
  `Voice::kFilterInputPad`/`kFilterMakeup` (0.25/4); `kNoiseMixGain` (0.35)
  in `Modulation.cpp`. The send smoothing coefficient is computed in
  `prepare()` instead of per block.
- `renderBlock` computes pan gains and the unison compensation once per
  block (note events split blocks, so both are constant within a block) and
  shrinks to the clock, the voice loop and `bus.process()`.
- The outdated signal-flow diagram in `SynthEngine.h` is replaced by an
  overview of the stages and the classes that own them.
- `SynthEngine.cpp`: 784 lines. 401 of 401 cases bit-exact, 17/17 CTest
  tests pass. Render time unchanged: two runs without code changes differ by
  up to ±10 %, and the filters dominate the cost.

### Liquid filter noise per instance (done, new baseline)

`ripples.hpp` adds noise at 1e-6 to the filter input to start
self-oscillation. It drew from a single `thread_local` generator in
`RackSimd.h`, shared by every Liquid filter of every engine on the thread.
The noise of one voice therefore depended on everything rendered before it on
that thread (inaudible). For the reference renders, Liquid cases were only
bit-identical in the same order: `scenario_multipart_routing` rendered alone
(`--smoke`) differed by about 3e-8 from the full run.

- `random::uniform(uint32_t& state)` takes the caller's state. Each
  `RipplesEngine` owns one and restarts it in `setSampleRate()`, which is
  also its reset.
- Effect against the step 4 baseline: only Liquid cases change (72 Elements
  matrix cases and five scenarios; no factory preset uses Liquid). Without
  and with medium resonance, the largest deviation is 6.5e-6. At full
  resonance the filter self-oscillates and the noise only sets the start
  phase: samples differ by up to 0.23, output RMS stays within 0.9998–1.03 of
  the previous render, and all samples are finite.
- A smoke run now matches the full run bit-exactly (7 of 7 cases). The
  baseline was recreated; all 17 CTest tests pass.

### Step 5a: Elements randomness per voice and uninitialised state (done)

Prerequisite for block rendering: rendering voice by voice changes the order
in which voices call into Elements. Elements draws from `stmlib::Random`, one
`thread_local` generator for all voices and instances, so the order changed
the result.

- `ElementsOsc` keeps its own generator state and swaps it into
  `stmlib::Random` for its own calls (`reset()`, `renderBlock()`), then
  restores the previous state. `Voice::init()` gives each voice its own seed,
  so the voices of a chord do not share one noise sequence.
- New reference scenario `scenario_elements_noise`: Elements string with bow
  and blow (these exciters draw random numbers), three voices starting at
  offsets 0/5/11 so that their internal 16-sample blocks are shifted.
- Found while checking it: this scenario was not deterministic, repeated
  runs of the same binary differed from frame 67 on. `Exciter::Init()` never
  set `phase_` (read position of the granular sample player used by blow)
  and `particle_range_`; `String::Init()` left `src_phase_` unset below
  11.7 Hz. The firmware keeps these in zeroed static storage, Overviber
  allocates voices on the heap. All three are now initialised. With the
  oscillator placed in zeroed memory the scenario was already stable, which
  confirmed the cause before the fix.
- `ElementsVoiceScenarioTest` TEST 9 builds an oscillator with bow and blow
  on `0x00` and on `0xFF` bytes and requires bit-identical output. It fails
  without the fix and passes with it.
- The existing 401 cases stay bit-exact (their Elements exciters draw no
  random numbers). Baseline recreated with 402 cases; four repeated runs are
  identical. All 17 CTest tests pass.

### Step 5b: segment-wise voice rendering (done, bit-exact)

- `renderBlock` splits the host block into segments at the samples on which
  a CV update (~4 kHz) or a clock tick fires, at most `kMaxSegment` = 64
  samples. The counters advance with the same float additions per sample as
  before; events fire only at a segment's first sample, where
  `currentSampleOffset` points at that sample.
- Each voice renders the segment into its own buffer with
  `Voice::process()`, which stops at the sample on which the voice becomes
  inactive. Within a segment it cannot restart, because only note and clock
  events start voices. The sum then runs per sample in the original voice
  order through `MasterBus`, so the result is bit-exact.
- Checked: 402 of 402 reference cases identical; 17/17 CTest tests pass.
  With the Elements generator swap from step 5a disabled,
  `scenario_elements_noise` differs (0.84), so 5a was necessary for this
  step. Render time in two runs: Wavetable with SSI2144 227–231 ms (237–240
  before), with SST 342–348 ms (355–366); Liquid and Shelves unchanged within
  noise.
- Prepared for later: filters can now get block variants
  (`process(float*, int)`), which the SIMD processing of Liquid and Shelves
  across voices needs.

### Step 6: editor model and audio engine separated (done, bit-exact)

**Model and engine**

- New `data/SynthModel.h/.cpp`: the editor's model. It owns the 16 parts
  (`AfxKit` with presets and `WaveManager`s), the `PresetManager`, routing,
  voice mixer and the arpeggiator sequence, loads preset and wave files and
  creates the `PreparedState`. It also holds the display data the processor
  reports from the audio engine (voice levels, arp position). It renders no
  audio.
- `SynthEngine` no longer has `PresetManager`, `WaveManager`, `AfxKit`,
  `loadPreset()` or `capturePreparedState()`. It keeps its 16 parts as plain
  preset data and wave arrays plus the note map and receives them only with
  `applyPreparedState()`. It has no file access.
- Plain data shared by both moved into their own files:
  `data/PresetData.h/.cpp` (out of `PresetManager.h`),
  `data/DefaultKit.h` (default AFX kit, out of `AfxKit`) and
  `dsp/DefaultWaves.h` (built-in waves, out of `WaveManager`), so a new
  engine starts with the same parts and waves as before.
- A panic generation change (preset load in the editor) now also clears the
  note CVs in the audio engine, as the former editor-side `loadPreset()` did.
- Processor: `SynthModel model` replaces the editor `SynthEngine`;
  `getEngine()` became `getModel()`. The UI (25 files) works on
  `SynthModel`; the identifier `engine` was renamed to `model` in code only,
  so comments, strings and the Modern skin fixtures stay unchanged.
  `refreshOscWaves()` calls in the UI are gone: edited waves reach the engine
  with the next prepared state.
- Tests use `Tests/TestSynth.h`, an engine plus its model: `prepare()` and
  `loadPreset()` read files through the model as the former engine did;
  `syncParts()` hands edited parts to the engine. `ModernSkinScenarioTest`
  works on a plain `SynthModel`; the arp-release check in
  `PluginTimingScenarioTest` now tests the engine directly.

**Cheaper state takeover**

- `WaveManager` stamps every change of its wave data (also through
  `getMutableWaveData()`) with a revision that is unique across all
  instances. `PreparedPart::waveRevision` carries it.
- The model copies waves into a reused state only for new revisions;
  `sameIgnoringWaveData()` compares states by revision instead of 310 KB of
  samples; the engine copies waves only for new revisions. Measured: one idle
  editor tick takes 0.44 µs instead of 11 µs.
- The saved session (JSON with embedded waves, 470 KB, 8 ms to encode) was
  encoded on every change, up to 30 times per second while a control moves.
  It is now encoded 200 ms after the last change, or immediately when the
  host saves on the message thread. A host saving from another thread within
  200 ms of an edit gets the previous session plus the current host
  parameters.

**Checks:** 402 of 402 reference cases bit-exact; 17/17 CTest tests pass,
including unchanged Modern skin fixtures. New checks in
`RefactoringScenarioTest`: unchanged waves are not copied again, edited
waves are copied, the engine takes them over.

### Liquid and Shelves filter cost (done, bit-exact)

A micro benchmark (six voices, 44.1 kHz, 10 s, filters called directly)
split the cost before changing anything:

| Part | Time |
|---|---|
| Liquid total | 1284 ms |
| of which anti-aliasing up/down filters (3× oversampling, 7 + 7 sections) | 715 ms |
| of which filter core (RK2 steps) | 466 ms |
| of which `exp2f` | 19 ms |
| Shelves total | 1170 ms |

So the largest share was not the analog model but the cascaded biquads
(`audible::SOSFilter`, used by both filters). Changes:

- `SOSFilter` keeps its coefficients as `float_4` vectors instead of
  broadcasting a `float` for each of the five multiplications per section,
  and runs the cascade with a compile-time section count (switch over 1–8),
  which the compiler unrolls. Arithmetic and order are unchanged: 212 →
  128 ms in an isolated test with bit-identical output.
- Shelves computed `FreqVCALevel` and `QVCALevel` (four `std::pow` each) for
  every sample. Like the existing gain cache, both are now recomputed only
  when their inputs change; the inputs are constant once the CV smoothing
  has settled. Lane 0 of the Q vector carries the audio input; its level
  only reaches lane 0 of the mid-band filter, which no output uses, so the
  Q cache is keyed on lanes 1–3.

Results: micro benchmark Liquid 1284 → 911 ms (anti-aliasing 715 → 338 ms),
Shelves 1170 → 598 ms. Complete engine, two runs each:

| Six voices, 10 s | Before | After |
|---|---|---|
| Wavetable + Liquid | 1490 ms | 1091 ms (−27 %) |
| Wavetable + Shelves | 1315 ms | 743–749 ms (−43 %) |
| Elements + Liquid | 1802 ms | 1350–1378 ms (−24 %) |
| Elements + Shelves | 1628 ms | 1043–1141 ms (−33 %) |

402 of 402 reference cases bit-exact; 17/17 CTest tests pass.

Not done: processing four voices per SIMD register. Ripples already
vectorises within a voice (its four signals share one `float_4`), so a
voice-wide layout would replace that vectorisation instead of adding to it,
and requires rewriting the filter model and grouping voices by filter model.
The remaining core cost of Liquid (466 ms) is the candidate if more is
needed; `exp2f` is not worth optimising.

### SEM filter replaces Liquid (changes the sound)

Filter model 1 is now "SEM", an Oberheim SEM-style 2-pole state-variable
filter (`dsp/SemFilter.h`). A dropdown in the filter card (`spSemModel`,
also a host parameter) selects the model; the former Liquid filter remains
available as one of them:

| Variant | Source | License | Character |
|---|---|---|---|
| OB-Xd 12 dB (default) | sst-filters `OBXDFilter.h`, from OB-Xd `Filter.h` | GPL-3.0 | zero-delay SVF, diode-pair resistance in the feedback |
| Oberheim | FAUST `ve.oberheim` (Tarr, after Pirkle) | MIT-style STK-4.3 | cubic soft clipper inside the loop, strong compression |
| Vult SVF | Vult examples `svf.vult`, `saturate_soft.vult` | MIT | trapezoidal SVF, `16*tanh(x/16)` output |
| Cytomic SVF | sst-filters `CytomicSVF.h` (Simper) | GPL-3.0 | linear trapezoidal SVF |
| Liquid | Mutable Instruments Ripples (unchanged) | GPL-3.0 | former Liquid filter, LP4/LP2/BP2 |

- The code is ported to per-voice scalar code with attribution in the
  header, as for the SST ladder; nothing was vendored. The Vult Stabile module
  itself is not open source; the Vult variant uses the published MIT SVF by
  the same author, whose notch is the SEM-style sum of lowpass and highpass.
- Modes: lowpass, bandpass, highpass, notch (Liquid keeps its three modes;
  the mode buttons follow the selected variant). OB-Xd and Oberheim run at 2x
  the sample rate like the SSI2144 and SST filters. Each variant has its own
  small-signal gain calibration in `prepare()`.
- One resonance curve for all SVF variants, Q = 0.5 + 19.5 r^3: in all four
  models the small-signal damping is 1/Q. Measured at cutoff 1 kHz, the peak
  at resonance 0.8 is 17.8 (OB-Xd), 18.3 (Oberheim), 20.4 (Vult, Cytomic) dB.
  Like the SEM, none of them self-oscillates.
- Integration check: with the Liquid variant forced, all 402 reference cases
  were bit-identical to the former Liquid filter. With OB-Xd as default, only
  the Liquid cases change (72 Elements matrix cases and five scenarios, since
  the default AFX kit also uses this model); the baseline was recreated.
- Tests: `FilterScenarioTest` covers all variants and modes (1629 runs, no
  NaN/Inf, peaks bounded) and a new scenario 8 checks the response shape of
  every mode and the common resonance. The Modern skin fixtures show the SEM
  toggle, the variant dropdown and the four SEM modes.
- Cost: six voices, 10 s, wavetable: SEM (OB-Xd) 230 ms, the former Liquid
  1091 ms; in `FilterScenarioTest` the SVF variants run at 40-58x real time.

### AFX notes too short: envelope times (changes the sound)

Report: in AFX mode the notes play too short.

- `AfxScenarioTest` (new) holds every part of the default kit for 1 s in AFX
  mode and plays the same part through its MIDI channel in Multi-Channel
  mode. Both sounded exactly equally long, so the AFX routing was correct.
- The cause was the envelope times. They follow the hardware's strongly
  exponential curve (knob 500 = 31 ms, 700 = 196 ms, 800 = 0.5 s, 999 = 3 s;
  the slow range is 4x). The default kit and the Init sound were written as
  if the knob value were milliseconds: kit decay 140 was 1.1 ms, release 250
  was 3 ms, and the Init release "~200ms" was 2 ms. Percussive parts lasted
  4 ms, sustained parts had no audible release.
- The envelope knobs also showed wrong times ((value/999)^2 x 8000 ms: knob
  500 showed "2.00 s" for a 31 ms stage), so the confusion was visible in the
  UI as well.

Changes:

- `adsr.h`: `adsrStageMilliseconds()` and `adsrCVForMilliseconds()`
  convert between time CV and stage duration using the envelope's own table.
- The default kit and the Init sound use the intended milliseconds through
  `adsrCVForMilliseconds()`; sustain levels are unchanged. The Init release
  is now 203 ms.
- The envelope knobs (attack, decay, release of all three envelopes) show
  the real duration, including the x4 slow range, and accept typed times
  ("250 ms", "1.5 s"). Glide and LFO delay keep their previous display; their
  curves differ from the envelope's and were not checked here.
- Results, held 1 s: sustained parts sound 1.05-1.35 s, plucks about 285 ms,
  percussion about 95 ms (was 4-16 ms). `AfxScenarioTest` checks that AFX and
  Multi-Channel match and that every part is audible for at least 50 ms.
- Factory presets and the Elements matrix are unchanged (their times come
  from the preset files or are set explicitly); the reference scenarios based
  on the Init sound or the kit changed. Baseline recreated; 18/18 CTest tests
  pass.

### Glide and LFO start delay as in the firmware (changes the sound)

Checking the glide and LFO delay knob displays found two porting errors:

- Glide ran in the arpeggiator clock, whose rate follows the tempo (96 Hz at
  120 BPM) and which stops when the host transport is stopped with host sync.
  Glide was about 5x slower than the hardware, tempo dependent, and with a
  stopped DAW the pitch stayed on the previous note. The firmware runs glide
  on a fixed 500 Hz tick.
- `cpModDelay` (LFO start delay) was not read anywhere; the knobs had no
  effect. The firmware waits N ticks after the first key press and then fades
  the LFO in over N ticks along the attack curve.
- Both knobs showed (value/999)^2 x 8000 ms, which matches neither.

Changes:

- `dsp/ControlTimes.h`: the firmware formulas for glide and modulation
  delay, with their durations for the UI.
- The engine runs a 500 Hz control tick (every 8th CV update) for glide and
  the delay, independent of tempo and transport.
- Modulation delay per part on LFO 1: the part's first key press starts it;
  it restarts when the part was silent. Below the pot dead zone it is off and
  the LFO behaves as before.
- UI: glide shows the time for one octave ("192 ms/oct", "Off"), the LFO 1
  delay shows its waiting time ("974 ms", "Off"). The delay knob in the LFO 2
  card edited the same parameter and is removed; LFO 2's two remaining knobs
  share the row.

Times (knob 100 / 300 / 500 / 700 / 999): glide per octave 5 / 18 / 58 / 192 /
1229 ms; delay 36 / 108 / 326 / 974 / 5000 ms (then the same time to fade in).

Tests: `GlideDelayScenarioTest` (new) checks the octave glide time at 60 and
180 BPM and with a stopped transport (0.192 s each), and the delay (LFO 1 at 0
during the delay, fading in, full after twice the delay, full at once without
delay). Changed references: the two glide scenarios and 16 preset cases,
namely presets 8, 9, 13, 34, 36, 44 (LFO delay) and 12, 13, 32, 41, 47
(glide, six-voice variants). Baseline recreated; 19/19 CTest tests pass.

### Firmware parameter audit: WaveMod "Frequency" and filter tracking (changes the sound)

An audit of all Overcycler parameters against the firmware (synth.c) found
the parameter lists identical and these deviations in their use:

- WaveMod type "Frequency" had no effect. The firmware halves the base
  WaveMod for this type and adds the modulated WaveMod to the oscillator
  pitch. Both are now in `Modulation.cpp`; `waveMod()` runs before `pitch()`.
- Filter keyboard tracking was about half as strong: the tracking offset
  used 256 CV per semitone, but the filters span 20 Hz x 1300 (about 124
  semitones) over the 16-bit CV, i.e. about 528 per semitone
  (`FILTER_CV_SEMITONE`). The firmware tunes its filters per semitone, so
  full tracking follows the keys. The SST ladder (15 Hz x 1600) is within 3 %.

Open from the audit, since done (see "Performance parameters as in the
firmware" below): bender range
(firmware 4/7/12 semitones) and target, modwheel range and target meaning
(firmware: adds to LFO 1 or LFO 2 amount), pressure targets LFO 1/2 and
volume (firmware: mixer levels), `spChromaticPitch`, the firmware's resonance
compensation of the mixer levels (0.35x without to about 2x at full
resonance).

Tests: `FirmwareParamScenarioTest` (new) checks full tracking over an octave
(23 semitones after the firmware's truncation) and the "Frequency" halving
and pitch offset. 42 of 402 reference cases change, mostly the six-voice
preset renders. Baseline recreated (old copy as audio-baseline-glide);
20/20 CTest tests pass.

### Envelope speed as in the firmware (changes the sound)

The firmware sets the envelope speed shift to 2 (normal) and 4 (slow)
(`synth.c` refreshEnvSettings); Overviber used 0 and 2, so all envelopes
ran four times faster than the hardware. Phase tables and the 4 kHz update
rate are identical to the firmware. `adsrSpeedShift(slow)` in `adsr.h` now
holds the hardware values and is used by `VoiceConfig`, the stage duration
functions and the UI.

Times per stage (knob 100 / 300 / 500 / 700 / 999): normal 3 ms / 19 ms /
125 ms / 785 ms / 12.5 s, slow four times that. The factory presets come
from the hardware (Solidtrax, see the manual) and now play with their
intended times. The default kit and the Init sound are given in
milliseconds (`adsrCVForMilliseconds`) and keep their times; their knob
values move (Init release 200 ms: knob 704 -> 552).

Tests: `FirmwareParamScenarioTest` checks the attack at knob 500 (125 ms,
slow 500 ms). `RefactoringScenarioTest` waits for preset 43's slow release
(about 12 s) by its stage duration; `ArpScenarioTest` plays 1.1 s instead of
0.27 s so the slow-attack pad (preset 42, 2.5 s) becomes audible. Skin
fixtures updated. 398 of 402 reference cases change. Baseline recreated
(old copy as audio-baseline-tracking); 20/20 CTest tests pass.

### Resonance calibration per filter model (changes the sound)

The Overcycler manual says of FRes: "Self-oscillation can be heard in the
last third of amount". The firmware gets there by sending half the
resonance CV to its SSI2144 (`resVal>>1`), because the analog filter already
oscillates at half scale. That halving is specific to the hardware filter;
Overviber's software filters each had their own curve instead, with the
self-oscillation onset somewhere between 78 % and 99 % of the knob,
depending on the model and the cutoff.

Now every self-oscillating model starts to oscillate at two thirds of the
knob (`kFilterResonanceOnset` in `OvercyclerTypes.h`). The calibration sits
in each model's own resonance curve, not in an extra mapping in `Voice`:

- SSI2144 (ZDF ladder) and SST ladder: `ladderResonanceFeedback()` keeps
  the former shape (0.6 t + 0.4 t^3) up to the onset, where the feedback
  reaches 4.0, the oscillation limit of a 4-pole ladder, and then rises
  linearly to 5.0 at full scale so the self-oscillation still has range,
  as on the hardware. Before, the SSI2144 reached 4.0 only at the very end
  of the knob.
- Liquid (Ripples): self-oscillates at a fixed 78 % of its resonance CV,
  independent of the cutoff. `SemFilter::liquidResonance()` maps the knob
  so that point lies at two thirds, linear below and above.
- SEM variants OB-Xd, Oberheim, Vult and Cytomic, and Shelves, do not
  self-oscillate (like the SEM itself) and keep their curves.

Measured onset (knob position at about 0.5 / 1.5 / 4 kHz): SSI2144 0.66 /
0.65 / 0.63, SST LP4 0.74 / 0.70 / 0.65, SST LP2 0.67 / 0.66 / 0.63, Liquid
0.67 at all three. The SST ladder feeds back with a one-sample delay, so its
limit moves with the cutoff (feedback 3.8 .. 4.25).

Not taken over: the firmware's resonance compensation of the mixer levels
(still open, see the audit section).

Tests: `ResonanceCalibrationTest` (new) excites each model with a short
burst and finds the lowest knob position at which the ringing does not
decay; it checks the onset at two thirds (+-0.08) for the self-oscillating
models and no self-oscillation for the others. `FactoryPresetHeadroom`:
preset 34 (stacked unison, SSI2144 at full resonance) now really
self-oscillates; its voice sum rises to 1.10 (output 0.53), so the unison
limit on the voice sum before the bus headroom is 1.2 instead of 1.0.
170 of 402 reference cases change. Baseline recreated (old copy as
audio-baseline-envspeed); 21/21 CTest tests pass.

### Performance parameters as in the firmware (changes the sound)

Bender, modwheel, pressure and the chromatic pitch now follow the firmware
(`synth_wheelEvent`, `synth_pressureEvent`, `getStaticCV`,
`refreshLfoSettings`, `refreshTunedCVs`). All of them are per part.

- Bender: `MidiInput` keeps the raw bend (full scale); `modulation::
  benderAmount()` scales it by the part's range and target. Ranges 4 / 7 /
  12 semitones (was 3 / 5 / 12). Targets: pitch (+-range), filter (+-4 x
  range semitones of cutoff), volume and WaveMod (bend / 12 x range). The
  target was ignored before (always pitch). An MPE per-note bend still
  bends the pitch.
- Modwheel: `spModwheelTarget` 0 adds the wheel to LFO 1's depth, 1 to LFO
  2's, shifted by `spModwheelRange` (>> 5 / 3 / 1 / 0). Before, the wheel
  scaled LFO 1 onto the pitch, cutoff or WaveMod by the target's index.
- Pressure: the LFO 1 and LFO 2 targets add to that LFO's depth (were
  ignored). Pitch (downwards, a quarter), filter and WaveMod as before.
- Volume (bender, pressure and the Overviber timbre input): scales the
  oscillator and noise mixer levels before the filter, 0 .. 2x with 1x at
  rest, instead of adding to the VCA.
- Resonance compensation: for the ladder filters (SSI2144, SST) the mixer
  levels rise with the resonance as in the firmware, 1x to 5.7x, which
  makes up for the ladder's passband loss (1 / (1 + k)). The SEM variants,
  Liquid and Shelves have no such loss and keep 1x.
- LFO start delay: acts on the LFO the modwheel does not control (with the
  wheel on LFO 1, the default, that is LFO 2). The earlier glide/delay step
  had put it on LFO 1. The knob in the LFO 1 card now says which LFO it
  delays ("DELAY LFO 2").
- `modulation::lfoAmounts()` computes both LFO depths; the engine applies
  them on every 500 Hz control tick.
- Timbre on LFO 1 / LFO 2 (an Overviber target) adds the bipolar timbre / 2
  to that LFO's depth.
- Chromatic pitch: 1 drops the fine part of the oscillator base pitch, 2
  also rounds it down to whole octaves (`VoiceAllocator::startNote`).
- Preset loading no longer copies the performance targets into matrix
  slots (the engine applied them a second time). A preset without matrix
  slots, such as every hardware preset, has an empty matrix.

Tuning found on the way: the oscillator frequency is 0 .. 64 semitones over
the knob (firmware), with 0 at concert pitch; the factory presets use 0.
The Init sound had knob 500 ("center"), which is +32 semitones: Init and the
default kit played A4 at 2800 Hz. Init now uses 0 as in the firmware. The
Modern skin showed the frequency as -12 .. +12 st and master tune as +-12
st; frequency now shows 0 .. 64 st (unipolar knob), master tune +-100 ct
(firmware: +-1 semitone). The host parameters use the same units, and the
Classic skin's note display uses the firmware's value >> 10.

Tests: `FirmwareParamScenarioTest` covers bender ranges and targets, the
volume target on the mixer, the resonance compensation, modwheel and
pressure LFO depths with the start delay, and the chromatic modes.
`GlideDelayScenarioTest` checks the delay on the LFO the wheel does not
control, both ways. `AdvancedMidiScenarioTest` checks the 4/7/12 ranges;
`ModMatrixScenarioTest` checks that a preset without matrix slots has none
(replaces the legacy migration checks). `ElementsVoiceScenarioTest`
references are at concert pitch now (Elements takes the oscillator pitch
as its note; it played 32 semitones too high). `RefactoringScenarioTest`
uses free tuning for its semitone offset. Headroom: preset 34's voice sum
rises to 1.26 with the compensation, the unison limit is 1.5. Skin fixtures
updated. 392 of 402 reference cases change. Baseline recreated (old copy as
audio-baseline-resonance); 21/21 CTest tests pass.

One `--out` run produced different audio for two cases
(`elements_0_filter_2_res_{500,999}_96000_v6`) than all later runs; the
baseline is taken from a run that later runs reproduce. The cause is
found and fixed, see "Deterministic rendering: uninitialised Elements
resonator" below.

### Deterministic rendering: uninitialised Elements resonator

The render that differed once came from Elements' modal resonator:
`Resonator::Init()` (Mutable Instruments' code) leaves `lfo_phase_`,
`clock_divider_`, `modulation_frequency_` and `modulation_offset_` unset.
The hardware keeps the object in zeroed static memory; in the plugin it
lives on the heap. Fresh memory from the system is zero, so the renders
were nearly always the same, but reused memory changed which half of the
modes the first update computes and the position LFO's start, audible
from about 0.5 ms on. `Init()` now sets all four to zero, which is what
the renders had in practice: the baseline does not change.

Found by filling every heap allocation of the reference renderer with a
byte pattern (`OVERVIBER_POISON=<byte>`): with 0x7F or 0xFF, 73 cases
(all Elements model 0 renders and `scenario_hybrid`) differed; after the
fix all 402 cases are identical with 0x00, 0x55, 0x7F and 0xFF, and eight
consecutive `--out` runs gave the same hashes. The new CTest
`AudioReferencePoison` runs the comparison with 0x7F, so a read of
uninitialised heap state fails every time instead of now and then.
Uninitialised stack state is not covered (the engines live on the heap).

### Voice console meters show the console's working range

The console encodes each voice (`e = 1 - (1 - x)^phi`, `x = level x drive
x 0.618`), sums the encoded voices and decodes the sum. A single voice
passes unchanged until the bus knee; the colour comes from the sum and the
bus saturation, so it depends on the bus load S (the encoded sum), not on
a voice's level. Measured with a sine, pan centre, drive 1.0: one voice is
linear up to S = 0.65 (+3.8 dB voice level); with several voices audible
warmth (THD -40 dB) starts at S = 0.3 .. 0.4 and strong saturation
(-20 dB) around S = 1 (six voices: -20 dB and -7.5 dB voice level). Drive
4.0 moves everything about 12 dB lower.

The meters did not show that: the voice meters had 0 dB at voice level
1.0 before drive (white "clip" segments where one voice is still clean,
mid green where six voices already saturate), the master meter was an
estimate (sum of the voice meters x 0.22, linear), only the last audio
block's peak reached the UI (most peaks were lost) and the fader text was
wrong (0.5 read -18 dB, it is -6 dB).

Now:

- `MasterBus::addVoice()` returns the voice's encoded level; the engine
  keeps per block each voice's share of the bus load and the bus load L/R
  (`getVoiceBusLoad()`, `getBusLoad()`). `getVoicePeakLevel()` (voice
  amplitude, used by tests) is unchanged.
- The processor keeps the largest value until its timer takes it, the
  model keeps the largest until the view takes it (`addMeterLevels()`,
  `takeMeterLevels()`); no peak between two reads is lost.
- Scale: dB relative to the console knee (S = 0.65). -6 dB is S = 0.33,
  where the warmth starts; +2 dB is S = 0.82, at the bus ceiling (0.86).
  -60 .. -6 dB fill the lower 40 % linearly, -6 .. +2 dB rise degressively
  (height ~ t^0.8: -6 .. -3 dB about five segments, -3 .. 0 dB two, the
  knee 0 .. +2 dB about two of the fifteen level segments), the knee lights
  white, and from
  +2 dB the top segment lights red, a fixed colour independent of the skin.
- Ballistics in the panel: instant rise, 24 dB/s fall, 1.5 s peak hold
  (a line; red while the hold is over +2 dB).
- The master strip shows the real bus load, left and right (since
  changed to the output, see "Console gain staging" below).
- Fader text: 20 log10 of the linear fader gain.
- The voice LEDs (Modern and Classic) show the share of the bus load,
  full at the knee.

### Session restore no longer overrides a newer preset

With a restored session (host project load, standalone start) and a preset
chosen right after it, both reached the audio engine in the same block, the
session last: its voice patterns, matrix and waves overwrote the chosen
preset's (the main part's parameters came from the host parameters and
looked right). The editor did not publish again, as its state had not
changed, so the engine stayed with the old data. Seen with preset 12
"Choir Voices" (six voices unison) after a session with preset 0: only one
voice played, and the voice meters showed exactly that.

Now `processBlock` applies a restored session before queued editor states,
and the timer publishes the editor state again after decoding a session.

Test: `PluginSessionScenarioTest` (new) saves a session with preset 0,
restores it in a new processor, chooses preset 12 before the first block
and counts the voices on the console bus (six; one before the fix). It
also checks that a plain restore plays the session's preset.

### Console gain staging, voice faders to +12 dB, master meter on the output

With `acid.mid` played through all 50 factory presets, the console bus was
often at its ceiling: presets 40, 34 and 39 were red in 60 .. 70 % of the
meter readings, 46, 8 and 10 in 20 .. 30 %, also presets without
resonance. The output stayed near -6 dBFS because the console's bus
saturation limited it. The voices simply entered the console too hot.

- `MasterBus::kConsoleInputGain` = 0.5: voices enter the console 6 dB
  down; `kBusHeadroom` 0.45 -> 0.9 makes that up after the decoder, so
  clean signals keep their level and the saturation starts 6 dB later.
  With `acid.mid` no preset reaches red on any meter now; the loudest
  voices peak at -3.8 dB (warm range), the output at -2.1 dBFS.
- Voice faders: position <-> gain in dB, 0 dB (unity, the default) at
  75 %, -60 .. 0 dB below, up to +12 dB above to push a voice into the
  saturation; double click returns to 0 dB, the bottom mutes. The model
  and the engine keep the linear gain (now up to 4.0), so sessions stay
  valid. The faders follow the model after a session restore.
- Master strip: the output peak after pad, Mackity send and ceiling
  (`getOutputPeak()`), so the master fader and the Mackity send show on
  it. Its scale reads +2 dB (red) where the output ceiling starts (0.9,
  `kOutputMeterReference`). The voice strips keep the console load.
- The SSI2144's maximum feedback stays 5.0: 4.5 or even 4.0 changed
  preset 14's bus peak by only 0.1 .. 0.3 dB, since the filter's own
  saturation limits the self-oscillation.

Tests: `FactoryPresetHeadroom` allows the output ceiling's knee for the
sequence's low four-note chords (up to 0.975, the ceiling ends at 0.98;
presets 5, 9, 14, 40 reach 0.92 .. 0.97 there). Skin fixtures updated
(fader positions). All 402 reference cases change (the console encoder is
nonlinear, so -6 dB in and +6 dB out is not bit-identical); baseline
recreated (old copy as audio-baseline-performance); 23/23 CTest tests pass.

### Filter selection in four families, mixer strips

The filter card no longer mirrors the engine's model / SEM variant / mode
parameters one to one. `FilterVcaTab::filterChoices()` groups them in four
families, each entry standing for a model, variant and mode:

| Family | Entries | Engine |
|---|---|---|
| LADDER | Lowpass 24 / 18 / 12 / 6 dB | SSI2144; SST ladder modes 1 .. 3 |
| RIPPLES | Lowpass 24 / 12 dB, Bandpass 12 dB | SEM variant Liquid |
| SEM | Lowpass / Bandpass / Highpass 12 dB, Notch | SEM variant Cytomic |
| SHELVES | 4-Band Parametric | Shelves EQ mode |

(Order since the family switch option: LADDER, RIPPLES, SEM, SHELVES.)

The SEM variant menu is gone. The other SEM variants, the SST's 24 dB mode
and Shelves' SVF modes stay in the engine; a preset using one shows under
its family (the first entry, or the same mode for other SEM variants).
Switching families keeps the entry when the new family has it.

- The SST ladder uses the SSI2144's cutoff range (20 Hz x 1300 instead of
  15 Hz x 1600), so LADDER 24 -> 18 dB keeps the cutoff (it jumped about
  three semitones). The cutoff knob and the response curve now show that
  range (they showed 20 Hz .. 20 kHz, e.g. 632 Hz for the actual 721 Hz).
- Shelves: band defaults at 100 Hz, 2.5 kHz and 8 kHz (they were 80 Hz,
  1.8 kHz and 5 kHz; Hz = 20 x 1000^(pot/999)). The mid low band's
  frequency is the cutoff, which the other filters leave at 26 kHz;
  switching to SHELVES sets it to 400 Hz. The band buttons use the toggle
  font size and sit below the knob row.
- Mixer: the strips are called VOICE 1 .. 6; each strip's LED shows its
  meter level like the voice LEDs in the title bar (brightness follows
  the level, full at 0 dB, accent border above 10 %).

Tests: skin fixtures updated; the "ripples" scenario sets the Liquid
variant (it used model 1, which is SEM since the SEM filter replaced
Liquid). Reference cases with Shelves or the SST ladder change; baseline
recreated (old copy as audio-baseline-gainstaging); 23/23 CTest tests pass.

### Master strip without fader: output meters and MUTE

The master fader was the console's output pad (`cpConsolePad`) after the
decoder: it could only lower the level (to mute), never drive Console X.
It is gone; level and saturation come from `ampLevelKnob`,
`consoleDriveKnob` and the voice faders.

- The console's pad stays at 1.0; `cpConsolePad` is no longer a host
  parameter and not read (the parameter index stays).
- The master strip's L/R output meters use the fader's width; a MUTE
  button sits under them. Mute is a mixer state like the voice faders:
  model -> prepared state -> `MasterBus::setMuted()`, faded over 5 ms,
  saved in the session (`masterMute`), not in presets.
- Fixed on the way: `SessionState::decode` rejected sessions with a voice
  fader above 2.0, but the faders reach 4.0 (+12 dB) since the gain
  staging change; the limit is 4.0 now.

Tests: `PluginSessionScenarioTest` checks that a session keeps the mute
and a fader at 3.0 (+9.5 dB) and that a muted master outputs silence.
The skin test restores the mute between scenarios (a MUTE click leaked
into the following ones). Skin fixtures updated.

### Settings debug card with state copy; master PAD button

- Settings: the debug tools are a card of their own ("DEVELOPER & DEBUG")
  below the appearance card, with the inspector switch and a button "COPY
  STATE TO CLIPBOARD". `SettingsTab::describeState()` writes the main
  preset in the preset file format (all parameters and the matrix; readable
  by `PresetManager::parsePresetString`, continuous values in pot units),
  the exact 16-bit values of the continuous parameters as comment lines,
  the mixer (voice faders, pans, master mute) and the part routing.
- Master strip: the Mackity return PAD is a button like MUTE, above it;
  the output meters end above the two buttons. The two meter columns are
  centred in the strip, the scale labels right of them.

Test: `PluginSessionScenarioTest` reads the state copy back (same values
at pot resolution, stepped parameters exact), finds the raw values and
the mixer in it. Skin fixtures updated.

### One scale for meters and voice faders

The voice faders had their own curve (0 dB at 75 %), the meters another
(0 dB = knee at 88 %), so a fader at 0 dB did not sit on the meters' 0.
Both now use one scale (`scalePosition()` / `scaleDb()` in
`ModernVoiceMeterPanel`):

| dB | height | |
|---|---|---|
| -60 .. -6 | 0 .. 22 % | linear |
| -6 .. 0 | 22 .. 50 % | degressive (t^0.8) |
| 0 .. +2 | 50 .. 58 % | knee, white |
| +2 .. +12 | 58 .. 100 % | red: over on the meters, saturation travel of the faders |

- The fader default (0 dB, unity) is at half height, on the meters' 0;
  the upper half of the fader (0 .. +12 dB) drives the voice into the
  console's saturation. The meters span the faders' track
  (`Slider::getPositionOfValue`), so the scale lines up with the caps.
- The meters have 24 segments; the red zone lights segment by segment
  (the former single over segment is gone). Labels: +12, +6, +2 (red), 0,
  -6, -inf.
- Master: each output column is twice as wide as a voice meter (14 px),
  the pair centred, labels right of it, ending above PAD and MUTE. PAD and
  MUTE look like the EQ band buttons (compact font, 14 px, connected
  edges).

Skin fixtures updated (fader default position 0.5).

### Discontinuity knob over its whole range; filter families remember their entry

- Console X discontinuity (AIR knob): the threshold was 2.0 - 1.75 x knob
  on the decoded bus, which the bus saturation limits to about 1.14, so
  the knob's lower half (below about 49 %) did nothing and the default
  (50 %) acted only at the very top. The threshold now runs from the
  former default's value (1.124) at knob 17 (2 %, the new default) to
  0.25 at the end; below that it rises further, off at 0. The presets
  sound as before (reference renders equal within one float LSB); the
  rest of the knob now works.
- Filter card: each family (LADDER, SEM, RIPPLES, SHELVES) remembers its
  last entry and returns to it when chosen again. A family not visited yet
  starts with the same filter as the current one if it has it (by label,
  e.g. Lowpass 12 dB from SEM to RIPPLES or LADDER, for comparing), else
  with the same type (Lowpass 24 dB from LADDER to SEM: Lowpass 12 dB),
  else with its first entry. Before, the entry index carried over (RIPPLES
  Bandpass -> LADDER Lowpass 12 dB). The memory is the editor's
  state, not saved. Equal filters share a row across the families:
  Lowpass 24 dB in row 1 (LADDER, RIPPLES), Bandpass 12 dB in row 2 (SEM,
  RIPPLES), Lowpass 12 dB in row 3 (all three); SEM's Highpass and Notch
  take rows 1 and 4.
- Mixer: in each voice strip the meter (with its scale labels) and the
  fader form two columns centred as a pair (`voiceColumns()`).

Skin fixtures updated; baseline recreated (old copy as
audio-baseline-filterfamilies).

### Host tempo in the editor model; master waterfall while muted

- The host tempo reached only the audio engine; `SynthModel` kept 120 BPM,
  so with host sync every display using `getEffectiveBpm()` (the arp
  status line) showed 120. The processor now passes the host tempo to the
  model on its timer (`hostBpmForEditor`, `SynthModel::setHostBpm`).
- While MUTE is on, the master's output columns show a waterfall instead
  of the (silent) level: starting blank when MUTE is pressed, lit bands
  fall from above through the segments in their zone colours, a new one
  every bar (four beats) of the current tempo; 16 of the 24 segments form
  a band's tail, fading paler; the right column runs a quarter of the
  segments behind, and the left starts its next band when the right one
  is halfway down, so bands overlap. PAD and MUTE have the EQ band buttons'
  size (64 x 14), centred in the strip. The phase follows
  the clock, not the host's beat position.

Test: `PluginSessionScenarioTest` runs the processor with a play head at
93 BPM and host sync on and checks the model's tempo.

### Filter family switch option; EQ label gap

- Settings, new card "EDITOR BEHAVIOUR": when switching filter families,
  preselect the same filter (default; by label, else the same type, else
  the family's last choice; on every switch, for comparing) or the
  family's last choice. A family not visited yet always tries the same
  filter first. Saved in skin_config.conf (`filterFamilySwitch=same|last`)
  and passed to the filter tab through `SettingsTab::Host`.
- Filter card, SHELVES: the knob row sits 3 px lower, so its labels end
  2 px above the band buttons (the gap looked too large).

Skin fixtures updated.

### Filter knob labels from the filter table

The cutoff and resonance labels were chosen by engine model and mode
numbers from before the SEM filter replaced Liquid as model 1, so SEM's
Highpass read "CENTER FREQ" and its Bandpass "CUTOFF", and the resonance
label switched between "RESONANCE" and "RESONANCE (Q)" without a rule.
The labels now follow the selected entry of the filter table: Bandpass
and Notch show "FREQ", all others "CUTOFF"; the resonance knob is always
"RESONANCE" (the Shelves EQ keeps its band labels).
`updateFilterUIState()` no longer takes model and mode.

Shelves EQ: KEY TRACK is no longer the third band knob of the LOW and
HIGH shelves (it tracks the mid low band's frequency for all bands). It
sits in the mode column below 4-Band Parametric, flush above ENV DEPTH;
the third band knob (`eqQKnob`) is the Q of MID LOW and MID HIGH and is
hidden for the shelves, which have no Q.

Shelves mid band Q: the engine maps the knob to Q = 0.5 x 80^(pot/999)
(0.5 .. 40), the UI showed 0.5 + 9.5 x pot/999; knob, text entry and the
curve now use the engine's curve (`shelvesQ()`). Both mid bands default
to Q 1.0 (pot 158): MID HIGH through `cpShelvesP2Q`, MID LOW, whose Q is
the shared resonance, is set to it when switching to SHELVES (like the
400 Hz for its frequency). MID HIGH showed "Q 3.35" at its former default
300, which is Q 1.86.

### Next steps

1. Done, see step 1 above.
2. Done, see step 2 below.
3. Done, see step 3 below.
4. Done, see step 4 below.
5. Done, see steps 5a and 5b.
6. Done, see step 6 below.

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
  table). Addressed in the step "Liquid and Shelves filter cost" below:
  −24 to −27 % for Liquid, −33 to −43 % for Shelves, bit-exact.
- **Modulation matrix:** 11 of 32 targets were offered in the UI but ignored
  by the engine. Removed in step 1.
- **State takeover:** a `PreparedState` holds the waves of all 16 parts
  (16 × 4 × 2400 samples ≈ 300 KB). The editor copied and compared it on
  every timer tick, and the audio thread compared all waves on every
  takeover. Solved in step 6 with wave revisions.
- `Voice::isActive()` is evaluated twice per sample and voice (in
  `renderBlock` and `processSample`). Step 5 checks it once per segment.
