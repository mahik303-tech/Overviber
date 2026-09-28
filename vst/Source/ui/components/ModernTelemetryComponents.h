#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../dsp/SynthEngine.h"
#include "ModernLookAndFeel.h"
#include <array>
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

private:
    class ConsoleFaderLookAndFeel : public ModernLookAndFeel {
    public:
        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              juce::Slider::SliderStyle style, juce::Slider& slider) override;
    };

    SynthEngine& engine;
    float currentLevels[SYNTH_VOICE_COUNT] = { 0.0f };
    float masterPeakL = 0.0f;
    float masterPeakR = 0.0f;

    ConsoleFaderLookAndFeel faderLnf;
    std::array<std::unique_ptr<juce::Slider>, SYNTH_VOICE_COUNT> voiceFaders;
    std::array<std::unique_ptr<juce::Slider>, SYNTH_VOICE_COUNT> voicePans;
    std::unique_ptr<juce::Slider> masterFader;
    std::unique_ptr<juce::Slider> mackitySendKnob; // master strip: parallel Mackity send
    juce::TextButton mackityPadToggle{ "-6 dB" };  // on: send return 6 dB lower
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
