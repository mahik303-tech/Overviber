#pragma once

#include "ModernTabContext.h"
#include "../components/ModernChoiceButtons.h"
#include "../components/ModernSectionCard.h"
#include "../components/ModernTelemetryComponents.h"
#include <array>

// LFO / ARP tab: both LFOs with their wave previews and the arpeggiator.
// The two LFOs are the same section, described by an LfoDescriptor. Every
// choice is a row of buttons (shape as symbols, speed range, trigger, pitch
// target; arp mode, octaves, rate).
class LfoArpTab : public ModernTabModule {
public:
    explicit LfoArpTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

    // Advances the LFO preview traces by one editor timer tick (30 Hz).
    void advancePreviewAnimation();

    // The symbol of an LFO shape (paramlabels::kLfoShapes order).
    static void paintLfoShape(juce::Graphics& g, juce::Rectangle<float> area, int shape, juce::Colour colour);

private:
    static constexpr int kDepths = 5;   // pitch, wavemod, cutoff, reso, volume

    // What differs between the two LFOs.
    struct LfoDescriptor {
        int number;   // 1, 2: component IDs "lfo<n>...", knob names "<n>..."
        continuousParameter_t speed, amount;
        std::array<continuousParameter_t, kDepths> depths;
        steppedParameter_t shape, speedRange, targets, trigger;
    };
    static const std::array<LfoDescriptor, 2> kLfos;

    // One LFO's controls, children of the tab.
    struct LfoSection {
        std::unique_ptr<juce::Slider> speedKnob, amountKnob, delayKnob;
        std::unique_ptr<juce::Label> speedLabel, amountLabel, delayLabel;
        std::array<std::unique_ptr<juce::Slider>, kDepths> depthKnobs;
        std::array<std::unique_ptr<juce::Label>, kDepths> depthLabels;
        std::unique_ptr<ModernChoiceButtons> shapeChoice, rangeChoice, triggerChoice, pitchTargetChoice;
        std::unique_ptr<juce::Label> pitchTargetLabel;
        std::unique_ptr<LfoWavePreviewComponent> preview;
        float phase = 0.0f;   // preview animation, in cycles
        bool showsDelay = false;   // the start delay acts on this LFO
    };

    void createLfo(LfoSection& lfo, const LfoDescriptor& d);
    void createArp();
    void assignComponentIDs();
    void assignComponentIDs(LfoSection& lfo, const LfoDescriptor& d);
    void updateLfo(LfoSection& lfo, const LfoDescriptor& d);
    void layoutLfo(LfoSection& lfo, int x, int colW);
    void layoutArp(int x, int colW);

    ModernSectionCard lfo1Card{"LFO 1", "MAIN"};
    ModernSectionCard lfo2Card{"LFO 2", "AUX / VIB"};
    ModernSectionCard arpCard{"ARPEGGIATOR", "PLAYBACK"};

    std::array<LfoSection, 2> lfos;   // as kLfos

    std::unique_ptr<ModernChoiceButtons> arpModeChoice, arpOctaveChoice, arpRateChoice;
    std::unique_ptr<juce::ToggleButton> arpHoldToggle, arpSyncToggle;
    std::unique_ptr<juce::Slider> arpGateKnob, arpSwingKnob, arpBpmKnob;
    std::unique_ptr<juce::Label> arpGateLabel, arpSwingLabel, arpBpmLabel;
    std::unique_ptr<ArpVisualizerComponent> arpVisualizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoArpTab)
};
