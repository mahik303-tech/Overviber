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
violation under CTest (preset 0067, "MI Elements Hybrid Strike"), while direct
runs passed. AddressSanitizer located it deterministically in
`elements::MultistageEnvelope::Process`: a finished envelope reads
`shape_[num_segments_]` and `level_[num_segments_ + 1]`, which `set_adsr()`
never writes. The firmware keeps its voices in zero-initialised static storage;
Overviber allocates them on the heap. The segment arrays are now
zero-initialised, and TEST 8 of `ElementsVoiceScenarioTest` covers
uninitialised storage.

Windows Release builds the VST3, Standalone and Skin Designer; all 16 CTest
scenarios pass.
