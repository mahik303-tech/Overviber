#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../data/SynthModel.h"
#include "ModernLookAndFeel.h"
#include <array>
#include <functional>
#include <memory>

// ==============================================================================
// Voice console mixer: per voice its share of the console bus load, on the
// master strip the bus load itself, in dB relative to the console's knee.
// ==============================================================================
class ModernVoiceMeterPanel : public juce::Component {
public:
    explicit ModernVoiceMeterPanel(SynthModel& eng);
    ~ModernVoiceMeterPanel() override;

    // Peaks since the last call (SynthModel::takeMeterLevels()).
    void updateLevels(const SynthModel::MeterLevels& peaks);

    // Meter scale: dB relative to the console knee (MasterBus::kConsoleKnee).
    // -60 .. -6 dB fill the lower 40 % linearly; -6 .. +2 dB, where the
    // console saturates, rise degressively (height ~ t^0.8, each dB takes a
    // little less); at +2 dB the bus is at its ceiling and the top segment
    // turns red.
    static constexpr float kMeterFloorDb = -60.0f;
    static constexpr float kMeterZoneDb = -6.0f;
    static constexpr float kMeterOverDb = 2.0f;
    static constexpr float kMeterZonePosition = 0.4f;
    static constexpr float kMeterDegression = 0.8f;
    static float meterPosition(float db);
    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::Slider* getVoiceFader(int v) { return (v >= 0 && v < SYNTH_VOICE_COUNT) ? voiceFaders[v].get() : nullptr; }
    juce::Slider* getVoicePan(int v) { return (v >= 0 && v < SYNTH_VOICE_COUNT) ? voicePans[v].get() : nullptr; }
    juce::Slider* getMasterFader() { return masterFader.get(); }
    juce::Slider* getMackitySend() { return mackitySendKnob.get(); }

    // Footer row below the channel strips: the owner places its own controls
    // in getFooterControlArea() (left of the PAD toggle) under this caption.
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

    static constexpr float kFooterH = 40.0f;   // footer row: divider + controls
    static constexpr int kFooterRowH = 18;     // height of the footer controls
    static constexpr int kPadToggleW = 60;

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
    std::unique_ptr<juce::Slider> masterFader;
    std::unique_ptr<juce::Slider> mackitySendKnob; // master strip: parallel Mackity send
    juce::ToggleButton mackityPadToggle{ "PAD" };  // on: Mackity send return 6 dB lower
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
