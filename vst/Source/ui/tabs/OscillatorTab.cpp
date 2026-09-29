#include "OscillatorTab.h"

#include <array>

namespace {
enum class ElementsGroup { plain, blow, strike };
struct ElementsKnobDef {
    const char* id;        // component IDs "elements<id>Knob" / "elements<id>Label"
    const char* name;
    const char* caption;
    continuousParameter_t cp;
    int init;
    ElementsGroup group;
};
// In OscillatorTab::ElementsKnob order: exciter (contour, levels, flow and
// mallet, timbres), then resonator.
const ElementsKnobDef kElementsKnobs[] = {
    { "Contour", "ElContour", "CONTOUR", cpElementsContour, 500, ElementsGroup::plain },
    { "Bow", "ElBow", "BOW", cpElementsBow, 0, ElementsGroup::plain },
    { "Blow", "ElBlow", "BLOW", cpElementsBlow, 0, ElementsGroup::blow },
    { "Strike", "ElStrike", "STRIKE", cpElementsStrike, 800, ElementsGroup::strike },
    { "Flow", "ElFlow", "FLOW", cpElementsFlow, 500, ElementsGroup::blow },
    { "Mallet", "ElMallet", "MALLET", cpElementsMallet, 500, ElementsGroup::strike },
    { "BowTimbre", "ElBowTimbre", "TIMBRE", cpElementsBowTimbre, 500, ElementsGroup::plain },
    { "BlowTimbre", "ElBlowTimbre", "TIMBRE", cpElementsBlowTimbre, 500, ElementsGroup::blow },
    { "StrikeTimbre", "ElStrikeTimbre", "TIMBRE", cpElementsStrikeTimbre, 500, ElementsGroup::strike },
    { "Geometry", "ElGeom", "GEOMETRY", cpElementsGeometry, 250, ElementsGroup::plain },
    { "Brightness", "ElBright", "BRIGHTNESS", cpElementsBrightness, 500, ElementsGroup::plain },
    { "Damping", "ElDamp", "DAMPING", cpElementsDamping, 300, ElementsGroup::plain },
    { "Position", "ElPos", "POSITION", cpElementsPosition, 400, ElementsGroup::plain },
    { "Space", "ElSpace", "SPACE", cpElementsSpace, 200, ElementsGroup::plain },
};
}

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

    oscAFreqKnob = createKnob("AFreq", 0, 999, 0, KnobMode::PitchSemitones);
    oscAFreqKnob->onValueChange = [this]() {
        setContinuousParam(cpAFreq, (float)oscAFreqKnob->getValue());
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

    oscBFreqKnob = createKnob("BFreq", 0, 999, 0, KnobMode::PitchSemitones);
    oscBFreqKnob->onValueChange = [this]() {
        setContinuousParam(cpBFreq, (float)oscBFreqKnob->getValue());
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

    for (int k = 0; k < elKnobCount; ++k) {
        const auto& d = kElementsKnobs[k];
        elementsKnobs[(size_t)k] = createParamKnob(elementsLabels[(size_t)k], d.caption, d.name, 0, 999, d.init,
                                                   KnobMode::Percent, d.cp);
        // As on the Elements panel: hardware knobs with white caps, the blow
        // controls red, the strike controls teal (cap and caption).
        auto& props = elementsKnobs[(size_t)k]->getProperties();
        props.set("knobStyle", "elements");
        if (d.group == ElementsGroup::plain) continue;
        const juce::Colour colour = d.group == ElementsGroup::blow ? juce::Colour(0xffe0195f) : juce::Colour(0xff0aa6c0);
        props.set("capColour", (juce::int64)colour.getARGB());
        elementsLabels[(size_t)k]->setColour(juce::Label::textColourId, colour);
    }

    assignComponentIDs();
}

void OscillatorTab::assignComponentIDs() {
    oscACard.setComponentID("oscACard");
    oscBCard.setComponentID("oscBCard");
    elementsCard.setComponentID("elementsCard");
    for (int k = 0; k < elKnobCount; ++k) {
        const juce::String id = juce::String("elements") + kElementsKnobs[k].id;
        if (elementsKnobs[(size_t)k]) elementsKnobs[(size_t)k]->setComponentID(id + "Knob");
        if (elementsLabels[(size_t)k]) elementsLabels[(size_t)k]->setComponentID(id + "Label");
    }
    for (int i = 0; i < 4; ++i)
        if (elementsModelButtons[i]) elementsModelButtons[i]->setComponentID("elementsModelButton[" + juce::String(i) + "]");

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
    safeSetKnob(oscAFreqKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAFreq]));
    safeSetKnob(oscAWModKnob.get(), scan_potFrom16bits(preset.continuousParams[cpABaseWMod]) - 500);
    safeSetKnob(oscAWModEnvKnob.get(), scan_potFrom16bits(preset.continuousParams[cpWModAEnv]) - 500);
    int aType = preset.steppedParams[spAWModType];
    for (int i = 0; i < 7; ++i) {
        if (oscAWModButtons[i]) oscAWModButtons[i]->setToggleState(i == aType, juce::dontSendNotification);
    }

    safeSetKnob(oscBVolKnob.get(), scan_potFrom16bits(preset.continuousParams[cpBVol]));
    safeSetKnob(oscBFreqKnob.get(), scan_potFrom16bits(preset.continuousParams[cpBFreq]));
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

    for (int k = 0; k < elKnobCount; ++k)
        safeSetKnob(elementsKnobs[(size_t)k].get(), scan_potFrom16bits(preset.continuousParams[kElementsKnobs[k].cp]));

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
    for (int k = 0; k < elKnobCount; ++k) {
        if (elementsKnobs[(size_t)k]) elementsKnobs[(size_t)k]->setVisible(visible);
        if (elementsLabels[(size_t)k]) elementsLabels[(size_t)k]->setVisible(visible);
    }
}

