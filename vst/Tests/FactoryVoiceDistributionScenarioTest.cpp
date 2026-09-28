#include "TestSynth.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>

int main() {
    auto engine = std::make_unique<TestSynth>();
    engine->getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
    engine->getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
    engine->prepare(48000.0f);

    int failures = 0;
    float left[128]{}, right[128]{};
    auto& manager = engine->getPresetManager();
    for (int index = 0; index < manager.getPresetCount(); ++index) {
        engine->panic();
        engine->loadPreset(index);
        engine->setSteppedParam(spArpMode, 0);

        const auto& preset = engine->getCurrentPreset();
        const int enabledVoices = std::clamp((int)preset.steppedParams[spVoiceCount] + 1, 1, SYNTH_VOICE_COUNT);
        int expectedVoices = enabledVoices;
        if (preset.steppedParams[spUnison] != 0) {
            int patternNotes = 0;
            while (patternNotes < SYNTH_VOICE_COUNT && preset.voicePattern[patternNotes] != ASSIGNER_NO_NOTE)
                ++patternNotes;
            if (patternNotes == 0) patternNotes = SYNTH_VOICE_COUNT;
            expectedVoices = std::min(enabledVoices, patternNotes);
            engine->noteOn(60, 55000, 1);
        } else {
            for (int voice = 0; voice < enabledVoices; ++voice)
                engine->noteOn((uint8_t)(48 + voice * 3), 55000, 1);
        }

        bool seen[SYNTH_VOICE_COUNT]{};
        double leftEnergy = 0.0, rightEnergy = 0.0;
        bool finite = true;
        for (int block = 0; block < 64; ++block) {
            engine->renderBlock(left, right, 128);
            for (int voice = 0; voice < SYNTH_VOICE_COUNT; ++voice)
                seen[voice] |= engine->getVoiceAmpLevel(voice) > 0;
            for (int sample = 0; sample < 128; ++sample) {
                finite &= std::isfinite(left[sample]) && std::isfinite(right[sample]);
                leftEnergy += std::abs(left[sample]);
                rightEnergy += std::abs(right[sample]);
            }
        }

        int activeVoices = 0;
        for (bool active : seen) activeVoices += active;
        const double ratio = rightEnergy > 0.0 ? leftEnergy / rightEnergy : 0.0;
        // Stereo effects can introduce a very small channel mismatch even when the
        // voice itself is centered. A one-percent window still catches the former
        // fixed 85/15 pan decisively while avoiding false failures from the FX tail.
        const bool centeredMono = expectedVoices != 1 || std::abs(ratio - 1.0) < 0.01;
        const bool passed = finite && activeVoices == expectedVoices && centeredMono;
        failures += !passed;
        std::cout << (passed ? "PASS " : "FAIL ")
                  << manager.getPresetNumber(index) << ' ' << manager.getPresetName(index)
                  << " voices=" << activeVoices << '/' << expectedVoices
                  << " L/R=" << ratio << '\n';
    }

    std::cout << manager.getPresetCount() << " factory presets checked, " << failures << " failures\n";
    return failures ? 1 : 0;
}
