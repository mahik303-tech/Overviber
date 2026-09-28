#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"

// MOD MATRIX tab: the 8-slot modulation matrix and the performance
// controllers (pitch bend, mod wheel, aftertouch).
class ModMatrixTab : public ModernTabModule {
public:
    explicit ModMatrixTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

private:
    void assignComponentIDs();

    ModernSectionCard benderCard{"PITCH BEND", "WHEEL"};
    ModernSectionCard modwheelCard{"MODULATION WHEEL", "MIDI CC 1"};
    ModernSectionCard pressureCard{"AFTERTOUCH", "PRESSURE"};
    ModernSectionCard modMatrixCard{"MODULATION MATRIX", "8 SLOTS"};

    // Performance controllers
    std::unique_ptr<juce::ToggleButton> benderRangeToggles[3];
    std::unique_ptr<juce::ToggleButton> benderTargetToggles[5];
    std::unique_ptr<juce::ToggleButton> modwheelRangeToggles[4];
    std::unique_ptr<juce::ToggleButton> modwheelTargetToggles[2];
    std::unique_ptr<juce::ToggleButton> pressureRangeToggles[4];
    std::unique_ptr<juce::ToggleButton> pressureTargetToggles[7];

    // Modulation matrix (8 slots)
    std::unique_ptr<juce::ToggleButton> matrixEnToggles[MOD_MATRIX_SLOT_COUNT];
    juce::ComboBox matrixSrcCombos[MOD_MATRIX_SLOT_COUNT];
    juce::ComboBox matrixViaCombos[MOD_MATRIX_SLOT_COUNT];
    juce::ComboBox matrixDestCombos[MOD_MATRIX_SLOT_COUNT];
    std::unique_ptr<juce::Slider> matrixDepthKnobs[MOD_MATRIX_SLOT_COUNT];
    std::unique_ptr<juce::Label> matrixSlotLabels[MOD_MATRIX_SLOT_COUNT];
    std::unique_ptr<juce::Label> matrixColLabels[5];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrixTab)
};
