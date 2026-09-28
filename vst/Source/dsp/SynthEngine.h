#pragma once

#include "OvercyclerTypes.h"
#include "Voice.h"
#include "Modulation.h"
#include "MidiInput.h"
#include "VoiceAllocator.h"
#include "lfo.h"
#include "assigner.h"
#include "arp.h"
#include "ConsoleXProcessor.h"
#include "MackityProcessor.h"
#include "AfxKit.h"
#include "../data/WaveManager.h"
#include "../data/PresetManager.h"
#include <vector>
#include <memory>
#include "PreparedState.h"
#include "FixedBuffer.h"

// ==============================================================================
// GliGli Overcycler - Core Sound Engine & Polyphonic DSP Synthesizer
// ==============================================================================
//
// Complete Audio Signal Flow Architecture:
//
//   [MIDI / Keyboard / Arpeggiator / Voice Assigner]
//                          |
//                          v (6 Polyphonic Hardware Voices)
//   +--------------------------------------------------------------+
//   | Voice Module (v = 0 .. 5):                                  |
//   |                                                              |
//   |   +---------------+   +---------------+   +--------------+   |
//   |   | Wavetable A   |   | Wavetable B   |   | LFSR Noise   |   |
//   |   | (Pitch/WaveMod)   | (Sync/Detune) |   | Generator    |   |
//   |   +-------+-------+   +-------+-------+   +------+-------+   |
//   |           |                   |                  |           |
//   |           +---------+---------+------------------+           |
//   |                     | Mixer (OscA + OscB + Noise)            |
//   |                     v                                        |
//   |   +------------------------------------------------------+   |
//   |   | Voice Filter (VCF):                                  |   |
//   |   |   - SSI2144 (24dB 4-Pole Ladder ZDF)                 |   |
//   |   |   - Liquid Ripples (Mutable Instruments OTA 24dB)    |   |
//   |   |   - Shelves EQ (4-Band Parametric / 12dB SVF)        |   |
//   |   +--------------------------+---------------------------+   |
//   |                              v                               |
//   |   +------------------------------------------------------+   |
//   |   | Voice VCA: LM13700 OTA Soft Saturation & Env Gain    |   |
//   |   +--------------------------+---------------------------+   |
//   +------------------------------|-------------------------------+
//                                  v
//   +--------------------------------------------------------------+
//   | Master Voice Summer & Stereo Panning Bus (Fixed Hardware Pan)|
//   +------------------------------+-------------------------------+
//                                  v
//   +--------------------------------------------------------------+
//   | Master AMP Level & Overall Volume Pot (cpAmpLevel)           |
//   +------------------------------+-------------------------------+
//                                  v
//   +--------------------------------------------------------------+
//   | Airwindows ConsoleX master decode, then a parallel send:     |
//   | Airwindows Mackity saturation added on top (cpMackitySend,   |
//   | default 0 = off; drive from cpMackityDrive)                  |
//   +------------------------------+-------------------------------+
//                                  v
//   +--------------------------------------------------------------+
//   | DAW Audio Output Buffer (Stereo Left / Right)                |
//   +--------------------------------------------------------------+
// ==============================================================================
struct MidiOutEvent {
    uint8_t note = 0;
    uint8_t velocity = 0;
    uint8_t channel = 1;
    bool isNoteOn = false;
    int sampleOffset = 0;
};

struct ArpVisualizationState {
    bool valid = false;
    uint32_t tick = 0;
    int currentStep = 0;
    bool gateActive = false;
    int activeCount = 0;
    std::array<uint8_t, 16> activeNotes{};
    std::array<uint8_t, 16> patternNotes{};
};

class SynthEngine {
public:
    SynthEngine();
    ~SynthEngine() = default;

    void prepare(float sampleRate);
    void reset();

