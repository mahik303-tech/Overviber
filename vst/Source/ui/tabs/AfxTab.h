#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include <array>

// AFX tab: one sound per key. AFX MODE switches the keyboard from "MIDI
// channel N plays part N" to the kit: sixteen pads, each a sound (a preset
// with its waves), and a key map saying which pad each key plays. The pads
// light up while their sound plays; the panel beside them sets the selected
// pad's sound (a preset list) and level; the keyboard below paints keys onto
// it. Kits are
// saved and loaded with the complete setup (.ovm).
class AfxTab : public ModernTabModule {
public:
    explicit AfxTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;
    // Editor timer: lights the pads whose sound plays.
    void advanceActivity();

    void selectPad(int pad);
    int getSelectedPad() const { return selectedPad; }

    static juce::Colour padColour(int pad);
    static juce::String noteName(int note);                 // 60: "C4"
    // The keys a pad plays: "C1 - B1", "C1, D#1, F1", "24 keys" or "no keys".
    static juce::String keysText(const AfxKit& kit, int pad);

    // The sixteen pads, 4 x 4: number, name and keys; click (or arrow keys)
    // to select one.
    class PadGrid : public juce::Component {
    public:
        PadGrid(SynthModel& model, ModernLookAndFeel& lnf);
        juce::Rectangle<int> padBounds(int pad) const;
        void setSelected(int pad) { selected = pad; repaint(); }
        // Parts sounding since the last call; the pads fade out after.
        void setSounding(uint16_t parts);
        float getGlow(int pad) const { return glow[static_cast<size_t>(pad & 15)]; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        bool keyPressed(const juce::KeyPress& key) override;
        std::function<void(int pad)> onSelect;

    private:
        SynthModel& model;
        ModernLookAndFeel& lnf;
        int selected = 0;
        std::array<float, 16> glow{};
        uint16_t previousParts = 0;   // the audio and editor timers are not in step
    };

    // The 128 MIDI notes as a keyboard (C-1 .. G9), each key in its pad's
    // colour, the selected pad's keys bright. Click or drag paints keys onto
    // the selected pad.
    class KeyMap : public juce::Component {
    public:
        KeyMap(SynthModel& model, ModernLookAndFeel& lnf);
        void setSelected(int pad) { selected = pad; repaint(); }
        int noteAt(juce::Point<float> p) const;
        juce::Rectangle<float> keyBounds(int note) const;
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        std::function<void(int note)> onPaint;   // a key put on the selected pad
        std::function<void(int note)> onHover;   // -1: none

    private:
        void hover(int note);
        SynthModel& model;
        ModernLookAndFeel& lnf;
        int selected = 0;
        int hovered = -1;
    };

    // The selected pad's number and name, large and in its colour.
    class PadTitle : public juce::Component {
    public:
        explicit PadTitle(ModernLookAndFeel& skin) : lnf(skin) {}
        void set(int pad, const juce::String& name);
        juce::String getText() const { return text; }
        void paint(juce::Graphics& g) override;

    private:
        ModernLookAndFeel& lnf;
        juce::String text;
        juce::Colour colour;
    };

    // The presets as a list: selecting one (click or arrow keys) loads it
    // into the selected pad.
    class SoundList : public juce::ListBox, private juce::ListBoxModel {
    public:
        SoundList(PresetManager& presets, ModernLookAndFeel& lnf);
        // Marks the preset of that name without loading it (none: the pad
        // holds a sound not in the list).
        void show(const juce::String& name);
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void selectedRowsChanged(int lastRow) override;
        void paint(juce::Graphics& g) override;
        void paintOverChildren(juce::Graphics& g) override;
        std::function<void(int preset)> onChoose;

    private:
        PresetManager& presets;
        ModernLookAndFeel& lnf;
        bool showing = false;   // show() selects without loading
    };

    PadGrid* getPadGrid() { return pads.get(); }
    KeyMap* getKeyMap() { return keyMap.get(); }
    SoundList* getSoundList() { return soundList.get(); }

private:
    void assignComponentIDs();
    void setupKitControls();
    void setupSoundControls();
    void setupKeyControls();
    void setupSetupFiles();
    void showSelectedPad();
    void loadSoundIntoPad(int presetIndex);
    void copyEditedSound();
    void setPadLevel(int pot);
    void keysChanged();
    void showHover(int note);

    ModernSectionCard kitCard{"AFX SOUND KIT", "SOUND PER KEY"};
    ModernSectionCard soundCard{"SELECTED PAD", "PAD 1"};
    ModernSectionCard keyCard{"KEYBOARD", "128 KEYS"};

    int selectedPad = 0;

    // Kit
    juce::TextButton afxModeButton{"AFX MODE: OFF"};
    juce::Label modeLabel;
    juce::TextButton saveSetupButton{"SAVE KIT"}, loadSetupButton{"LOAD KIT"};
    std::unique_ptr<juce::FileChooser> setupChooser;
    std::unique_ptr<PadGrid> pads;

    // Selected pad
    PadTitle padTitle{modernLnf};
    juce::Label padHintLabel, keysLabel, keysHintLabel;
    std::unique_ptr<SoundList> soundList;
    std::unique_ptr<juce::Slider> levelKnob;
    std::unique_ptr<juce::Label> levelLabel;
    juce::TextButton copyEditButton{"COPY EDITED SOUND TO THIS PAD"};

    // Keyboard
    std::unique_ptr<KeyMap> keyMap;
    juce::TextButton octaveMapButton{"OCTAVES"}, chromaticMapButton{"CHROMATIC"},
        allKeysButton{"ALL KEYS"}, defaultMapButton{"DEFAULT"};
    juce::Label hoverLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AfxTab)
};
