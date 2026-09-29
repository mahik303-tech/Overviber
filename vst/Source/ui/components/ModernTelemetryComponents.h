#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../data/SynthModel.h"
#include "ModernLookAndFeel.h"
#include <array>
#include <functional>
#include <memory>

// ==============================================================================
// Voice console mixer: per voice its share of the console bus load in dB
// relative to the console's knee; on the master strip the output, with +2 dB
// (red) where the output ceiling starts.
// ==============================================================================
class ModernVoiceMeterPanel : public juce::Component {
public:
    explicit ModernVoiceMeterPanel(SynthModel& eng);
    ~ModernVoiceMeterPanel() override;

    // Peaks since the last call (SynthModel::takeMeterLevels()).
    void updateLevels(const SynthModel::MeterLevels& peaks);

    // One scale for meters and voice faders, so a fader at 0 dB sits on the
    // meters' 0 (the console knee; for the master the output reference):
    //   -60 .. -6 dB   0 .. 22 %  linear
    //    -6 ..  0 dB  22 .. 50 %  degressive (height ~ t^0.8)
    //     0 .. +2 dB  50 .. 58 %  the knee, white
    //    +2 .. +12 dB 58 .. 100 % red: over on the meters, the faders' way
    //                             into the console's saturation
    static constexpr float kMeterFloorDb = -60.0f;
    static constexpr float kScaleZoneDb = -6.0f, kScaleOverDb = 2.0f, kScaleMaxDb = 12.0f;
    static constexpr float kScaleZonePos = 0.22f, kScaleUnityPos = 0.50f, kScaleOverPos = 0.58f;
    static constexpr float kScaleDegression = 0.8f;
    static float scalePosition(float db);
    static float scaleDb(float position);

    // Voice fader: position 0..1 <-> linear gain on that scale; 0 dB (unity,
    // the default) at half height, the bottom mutes.
    static constexpr float kFaderUnityPosition = kScaleUnityPos;
    static float faderGain(double position);
    static double faderPosition(float gain);
    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::Slider* getVoiceFader(int v) { return (v >= 0 && v < SYNTH_VOICE_COUNT) ? voiceFaders[v].get() : nullptr; }
    juce::Slider* getVoicePan(int v) { return (v >= 0 && v < SYNTH_VOICE_COUNT) ? voicePans[v].get() : nullptr; }
    juce::TextButton& getMasterMute() { return masterMuteButton; }
    juce::Slider* getMackitySend() { return mackitySendKnob.get(); }

    // Footer row below the channel strips: the owner places its own controls
    // in getFooterControlArea() under this caption.
    void setFooterCaption(const juce::String& caption) { footerCaption = caption; repaint(); }
    juce::Rectangle<int> getFooterControlArea() const;

    // Preset parameters go through the owner (processor/APVTS) when set, so
    // host state does not overwrite them; without them the engine is written.
    std::function<void(continuousParameter_t, float)> onContinuousParam;
    std::function<void(steppedParameter_t, uint8_t)> onSteppedParam;

private:
    class ConsoleFaderLookAndFeel : public ModernLookAndFeel {
    public:
        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              juce::Slider::SliderStyle style, juce::Slider& slider) override;
    };

    // Shared by resized() and paint() so controls and drawing stay aligned.
    struct StripGeometry {
        float startY, usableH, marginX, stripW;
        float knobSize, knobY;     // pan / send encoder
        float readoutY;            // value text under the encoder
        float faderTop, faderH;
        float masterX() const { return marginX + (float)SYNTH_VOICE_COUNT * stripW; }
    };
    StripGeometry getStripGeometry() const;

    // A voice strip's two columns, centred as a pair: the meter with its
    // scale labels, then the fader.
    struct VoiceColumns { float meterX, meterW, faderX, faderW; };
    static constexpr float kVoiceMeterW = 7.0f, kScaleLabelW = 16.0f, kColumnGap = 4.0f;
    VoiceColumns voiceColumns(const StripGeometry& geo, float stripX) const;

    static constexpr float kFooterH = 40.0f;   // footer row: divider + controls
    static constexpr int kFooterRowH = 18;     // height of the footer controls
    static constexpr int kMasterButtonH = 14;   // PAD and MUTE under the master meters (EQ band button height)

    void writeContinuous(continuousParameter_t cp, float potValue);
    void writeStepped(steppedParameter_t sp, uint8_t value);

    SynthModel& model;
    // Displayed level and peak hold per meter (six voices, bus L, bus R), dB.
    std::array<float, SynthModel::kMeterCount> shownDb{};
    std::array<float, SynthModel::kMeterCount> holdDb{};
    std::array<double, SynthModel::kMeterCount> holdUntilMs{};
    double lastUpdateMs = 0.0;

    ConsoleFaderLookAndFeel faderLnf;
    std::array<std::unique_ptr<juce::Slider>, SYNTH_VOICE_COUNT> voiceFaders;
    std::array<std::unique_ptr<juce::Slider>, SYNTH_VOICE_COUNT> voicePans;
    juce::TextButton masterMuteButton{ "MUTE" };   // master strip, under the meters
    std::unique_ptr<juce::Slider> mackitySendKnob; // master strip: parallel Mackity send
    juce::TextButton mackityPadToggle{ "PAD" };    // on: Mackity send return 6 dB lower
    juce::String footerCaption;
};

// ==============================================================================
// LFO Real-time Waveform Preview Component
// ==============================================================================
class LfoWavePreviewComponent : public juce::Component {
public:
    LfoWavePreviewComponent(SynthModel& eng, int lfoIndex);
    ~LfoWavePreviewComponent() override = default;

    void setShape(int shapeIndex);
    void setPhase(float phase);
    void paint(juce::Graphics& g) override;

private:
    SynthModel& model;
    int lfoNum = 1;
    int currentShape = 0;
    float currentPhase = 0.0f;
};

// ==============================================================================
// Real-time Tempo-Synchronous Arpeggiator Display & Playback Matrix
// ==============================================================================
class ArpVisualizerComponent : public juce::Component, private juce::Timer {
public:
    explicit ArpVisualizerComponent(SynthModel& eng);
    ~ArpVisualizerComponent() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override { repaint(); }

    SynthModel& model;
};
