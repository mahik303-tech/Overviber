#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../ModernEditorView.h"
#include "../../dsp/SynthEngine.h"

// ==============================================================================
// ModernShowcaseComponent: All-In-One UI Elements Gallery for Modern Skin
// Renders every dial mode, button, toggle, combo, visualizer, and card container
// side-by-side to allow comprehensive theme & typography evaluation at a glance.
// ==============================================================================
class ModernShowcaseComponent : public juce::Component {
public:
    explicit ModernShowcaseComponent(SynthEngine& eng, ModernLookAndFeel& lnf)
        : engine(eng), modernLnf(lnf) {
        setLookAndFeel(&modernLnf);

        addAndMakeVisible(knobsCard);
        knobsCard.addDivider(30, "KNOB MODES & SCALES");

        addAndMakeVisible(controlsCard);
        controlsCard.addDivider(30, "TOGGLES, BUTTONS & COMBOS");

        addAndMakeVisible(cardsDemoCard);
        cardsDemoCard.addDivider(30, "SUB-DIVIDERS & BADGE HEADERS");

        // Knobs setup
        auto setupKnob = [this](std::unique_ptr<juce::Slider>& s, std::unique_ptr<juce::Label>& l,
                                const juce::String& name, double min, double max, double init,
                                const juce::String& suffix = "") {
            s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            s->setRange(min, max, 1.0);
            s->setValue(init, juce::dontSendNotification);
            s->setTextValueSuffix(suffix);
            addAndMakeVisible(*s);

            l = std::make_unique<juce::Label>("", name);
            l->setJustificationType(juce::Justification::centred);
            l->setFont(modernLnf.getCustomFont(10.0f, juce::Font::plain));
            l->setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
            addAndMakeVisible(*l);
        };

        setupKnob(knobPercent, lblPercent, "PERCENT", 0, 100, 75, " %");
        setupKnob(knobBipolar, lblBipolar, "BIPOLAR", -100, 100, 0, " %");
        setupKnob(knobFreq, lblFreq, "FREQUENCY", 20, 20000, 2500, " Hz");
        setupKnob(knobTime, lblTime, "ATTACK", 1, 5000, 120, " ms");
        setupKnob(knobGain, lblGain, "DRIVE", 0, 40, 12, " dB");

        // Controls setup
        toggle1 = std::make_unique<juce::ToggleButton>("UNISON MODE");
        toggle1->setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(*toggle1);

        toggle2 = std::make_unique<juce::ToggleButton>("HARD SYNC");
        toggle2->setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(*toggle2);

        toggle3 = std::make_unique<juce::ToggleButton>("MACKITY SAT");
        toggle3->setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(*toggle3);

        combo1.addItem("SSI2144 (Ladder)", 1);
        combo1.addItem("Liquid (Ripples)", 2);
        combo1.addItem("Shelves (EQ / SVF)", 3);
        combo1.setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(combo1);

        combo2.addItem("Pulse Width (PWM)", 1);
        combo2.addItem("Wavefolder", 2);
        combo2.addItem("CrossOver", 3);
        combo2.addItem("BitCrush", 4);
        combo2.setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(combo2);

        for (int i = 0; i < 4; ++i) {
            const char* names[4] = { "LOW", "MID 1", "MID 2", "HIGH" };
            bandBtns[i].setButtonText(names[i]);
            bandBtns[i].setToggleState(i == 1, juce::dontSendNotification);
            bandBtns[i].onClick = [this, i]() {
                for (int b = 0; b < 4; ++b) bandBtns[b].setToggleState(b == i, juce::dontSendNotification);
            };
            addAndMakeVisible(bandBtns[i]);
        }

        btnAction1.setButtonText("INITIALIZE");
        addAndMakeVisible(btnAction1);

        btnAction2.setButtonText("NORMALIZE");
        addAndMakeVisible(btnAction2);

        // Visualizers
        filterCurve = std::make_unique<FilterCurveComponent>(engine, *knobFreq, *knobPercent);
        addAndMakeVisible(*filterCurve);

        adsrCurve = std::make_unique<AdsrCurveComponent>(engine, *knobTime, *knobPercent, *knobPercent, *knobTime, "ADSR ENVELOPE");
        addAndMakeVisible(*adsrCurve);

        lfoPreview = std::make_unique<LfoWavePreviewComponent>(engine, 0);
        lfoPreview->setShape(1); // Triangle
        addAndMakeVisible(*lfoPreview);

        voiceMeter = std::make_unique<ModernVoiceMeterPanel>(engine);
        addAndMakeVisible(*voiceMeter);
    }

    ~ModernShowcaseComponent() override {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(modernLnf.getTheme().windowBg);
    }