    // Advanced MIDI Handlers (Standard MIDI, Polyphonic Aftertouch, MPE, MIDI 2.0 / VST3 Note Expressions)
    void noteOn(uint8_t note, uint16_t velocity, uint8_t channel = 1);
    void noteOff(uint8_t note, uint16_t velocity, uint8_t channel = 1);
    void pitchBend(int16_t bend, uint8_t channel = 1);        // -8192 .. +8191
    void modWheel(uint16_t mod, uint8_t channel = 1);          // 0 .. 65535
    void channelPressure(uint16_t press, uint8_t channel = 1); // Channel Aftertouch
    void polyAftertouch(uint8_t note, uint16_t press, uint8_t channel = 1); // Polyphonic Aftertouch
    void timbreSlide(uint16_t timbre, uint8_t channel = 1);    // CC 74 / Y-axis / Slide
    void breathController(uint16_t breath, uint8_t channel = 1); // CC 2
    void expressionController(uint16_t expr, uint8_t channel = 1); // CC 11
    void controlChange(uint8_t ccNumber, uint8_t value, uint8_t channel = 1);
    void aftertouch(uint16_t press) { channelPressure(press, 1); } // Legacy fallback
    void holdPedal(bool down);
    void allNotesOff();
    void panic();
    void refreshOscWaves();

    // Modulation Matrix Configuration
    void setMatrixSlot(int slotIndex, modSource_t src, modDest_t dest, modSource_t via, int16_t depth, bool enabled = true);
    float evaluateModSource(int8_t voiceIndex, uint8_t src) const;

    // Preset & Wave Access
    WaveManager& getWaveManager() { return waveManager; }
    PresetManager& getPresetManager() { return presetManager; }
    PresetData& getCurrentPreset() { return currentPreset; }

    void loadPreset(int presetIndex);
    void applyPreset();
    void applyControls();
    void capturePreparedState(PreparedState& state) const;
    void applyPreparedState(const PreparedState& state, bool preserveMainParameters = false);
    PartRoute& getPartRoute(int index) { return allocator.route(index); }
    bool usesCustomRouting() const { return allocator.usesCustomRouting(); }
    void setDisplayLevels(const std::array<int, 6>& levels) { displayLevels = levels; useDisplayLevels = true; }
    void setCustomRouting(bool enabled) { allocator.setCustomRouting(enabled); }
    void setEventOffset(int offset) { currentSampleOffset = offset; }
    void setContinuousParam(continuousParameter_t cp, uint16_t value);
    void setSteppedParam(steppedParameter_t sp, uint8_t value);

    // Audio Rendering
    void beginVoiceMeterBlock() { voiceMeterPeaks.fill(0.0f); }
    void renderBlock(float* leftOut, float* rightOut, int numSamples, int hostOffset = 0);
#ifdef OVERVIBER_DIAGNOSTICS
    void setDiagnostics(RenderDiagnostics* value) {
        diagnostics = value;
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
            voices[v].diagnostics = value ? &value->voices[v] : nullptr;
    }
#endif

