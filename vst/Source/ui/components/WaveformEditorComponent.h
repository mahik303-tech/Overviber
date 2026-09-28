#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../ModernPresetManager.h"
#include "../../data/SynthModel.h"
#include <functional>
#include <memory>

// ==============================================================================
// Waveform Save As Modal Dialog
// ==============================================================================
class WaveformSaveModal : public juce::Component {
public:
    WaveformSaveModal();
    void show(const juce::String& currentName, std::function<void(const juce::String& newName)> onSave);
    void hide();
    void setAccentColour(juce::Colour c) { accentCol = c; saveBtn.setAccentColour(c); repaint(); }
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label descLabel;
    juce::TextEditor nameEditor;
    ModernHeaderButton saveBtn{"saveWaveModalBtn", "SAVE"};
    ModernHeaderButton cancelBtn{"cancelWaveModalBtn", "CANCEL"};
    std::function<void(const juce::String&)> saveCallback;
    juce::Colour accentCol{0xff18b5c9};
};

// ==============================================================================
// Interactive Wavetable Waveform Editor & Preset Manager (Dedicated per OSC)
// ==============================================================================
class WaveformEditorComponent : public juce::Component {
public:
    explicit WaveformEditorComponent(SynthModel& eng, abx_t targetOsc = abxAMain);
    ~WaveformEditorComponent() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void resized() override;

    void setTargetOsc(abx_t osc);
    abx_t getTargetOsc() const { return currentOsc; }

    void invertWave();
    void smoothWave(int passes = -1);
    void normalizeWave(float targetGain = -1.0f);
    void refreshPresetDisplay();
    void stepFrame(int delta);
    void openSaveModal();
    void openImportDialog();
    void setupSubComponentIDs(const juce::String& prefix);
    void setModified(bool modified);

    std::function<void()> onWaveformChanged;
    std::function<void(abx_t osc)> onOpenWaveBrowser;

    class SaveDisketteButton : public juce::Button {
    public:
        SaveDisketteButton();
        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    };

private:
    void applySampleEdit(int x, int y);
    void updateFrameControls();
    void showPresetMenu();

    SynthModel& model;
    abx_t currentOsc = abxAMain;
    int lastEditX = -1;
    int lastEditY = -1;
    bool isWaveModified = false;

    juce::String titleText = "WAVEFORM A";
    juce::String badgeText = "CORE";

    ModernPresetDisplayButton waveDisplayBtn;
    SaveDisketteButton saveDisketteBtn;
    juce::TextButton prevFrameBtn{"<"};
    juce::Label frameLabel;
    juce::TextButton nextFrameBtn{">"};

    juce::TextButton invertBtn{"INVERT"};
    juce::TextButton smoothBtn{"SMOOTH"};
    std::unique_ptr<juce::Slider> smoothKnob;
    juce::TextButton normBtn{"NORMALIZE"};
    std::unique_ptr<juce::Slider> normKnob;

    WaveformSaveModal saveModal;
    std::unique_ptr<juce::FileChooser> fileChooser;
};
