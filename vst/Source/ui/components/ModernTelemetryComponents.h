#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../dsp/SynthEngine.h"
#include "ModernLookAndFeel.h"
#include <array>
#include <functional>
#include <memory>

// ==============================================================================
// Hardware 6-Voice Activity & LM13700 VCA Gain Meter Panel
// ==============================================================================
class ModernVoiceMeterPanel : public juce::Component {
public:
    explicit ModernVoiceMeterPanel(SynthEngine& eng);
    ~ModernVoiceMeterPanel() override;

    void updateLevels(const float* levels);
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

    SynthEngine& engine;
    float currentLevels[SYNTH_VOICE_COUNT] = { 0.0f };
    float masterPeakL = 0.0f;
    float masterPeakR = 0.0f;

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
    LfoWavePreviewComponent(SynthEngine& eng, int lfoIndex);
    ~LfoWavePreviewComponent() override = default;

    void setShape(int shapeIndex);
    void setPhase(float phase);
    void paint(juce::Graphics& g) override;

private:
    SynthEngine& engine;
    int lfoNum = 1;
    int currentShape = 0;
    float currentPhase = 0.0f;
};

// ==============================================================================
// Real-time Tempo-Synchronous Arpeggiator Display & Playback Matrix
// ==============================================================================
class ArpVisualizerComponent : public juce::Component, private juce::Timer {
public:
    explicit ArpVisualizerComponent(SynthEngine& eng);
    ~ArpVisualizerComponent() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override { repaint(); }

    SynthEngine& engine;
};
