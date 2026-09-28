#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include "../components/AdsrCurveComponent.h"

// ENV tab: amplifier and wavemod envelopes, the filter envelope's ADSR and
// the interactive ADSR curve previews.
class EnvelopeTab : public ModernTabModule {
public:
    explicit EnvelopeTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

private:
    void assignComponentIDs();

    ModernSectionCard filEnvCard{"FILTER ENV", "VCF"};
    ModernSectionCard ampEnvCard{"AMP ENV", "VCA"};
    ModernSectionCard wmodEnvCard{"WAVEMOD ENV", "SHAPER"};

    std::unique_ptr<juce::Slider> filAttKnob, filDecKnob, filSusKnob, filRelKnob, filVelKnob;
    std::unique_ptr<juce::Label> filAttLabel, filDecLabel, filSusLabel, filRelLabel, filVelLabel;

    std::unique_ptr<juce::Slider> ampAttKnob, ampDecKnob, ampSusKnob, ampRelKnob, ampVelKnob;
    std::unique_ptr<juce::ToggleButton> ampEnvTypeToggles[4];
    std::unique_ptr<juce::ToggleButton> ampEnvLoopToggle;
    std::unique_ptr<juce::Label> ampAttLabel, ampDecLabel, ampSusLabel, ampRelLabel, ampVelLabel;

    std::unique_ptr<juce::Slider> wmodAttKnob, wmodDecKnob, wmodSusKnob, wmodRelKnob, wmodVelKnob;
    std::unique_ptr<juce::ToggleButton> wmodEnvTypeToggles[4];
    std::unique_ptr<juce::ToggleButton> wmodEnvLoopToggle;
    std::unique_ptr<juce::Label> wmodAttLabel, wmodDecLabel, wmodSusLabel, wmodRelLabel, wmodVelLabel;

    // Interactive ADSR curves (reference the knobs above, so declared after them)
    std::unique_ptr<AdsrCurveComponent> filAdsrCurve;
    std::unique_ptr<AdsrCurveComponent> ampAdsrCurve;
    std::unique_ptr<AdsrCurveComponent> wmodAdsrCurve;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeTab)
};
