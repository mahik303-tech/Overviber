#pragma once
#include <cmath>

// Q of a Shelves mid band for its knob (0..999), as the Shelves engine
// maps it: 0.5 .. 40, exponential (kQKnobMin/Max in audible/shelves.hpp).
inline float shelvesQ(float pot) { return 0.5f * std::pow(80.0f, pot / 999.0f); }
inline float shelvesQPot(float q) { return 999.0f * std::log(2.0f * q) / std::log(80.0f); }
// Default Q of both mid bands: 1.0.
constexpr float kShelvesDefaultQPot = 158.0f;

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../data/SynthModel.h"
#include "ModernLookAndFeel.h"
#include "RetroSpectrum.h"
#include "SpectrumWaterfall.h"
#include <array>
#include <functional>
#include <vector>

// ==============================================================================
// Filter Frequency Response Curve & Interactive Editor
// ==============================================================================
class FilterCurveComponent : public juce::Component, private juce::Slider::Listener, private juce::Timer {
public:
    FilterCurveComponent(SynthModel& eng, juce::Slider& cutoffKnob, juce::Slider& resoKnob);
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
    void timerCallback() override;

    // A Shelves band as drawn: frequency pot (0..999) and Hz, gain in dB,
    // Q (mid bands).
    struct EqBandView { float freqPot, hz, gainDb, q; };
    std::array<EqBandView, 4> eqBandViews() const;
    static float filterMagnitude(int model, int mode, bool liquid, float resoPot, float Q, float r);
    const std::vector<float>& responseDb(int width, bool shelvesEq, const std::array<EqBandView, 4>& bands);
    void paintHeader(juce::Graphics& g, juce::Rectangle<float> bounds, const ModernTheme& theme,
                     ModernLookAndFeel* lnf, const juce::String& badge);
    void paintGrid(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Rectangle<float> disp,
                   const ModernTheme& theme, ModernLookAndFeel* lnf, bool shelvesEq, juce::Colour curveColour);
    void paintEqHandles(juce::Graphics& g, juce::Rectangle<float> disp, const ModernTheme& theme,
                        ModernLookAndFeel* lnf, const std::array<EqBandView, 4>& bands, juce::Colour curveColour);
    void paintFilterHandle(juce::Graphics& g, juce::Rectangle<float> disp, const ModernTheme& theme, juce::Colour curveColour);
    juce::String telemetryText(bool shelvesEq, const std::array<EqBandView, 4>& bands) const;

    SynthModel& model;
    juce::Slider& cutoff;
    juce::Slider& reso;
    int activeBand = 1; // 0=Low Shelf, 1=Mid Low, 2=Mid High, 3=High Shelf
    RetroSpectrum spectrum{ model.getOutputScope() };   // master output, behind the curve

    // Above the 8-bit spectrum: a line waterfall of the same output.
    SpectrumWaterfall waterfall{ model.getOutputScope(), RetroSpectrum::kUpdateHz };
    int draggedNode = -1;
    // The last response (dB per x) and the settings it was computed for.
    std::array<float, 17> responseKey{};
    std::vector<float> response;
};