// Elements card arranged like the Elements panel, in its two knob sizes.
// Exciter on the left: CONTOUR, BOW, BLOW, STRIKE (medium), FLOW and MALLET
// (large), the three TIMBRE knobs (medium). Resonator on the right: the model
// selector where the panel has COARSE / FINE / FM (the pitch comes from
// Osc A), GEOMETRY and BRIGHTNESS (large), DAMPING, POSITION, SPACE (medium).
// The rows line up across both halves.
void OscillatorTab::layoutElementsCard(juce::Rectangle<int> bounds) {
    elementsCard.setVisible(true);
    elementsCard.setBounds(bounds);
    elementsCard.clearDividers();
    setElementsControlsVisible(true);

    constexpr int margin = 14, labelH = 16, dividerY = 34, dividerToContent = 12;
    const int cardX = bounds.getX(), cardY = bounds.getY();
    const int halfW = bounds.getWidth() / 2;
    const int halfInnerW = halfW - 2 * margin;
    const int leftX = cardX + margin, rightX = cardX + halfW + margin;
    elementsCard.addDivider(margin, dividerY, halfInnerW, "EXCITER");
    elementsCard.addDivider(halfW + margin, dividerY, halfInnerW, "RESONATOR & SPACE");
    elementsCard.addVerticalDivider(halfW, dividerY + 10, bounds.getHeight() - 8);

    // Three rows: medium, large, medium knobs, centred in the height.
    const int top = cardY + dividerY + dividerToContent;
    const int avail = cardY + bounds.getHeight() - 8 - top;
    const int medium = getStandardKnobSize();
    const int large = juce::jlimit(medium, 80, avail - 2 * medium - 3 * labelH - 8);
    const int content = 2 * (medium + labelH) + large + labelH;
    const int gap = juce::jlimit(4, 18, (avail - content) / 2);
    const int row1 = top + juce::jmax(0, (avail - content - 2 * gap) / 2);
    const int row2 = row1 + medium + labelH + gap;
    const int row3 = row2 + large + labelH + gap;

    // Knobs centred in equal slots across [x, x + w).
    auto row = [&](int x, int w, int y, int size, std::initializer_list<ElementsKnob> knobs) {
        const int slotW = w / (int)knobs.size();
        int i = 0;
        for (auto k : knobs)
            layoutKnob(elementsKnobs[(size_t)k].get(), elementsLabels[(size_t)k],
                       x + (i++) * slotW + (slotW - size) / 2, y, size);
    };
    row(leftX, halfInnerW, row1, medium, { elContour, elBow, elBlow, elStrike });
    row(leftX, halfInnerW, row2, large, { elFlow, elMallet });
    row(leftX, halfInnerW, row3, medium, { elBowTimbre, elBlowTimbre, elStrikeTimbre });
    row(rightX, halfInnerW, row2, large, { elGeometry, elBrightness });
    row(rightX, halfInnerW, row3, medium, { elDamping, elPosition, elSpace });

    // Model selector in the first row of the right half: one row of four
    // buttons when they fit, else two by two.
    constexpr int btnGap = 6, btnH = 22;
    const int columns = halfInnerW >= 4 * 100 + 3 * btnGap ? 4 : 2;
    const int rows = 4 / columns;
    const int btnW = (halfInnerW - (columns - 1) * btnGap) / columns;
    const int blockH = rows * btnH + (rows - 1) * btnGap;
    const int btnY = row1 + (medium + labelH - blockH) / 2;
    for (int i = 0; i < 4; ++i)
        if (elementsModelButtons[i])
            elementsModelButtons[i]->setBounds(rightX + (i % columns) * (btnW + btnGap),
                                               btnY + (i / columns) * (btnH + btnGap), btnW, btnH);
}
