#include "TestSynth.h"
#include "dsp/audible/stmlib/utils/random.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct PresetHeadroomResult {
    int number = 0;
    std::string name;
    double busPeak = 0.0;
    double outputPeak = 0.0;
    uint64_t nonFinite = 0;
    bool stackedUnison = false;
    bool cutoffFailed = false;
};

int main() {
    constexpr int sampleRate = 44100;
    constexpr int blockSize = 256;
    constexpr uint16_t velocity = 36122; // MIDI velocity 70, as in the supplied file.
    constexpr std::array<uint8_t, 4> firstChord{19, 31, 34, 38};
    constexpr std::array<uint8_t, 4> secondChord{48, 60, 63, 67};

    auto renderSeconds = [=](TestSynth& engine, double seconds) {
        std::array<float, 256> left{}, right{};
        int remaining = static_cast<int>(std::llround(seconds * sampleRate));
        while (remaining > 0) {
            const int count = std::min(blockSize, remaining);
            engine.renderBlock(left.data(), right.data(), count);
            remaining -= count;
        }
    };

    auto catalog = std::make_unique<TestSynth>();
    catalog->getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
    const int presetCount = catalog->getPresetManager().getPresetCount();
    std::vector<PresetHeadroomResult> results;
    int failures = 0;

    for (int preset = 0; preset < presetCount; ++preset) {
        stmlib::Random::Seed(33);
        std::srand(33);
        auto engine = std::make_unique<TestSynth>();
        engine->getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
        engine->getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
        engine->prepare(sampleRate);
        engine->loadPreset(preset);

        // A cutoff move must affect an already held voice, without requiring a
        // second note-on. Disable the arp so this checks the filter path itself.
        engine->setSteppedParam(spArpMode, amOff);
        engine->noteOn(60, velocity, 1);
        renderSeconds(*engine, 0.05);
        std::array<uint16_t, SYNTH_VOICE_COUNT> cutoffBefore{};
        std::array<bool, SYNTH_VOICE_COUNT> activeBefore{};
        int activeVoices = 0;
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            activeBefore[v] = engine->isVoiceActive(v);
            cutoffBefore[v] = engine->getFilterNoteCV(v);
            activeVoices += activeBefore[v] ? 1 : 0;
        }
        const uint16_t oldCutoff = engine->getCurrentPreset().continuousParams[cpCutoff];
        const uint16_t newCutoff = oldCutoff < 32768
            ? static_cast<uint16_t>(std::min<int>(65535, oldCutoff + 12000))
            : static_cast<uint16_t>(std::max<int>(0, oldCutoff - 12000));
        engine->setContinuousParam(cpCutoff, newCutoff);
        renderSeconds(*engine, 0.025);
        bool cutoffMoved = false;
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (!activeBefore[v]) continue;
            const uint16_t after = engine->getFilterNoteCV(v);
            cutoffMoved |= newCutoff > oldCutoff ? after > cutoffBefore[v] : after < cutoffBefore[v];
        }
        const int presetNumber = engine->getPresetManager().getPresetNumber(preset);
        if (presetNumber == 23)
            std::cout << "Preset 23 held-note cutoff: " << cutoffBefore[0] << " -> "
                      << engine->getFilterNoteCV(0) << " (target delta "
                      << (static_cast<int>(newCutoff) - oldCutoff) << ")\n";
        const bool cutoffFailed = activeVoices == 0 || !cutoffMoved;

        // Restore the factory state before the supplied long MIDI/headroom run.
        engine->loadPreset(preset);
        RenderDiagnostics diagnostics;
        engine->setDiagnostics(&diagnostics);

        for (auto note : firstChord) engine->noteOn(note, velocity, 1);
        renderSeconds(*engine, 8.0);
        for (auto note : firstChord) engine->noteOff(note, 32767, 1);
        for (auto note : secondChord) engine->noteOn(note, velocity, 1);
        renderSeconds(*engine, 7.6302);
        for (auto note : secondChord) engine->noteOff(note, 32767, 1);
        renderSeconds(*engine, 1.0);
        engine->setDiagnostics(nullptr);

        PresetHeadroomResult result;
        result.number = engine->getPresetManager().getPresetNumber(preset);
        result.name = engine->getCurrentPreset().presetName;
        result.busPeak = std::max(diagnostics.busLeft.peak, diagnostics.busRight.peak);
        result.outputPeak = std::max(diagnostics.outputLeft.peak, diagnostics.outputRight.peak);
        result.nonFinite = diagnostics.busLeft.nonFinite + diagnostics.busRight.nonFinite
            + diagnostics.outputLeft.nonFinite + diagnostics.outputRight.nonFinite;
        int patternVoices = 0;
        while (patternVoices < SYNTH_VOICE_COUNT
               && engine->getCurrentPreset().voicePattern[patternVoices] != ASSIGNER_NO_NOTE) ++patternVoices;
        result.stackedUnison = engine->getCurrentPreset().steppedParams[spUnison] != 0 && patternVoices > 1;
        result.cutoffFailed = cutoffFailed;
        const bool failed = result.cutoffFailed || result.nonFinite != 0 || result.outputPeak > 0.90
            // Stacked unison sums up to six voices before the bus headroom
            // (0.45); preset 34 self-oscillates at full resonance with the
            // firmware's resonance level compensation (about 1.26).
            || (result.stackedUnison && result.busPeak > 1.5);
        failures += failed;
        results.push_back(result);
    }

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.busPeak > b.busPeak;
    });
    std::cout << "Factory preset headroom with supplied two-chord MIDI sequence\n";
    for (const auto& result : results) {
        if (result.busPeak > 0.80 || result.outputPeak > 0.80 || result.nonFinite != 0 || result.cutoffFailed)
            std::cout << std::setw(4) << result.number << "  bus " << std::fixed << std::setprecision(4)
                      << result.busPeak << "  output " << result.outputPeak
                      << (result.stackedUnison ? "  unison  " : "  poly    ") << result.name
                      << (result.cutoffFailed ? "  CUTOFF FAILED" : "") << '\n';
    }
    std::cout << presetCount << " presets checked; " << failures << " headroom/cutoff failures\n";
    return failures ? 1 : 0;
}
