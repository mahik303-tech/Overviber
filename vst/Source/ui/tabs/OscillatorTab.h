#pragma once

#include <array>

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include "../components/WaveformEditorComponent.h"

// OSC tab: the sources. Oscillator engine and noise level in the top row,
// OSC A / OSC B with their waveform editors and the Elements modal
// resonator. In Elements mode the controls without effect there are dimmed
// (all of OSC B, OSC A's WaveMod; OSC A's pitch and level play Elements).
class OscillatorTab : public ModernTabModule {
public:
    explicit OscillatorTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

private:
    void assignComponentIDs();
    void layoutElementsCard(juce::Rectangle<int> bounds);
    void setElementsControlsVisible(bool visible);
    void showEngineScope(uint8_t engine);

    ModernSectionCard oscACard{"OSC A", "WAVETABLE"};
    ModernSectionCard oscBCard{"OSC B", "WAVETABLE"};
    ModernSectionCard elementsCard{"PHYSICAL ACOUSTIC MODELING", "ELEMENTS MODAL RESONATOR"};

    std::unique_ptr<juce::TextButton> oscEngineButtons[3]; // [ DUAL WAVETABLE ] [ ELEMENTS MODAL ] [ HYBRID ]
    std::unique_ptr<juce::Slider> noiseSlider;              // noise generator level, a source like the oscillators
    int shownEngine = -1;

    std::unique_ptr<juce::Slider> oscAVolKnob, oscAFreqKnob, oscAWModKnob, oscAWModEnvKnob;
    std::unique_ptr<juce::TextButton> oscAWModButtons[7];
    std::unique_ptr<juce::Label> oscAVolLabel, oscAFreqLabel, oscAWModLabel, oscAWModEnvLabel;

    std::unique_ptr<juce::Slider> oscBVolKnob, oscBFreqKnob, oscBDetuneKnob, oscBWModKnob, oscBWModEnvKnob;
    std::unique_ptr<juce::TextButton> oscBWModButtons[7];
    std::unique_ptr<juce::ToggleButton> oscSyncToggle;
    std::unique_ptr<juce::Label> oscBVolLabel, oscBFreqLabel, oscBDetuneLabel, oscBWModLabel, oscBWModEnvLabel;

    // Elements Modal Resonator Controls
    std::unique_ptr<juce::TextButton> elementsModelButtons[4];
    // The Elements knobs, as on the panel (table in OscillatorTab.cpp)
    enum ElementsKnob { elContour, elBow, elBlow, elStrike, elFlow, elMallet, elBowTimbre, elBlowTimbre,
                        elStrikeTimbre, elGeometry, elBrightness, elDamping, elPosition, elSpace, elKnobCount };
    std::array<std::unique_ptr<juce::Slider>, elKnobCount> elementsKnobs;
    std::array<std::unique_ptr<juce::Label>, elKnobCount> elementsLabels;

    // Interactive waveform editors
    std::unique_ptr<WaveformEditorComponent> waveformEditorA;
    std::unique_ptr<WaveformEditorComponent> waveformEditorB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscillatorTab)
};
