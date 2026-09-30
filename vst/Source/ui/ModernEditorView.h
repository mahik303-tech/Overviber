#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../data/SynthModel.h"
#include "theme/ModernTheme.h"
#include "theme/ModernFontManager.h"
#include "ModernPresetManager.h"

class OvercyclerAudioProcessor;

#include "components/ModernLookAndFeel.h"
#include "components/ModernTabBar.h"
#include "tabs/OscillatorTab.h"
#include "tabs/FilterVcaTab.h"
#include "tabs/EnvelopeTab.h"
#include "tabs/LfoArpTab.h"
#include "tabs/AfxTab.h"
#include "tabs/ModMatrixTab.h"
#include "tabs/SettingsTab.h"

// ==============================================================================
// Main Modern Editor View Component
// ==============================================================================
class ModernEditorView : public juce::Component, private juce::Timer, private SettingsTab::Host {
public:
    explicit ModernEditorView(SynthModel& model, OvercyclerAudioProcessor* processor = nullptr);
    ~ModernEditorView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void updateFromEngine();

    void setContinuousParam(continuousParameter_t cp, float potVal);
    void setSteppedParam(steppedParameter_t sp, uint8_t stepVal);

    // Theming & Typography
    void setTheme(const ModernTheme& theme);
    void setFontFamily(const juce::String& familyName);
    void setFontScale(float scale);
    const ModernTheme& getTheme() const { return modernLnf.getTheme(); }
    const juce::String& getFontFamily() const { return modernLnf.getFontFamily(); }
    float getFontScale() const { return modernLnf.getFontScale(); }
    ModernLookAndFeel& getModernLookAndFeel() { return modernLnf; }

    void selectTab(int tabIndex);
    int getSelectedTabIndex() const;

    // Right click on a knob that shows a matrix destination: a menu with the
    // slots modulating it, "add a source" and a way to the MOD MATRIX tab.
    bool showModulationMenu(juce::Slider& knob);
    // Puts `source` on the destination of the knob with `knobId` in a free
    // matrix slot (what the menu's "add" does); the slot, or -1.
    int addModulation(const juce::String& knobId, modSource_t source);
    ModMatrixTab& getModMatrixTab() { return modMatrixTab; }

    std::function<void(bool modern)> onSkinModeChanged;
    std::function<void(float scale)> onWindowScaleChanged;
    std::function<void(const ModernTheme& theme)> onThemeChanged;
    std::function<void(abx_t osc)> onOpenWaveBrowser;
    float getSavedWindowScale() const { return settingsTab.getSavedWindowScale(); }
    void setSavedWindowScale(float s) { settingsTab.setSavedWindowScale(s); }
    static juce::String getDebugHoverTextFor(juce::Component& component);

private:
    void timerCallback() override;
    // Knobs that show a matrix destination, with their "modDepth" arcs.
    void collectModulationTargets();
    void updateModulationIndicators();
    std::vector<std::pair<juce::Component::SafePointer<juce::Slider>, modDest_t>> modulationTargets;

    // SettingsTab::Host
    void applyTheme(const ModernTheme& theme) override;
    void applyFontFamily(const juce::String& familyName) override;
    void applyFontScale(float scale) override;
    void windowScaleChanged(float scale) override;
    void switchToClassicSkin() override;
    void debugModeChanged(bool enabled) override;
    void filterFamilySwitchChanged(bool matchSameFilter) override;
    void showColourPicker(juce::Colour initialColour, const juce::String& roleTitle,
                          std::function<void(juce::Colour)> onColourChanged,
                          std::function<void(juce::Colour)> onApply) override;
    void showPaletteSaveDialog(const juce::String& initialName,
                               std::function<void(const juce::String&)> onSave) override;

    SynthModel& model;
    OvercyclerAudioProcessor* processor = nullptr;
    ModernLookAndFeel modernLnf;
    ModernTabContext tabContext{model, processor, modernLnf};

    // Navigation Tab Buttons
    using TabIndex = ModernTabBar::Tab;
    ModernTabBar tabBar;
    TabIndex currentTab = TabIndex::Osc;
    void selectTab(TabIndex tab);

    // Tab modules: each owns, syncs and lays out its controls (see tabs/)
    OscillatorTab oscillatorTab{tabContext};
    FilterVcaTab filterTab{tabContext};
    EnvelopeTab envTab{tabContext};
    LfoArpTab lfoTab{tabContext};
    AfxTab afxTab{tabContext};
    ModMatrixTab modMatrixTab{tabContext};
    SettingsTab settingsTab{tabContext, *this};

    // Voice activity levels for meter rendering
    float voiceLevels[SYNTH_VOICE_COUNT] = { 0.0f };

    class ModernPaletteSaveModal : public juce::Component {
    public:
        ModernPaletteSaveModal();
        void show(const juce::String& currentName, std::function<void(const juce::String& newName)> onSave);
        void hide();
        void setAccentColour(juce::Colour c);
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        juce::Label titleLabel;
        juce::Label descLabel;
        juce::TextEditor nameEditor;
        ModernHeaderButton saveBtn{"savePalModalBtn", "SAVE"};
        ModernHeaderButton cancelBtn{"cancelPalModalBtn", "CANCEL"};

        std::function<void(const juce::String&)> saveCallback;
        juce::Colour accentCol{0xff18b5c9};
    };
    ModernPaletteSaveModal paletteSaveModal;

    class ModernColorPickerModal : public juce::Component, private juce::ChangeListener {
    public:
        ModernColorPickerModal();
        ~ModernColorPickerModal() override;

        void show(juce::Colour initialColour, const juce::String& roleTitle,
                  std::function<void(juce::Colour)> onColourChanged,
                  std::function<void(juce::Colour)> onApply);
        void hide();
        void setAccentColour(juce::Colour c);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        bool keyPressed(const juce::KeyPress& key) override;

    private:
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;

        juce::Label titleLabel;
        juce::Label roleLabel;
        std::unique_ptr<juce::ColourSelector> colourSelector;
        ModernHeaderButton applyBtn{"colorPickerApply", "APPLY & CLOSE"};
        ModernHeaderButton cancelBtn{"colorPickerCancel", "CANCEL"};

        std::function<void(juce::Colour)> changeCallback;
        std::function<void(juce::Colour)> applyCallback;

        juce::Colour originalColour;
        juce::Colour currentColour;
        juce::Colour accentCol{0xff18b5c9};
    };
    ModernColorPickerModal colorPickerModal;

    class ModernDebugTooltipWindow;
    std::unique_ptr<ModernDebugTooltipWindow> tooltipWindow;
    class ModernDebugHighlightOverlay;
    std::unique_ptr<ModernDebugHighlightOverlay> highlightOverlay;
    void setupComponentIDs();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModernEditorView)
};
