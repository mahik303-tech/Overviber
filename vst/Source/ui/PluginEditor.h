#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ClassicParamSchema.h"
#include "LcdDisplay.h"
#include "OvercyclerKnob.h"
#include "OvercyclerKeypad.h"
#include "ModernEditorView.h"
#include "ModernPresetManager.h"

class OvercyclerAudioProcessor;

class OvercyclerAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OvercyclerAudioProcessorEditor(OvercyclerAudioProcessor&);
    ~OvercyclerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void setGuiMode(bool modern);

private:
    void timerCallback() override;
    void switchPage(ClassicUI::PageId newPage);
    void handleKeyPress(char key);
    void handleActionKey(char key);
    void handleNumericInput(char digit);
    void updateKnobMappings();
    void onKnobChanged(int knobIndex, float value);

    OvercyclerAudioProcessor& processor;

    // Top-Bar Branding Components (inspectable in debug mode)
    class VersionBadgeComponent : public juce::Component {
    public:
        VersionBadgeComponent();
        void setAccentColour(juce::Colour c) { accent = c; repaint(); }
        void paint(juce::Graphics& g) override;
    private:
        juce::Colour accent{0xff18b5c9};
    };

    class TopBarSubtitleComponent : public juce::Component {
    public:
        TopBarSubtitleComponent();
        void setColours(juce::Colour body, juce::Colour acc) { bodyCol = body; accentCol = acc; repaint(); }
        void paint(juce::Graphics& g) override;
    private:
        juce::Colour bodyCol{0xffe2e5eb};
        juce::Colour accentCol{0xff18b5c9};
    };

    juce::Label brandLogoLabel;
    VersionBadgeComponent versionBadgeLabel;
    TopBarSubtitleComponent subtitleLabel;

    // Classic Hardware Components
    LcdDisplay lcdDisplay;
    std::unique_ptr<OvercyclerKnob> knobs[10];
    OvercyclerKeypad keypad;

    // Modern Interface Component
    std::unique_ptr<ModernEditorView> modernView;
    bool isModernMode = true;
    juce::TextButton guiModeButton{"SWITCH TO MODERN SKIN"};

    // Modern Preset Manager Components
    std::unique_ptr<ModernPresetBar> modernPresetBar;
    std::unique_ptr<ModernPresetBrowserOverlay> presetBrowserOverlay;
    std::unique_ptr<ModernSaveAsModal> saveAsModal;

    // Classic Header Components
    juce::ComboBox presetSelector;
    juce::TextButton prevPresetBtn{"<"};
    juce::TextButton nextPresetBtn{">"};

    ClassicUI::PageId activePage{ ClassicUI::PageId::Osc };

    // Classic State & Interaction
    bool isNumericInputActive{ false };
    std::string numericBuffer;
    int lastEditedKnobIndex{ -1 };
    int transposeOffset{ 0 };

    bool knobAcquired[10]{ true, true, true, true, true, true, true, true, true, true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OvercyclerAudioProcessorEditor)
};