    void lookAndFeelChanged() override {
        auto updateLbl = [this](std::unique_ptr<juce::Label>& l) {
            if (l) {
                l->setFont(modernLnf.getCustomFont(10.0f, juce::Font::plain));
                l->setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
            }
        };
        updateLbl(lblPercent);
        updateLbl(lblBipolar);
        updateLbl(lblFreq);
        updateLbl(lblTime);
        updateLbl(lblGain);
        repaint();
    }

    void refreshTheme() {
        sendLookAndFeelChange();
        repaint();
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(12);

        // Top Row: Cards (Knobs, Toggles/Combos, Demo Card)
        int colW = (bounds.getWidth() - 24) / 3;
        int topRowH = 175;

        knobsCard.setBounds(bounds.getX(), bounds.getY(), colW, topRowH);
        controlsCard.setBounds(bounds.getX() + colW + 12, bounds.getY(), colW, topRowH);
        cardsDemoCard.setBounds(bounds.getX() + (colW + 12) * 2, bounds.getY(), colW, topRowH);

        // Knobs layout
        int knobSz = 50;
        int knobSpacing = (colW - 20) / 5;
        auto placeKnob = [&](auto& knob, auto& lbl, int idx) {
            int kx = bounds.getX() + 10 + idx * knobSpacing;
            int ky = bounds.getY() + 50;
            if (knob) knob->setBounds(kx, ky, knobSz, knobSz);
            if (lbl) lbl->setBounds(kx - 10, ky + knobSz - 2, knobSz + 20, 16);
        };
        placeKnob(knobPercent, lblPercent, 0);
        placeKnob(knobBipolar, lblBipolar, 1);
        placeKnob(knobFreq, lblFreq, 2);
        placeKnob(knobTime, lblTime, 3);
        placeKnob(knobGain, lblGain, 4);

        // Controls layout
        int col2X = bounds.getX() + colW + 20;
        toggle1->setBounds(col2X, bounds.getY() + 45, 120, 24);
        toggle2->setBounds(col2X + 130, bounds.getY() + 45, 120, 24);
        toggle3->setBounds(col2X, bounds.getY() + 75, 120, 24);
        combo1.setBounds(col2X + 130, bounds.getY() + 75, colW - 160, 24);
        combo2.setBounds(col2X, bounds.getY() + 105, colW - 40, 24);

        int bandW = (colW - 50) / 4;
        for (int i = 0; i < 4; ++i) {
            bandBtns[i].setBounds(col2X + i * (bandW + 4), bounds.getY() + 138, bandW, 24);
        }

        // Card Demo layout
        int col3X = bounds.getX() + (colW + 12) * 2 + 20;
        btnAction1.setBounds(col3X, bounds.getY() + 55, 120, 28);
        btnAction2.setBounds(col3X + 130, bounds.getY() + 55, 120, 28);

        // Bottom Row: Visualizers & Editors
        int botY = bounds.getY() + topRowH + 12;
        int botH = bounds.getHeight() - (topRowH + 12);

        int visW1 = (int)((bounds.getWidth() - 24) * 0.42f);
        int visW2 = (int)((bounds.getWidth() - 24) * 0.32f);
        int visW3 = bounds.getWidth() - visW1 - visW2 - 24;

        if (filterCurve) filterCurve->setBounds(bounds.getX(), botY, visW1, botH);
        if (adsrCurve) adsrCurve->setBounds(bounds.getX() + visW1 + 12, botY, visW2, botH);

        int rightX = bounds.getX() + visW1 + visW2 + 24;
        int halfH = (botH - 12) / 2;
        if (lfoPreview) lfoPreview->setBounds(rightX, botY, visW3, halfH);
        if (voiceMeter) voiceMeter->setBounds(rightX, botY + halfH + 12, visW3, halfH);
    }

private:
    SynthEngine& engine;
    ModernLookAndFeel& modernLnf;

    ModernSectionCard knobsCard{ "ROTARY CONTROLS", "DIALS" };
    ModernSectionCard controlsCard{ "TOGGLES & SELECTORS", "CONTROLS" };
    ModernSectionCard cardsDemoCard{ "ACTION BUTTONS & BADGES", "CONTAINERS" };

    std::unique_ptr<juce::Slider> knobPercent, knobBipolar, knobFreq, knobTime, knobGain;
    std::unique_ptr<juce::Label> lblPercent, lblBipolar, lblFreq, lblTime, lblGain;

    std::unique_ptr<juce::ToggleButton> toggle1, toggle2, toggle3;
    juce::ComboBox combo1, combo2;
    juce::TextButton bandBtns[4];
    juce::TextButton btnAction1, btnAction2;

    std::unique_ptr<FilterCurveComponent> filterCurve;
    std::unique_ptr<AdsrCurveComponent> adsrCurve;
    std::unique_ptr<LfoWavePreviewComponent> lfoPreview;
    std::unique_ptr<ModernVoiceMeterPanel> voiceMeter;
};