    // Visualisation / State Query
    int32_t getVoiceAmpLevel(int voiceIndex);
    int32_t getVoicePeakLevel(int voiceIndex) const;
    uint32_t getCurrentTick() const { return currentTick; }
    void setHostBpm(float bpm) { hostBpm = std::clamp(bpm, 20.0f, 400.0f); }
    void setHostTransport(double ppqPosition, bool playing);
    float getHostBpm() const { return hostBpm; }
    void setInternalBpm(float bpm) { internalBpm = std::clamp(bpm, 20.0f, 300.0f); }
    float getInternalBpm() const { return internalBpm; }
    void setHostSyncEnabled(bool sync) { hostSyncEnabled = sync; }
    bool isHostSyncEnabled() const { return hostSyncEnabled; }
    bool isMpeMemberChannel(uint8_t channel) const;
    float getEffectiveBpm() const { return hostSyncEnabled ? hostBpm : internalBpm; }
    // LFOs of the main part (part 1), as shown by the editor.
    const LfoModule& getLfo(int idx) const { return partLfos[0][idx & 1]; }
    Arpeggiator& getArpeggiator() { return arpeggiator; }
    const ArpVisualizationState& getArpVisualizationState() const { return arpVisualizationState; }
    void setArpVisualizationState(const ArpVisualizationState& state) { arpVisualizationState = state; }
    uint16_t getOscANoteCV(int v) const { return allocator.oscANote(v); }
    uint16_t getFilterNoteCV(int v) const { return (v >= 0 && v < SYNTH_VOICE_COUNT) ? allocator.filterNote(v) : 0; }
    bool isVoiceActive(int v) const { return v >= 0 && v < SYNTH_VOICE_COUNT && voices[v].isActive(); }
    bool hasDirectKeysPressed() { return assigner.getAnyPressed() != 0; }
    int findVoiceByNote(uint8_t note) const { return assigner.getVoiceByNote(note); }
    int findVoiceByChannel(uint8_t channel) const { return assigner.getVoiceByChannel(channel); }
    const VoiceExpressionState* getVoiceExpressionState(int voice) const {
        return voice >= 0 && voice < SYNTH_VOICE_COUNT ? &midiInput.voice(voice) : nullptr;
    }
    uint16_t getGlobalPressure() const { return midiInput.getPressure(); }
    uint16_t getGlobalModWheel() const { return midiInput.getModWheel(); }
    uint16_t getGlobalTimbre() const { return midiInput.getTimbre(); }
    int16_t getGlobalPitchBend() const { return midiInput.getPitchBend(); }
    uint16_t getOscATargetCV(int v) const { return allocator.oscATarget(v); }
    int16_t getGlideAmount() const { return exponentialCourse(currentPreset.continuousParams[cpGlide], 11000.0f, 2100.0f); }
    int8_t getGliding() const { return getGlideAmount() < 2000; }
    MackityProcessor& getMackity() { return mackity; }
    ConsoleXProcessor& getConsoleX() { return consoleX; }
    const ConsoleXProcessor& getConsoleX() const { return consoleX; }
    void setVoiceFader(int voiceIndex, float faderVal) {
        if (voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT) {
            voiceFader[voiceIndex] = std::clamp(faderVal, 0.0f, 2.0f);
        }
    }
    float getVoiceFader(int voiceIndex) const {
        if (voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT) {
            return voiceFader[voiceIndex];
        }
        return 1.0f;
    }
    void setVoicePan(int voiceIndex, float pan, bool customized = true) {
        if (voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT) {
            voicePan[voiceIndex] = std::clamp(pan, -1.0f, 1.0f);
            voicePanCustomized[voiceIndex] = customized;
        }
    }
    float getVoicePan(int voiceIndex) const {
        if (voiceIndex < 0 || voiceIndex >= SYNTH_VOICE_COUNT) return 0.0f;
        if (!voicePanCustomized[voiceIndex] && presetUsesSingleVoice()) return 0.0f;
        return voicePan[voiceIndex];
    }
    float getStoredVoicePan(int voiceIndex) const {
        return (voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT) ? voicePan[voiceIndex] : 0.0f;
    }
    bool isVoicePanCustomized(int voiceIndex) const {
        return voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT && voicePanCustomized[voiceIndex];
    }
    AfxKit& getAfxKit() { return afxKit; }
    const AfxKit& getAfxKit() const { return afxKit; }
    void pullPendingMidiOut(std::vector<MidiOutEvent>& outEvents);
    const FixedBuffer<MidiOutEvent, 4096>& getPendingMidiOut() const { return pendingMidiOut; }
    void clearPendingMidiOut() { pendingMidiOut.clear(); midiOverflow = false; }
    bool hasMidiOverflow() const { return midiOverflow; }
    uint32_t getPanicGeneration() const { return panicGeneration; }

private:
    bool presetUsesSingleVoice() const {
        if (currentPreset.steppedParams[spVoiceCount] == 0) return true;
        if (currentPreset.steppedParams[spUnison] == 0) return false;
        int patternNotes = 0;
        while (patternNotes < SYNTH_VOICE_COUNT
               && currentPreset.voicePattern[patternNotes] != ASSIGNER_NO_NOTE) ++patternNotes;
        return patternNotes == 1;
    }
    bool followsMainPart(int voice) const { return allocator.followsMainPart(voice); }
    template <typename Fn> void forEachMainPartVoice(Fn&& fn) {
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
            if (followsMainPart(v)) fn(voices[v]);
    }
    // The preset of the part a voice plays; the main preset before assignment.
    const PresetData& voicePreset(int voice) const {
        const int part = allocator.part(voice);
        return part >= 0 ? afxKit.getSlot(part).preset : currentPreset;
    }
    void configureVoicePart(int voice, uint8_t slotIdx, uint16_t velocity);
    int voiceLfoPart(int voice) const { return std::max(0, static_cast<int>(allocator.part(voice))); }
    void applyMasterBusParameters();
#ifdef OVERVIBER_DIAGNOSTICS
    RenderDiagnostics* diagnostics = nullptr;
#endif
    ModulationInputs modulationInputs(int voice) const;
    void updateCVs();
    void updateSingleVoice(int8_t v, bool advanceEnv);
    void tickTimerEvent(uint8_t phase);
    void assignerEvent(uint8_t note, int8_t gate, int8_t voice, uint16_t velocity, uint8_t flags);

