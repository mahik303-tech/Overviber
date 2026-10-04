#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include "../components/AdsrCurveComponent.h"
#include "../components/ModernChoiceButtons.h"
#include "../../dsp/VoiceConfig.h"
#include <array>

// ENV tab: the filter, amplifier and wavemod envelopes with their
// interactive ADSR curves, each curve's type and loop below it. The three
// envelopes are the same section, described by an EnvelopeDescriptor.
class EnvelopeTab : public ModernTabModule {
public:
    explicit EnvelopeTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

private:
    // What differs between the three envelopes.
    struct EnvelopeDescriptor {
        const char* idPrefix;     // component IDs: "<prefix>AttKnob", ...
        const char* knobPrefix;   // knob names: "<prefix>Atk", ...
        const char* curveTitle;
        voiceconfig::EnvelopeParams params;
        std::array<int, 4> defaults;   // attack, decay, sustain, release (pot)
    };
    static const std::array<EnvelopeDescriptor, 3> kEnvelopes;   // filter, amp, wavemod

    // One envelope's controls, children of the tab.
    struct EnvelopeSection {
        static constexpr int kVelocity = 4;
        std::array<std::unique_ptr<juce::Slider>, 5> knobs;   // attack, decay, sustain, release, velocity
        std::array<std::unique_ptr<juce::Label>, 5> labels;
        std::unique_ptr<ModernChoiceButtons> typeChoice;   // index: slow + 2 * linear
        std::unique_ptr<juce::ToggleButton> loopToggle;
        std::unique_ptr<AdsrCurveComponent> curve;   // references the knobs
    };

    void createControls(EnvelopeSection& section, const EnvelopeDescriptor& d);
    void createCurve(EnvelopeSection& section, const EnvelopeDescriptor& d);
    void assignComponentIDs(EnvelopeSection& section, const EnvelopeDescriptor& d);
    void updateSection(EnvelopeSection& section, const EnvelopeDescriptor& d);
    void layoutKnobs(EnvelopeSection& section, int startX, int adsrAreaW, int sepX, int velColW);
    static void layoutCurveControls(EnvelopeSection& section, int startX, int cardW, int startY);
    void assignComponentIDs();

    ModernSectionCard filEnvCard{"FILTER ENV", "VCF"};
    ModernSectionCard ampEnvCard{"AMP ENV", "VCA"};
    ModernSectionCard wmodEnvCard{"WAVEMOD ENV", "SHAPER"};

    std::array<EnvelopeSection, 3> sections;   // as kEnvelopes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeTab)
};
