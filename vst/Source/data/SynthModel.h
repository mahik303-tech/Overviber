#pragma once

#include "PresetManager.h"
#include "WaveManager.h"
#include "../dsp/AfxKit.h"
#include "../dsp/arp.h"
#include "../dsp/PreparedState.h"
#include <array>

// Display data the audio engine reports to the editor.
struct ArpVisualizationState {
    bool valid = false;
    uint32_t tick = 0;
    int currentStep = 0;
    bool gateActive = false;
    int activeCount = 0;
    std::array<uint8_t, 16> activeNotes{};
    std::array<uint8_t, 16> patternNotes{};
};

// ==============================================================================
// The editor's model of the instrument: 16 parts (presets and wave data),
// routing, voice mixer, arpeggiator pattern, and the preset and wave files.
//
// The editor and the plugin's parameter handling change the model; the
// processor turns it into a PreparedState for the audio engine, which has no
// file access of its own. The model renders no audio.
// ==============================================================================
class SynthModel {
public:
    SynthModel();

    // ---- Parts
    PresetData& getCurrentPreset() { return currentPreset; }        // part 1
    const PresetData& getCurrentPreset() const { return currentPreset; }
    WaveManager& getWaveManager() { return waveManager; }            // part 1 waves
    AfxKit& getAfxKit() { return afxKit; }
    const AfxKit& getAfxKit() const { return afxKit; }
    PartRoute& getPartRoute(int index) { return routes[std::clamp(index, 0, 15)]; }
    bool usesCustomRouting() const { return customRouting; }
    void setCustomRouting(bool enabled) { customRouting = enabled; }

    // ---- Parameters of part 1
    void setContinuousParam(continuousParameter_t cp, uint16_t value);
    void setSteppedParam(steppedParameter_t sp, uint8_t value);
    void setMatrixSlot(int slotIndex, modSource_t src, modDest_t dest, modSource_t via, int16_t depth, bool enabled = true);

    // ---- Preset and wave files
    PresetManager& getPresetManager() { return presetManager; }
    // Loads a preset into part 1, including its wave files. Voices sounding
    // with the previous preset are retired in the audio engine.
    void loadPreset(int presetIndex);
    // Loads part 1's waves from the bank and wave names of its preset.
    void applyPreset();
    // Retires all sounding voices in the audio engine.
    void panic() { ++panicGeneration; }
    // Updates settings derived from part 1's preset (arpeggiator) after its
    // parameters were written directly, e.g. when a setup is decoded.
    void presetChanged() { configureArpeggiator(); }
    uint32_t getPanicGeneration() const { return panicGeneration; }

    // ---- Voice mixer
    void setVoiceFader(int voice, float value);
    float getVoiceFader(int voice) const;
    void setVoicePan(int voice, float pan, bool customized = true);
    float getVoicePan(int voice) const;           // centred for single-voice presets
    float getStoredVoicePan(int voice) const;
    bool isVoicePanCustomized(int voice) const;

    // ---- Arpeggiator settings and step sequence (no clock runs here)
    Arpeggiator& getArpeggiator() { return arpeggiator; }
    bool isHostSyncEnabled() const { return currentPreset.steppedParams[spArpSync] != 0; }
    float getInternalBpm() const;
    float getEffectiveBpm() const { return isHostSyncEnabled() ? hostBpm : getInternalBpm(); }

    // ---- Display data reported by the audio engine
    void setDisplayLevels(const std::array<int, 6>& levels) { displayLevels = levels; }
    int32_t getVoiceAmpLevel(int voice) const { return voice >= 0 && voice < 6 ? displayLevels[voice] : 0; }
    void setArpVisualizationState(const ArpVisualizationState& state) { arpVisualizationState = state; }
    const ArpVisualizationState& getArpVisualizationState() const { return arpVisualizationState; }
    uint32_t getCurrentTick() const { return arpVisualizationState.tick; }

    // Complete state for the audio engine. Reusing the same state object
    // skips copying waves that did not change since the last capture.
    void capturePreparedState(PreparedState& state) const;

private:
    void configureArpeggiator();
    bool presetUsesSingleVoice() const;

    AfxKit afxKit;
    PresetData& currentPreset;
    WaveManager& waveManager;
    PresetManager presetManager;
    PartRoute routes[16];
    bool customRouting = false;
    uint32_t panicGeneration = 0;

    float voiceFader[SYNTH_VOICE_COUNT] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float voicePan[SYNTH_VOICE_COUNT] = { -0.70f, 0.70f, -0.35f, 0.35f, -0.10f, 0.10f };
    bool voicePanCustomized[SYNTH_VOICE_COUNT] = { false, false, false, false, false, false };

    Arpeggiator arpeggiator;
    float hostBpm = 120.0f;
    std::array<int, 6> displayLevels{};
    ArpVisualizationState arpVisualizationState{};
};
