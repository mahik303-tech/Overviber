#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include "../components/ModernTelemetryComponents.h"
#include <array>

// LFO / ARP tab: both LFOs with their wave previews and the arpeggiator.
// The two LFOs are the same section, described by an LfoDescriptor.
class LfoArpTab : public ModernTabModule {
public:
    explicit LfoArpTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

    // Advances the LFO preview traces by one editor timer tick (30 Hz).
    void advancePreviewAnimation();

private:
    static constexpr int kDepths = 5;   // pitch, wavemod, cutoff, reso, volume

    // What differs between the two LFOs.
    struct LfoDescriptor {
        int number;   // 1, 2: component IDs "lfo<n>...", knob names "<n>..."
        continuousParameter_t speed, amount;
        std::array<continuousParameter_t, kDepths> depths;
        steppedParameter_t shape, speedRange, targets, trigger;
        bool hasStartDelay;   // cpModDelay: on LFO 1's panel only, as in the firmware
    };
    static const std::array<LfoDescriptor, 2> kLfos;

    // One LFO's controls, children of the tab.
    struct LfoSection {
        std::unique_ptr<juce::Slider> speedKnob, amountKnob, delayKnob;
        std::unique_ptr<juce::Label> speedLabel, amountLabel, delayLabel;
        std::array<std::unique_ptr<juce::Slider>, kDepths> depthKnobs;
        std::array<std::unique_ptr<juce::Label>, kDepths> depthLabels;
        juce::ComboBox shapeCombo, speedCombo, targetsCombo, triggerCombo;
        std::unique_ptr<LfoWavePreviewComponent> preview;
        float phase = 0.0f;   // preview animation, in cycles
    };

    void createLfo(LfoSection& lfo, const LfoDescriptor& d);
    void createArp();
    void assignComponentIDs();
    void assignComponentIDs(LfoSection& lfo, const LfoDescriptor& d);
    void updateLfo(LfoSection& lfo, const LfoDescriptor& d);
    void layoutLfo(LfoSection& lfo, const LfoDescriptor& d, int x, int colW);
    void layoutArp(int x, int colW);

    ModernSectionCard lfo1Card{"LFO 1", "MAIN"};
    ModernSectionCard lfo2Card{"LFO 2", "AUX / VIB"};
    ModernSectionCard arpCard{"ARPEGGIATOR", "PLAYBACK"};

    std::array<LfoSection, 2> lfos;   // as kLfos

    juce::ComboBox arpModeCombo, arpOctaveCombo, arpRateCombo;
    std::unique_ptr<juce::ToggleButton> arpHoldToggle, arpSyncToggle;
    std::unique_ptr<juce::Slider> arpGateKnob, arpSwingKnob, arpBpmKnob;
    std::unique_ptr<juce::Label> arpGateLabel, arpSwingLabel, arpBpmLabel;
    std::unique_ptr<ArpVisualizerComponent> arpVisualizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoArpTab)
};
