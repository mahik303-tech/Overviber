#include "OscillatorTab.h"

#include <array>

OscillatorTab::OscillatorTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void OscillatorTab::setup() {
    // Add visual grouping section cards to back
    addAndMakeVisible(oscACard);
    addAndMakeVisible(oscBCard);
    oscACard.toBack();
    oscBCard.toBack();

    // Osc A
    oscAVolKnob = createKnob("AVol", 0, 999, 999, KnobMode::Percent);
    oscAVolKnob->onValueChange = [this]() { setContinuousParam(cpAVol, (float)oscAVolKnob->getValue()); };
    addAndMakeVisible(*oscAVolKnob);
    oscAVolLabel = createLabel("LEVEL", *this);

    oscAFreqKnob = createKnob("AFreq", -499, 499, 0, KnobMode::PitchSemitones);
    oscAFreqKnob->onValueChange = [this]() {
        setContinuousParam(cpAFreq, (float)oscAFreqKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*oscAFreqKnob);
    oscAFreqLabel = createLabel("COARSE PITCH", *this);

    const char* wmodTypes[] = { "OFF", "GRIT", "PWM", "FM", "MORPH", "FOLD", "CRUSH" };
    for (int i = 0; i < 7; ++i) {
        oscAWModButtons[i] = std::make_unique<juce::TextButton>(wmodTypes[i]);
        oscAWModButtons[i]->setClickingTogglesState(true);
        oscAWModButtons[i]->setRadioGroupId(1001);
        oscAWModButtons[i]->onClick = [this, i]() {
            setSteppedParam(spAWModType, (uint8_t)i);
        };
        addAndMakeVisible(*oscAWModButtons[i]);
    }

    oscAWModKnob = createKnob("AWMod", -499, 499, 0, KnobMode::BipolarPercent);
    oscAWModKnob->onValueChange = [this]() {
        setContinuousParam(cpABaseWMod, (float)oscAWModKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*oscAWModKnob);
    oscAWModLabel = createLabel("MOD DEPTH", *this);

    oscAWModEnvKnob = createKnob("AWEA", -499, 499, 0, KnobMode::BipolarPercent);
    oscAWModEnvKnob->onValueChange = [this]() {
        setContinuousParam(cpWModAEnv, (float)oscAWModEnvKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*oscAWModEnvKnob);
    oscAWModEnvLabel = createLabel("ENV DEPTH", *this);

    // Osc B
    oscBVolKnob = createKnob("BVol", 0, 999, 999, KnobMode::Percent);
    oscBVolKnob->onValueChange = [this]() { setContinuousParam(cpBVol, (float)oscBVolKnob->getValue()); };
    addAndMakeVisible(*oscBVolKnob);
    oscBVolLabel = createLabel("LEVEL", *this);

    oscBFreqKnob = createKnob("BFreq", -499, 499, 0, KnobMode::PitchSemitones);
    oscBFreqKnob->onValueChange = [this]() {
        setContinuousParam(cpBFreq, (float)oscBFreqKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*oscBFreqKnob);
    oscBFreqLabel = createLabel("COARSE PITCH", *this);

    oscBDetuneKnob = createKnob("Detune", -499, 499, 0, KnobMode::FineDetuneCents);
    oscBDetuneKnob->onValueChange = [this]() {
        setContinuousParam(cpDetune, (float)oscBDetuneKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*oscBDetuneKnob);
    oscBDetuneLabel = createLabel("FINE DETUNE", *this);

    for (int i = 0; i < 7; ++i) {
        oscBWModButtons[i] = std::make_unique<juce::TextButton>(wmodTypes[i]);
        oscBWModButtons[i]->setClickingTogglesState(true);
        oscBWModButtons[i]->setRadioGroupId(1002);
        oscBWModButtons[i]->onClick = [this, i]() {
            setSteppedParam(spBWModType, (uint8_t)i);
        };
        addAndMakeVisible(*oscBWModButtons[i]);
    }

    oscBWModKnob = createKnob("BWMod", -499, 499, 0, KnobMode::BipolarPercent);
    oscBWModKnob->onValueChange = [this]() {
        setContinuousParam(cpBBaseWMod, (float)oscBWModKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*oscBWModKnob);
    oscBWModLabel = createLabel("MOD DEPTH", *this);

    oscBWModEnvKnob = createKnob("BWEA", -499, 499, 0, KnobMode::BipolarPercent);
    oscBWModEnvKnob->onValueChange = [this]() {
        setContinuousParam(cpWModBEnv, (float)oscBWModEnvKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*oscBWModEnvKnob);
    oscBWModEnvLabel = createLabel("ENV DEPTH", *this);

    oscSyncToggle = std::make_unique<juce::TextButton>("HARD SYNC");
    oscSyncToggle->setClickingTogglesState(true);
    oscSyncToggle->onClick = [this]() {
        setSteppedParam(spOscSync, oscSyncToggle->getToggleState() ? 1 : 0);
    };
    addAndMakeVisible(*oscSyncToggle);

    // Dedicated Waveform Editors for OSC A and OSC B
    waveformEditorA = std::make_unique<WaveformEditorComponent>(model, abxAMain);
    waveformEditorA->onWaveformChanged = [this]() {
        if (waveformEditorA) waveformEditorA->repaint();
    };
    waveformEditorA->onOpenWaveBrowser = [this](abx_t osc) {
        if (context.openWaveBrowser) context.openWaveBrowser(osc);
    };
    addAndMakeVisible(*waveformEditorA);

    waveformEditorB = std::make_unique<WaveformEditorComponent>(model, abxBMain);
    waveformEditorB->onWaveformChanged = [this]() {
        if (waveformEditorB) waveformEditorB->repaint();
    };
    waveformEditorB->onOpenWaveBrowser = [this](abx_t osc) {
        if (context.openWaveBrowser) context.openWaveBrowser(osc);
    };
    addAndMakeVisible(*waveformEditorB);

    // Oscillator Engine Selection
    const char* engineNames[] = { "DUAL WAVETABLE", "ELEMENTS MODAL", "HYBRID" };
    for (int i = 0; i < 3; ++i) {
        oscEngineButtons[i] = std::make_unique<juce::TextButton>(engineNames[i]);
        oscEngineButtons[i]->setClickingTogglesState(true);
        oscEngineButtons[i]->setRadioGroupId(1000);
        oscEngineButtons[i]->onClick = [this, i]() {
            setSteppedParam(spOscEngine, (uint8_t)i);
            resized();
        };
        addAndMakeVisible(*oscEngineButtons[i]);
    }

    // Elements Modal Resonator Card & Controls
    addAndMakeVisible(elementsCard);
    elementsCard.toBack();

    const char* elModelNames[] = { "MODAL 64", "NON-LIN STRING", "CHORDS", "OMINOUS CHOIR" };
    for (int i = 0; i < 4; ++i) {
        elementsModelButtons[i] = std::make_unique<juce::TextButton>(elModelNames[i]);
        elementsModelButtons[i]->setClickingTogglesState(true);
        elementsModelButtons[i]->setRadioGroupId(1003);
        elementsModelButtons[i]->onClick = [this, i]() {
            setSteppedParam(spElementsModel, (uint8_t)i);
        };
        addAndMakeVisible(*elementsModelButtons[i]);
    }

    elementsGeometryKnob = createKnob("ElGeom", 0, 999, 250, KnobMode::Percent);
    elementsGeometryKnob->onValueChange = [this]() { setContinuousParam(cpElementsGeometry, (float)elementsGeometryKnob->getValue()); };
    addAndMakeVisible(*elementsGeometryKnob);
    elementsGeometryLabel = createLabel("GEOMETRY", *this);

    elementsBrightnessKnob = createKnob("ElBright", 0, 999, 500, KnobMode::Percent);
    elementsBrightnessKnob->onValueChange = [this]() { setContinuousParam(cpElementsBrightness, (float)elementsBrightnessKnob->getValue()); };
    addAndMakeVisible(*elementsBrightnessKnob);
    elementsBrightnessLabel = createLabel("BRIGHTNESS", *this);

    elementsDampingKnob = createKnob("ElDamp", 0, 999, 300, KnobMode::Percent);
    elementsDampingKnob->onValueChange = [this]() { setContinuousParam(cpElementsDamping, (float)elementsDampingKnob->getValue()); };
    addAndMakeVisible(*elementsDampingKnob);
    elementsDampingLabel = createLabel("DAMPING", *this);

    elementsPositionKnob = createKnob("ElPos", 0, 999, 400, KnobMode::Percent);
    elementsPositionKnob->onValueChange = [this]() { setContinuousParam(cpElementsPosition, (float)elementsPositionKnob->getValue()); };
    addAndMakeVisible(*elementsPositionKnob);
    elementsPositionLabel = createLabel("POSITION", *this);

    elementsSpaceKnob = createKnob("ElSpace", 0, 999, 200, KnobMode::Percent);
    elementsSpaceKnob->onValueChange = [this]() { setContinuousParam(cpElementsSpace, (float)elementsSpaceKnob->getValue()); };
    addAndMakeVisible(*elementsSpaceKnob);
    elementsSpaceLabel = createLabel("SPACE", *this);

    elementsBowKnob = createKnob("ElBow", 0, 999, 0, KnobMode::Percent);
    elementsBowKnob->onValueChange = [this]() { setContinuousParam(cpElementsBow, (float)elementsBowKnob->getValue()); };
    addAndMakeVisible(*elementsBowKnob);
    elementsBowLabel = createLabel("BOW", *this);

    elementsBlowKnob = createKnob("ElBlow", 0, 999, 0, KnobMode::Percent);
    elementsBlowKnob->onValueChange = [this]() { setContinuousParam(cpElementsBlow, (float)elementsBlowKnob->getValue()); };
    addAndMakeVisible(*elementsBlowKnob);
    elementsBlowLabel = createLabel("BLOW", *this);

    elementsStrikeKnob = createKnob("ElStrike", 0, 999, 800, KnobMode::Percent);
    elementsStrikeKnob->onValueChange = [this]() { setContinuousParam(cpElementsStrike, (float)elementsStrikeKnob->getValue()); };
    addAndMakeVisible(*elementsStrikeKnob);
    elementsStrikeLabel = createLabel("STRIKE", *this);

    elementsMalletKnob = createKnob("ElMallet", 0, 999, 500, KnobMode::Percent);
    elementsMalletKnob->onValueChange = [this]() { setContinuousParam(cpElementsMallet, (float)elementsMalletKnob->getValue()); };
    addAndMakeVisible(*elementsMalletKnob);
    elementsMalletLabel = createLabel("MALLET", *this);

    assignComponentIDs();
}

void OscillatorTab::assignComponentIDs() {
    oscACard.setComponentID("oscACard");
    oscBCard.setComponentID("oscBCard");
    elementsCard.setComponentID("elementsCard");

    // Tab 1: Osc A
    if (oscAVolKnob) oscAVolKnob->setComponentID("oscAVolKnob");
    if (oscAVolLabel) oscAVolLabel->setComponentID("oscAVolLabel");
    if (oscAFreqKnob) oscAFreqKnob->setComponentID("oscAFreqKnob");
    if (oscAFreqLabel) oscAFreqLabel->setComponentID("oscAFreqLabel");
    for (int i = 0; i < 7; ++i) {
        if (oscAWModButtons[i]) oscAWModButtons[i]->setComponentID("oscAWModButton[" + juce::String(i) + "]");
    }
    if (oscAWModKnob) oscAWModKnob->setComponentID("oscAWModKnob");
    if (oscAWModLabel) oscAWModLabel->setComponentID("oscAWModLabel");
    if (oscAWModEnvKnob) oscAWModEnvKnob->setComponentID("oscAWModEnvKnob");
    if (oscAWModEnvLabel) oscAWModEnvLabel->setComponentID("oscAWModEnvLabel");

    // Tab 1: Osc B & Master
    if (oscBVolKnob) oscBVolKnob->setComponentID("oscBVolKnob");
    if (oscBVolLabel) oscBVolLabel->setComponentID("oscBVolLabel");
    if (oscBFreqKnob) oscBFreqKnob->setComponentID("oscBFreqKnob");
    if (oscBFreqLabel) oscBFreqLabel->setComponentID("oscBFreqLabel");
    if (oscBDetuneKnob) oscBDetuneKnob->setComponentID("oscBDetuneKnob");
    if (oscBDetuneLabel) oscBDetuneLabel->setComponentID("oscBDetuneLabel");
    for (int i = 0; i < 7; ++i) {
        if (oscBWModButtons[i]) oscBWModButtons[i]->setComponentID("oscBWModButton[" + juce::String(i) + "]");
    }
    if (oscSyncToggle) oscSyncToggle->setComponentID("oscSyncToggle");
    if (oscBWModKnob) oscBWModKnob->setComponentID("oscBWModKnob");
    if (oscBWModLabel) oscBWModLabel->setComponentID("oscBWModLabel");
    if (oscBWModEnvKnob) oscBWModEnvKnob->setComponentID("oscBWModEnvKnob");
    if (oscBWModEnvLabel) oscBWModEnvLabel->setComponentID("oscBWModEnvLabel");
    if (waveformEditorA) waveformEditorA->setupSubComponentIDs("waveformEditorA");
    if (waveformEditorB) waveformEditorB->setupSubComponentIDs("waveformEditorB");
}

void OscillatorTab::updateFromEngine() {
    const auto& preset = model.getCurrentPreset();

    safeSetKnob(oscAVolKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAVol]));
    safeSetKnob(oscAFreqKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAFreq]) - 500);
    safeSetKnob(oscAWModKnob.get(), scan_potFrom16bits(preset.continuousParams[cpABaseWMod]) - 500);
    safeSetKnob(oscAWModEnvKnob.get(), scan_potFrom16bits(preset.continuousParams[cpWModAEnv]) - 500);
    int aType = preset.steppedParams[spAWModType];
    for (int i = 0; i < 7; ++i) {
        if (oscAWModButtons[i]) oscAWModButtons[i]->setToggleState(i == aType, juce::dontSendNotification);
    }

    safeSetKnob(oscBVolKnob.get(), scan_potFrom16bits(preset.continuousParams[cpBVol]));
    safeSetKnob(oscBFreqKnob.get(), scan_potFrom16bits(preset.continuousParams[cpBFreq]) - 500);
    safeSetKnob(oscBDetuneKnob.get(), scan_potFrom16bits(preset.continuousParams[cpDetune]) - 500);
    safeSetKnob(oscBWModKnob.get(), scan_potFrom16bits(preset.continuousParams[cpBBaseWMod]) - 500);
    safeSetKnob(oscBWModEnvKnob.get(), scan_potFrom16bits(preset.continuousParams[cpWModBEnv]) - 500);
    int bType = preset.steppedParams[spBWModType];
    for (int i = 0; i < 7; ++i) {
        if (oscBWModButtons[i]) oscBWModButtons[i]->setToggleState(i == bType, juce::dontSendNotification);
    }
    safeSetToggle(oscSyncToggle.get(), preset.steppedParams[spOscSync] != 0);

    // Oscillator Engine & Elements Modal Resonator
    uint8_t oscEngine = preset.steppedParams[spOscEngine];
    for (int i = 0; i < 3; ++i) {
        if (oscEngineButtons[i])
            oscEngineButtons[i]->setToggleState(i == oscEngine, juce::dontSendNotification);
    }

    uint8_t elModel = preset.steppedParams[spElementsModel];
    for (int i = 0; i < 4; ++i) {
        if (elementsModelButtons[i])
            elementsModelButtons[i]->setToggleState(i == elModel, juce::dontSendNotification);
    }

    safeSetKnob(elementsGeometryKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsGeometry]));
    safeSetKnob(elementsBrightnessKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsBrightness]));
    safeSetKnob(elementsDampingKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsDamping]));
    safeSetKnob(elementsPositionKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsPosition]));
    safeSetKnob(elementsSpaceKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsSpace]));
    safeSetKnob(elementsBowKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsBow]));
    safeSetKnob(elementsBlowKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsBlow]));
    safeSetKnob(elementsStrikeKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsStrike]));
    safeSetKnob(elementsMalletKnob.get(), scan_potFrom16bits(preset.continuousParams[cpElementsMallet]));

    if (waveformEditorA) {
        waveformEditorA->refreshPresetDisplay();
        waveformEditorA->repaint();
    }
    if (waveformEditorB) {
        waveformEditorB->refreshPresetDisplay();
        waveformEditorB->repaint();
    }
}

