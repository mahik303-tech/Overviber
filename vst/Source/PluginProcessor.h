#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/SynthEngine.h"
#include "data/SynthModel.h"

class OvercyclerAudioProcessor : public juce::AudioProcessor,
                                 public juce::AudioProcessorValueTreeState::Listener,
                                 private juce::Timer {
public:
    explicit OvercyclerAudioProcessor(bool initializeUserStorage = true);
    ~OvercyclerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Overviber"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateAPVTSFromEngine();

    void setContinuousParamFromUI(continuousParameter_t cp, float potVal);
    void setSteppedParamFromUI(steppedParameter_t sp, uint8_t stepVal);
    bool checkAndResetHostParamsChanged() { return hostParamsChanged.exchange(false); }
    bool hasHostParamsChanged() const { return hostParamsChanged.load(); }

    static float potToParamVal(continuousParameter_t cp, float potVal);
    static float paramToPotVal(continuousParameter_t cp, float paramVal);

    // The editor's model; the audio engine is private to the audio thread.
    SynthModel& getModel() { return model; }
    ArpVisualizationState getArpVisualizationState() const;
    void setMidiInputChannel(int channel);
    int getMidiInputChannel() const { return midiInputChannel.load(); }
    int getDesiredContinuousParam(continuousParameter_t cp) const {
        return cp >= 0 && cp < cpCount ? desiredContinuous[cp].load() : 0;
    }
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    bool saveSetup(const juce::File& file);
    bool loadSetup(const juce::File& file);

private:
    void timerCallback() override;
    template <typename Target> void applyDesiredParameters(Target& target);
    void publishEditorState();
    void encodeSessionIfChanged(bool immediately);
    void setDesiredFromPreset(const PresetData& preset);
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleMidiCC(int cc, int val);

    SynthModel model;
    SynthEngine audioEngine;
    std::unique_ptr<PreparedStateQueue> stateQueue = std::make_unique<PreparedStateQueue>();
    std::unique_ptr<PreparedState> editorSnapshot = std::make_unique<PreparedState>();
    std::unique_ptr<PreparedState> lastPublished = std::make_unique<PreparedState>();
    bool hasPublished = false;
    static constexpr juce::uint32 kSessionSettleMs = 200;
    bool sessionDirty = false;
    juce::uint32 lastSessionChangeMs = 0;
    int publishedMidiInputChannel = -1;
    std::string publishedPresetName;
    std::array<std::atomic<int>, cpCount> desiredContinuous{};
    std::array<std::atomic<int>, spCount> desiredStepped{};
    std::array<std::atomic<int>, cpCount> midiContinuous{};
    std::array<std::atomic<int>, spCount> midiStepped{};
    std::array<std::array<std::atomic<int>, 5>, MOD_MATRIX_SLOT_COUNT> desiredMatrix{};
    std::array<std::array<juce::String, 5>, MOD_MATRIX_SLOT_COUNT> matrixIds;
    std::array<std::atomic<int>, SynthModel::kMeterCount> meterLevels{};   // console meters, see SynthModel
    std::array<std::atomic<int>, 16> arpActiveNotes{};
    std::array<std::atomic<int>, 16> arpPatternNotes{};
    std::atomic<int> arpActiveCount{0};
    std::atomic<int> arpCurrentStep{0};
    std::atomic<uint32_t> arpTick{0};
    std::atomic<bool> arpGateActive{false};
    std::atomic<int> requestedProgram{-1};
    std::atomic<PreparedState*> restoredState{nullptr}, retiredState{nullptr};
    juce::CriticalSection savedStateLock;
    juce::String savedState, editorRestore;
    juce::MidiBuffer outputMidi;
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> currentProgram;
    std::atomic<bool> isUpdatingAPVTS{false};
    std::atomic<bool> hostParamsChanged{false};
    std::atomic<int> midiInputChannel{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OvercyclerAudioProcessor)
};
