#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include "../components/ModernTelemetryComponents.h"

// LFO / ARP tab: both LFOs with their wave previews and the arpeggiator.
class LfoArpTab : public ModernTabModule {
public:
    explicit LfoArpTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

    // Advances the LFO preview traces by one editor timer tick (30 Hz).
    void advancePreviewAnimation();

private:
    void assignComponentIDs();

    ModernSectionCard lfo1Card{"LFO 1", "MAIN"};
    ModernSectionCard lfo2Card{"LFO 2", "AUX / VIB"};
    ModernSectionCard arpCard{"ARPEGGIATOR", "PLAYBACK"};

    std::unique_ptr<juce::Slider> lfo1FreqKnob, lfo1AmtKnob, lfo1DelayKnob;
    std::unique_ptr<juce::Slider> lfo1PitchKnob, lfo1WModKnob, lfo1FilKnob, lfo1ResKnob, lfo1AmpKnob;
    juce::ComboBox lfo1ShapeCombo, lfo1SpeedCombo, lfo1TargetsCombo, lfo1TrigCombo;
    std::unique_ptr<juce::Label> lfo1FreqLabel, lfo1AmtLabel, lfo1DelayLabel;
    std::unique_ptr<juce::Label> lfo1PitchLabel, lfo1WModLabel, lfo1FilLabel, lfo1ResLabel, lfo1AmpLabel;

    std::unique_ptr<juce::Slider> lfo2FreqKnob, lfo2AmtKnob, lfo2DelayKnob;
    std::unique_ptr<juce::Slider> lfo2PitchKnob, lfo2WModKnob, lfo2FilKnob, lfo2ResKnob, lfo2AmpKnob;
    juce::ComboBox lfo2ShapeCombo, lfo2SpeedCombo, lfo2TargetsCombo, lfo2TrigCombo;
    std::unique_ptr<juce::Label> lfo2FreqLabel, lfo2AmtLabel, lfo2DelayLabel;
    std::unique_ptr<juce::Label> lfo2PitchLabel, lfo2WModLabel, lfo2FilLabel, lfo2ResLabel, lfo2AmpLabel;

    juce::ComboBox arpModeCombo, arpOctaveCombo, arpRateCombo;
    std::unique_ptr<juce::ToggleButton> arpHoldToggle, arpSyncToggle;
    std::unique_ptr<juce::Slider> arpGateKnob, arpSwingKnob, arpBpmKnob;
    std::unique_ptr<juce::Label> arpGateLabel, arpSwingLabel, arpBpmLabel;

    // Visual previews (read the knobs above)
    std::unique_ptr<LfoWavePreviewComponent> lfo1WavePreview;
    std::unique_ptr<LfoWavePreviewComponent> lfo2WavePreview;
    std::unique_ptr<ArpVisualizerComponent> arpVisualizer;
    float lfo1Phase = 0.0f;
    float lfo2Phase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoArpTab)
};
