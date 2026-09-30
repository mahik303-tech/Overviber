#pragma once

#include "PresetManager.h"
#include "WaveManager.h"
#include "../dsp/AfxKit.h"
#include "../dsp/arp.h"
#include "../dsp/PreparedState.h"
#include <algorithm>
#include "OutputScope.h"
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
    uint32_t getPanicGeneration() const { return panicGeneration; }

    // ---- Voice mixer
    void setVoiceFader(int voice, float value);
    float getVoiceFader(int voice) const;
    // Master mute of the mixer: a session state like the faders, not saved in presets.
    void setMasterMute(bool muted) { masterMute = muted; }
    bool isMasterMuted() const { return masterMute; }
    void setVoicePan(int voice, float pan, bool customized = true);
    float getVoicePan(int voice) const;           // centred for single-voice presets
    float getStoredVoicePan(int voice) const;
    bool isVoicePanCustomized(int voice) const;

    // ---- Arpeggiator: the settings are part 1's parameters (spArp*,
    // cpArp*), the step sequence is session state; the audio engine's arp
    // plays both and reports its notes (ArpVisualizationState).
    ArpSequence& getArpSequence() { return arpSequence; }
    const ArpSequence& getArpSequence() const { return arpSequence; }
    bool isHostSyncEnabled() const { return currentPreset.steppedParams[spArpSync] != 0; }
    float getInternalBpm() const;
    float getEffectiveBpm() const { return isHostSyncEnabled() ? hostBpm : getInternalBpm(); }
    // The host tempo as reported by the audio engine (displays only).
    void setHostBpm(float bpm) { hostBpm = std::clamp(bpm, 20.0f, 400.0f); }

    // ---- Display data reported by the audio engine
    // Master output for the spectrum (written by the audio thread).
    OutputScope& getOutputScope() { return outputScope; }
    // Editor settings (skin_config.conf): the 8-bit spectrum behind the
    // filter, envelope and LFO displays, and the filter's spectrum waterfall.
    void setRetroSpectrumShown(bool shown) { retroSpectrumShown = shown; }
    bool isRetroSpectrumShown() const { return retroSpectrumShown; }
    void setSpectrumWaterfallShown(bool shown) { spectrumWaterfallShown = shown; }
    bool isSpectrumWaterfallShown() const { return spectrumWaterfallShown; }
    // The waterfall behind the envelope and LFO curves (off by default).
    bool spectrumWaterfallCurvesShown = false;
    // Their opacity (0..1): the 8-bit spectrum in the filter curve and behind
    // the envelope/LFO curves, and the waterfall (scales its own alphas).
    float retroOpacityFilter = 0.175f;
    float retroOpacityCurves = 0.25f;
    float waterfallOpacity = 1.0f;
    // 8-bit spectrum randomness (0..1): cells drop out, accent cells brighten.
    float retroRandomness = 0.25f;
    // Meters (x 65535): six voices' share of the console bus load, then the
    // output peak left and right. The largest values since the last
    // takeMeterLevels() are kept, so no peak between two reads is lost.
    static constexpr int kMeterCount = SYNTH_VOICE_COUNT + 2;
    using MeterLevels = std::array<int, kMeterCount>;
    void addMeterLevels(const MeterLevels& levels) {
        for (int i = 0; i < kMeterCount; ++i) {
            meterPeaks[i] = std::max(meterPeaks[i], levels[i]);
            meterLatest[i] = levels[i];
        }
    }
    MeterLevels takeMeterLevels() { const auto levels = meterPeaks; meterPeaks.fill(0); return levels; }
    // Latest voice value, for activity indicators.
    int32_t getVoiceActivity(int voice) const { return voice >= 0 && voice < SYNTH_VOICE_COUNT ? meterLatest[voice] : 0; }
    // Parts (1 << part) that sounded since the last takeSoundingParts(): the
    // AFX pads light up while their sound plays.
    void addSoundingParts(uint16_t parts) { soundingParts = static_cast<uint16_t>(soundingParts | parts); }
    uint16_t takeSoundingParts() { const auto parts = soundingParts; soundingParts = 0; return parts; }
    void setArpVisualizationState(const ArpVisualizationState& state) { arpVisualizationState = state; }
    const ArpVisualizationState& getArpVisualizationState() const { return arpVisualizationState; }
    uint32_t getCurrentTick() const { return arpVisualizationState.tick; }

    // Complete state for the audio engine. Reusing the same state object
    // skips copying waves that did not change since the last capture.
    void capturePreparedState(PreparedState& state) const;

private:
    bool presetUsesSingleVoice() const;

    AfxKit afxKit;
    PresetData& currentPreset;
    WaveManager& waveManager;
    PresetManager presetManager;
    PartRoute routes[16];
    bool customRouting = false;
    uint32_t panicGeneration = 0;

    float voiceFader[SYNTH_VOICE_COUNT] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    bool masterMute = false;
    float voicePan[SYNTH_VOICE_COUNT] = { -0.70f, 0.70f, -0.35f, 0.35f, -0.10f, 0.10f };
    bool voicePanCustomized[SYNTH_VOICE_COUNT] = { false, false, false, false, false, false };

    ArpSequence arpSequence;
    float hostBpm = 120.0f;
    OutputScope outputScope;
    bool retroSpectrumShown = true;
    bool spectrumWaterfallShown = true;
    MeterLevels meterPeaks{};
    MeterLevels meterLatest{};
    uint16_t soundingParts = 0;
    ArpVisualizationState arpVisualizationState{};
};
