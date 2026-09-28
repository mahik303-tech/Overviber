#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"

// AFX tab: voice allocation, the 16-slot AFX sound kit with its keyboard
// zone map, multi-part MIDI routing and complete-setup files.
class AfxTab : public ModernTabModule {
public:
    explicit AfxTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

private:
    void assignComponentIDs();
    void selectAfxSlot(int slotIndex);
    void prepareSelectedPartWaves();
    void setupRoutingControls();
    void updateAfxSlotButtons();

    class AfxKeyboardZoneComponent : public juce::Component {
    public:
        explicit AfxKeyboardZoneComponent(SynthEngine& eng);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        std::function<void(uint8_t note)> onNoteClicked;
    private:
        SynthEngine& engine;
    };

    ModernSectionCard voiceAllocCard{"VOICE ALLOCATION & PRIORITY", "POLYPHONY"};
    ModernSectionCard afxKitCard{"AFX SOUND KIT & PRESET ASSIGNMENT", "16 SLOTS / KEYBOARD ZONES"};

    std::unique_ptr<juce::Slider> voiceCountSlider;
    std::unique_ptr<juce::ToggleButton> assignerPrioToggles[3];

    int selectedAfxSlot = 0;
    juce::ToggleButton customRouteToggle{"SPLIT / LAYER ROUTING"}, routeEnabled{"PART ENABLED"}, calibratedToggle{"CALIBRATED GAIN"};
    juce::ComboBox routeChannel;
    juce::Slider routeLow, routeHigh;
    juce::TextButton saveSetupButton{"SAVE SETUP"}, loadSetupButton{"LOAD SETUP"};
    std::unique_ptr<juce::FileChooser> setupChooser;
    std::unique_ptr<juce::TextButton> afxSlotButtons[AFX_SLOT_COUNT];
    std::unique_ptr<juce::TextButton> octaveMapBtn, chromaticMapBtn, allToSlotBtn;
    std::unique_ptr<juce::TextButton> assignCurrentPresetBtn;
    std::unique_ptr<juce::ComboBox> slotPresetCombo;
    std::unique_ptr<juce::Label> afxSlotDetailLabel;
    std::unique_ptr<juce::Label> afxVoiceLiveStatusLabel;
    std::unique_ptr<AfxKeyboardZoneComponent> afxKeyboardZone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AfxTab)
};
