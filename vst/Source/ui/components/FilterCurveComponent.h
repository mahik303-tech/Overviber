#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../dsp/SynthEngine.h"
#include "ModernLookAndFeel.h"
#include <functional>

// ==============================================================================
// Filter Frequency Response Curve & Interactive Editor
// ==============================================================================
class FilterCurveComponent : public juce::Component, private juce::Slider::Listener {
public:
    FilterCurveComponent(SynthEngine& eng, juce::Slider& cutoffKnob, juce::Slider& resoKnob);
    ~FilterCurveComponent() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void setActiveBand(int band) { activeBand = std::clamp(band, 0, 3); repaint(); }
    int getActiveBand() const { return activeBand; }

    std::function<void(int band)> onBandSelected;
    std::function<void(int band, float potFreq, float potGain)> onBandParamChanged;
    std::function<void(int band, float deltaQ)> onBandQChanged;

private:
    void sliderValueChanged(juce::Slider*) override { repaint(); }

    SynthEngine& engine;
    juce::Slider& cutoff;
    juce::Slider& reso;
    int activeBand = 1; // 0=Low Shelf, 1=Mid 1, 2=Mid 2, 3=High Shelf
    int draggedNode = -1;
};
