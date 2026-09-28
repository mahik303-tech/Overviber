#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../data/SynthModel.h"
#include "ModernLookAndFeel.h"

// ==============================================================================
// Interactive ADSR Envelope Curve Visualizer
// ==============================================================================
class AdsrCurveComponent : public juce::Component, private juce::Slider::Listener {
public:
    AdsrCurveComponent(SynthModel& eng,
                       juce::Slider& attKnob, juce::Slider& decKnob,
                       juce::Slider& susKnob, juce::Slider& relKnob,
                       const juce::String& titleText);
    ~AdsrCurveComponent() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void sliderValueChanged(juce::Slider*) override { repaint(); }

    SynthModel& model;
    juce::Slider& att;
    juce::Slider& dec;
    juce::Slider& sus;
    juce::Slider& rel;
    juce::String title;
    int activeHandle = 0; // 0=none, 1=attack, 2=decay/sustain, 3=release
};
