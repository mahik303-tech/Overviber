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
