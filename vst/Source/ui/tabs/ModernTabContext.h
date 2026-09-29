#pragma once

#include "../components/ModernTabBar.h"
#include "../../data/SynthModel.h"
#include "../theme/ModernTheme.h"
#include "../components/ModernLookAndFeel.h"
#include <cmath>
#include <functional>
#include <memory>

class OvercyclerAudioProcessor;

// Shared, deliberately small dependency surface for all editor tabs.
// Tab modules use this context instead of depending on ModernEditorView.
struct ModernTabContext {
    SynthModel& model;
    OvercyclerAudioProcessor* processor = nullptr;
    ModernLookAndFeel& lookAndFeel;

    // Parameter writes are routed through the view so that the processor
    // (host automation, APVTS) sees them exactly like the Classic skin's.
    std::function<void(continuousParameter_t, float)> setContinuousParam;
    std::function<void(steppedParameter_t, uint8_t)> setSteppedParam;
    std::function<void(abx_t)> openWaveBrowser;
    // Re-syncs the whole editor, e.g. after a complete setup was loaded.
    std::function<void()> refreshFromEngine;
};

// Base class of every Modern skin tab. A tab owns its controls, creates them
// in setup(), mirrors the engine in updateFromEngine() and lays itself out in
// resized().
class ModernTabModule : public juce::Component {
public:
    explicit ModernTabModule(ModernTabContext& context);
    ~ModernTabModule() override = default;

    // Creates all controls. Called once by ModernEditorView after its
    // LookAndFeel is installed, because labels capture fonts on creation.
    virtual void setup() {}
    virtual void updateFromEngine() {}
    virtual void resetToDefaults() {}

    enum class KnobMode {
        Percent,
        BipolarPercent,
        PitchSemitones,     // oscillator frequency 0..999: 0 .. 64 semitones (firmware)
        FineDetuneCents,
        TuneCents,          // master tune -499..499: +-1 semitone (firmware)
        CutoffHz,
        TimeMs,
        LfoSpeedHz,
        Raw
    };

    ModernTabModule(const ModernTabModule&) = delete;
    ModernTabModule& operator=(const ModernTabModule&) = delete;

protected:
    void setContinuousParam(continuousParameter_t cp, float potVal);
    void setSteppedParam(steppedParameter_t sp, uint8_t stepVal);

    std::unique_ptr<juce::Slider> createKnob(const juce::String& name, double min, double max, double init,
                                             KnobMode mode = KnobMode::Raw, const juce::String& suffix = "");
    std::unique_ptr<juce::Label> createLabel(const juce::String& text, juce::Component& parent);
    std::unique_ptr<juce::ComboBox> createCombo();
    std::unique_ptr<juce::ToggleButton> createToggle(const juce::String& text);
    int getStandardKnobSize() const { return ComponentTokens::KnobSizes::Standard; }

    // Knob with its caption centred below: the Modern skin's standard cell.
    template <typename Knob, typename Caption>
    static void layoutKnob(const Knob& knob, const Caption& label, int x, int y, int size) {
        if (knob != nullptr) knob->setBounds(x, y, size, size);
        int labelW = (int)std::round(size * ComponentTokens::maxLabelWidthRatio);
        int labelX = x - (labelW - size) / 2;
        if (label != nullptr) label->setBounds(labelX, y + size, labelW, 16);
    }

    // Engine -> UI synchronisation never fights an ongoing user gesture.
    static void safeSetKnob(juce::Slider* s, double val);
    static void safeSetToggle(juce::Button* b, bool state);
    static void safeSetCombo(juce::ComboBox& c, int id);
    // Envelope time knob text <-> value; `slow` is the x4 envelope range.
    static juce::String formatEnvelopeTime(double potValue, bool slow);
    static double parseEnvelopeTime(const juce::String& text, bool slow);
    // Glide (time for one octave) and LFO 1 start delay, see dsp/ControlTimes.h.
    static juce::String formatGlideTime(double potValue);
    static juce::String formatModDelayTime(double potValue);

    ModernTabContext& context;
    SynthModel& model;
    OvercyclerAudioProcessor* const processor;
    ModernLookAndFeel& modernLnf;
};
