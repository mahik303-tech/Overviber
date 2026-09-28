#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/SynthEngine.h"
#include "theme/ModernTheme.h"
#include "theme/ModernFontManager.h"
#include <vector>
#include <functional>

class OvercyclerAudioProcessor;

// Helper to deduce acoustic/synth category from preset name
juce::String getPresetCategoryTag(const juce::String& name);

// ==============================================================================
// Modern Button with theme styling, hover glow, and flash animations
// ==============================================================================
class ModernHeaderButton : public juce::Button, private juce::Timer {
public:
    explicit ModernHeaderButton(const juce::String& name, const juce::String& text = "");
    ~ModernHeaderButton() override;

    void setButtonText(const juce::String& newText);
    void setAccentColour(juce::Colour c) { accentCol = c; repaint(); }
    void setFlashText(const juce::String& text, int durationMs = 1200);

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    void timerCallback() override;

    juce::String standardText;
    juce::String flashText;
    bool isFlashing = false;
    juce::Colour accentCol{0xff18b5c9};
};

// ==============================================================================
// Modern Preset Display Badge (Slot #, Name, Category Tag, Dropdown Arrow)
// ==============================================================================
class ModernPresetDisplayButton : public juce::Component {
public:
    ModernPresetDisplayButton();

    void setPreset(int presetNumber, int totalSlots, const juce::String& name, const juce::String& category);
    juce::String getPresetName() const { return presetName; }
    void setAccentColour(juce::Colour c) { accentCol = c; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override { isHovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { isHovered = false; repaint(); }
    void mouseDown(const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    int slot = 0;
    int total = 50;
    juce::String presetName = "Init Patch";
    juce::String category = "SYNTH";
    bool isHovered = false;
    juce::Colour accentCol{0xff18b5c9};
};

// ==============================================================================
// Modern Preset Bar (Header Component)
// ==============================================================================
class ModernPresetBar : public juce::Component {
public:
    ModernPresetBar(SynthEngine& engine, OvercyclerAudioProcessor* processor);
    ~ModernPresetBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateDisplay();
    juce::String getDisplayedPresetName() const { return displayBtn.getPresetName(); }
    void selectPreset(int index);
    void prevPreset();
    void nextPreset();
    void quickSave();
    void initPatch();
    void setSaveFlashText(const juce::String& text);

    void setAccentColour(juce::Colour c);

    std::function<void()> onToggleBrowser;
    std::function<void()> onOpenSaveAs;
    std::function<void(int newIndex)> onPresetChanged;

private:
    SynthEngine& engine;
    OvercyclerAudioProcessor* processor;

    ModernHeaderButton prevBtn{"prev", "<"};
    ModernPresetDisplayButton displayBtn;
    ModernHeaderButton nextBtn{"next", ">"};
    ModernHeaderButton saveBtn{"save", "SAVE"};
    ModernHeaderButton initBtn{"init", "INIT"};

    juce::Colour accentCol{0xff18b5c9};
};

// ==============================================================================
// Modern Preset Save As Modal Dialog
// ==============================================================================
class ModernSaveAsModal : public juce::Component {
public:
    ModernSaveAsModal();

    void show(const juce::String& currentName, std::function<void(const juce::String& newName)> onSave);
    void hide();

    void setAccentColour(juce::Colour c) { accentCol = c; saveBtn.setAccentColour(c); repaint(); }
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label descLabel;
    juce::TextEditor nameEditor;
    ModernHeaderButton saveBtn{"save", "SAVE PRESET"};
    ModernHeaderButton cancelBtn{"cancel", "CANCEL"};

    std::function<void(const juce::String&)> saveCallback;
    juce::Colour accentCol{0xff18b5c9};
};

// ==============================================================================
// Modern Preset Browser Overlay (Dropdown / Drawer with Search & Dual Modes)
// ==============================================================================
class ModernPresetBrowserOverlay : public juce::Component, public juce::ListBoxModel {
public:
    enum class BrowserMode {
        PatchPresets,
        Waveforms
    };

    ModernPresetBrowserOverlay(SynthEngine& engine, OvercyclerAudioProcessor* processor);
    ~ModernPresetBrowserOverlay() override;

    void setMode(BrowserMode mode, abx_t osc = abxAMain);
    BrowserMode getMode() const { return currentMode; }
    abx_t getTargetOsc() const { return targetOsc; }

    void refreshList();
    void refreshPresetList() { refreshList(); }
    void filterList();
    void setAccentColour(juce::Colour c);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    std::function<void(int index)> onPresetSelected;
    std::function<void(abx_t osc, const juce::String& bank, const juce::String& wave)> onWaveSelected;
    std::function<void()> onClose;

private:
    void setupCategoryChips();

    struct PresetEntry {
        int index = 0;
        int number = 0;
        juce::String name;
        juce::String category;
    };

    struct WaveEntry {
        int index = 0;
        juce::String bank;
        juce::String waveName;
        juce::String category;
        juce::String displayName;
    };

    BrowserMode currentMode = BrowserMode::PatchPresets;
    abx_t targetOsc = abxAMain;

    SynthEngine& engine;
    OvercyclerAudioProcessor* processor;

    ModernHeaderButton modePresetsBtn{"modePresetsBtn", "PATCH PRESETS"};
    ModernHeaderButton modeWavesBtn{"modeWavesBtn", "WAVEFORMS & TABLES"};

    juce::Label titleLabel;
    juce::TextEditor searchBox;
    juce::String selectedCategory = "ALL";
    std::vector<juce::String> categories;
    std::vector<std::unique_ptr<ModernHeaderButton>> categoryChips;

    juce::ListBox listBox;
    std::vector<PresetEntry> allPresets;
    std::vector<PresetEntry> filteredPresets;

    std::vector<WaveEntry> allWaves;
    std::vector<WaveEntry> filteredWaves;

    ModernHeaderButton closeBtn{"close", "✕"};
    ModernHeaderButton reloadBtn{"reload", "RELOAD"};
    ModernHeaderButton openFolderBtn{"folder", "OPEN FOLDER"};
    ModernHeaderButton closeBottomBtn{"closeBottom", "CLOSE"};
    juce::Label statusLabel;

    juce::Colour accentCol{0xff18b5c9};
};