    float sampleRate;
    float hostBpm = 120.0f;
    float internalBpm = 120.0f;
    bool hostSyncEnabled = true;
    bool hostTransportAvailable = false;
    bool hostTransportPlaying = true;
    uint32_t tickStep; // Clock step corresponding to sampleRate

    Voice voices[SYNTH_VOICE_COUNT];
    // Two LFOs per part. A part's LFOs run freely from its first note on;
    // part 1 always runs.
    std::array<std::array<LfoModule, 2>, 16> partLfos;
    uint16_t lfoPartsRunning = 1;
    void configurePartLfos(int part);
    VoiceAssigner assigner;
    Arpeggiator arpeggiator;
    AfxKit afxKit;
    WaveManager& waveManager;
    PresetManager presetManager;
    PresetData& currentPreset;
    MackityProcessor mackity;
    ConsoleXProcessor consoleX;
    // Mackity send return at full send: -6 dB, or -12 dB with the pad.
    static constexpr float kMackityReturnGain = 0.5f;
    static constexpr float kMackityReturnPadGain = 0.25f;
    float mackitySendLevel = 0.0f; // smoothed send amount, 0..1
    FixedBuffer<MidiOutEvent, 4096> pendingMidiOut;
    bool midiOverflow = false;
    bool useDisplayLevels = false;
    std::array<int, 6> displayLevels{};
    ArpVisualizationState arpVisualizationState{};
    std::array<float, SYNTH_VOICE_COUNT> voiceMeterPeaks{};
    uint32_t panicGeneration = 0;
    int currentSampleOffset = 0;
    float lastOutputLeft = 0.0f;
    float lastOutputRight = 0.0f;
    float presetTransitionLeft = 0.0f;
    float presetTransitionRight = 0.0f;
    int presetTransitionSamples = 0;
    int presetTransitionRemaining = 0;

    uint32_t currentTick;
    float cvSubSampleCounter;
    float tickSubSampleCounter;
    uint32_t arpGateCloseTick = UINT32_MAX;

    MidiInput midiInput;
    VoiceAllocator allocator;

    float voiceFader[SYNTH_VOICE_COUNT] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float voicePan[SYNTH_VOICE_COUNT] = { -0.70f, 0.70f, -0.35f, 0.35f, -0.10f, 0.10f };
    bool voicePanCustomized[SYNTH_VOICE_COUNT] = { false, false, false, false, false, false };

    // Thread confined: the editor and audio processor own separate engines.
};
