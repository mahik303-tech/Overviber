#pragma once

#include "dsp/SynthEngine.h"
#include "data/SynthModel.h"
#include <memory>

// Test harness: an audio engine plus the editor model that reads preset and
// wave files. As in the plugin, the model's parts reach the engine through a
// prepared state; the engine itself has no file access.
class TestSynth : public SynthEngine {
    // On the heap: tests keep engines on the stack, and engine and model each
    // hold about 300 KB of wave data.
    std::unique_ptr<SynthModel> modelStorage = std::make_unique<SynthModel>();

public:
    SynthModel& model = *modelStorage;

    PresetManager& getPresetManager() { return model.getPresetManager(); }
    WaveManager& getWaveManager() { return model.getWaveManager(); }
    // The model's parts; call syncParts() after editing them.
    AfxKit& getAfxKit() { return model.getAfxKit(); }

    // The former SynthEngine::prepare() also loaded part 1's wave files.
    void prepare(float sampleRate) {
        SynthEngine::prepare(sampleRate);
        loadWaves();
    }

    // Like the former SynthEngine::loadPreset(): retires all voices, then
    // takes over the preset and its wave files as part 1.
    void loadPreset(int index) {
        model.loadPreset(index);
        panic();
        takeOverParts(true);
        applyPreset();
    }

    // Loads part 1's wave files named by the model's preset.
    void loadWaves() {
        model.applyPreset();
        takeOverParts(true);
        applyPreset();
    }

    // Hands edited parts to the engine: the waves of all parts, the presets
    // of parts 2-16 and the note map. Part 1's parameters stay as set on
    // the engine.
    void syncParts() { takeOverParts(false); }

private:
    // Routing, voice mixer and arp sequence set directly on the engine stay.
    void takeOverParts(bool includingMainParameters) {
        auto state = std::make_unique<PreparedState>();
        model.capturePreparedState(*state);
        state->panicGeneration = getPanicGeneration();
        state->customRouting = usesCustomRouting();
        for (int part = 0; part < 16; ++part) state->parts[part].route = getPartRoute(part);
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            state->faders[v] = getVoiceFader(v);
            state->pans[v] = getStoredVoicePan(v);
            state->panCustomized[v] = isVoicePanCustomized(v) ? 1 : 0;
        }
        for (int step = 0; step < 16; ++step) {
            state->arpPattern[step] = getArpeggiator().getStepPattern(step);
            state->arpDegrees[step] = getArpeggiator().getStepDegree(step);
        }
        state->transpose = getArpeggiator().getTranspose();
        applyPreparedState(*state, !includingMainParameters);
    }
};