void OscillatorTab::resized() {
    const auto tabBounds = getLocalBounds();

    int colGap = 5;
    int colW = (tabBounds.getWidth() - colGap) / 2;
    int col1X = 0;
    int col2X = col1X + colW + colGap;

    // Top Header: Oscillator Engine Mode Selector
    int engineY = 0;
    int engineH = 24;
    int btnW = 140;
    if (oscEngineButtons[0]) oscEngineButtons[0]->setBounds(col1X, engineY, btnW, engineH);
    if (oscEngineButtons[1]) oscEngineButtons[1]->setBounds(col1X + btnW + 6, engineY, btnW + 10, engineH);
    if (oscEngineButtons[2]) oscEngineButtons[2]->setBounds(col1X + (btnW * 2) + 22, engineY, 90, engineH);

    int cardTopY = engineY + engineH + 6;
    int cardTopH = 208;

    oscACard.setBounds(col1X, cardTopY, colW, cardTopH);
    oscACard.clearDividers();
    oscACard.addDivider(20, "PITCH, LEVEL & WAVEMODULATION");
    oscACard.addDivider(122, "WAVEMODULATION SELECTOR");

    oscBCard.setBounds(col2X, cardTopY, colW, cardTopH);
    oscBCard.clearDividers();
    oscBCard.addDivider(20, "PITCH, DETUNE, LEVEL & WAVEMODULATION");
    oscBCard.addDivider(122, "WAVEMODULATION SELECTOR & HARD SYNC");

    int knobSz = getStandardKnobSize(); // 55px hardware standard
    int knobY1 = cardTopY + 36;

    // Column 1: Osc A Controls (4 Knobs in Row 1)
    int knob4SlotW = (colW - 30) / 4;
    layoutKnob(oscAVolKnob.get(), oscAVolLabel, col1X + 15 + (knob4SlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(oscAFreqKnob.get(), oscAFreqLabel, col1X + 15 + knob4SlotW + (knob4SlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(oscAWModKnob.get(), oscAWModLabel, col1X + 15 + knob4SlotW * 2 + (knob4SlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(oscAWModEnvKnob.get(), oscAWModEnvLabel, col1X + 15 + knob4SlotW * 3 + (knob4SlotW - knobSz) / 2, knobY1, knobSz);

    int gap = 4;
    int togColW = (colW - 30 - gap * 3) / 4;
    int togY1 = cardTopY + 142;
    int togY2 = cardTopY + 170;
    int togH = 22;

    // 2-row wavemod selector strip for Osc A:
    for (int i = 0; i < 4; ++i) {
        if (oscAWModButtons[i])
            oscAWModButtons[i]->setBounds(col1X + 15 + i * (togColW + gap), togY1, togColW, togH);
    }
    for (int i = 4; i < 7; ++i) {
        int col = i - 3;
        if (oscAWModButtons[i])
            oscAWModButtons[i]->setBounds(col1X + 15 + col * (togColW + gap), togY2, togColW, togH);
    }

    // Column 2: Osc B Controls (5 Knobs in Row 1)
    int knob5SlotW = (colW - 30) / 5;
    layoutKnob(oscBVolKnob.get(), oscBVolLabel, col2X + 15 + (knob5SlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(oscBFreqKnob.get(), oscBFreqLabel, col2X + 15 + knob5SlotW + (knob5SlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(oscBDetuneKnob.get(), oscBDetuneLabel, col2X + 15 + knob5SlotW * 2 + (knob5SlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(oscBWModKnob.get(), oscBWModLabel, col2X + 15 + knob5SlotW * 3 + (knob5SlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(oscBWModEnvKnob.get(), oscBWModEnvLabel, col2X + 15 + knob5SlotW * 4 + (knob5SlotW - knobSz) / 2, knobY1, knobSz);

    // 2-row wavemod selector strip for Osc B:
    for (int i = 0; i < 4; ++i) {
        if (oscBWModButtons[i])
            oscBWModButtons[i]->setBounds(col2X + 15 + i * (togColW + gap), togY1, togColW, togH);
    }
    if (oscSyncToggle)
        oscSyncToggle->setBounds(col2X + 15, togY2, togColW, togH);
    for (int i = 4; i < 7; ++i) {
        int col = i - 3;
        if (oscBWModButtons[i])
            oscBWModButtons[i]->setBounds(col2X + 15 + col * (togColW + gap), togY2, togColW, togH);
    }

    // Bottom area: Waveform Editors or Elements Modal Resonator Card
    int bottomGap = 6;
    int bottomY = cardTopY + cardTopH + bottomGap;
    int bottomH = tabBounds.getHeight() - bottomY;

    uint8_t currentEngine = model.getCurrentPreset().steppedParams[spOscEngine];
    bool isElements = (currentEngine == oeElements);
    bool isHybrid = (currentEngine == oeHybrid);

    if (bottomH > 40) {
        if (isElements) {
            // Full-width Elements card
            if (waveformEditorA) waveformEditorA->setVisible(false);
            if (waveformEditorB) waveformEditorB->setVisible(false);
            layoutElementsCard({ 0, bottomY, tabBounds.getWidth(), bottomH });
        } else if (isHybrid) {
            // Split: waveform editor A on the left, Elements card on the right
            if (waveformEditorA) {
                waveformEditorA->setVisible(true);
                waveformEditorA->setBounds(col1X, bottomY, colW, bottomH);
            }
            if (waveformEditorB) waveformEditorB->setVisible(false);
            layoutElementsCard({ col2X, bottomY, colW, bottomH });
        } else {
            // Classic Dual Wavetable
            elementsCard.setVisible(false);
            setElementsControlsVisible(false);

            int waveW = (tabBounds.getWidth() - colGap) / 2;
            if (waveformEditorA) {
                waveformEditorA->setVisible(true);
                waveformEditorA->setBounds(col1X, bottomY, waveW, bottomH);
            }
            if (waveformEditorB) {
                waveformEditorB->setVisible(true);
                waveformEditorB->setBounds(col2X, bottomY, waveW, bottomH);
            }
        }
    }
}

void OscillatorTab::setElementsControlsVisible(bool visible) {
    for (auto& button : elementsModelButtons)
        if (button) button->setVisible(visible);
    for (auto* knob : { &elementsGeometryKnob, &elementsBrightnessKnob, &elementsDampingKnob,
                        &elementsPositionKnob, &elementsSpaceKnob, &elementsBowKnob,
                        &elementsBlowKnob, &elementsStrikeKnob, &elementsMalletKnob })
        if (*knob) (*knob)->setVisible(visible);
    for (auto* label : { &elementsGeometryLabel, &elementsBrightnessLabel, &elementsDampingLabel,
                         &elementsPositionLabel, &elementsSpaceLabel, &elementsBowLabel,
                         &elementsBlowLabel, &elementsStrikeLabel, &elementsMalletLabel })
        if (*label) (*label)->setVisible(visible);
}

// Elements card in signal-flow order: MODEL, then the EXCITER that drives the
// RESONATOR (with SPACE as its stereo output). Every row starts below its
// divider label, so captions never run into the next section.
void OscillatorTab::layoutElementsCard(juce::Rectangle<int> bounds) {
    elementsCard.setVisible(true);
    elementsCard.setBounds(bounds);
    elementsCard.clearDividers();
    setElementsControlsVisible(true);

    constexpr int margin = 14;
    constexpr int labelH = 16;
    constexpr int dividerToContent = 12;
    const int cardX = bounds.getX();
    const int cardY = bounds.getY();
    const int innerW = bounds.getWidth() - 2 * margin;

    // Row 1: model selector
    const int modelDivY = 38;
    elementsCard.addDivider(modelDivY, "MODEL");
    constexpr int btnGap = 6, btnH = 24;
    const int btnW = (innerW - 3 * btnGap) / 4;
    const int btnY = cardY + modelDivY + dividerToContent;
    for (int i = 0; i < 4; ++i)
        if (elementsModelButtons[i])
            elementsModelButtons[i]->setBounds(cardX + margin + i * (btnW + btnGap), btnY, btnW, btnH);

    // Rows 2 and 3 share the remaining height: divider, knob, caption, air.
    const int rowsTop = modelDivY + dividerToContent + btnH + 18;
    const int rowH = (bounds.getHeight() - rowsTop - 8) / 2;
    const int knobSz = juce::jmin(getStandardKnobSize(), rowH - dividerToContent - labelH - 12);

    auto layoutRow = [&](int divY, const juce::String& title, auto& knobs, auto& labels) {
        elementsCard.addDivider(divY, title);
        const int count = (int)knobs.size();
        const int slotW = innerW / count;
        const int knobY = cardY + divY + dividerToContent + 2;
        for (int i = 0; i < count; ++i)
            layoutKnob(knobs[(size_t)i]->get(), *labels[(size_t)i],
                       cardX + margin + i * slotW + (slotW - knobSz) / 2, knobY, knobSz);
    };

    std::array<std::unique_ptr<juce::Slider>*, 4> exciterKnobs{
        &elementsBowKnob, &elementsBlowKnob, &elementsStrikeKnob, &elementsMalletKnob };
    std::array<std::unique_ptr<juce::Label>*, 4> exciterLabels{
        &elementsBowLabel, &elementsBlowLabel, &elementsStrikeLabel, &elementsMalletLabel };
    std::array<std::unique_ptr<juce::Slider>*, 5> resonatorKnobs{
        &elementsGeometryKnob, &elementsBrightnessKnob, &elementsDampingKnob,
        &elementsPositionKnob, &elementsSpaceKnob };
    std::array<std::unique_ptr<juce::Label>*, 5> resonatorLabels{
        &elementsGeometryLabel, &elementsBrightnessLabel, &elementsDampingLabel,
        &elementsPositionLabel, &elementsSpaceLabel };

    layoutRow(rowsTop, "EXCITER", exciterKnobs, exciterLabels);
    layoutRow(rowsTop + rowH, "RESONATOR & SPACE", resonatorKnobs, resonatorLabels);
}
