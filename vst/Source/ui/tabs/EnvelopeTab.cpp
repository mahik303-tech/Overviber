#include "EnvelopeTab.h"

EnvelopeTab::EnvelopeTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void EnvelopeTab::setup() {
    addAndMakeVisible(filEnvCard);
    addAndMakeVisible(ampEnvCard);
    addAndMakeVisible(wmodEnvCard);
    filEnvCard.toBack();
    ampEnvCard.toBack();
    wmodEnvCard.toBack();

    // Filter Envelope
    filAttKnob = createKnob("FAtk", 0, 999, 0, KnobMode::TimeMs);
    filAttKnob->onValueChange = [this]() { setContinuousParam(cpFilAtt, (float)filAttKnob->getValue()); };
    addAndMakeVisible(*filAttKnob);
    filAttLabel = createLabel("ATTACK", *this);

    filDecKnob = createKnob("FDec", 0, 999, 500, KnobMode::TimeMs);
    filDecKnob->onValueChange = [this]() { setContinuousParam(cpFilDec, (float)filDecKnob->getValue()); };
    addAndMakeVisible(*filDecKnob);
    filDecLabel = createLabel("DECAY", *this);

    filSusKnob = createKnob("FSus", 0, 999, 500, KnobMode::Percent);
    filSusKnob->onValueChange = [this]() { setContinuousParam(cpFilSus, (float)filSusKnob->getValue()); };
    addAndMakeVisible(*filSusKnob);
    filSusLabel = createLabel("SUSTAIN", *this);

    filRelKnob = createKnob("FRel", 0, 999, 500, KnobMode::TimeMs);
    filRelKnob->onValueChange = [this]() { setContinuousParam(cpFilRel, (float)filRelKnob->getValue()); };
    addAndMakeVisible(*filRelKnob);
    filRelLabel = createLabel("RELEASE", *this);

    filVelKnob = createKnob("FVel", 0, 999, 0, KnobMode::Percent);
    filVelKnob->onValueChange = [this]() { setContinuousParam(cpFilVelocity, (float)filVelKnob->getValue()); };
    addAndMakeVisible(*filVelKnob);
    filVelLabel = createLabel("SENSITIVITY", *this);

    // Amp Envelope
    ampAttKnob = createKnob("AAtk", 0, 999, 0, KnobMode::TimeMs);
    ampAttKnob->onValueChange = [this]() { setContinuousParam(cpAmpAtt, (float)ampAttKnob->getValue()); };
    addAndMakeVisible(*ampAttKnob);
    ampAttLabel = createLabel("ATTACK", *this);

    ampDecKnob = createKnob("ADec", 0, 999, 0, KnobMode::TimeMs);
    ampDecKnob->onValueChange = [this]() { setContinuousParam(cpAmpDec, (float)ampDecKnob->getValue()); };
    addAndMakeVisible(*ampDecKnob);
    ampDecLabel = createLabel("DECAY", *this);

    ampSusKnob = createKnob("ASus", 0, 999, 999, KnobMode::Percent);
    ampSusKnob->onValueChange = [this]() { setContinuousParam(cpAmpSus, (float)ampSusKnob->getValue()); };
    addAndMakeVisible(*ampSusKnob);
    ampSusLabel = createLabel("SUSTAIN", *this);

    ampRelKnob = createKnob("ARel", 0, 999, 500, KnobMode::TimeMs);
    ampRelKnob->onValueChange = [this]() { setContinuousParam(cpAmpRel, (float)ampRelKnob->getValue()); };
    addAndMakeVisible(*ampRelKnob);
    ampRelLabel = createLabel("RELEASE", *this);

    ampVelKnob = createKnob("AVel", 0, 999, 0, KnobMode::Percent);
    ampVelKnob->onValueChange = [this]() { setContinuousParam(cpAmpVelocity, (float)ampVelKnob->getValue()); };
    addAndMakeVisible(*ampVelKnob);
    ampVelLabel = createLabel("SENSITIVITY", *this);

    const char* ampEnvTypeNames[4] = { "Fast Exponential", "Slow Exponential (x4)", "Fast Linear", "Slow Linear (x4)" };
    for (int i = 0; i < 4; ++i) {
        ampEnvTypeToggles[i] = createToggle(ampEnvTypeNames[i]);
        ampEnvTypeToggles[i]->setRadioGroupId(1301);
        ampEnvTypeToggles[i]->onClick = [this, i]() {
            setSteppedParam(spAmpEnvSlow, (i & 1) ? 1 : 0);
            setSteppedParam(spAmpEnvLin, (i & 2) ? 1 : 0);
        };
        addAndMakeVisible(*ampEnvTypeToggles[i]);
    }

    ampEnvLoopToggle = std::make_unique<juce::ToggleButton>("LOOP ENVELOPE");
    ampEnvLoopToggle->onClick = [this]() {
        setSteppedParam(spAmpEnvLoop, ampEnvLoopToggle->getToggleState() ? 1 : 0);
    };
    addAndMakeVisible(*ampEnvLoopToggle);

    // WaveMod Envelope
    wmodAttKnob = createKnob("WAtk", 0, 999, 0, KnobMode::TimeMs);
    wmodAttKnob->onValueChange = [this]() { setContinuousParam(cpWModAtt, (float)wmodAttKnob->getValue()); };
    addAndMakeVisible(*wmodAttKnob);
    wmodAttLabel = createLabel("ATTACK", *this);

    wmodDecKnob = createKnob("WDec", 0, 999, 500, KnobMode::TimeMs);
    wmodDecKnob->onValueChange = [this]() { setContinuousParam(cpWModDec, (float)wmodDecKnob->getValue()); };
    addAndMakeVisible(*wmodDecKnob);
    wmodDecLabel = createLabel("DECAY", *this);

    wmodSusKnob = createKnob("WSus", 0, 999, 500, KnobMode::Percent);
    wmodSusKnob->onValueChange = [this]() { setContinuousParam(cpWModSus, (float)wmodSusKnob->getValue()); };
    addAndMakeVisible(*wmodSusKnob);
    wmodSusLabel = createLabel("SUSTAIN", *this);

    wmodRelKnob = createKnob("WRel", 0, 999, 500, KnobMode::TimeMs);
    wmodRelKnob->onValueChange = [this]() { setContinuousParam(cpWModRel, (float)wmodRelKnob->getValue()); };
    addAndMakeVisible(*wmodRelKnob);
    wmodRelLabel = createLabel("RELEASE", *this);

    wmodVelKnob = createKnob("WVel", 0, 999, 0, KnobMode::Percent);
    wmodVelKnob->onValueChange = [this]() { setContinuousParam(cpWModVelocity, (float)wmodVelKnob->getValue()); };
    addAndMakeVisible(*wmodVelKnob);
    wmodVelLabel = createLabel("SENSITIVITY", *this);

    const char* wmodEnvTypeNames[4] = { "Fast Exponential", "Slow Exponential (x4)", "Fast Linear", "Slow Linear (x4)" };
    for (int i = 0; i < 4; ++i) {
        wmodEnvTypeToggles[i] = createToggle(wmodEnvTypeNames[i]);
        wmodEnvTypeToggles[i]->setRadioGroupId(1302);
        wmodEnvTypeToggles[i]->onClick = [this, i]() {
            setSteppedParam(spWModEnvSlow, (i & 1) ? 1 : 0);
            setSteppedParam(spWModEnvLin, (i & 2) ? 1 : 0);
        };
        addAndMakeVisible(*wmodEnvTypeToggles[i]);
    }

    wmodEnvLoopToggle = std::make_unique<juce::ToggleButton>("LOOP ENVELOPE");
    wmodEnvLoopToggle->onClick = [this]() {
        setSteppedParam(spWModEnvLoop, wmodEnvLoopToggle->getToggleState() ? 1 : 0);
    };
    addAndMakeVisible(*wmodEnvLoopToggle);

    // Interactive ADSR Curve Visualizers
    filAdsrCurve = std::make_unique<AdsrCurveComponent>(engine, *filAttKnob, *filDecKnob, *filSusKnob, *filRelKnob, "Filter ADSR Curve");
    addAndMakeVisible(*filAdsrCurve);

    ampAdsrCurve = std::make_unique<AdsrCurveComponent>(engine, *ampAttKnob, *ampDecKnob, *ampSusKnob, *ampRelKnob, "Amplifier / VCA ADSR Curve");
    addAndMakeVisible(*ampAdsrCurve);

    wmodAdsrCurve = std::make_unique<AdsrCurveComponent>(engine, *wmodAttKnob, *wmodDecKnob, *wmodSusKnob, *wmodRelKnob, "WaveMod ADSR Curve");
    addAndMakeVisible(*wmodAdsrCurve);

    assignComponentIDs();
}

void EnvelopeTab::assignComponentIDs() {
    filEnvCard.setComponentID("filEnvCard");
    ampEnvCard.setComponentID("ampEnvCard");
    wmodEnvCard.setComponentID("wmodEnvCard");

    if (filAttKnob) filAttKnob->setComponentID("filAttKnob");
    if (filAttLabel) filAttLabel->setComponentID("filAttLabel");
    if (filDecKnob) filDecKnob->setComponentID("filDecKnob");
    if (filDecLabel) filDecLabel->setComponentID("filDecLabel");
    if (filSusKnob) filSusKnob->setComponentID("filSusKnob");
    if (filSusLabel) filSusLabel->setComponentID("filSusLabel");
    if (filRelKnob) filRelKnob->setComponentID("filRelKnob");
    if (filRelLabel) filRelLabel->setComponentID("filRelLabel");
    if (filVelKnob) filVelKnob->setComponentID("filVelKnob");
    if (filVelLabel) filVelLabel->setComponentID("filVelLabel");
    if (filAdsrCurve) filAdsrCurve->setComponentID("filAdsrCurve");

    if (ampAttKnob) ampAttKnob->setComponentID("ampAttKnob");
    if (ampAttLabel) ampAttLabel->setComponentID("ampAttLabel");
    if (ampDecKnob) ampDecKnob->setComponentID("ampDecKnob");
    if (ampDecLabel) ampDecLabel->setComponentID("ampDecLabel");
    if (ampSusKnob) ampSusKnob->setComponentID("ampSusKnob");
    if (ampSusLabel) ampSusLabel->setComponentID("ampSusLabel");
    if (ampRelKnob) ampRelKnob->setComponentID("ampRelKnob");
    if (ampRelLabel) ampRelLabel->setComponentID("ampRelLabel");
    if (ampVelKnob) ampVelKnob->setComponentID("ampVelKnob");
    if (ampVelLabel) ampVelLabel->setComponentID("ampVelLabel");
    for (int i = 0; i < 4; ++i) {
        if (ampEnvTypeToggles[i]) ampEnvTypeToggles[i]->setComponentID("ampEnvTypeToggle[" + juce::String(i) + "]");
    }
    if (ampEnvLoopToggle) ampEnvLoopToggle->setComponentID("ampEnvLoopToggle");
    if (ampAdsrCurve) ampAdsrCurve->setComponentID("ampAdsrCurve");

    if (wmodAttKnob) wmodAttKnob->setComponentID("wmodAttKnob");
    if (wmodAttLabel) wmodAttLabel->setComponentID("wmodAttLabel");
    if (wmodDecKnob) wmodDecKnob->setComponentID("wmodDecKnob");
    if (wmodDecLabel) wmodDecLabel->setComponentID("wmodDecLabel");
    if (wmodSusKnob) wmodSusKnob->setComponentID("wmodSusKnob");
    if (wmodSusLabel) wmodSusLabel->setComponentID("wmodSusLabel");
    if (wmodRelKnob) wmodRelKnob->setComponentID("wmodRelKnob");
    if (wmodRelLabel) wmodRelLabel->setComponentID("wmodRelLabel");
    if (wmodVelKnob) wmodVelKnob->setComponentID("wmodVelKnob");
    if (wmodVelLabel) wmodVelLabel->setComponentID("wmodVelLabel");
    for (int i = 0; i < 4; ++i) {
        if (wmodEnvTypeToggles[i]) wmodEnvTypeToggles[i]->setComponentID("wmodEnvTypeToggle[" + juce::String(i) + "]");
    }
    if (wmodEnvLoopToggle) wmodEnvLoopToggle->setComponentID("wmodEnvLoopToggle");
    if (wmodAdsrCurve) wmodAdsrCurve->setComponentID("wmodAdsrCurve");
}

void EnvelopeTab::updateFromEngine() {
    const auto& preset = engine.getCurrentPreset();

    safeSetKnob(filAttKnob.get(), scan_potFrom16bits(preset.continuousParams[cpFilAtt]));
    safeSetKnob(filDecKnob.get(), scan_potFrom16bits(preset.continuousParams[cpFilDec]));
    safeSetKnob(filSusKnob.get(), scan_potFrom16bits(preset.continuousParams[cpFilSus]));
    safeSetKnob(filRelKnob.get(), scan_potFrom16bits(preset.continuousParams[cpFilRel]));
    safeSetKnob(filVelKnob.get(), scan_potFrom16bits(preset.continuousParams[cpFilVelocity]));

    safeSetKnob(ampAttKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAmpAtt]));
    safeSetKnob(ampDecKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAmpDec]));
    safeSetKnob(ampSusKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAmpSus]));
    safeSetKnob(ampRelKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAmpRel]));
    safeSetKnob(ampVelKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAmpVelocity]));
    safeSetToggle(ampEnvLoopToggle.get(), preset.steppedParams[spAmpEnvLoop] != 0);
    int aLin = preset.steppedParams[spAmpEnvLin] ? 2 : 0;
    int aSlow = preset.steppedParams[spAmpEnvSlow] ? 1 : 0;
    int ampEnvTypeId = aLin + aSlow;
    for (int i = 0; i < 4; ++i) {
        if (ampEnvTypeToggles[i])
            ampEnvTypeToggles[i]->setToggleState(i == ampEnvTypeId, juce::dontSendNotification);
    }

    safeSetKnob(wmodAttKnob.get(), scan_potFrom16bits(preset.continuousParams[cpWModAtt]));
    safeSetKnob(wmodDecKnob.get(), scan_potFrom16bits(preset.continuousParams[cpWModDec]));
    safeSetKnob(wmodSusKnob.get(), scan_potFrom16bits(preset.continuousParams[cpWModSus]));
    safeSetKnob(wmodRelKnob.get(), scan_potFrom16bits(preset.continuousParams[cpWModRel]));
    safeSetKnob(wmodVelKnob.get(), scan_potFrom16bits(preset.continuousParams[cpWModVelocity]));
    safeSetToggle(wmodEnvLoopToggle.get(), preset.steppedParams[spWModEnvLoop] != 0);
    int wLin = preset.steppedParams[spWModEnvLin] ? 2 : 0;
    int wSlow = preset.steppedParams[spWModEnvSlow] ? 1 : 0;
    int wmodEnvTypeId = wLin + wSlow;
    for (int i = 0; i < 4; ++i) {
        if (wmodEnvTypeToggles[i])
            wmodEnvTypeToggles[i]->setToggleState(i == wmodEnvTypeId, juce::dontSendNotification);
    }

    if (filAdsrCurve) filAdsrCurve->repaint();
    if (ampAdsrCurve) ampAdsrCurve->repaint();
    if (wmodAdsrCurve) wmodAdsrCurve->repaint();
}

void EnvelopeTab::resized() {
    const auto tabBounds = getLocalBounds();

    int colGap = 5;
    int colW = (tabBounds.getWidth() - colGap * 2) / 3;
    int col1X = 0;
    int col2X = col1X + colW + colGap;
    int col3X = col2X + colW + colGap;
    int cardTopH = 114;

    int velColW = 74;
    int adsrAreaW = colW - velColW - 14;
    int sepX = 6 + adsrAreaW + 2;

    auto setupCardDividers = [cardTopH, adsrAreaW, sepX, velColW](ModernSectionCard& card) {
        card.clearDividers();
        card.addDivider(6, 34, adsrAreaW, "ADSR");
        card.addDivider(sepX + 4, 34, velColW - 8, "VELOCITY");
        card.addVerticalDivider(sepX, 28, cardTopH - 6);
    };

    filEnvCard.setBounds(col1X, 0, colW, cardTopH);
    setupCardDividers(filEnvCard);

    ampEnvCard.setBounds(col2X, 0, colW, cardTopH);
    setupCardDividers(ampEnvCard);

    wmodEnvCard.setBounds(col3X, 0, colW, cardTopH);
    setupCardDividers(wmodEnvCard);

    auto layoutAdsrWithVel = [this, adsrAreaW, sepX, velColW](
        int startX,
        const auto& aKnob, const auto& aLbl,
        const auto& dKnob, const auto& dLbl,
        const auto& sKnob, const auto& sLbl,
        const auto& rKnob, const auto& rLbl,
        const auto& vKnob, const auto& vLbl)
    {
        int knob4W = adsrAreaW / 4;
        int knobSz = getStandardKnobSize();
        int knobY = 40;
        layoutKnob(aKnob.get(), aLbl, startX + 6 + (knob4W - knobSz) / 2, knobY, knobSz);
        layoutKnob(dKnob.get(), dLbl, startX + 6 + knob4W + (knob4W - knobSz) / 2, knobY, knobSz);
        layoutKnob(sKnob.get(), sLbl, startX + 6 + knob4W * 2 + (knob4W - knobSz) / 2, knobY, knobSz);
        layoutKnob(rKnob.get(), rLbl, startX + 6 + knob4W * 3 + (knob4W - knobSz) / 2, knobY, knobSz);

        int velX = startX + sepX + (velColW - knobSz) / 2;
        layoutKnob(vKnob.get(), vLbl, velX, knobY, knobSz);
    };

    // Filter ADSR + Velocity
    layoutAdsrWithVel(col1X, filAttKnob, filAttLabel, filDecKnob, filDecLabel,
                      filSusKnob, filSusLabel, filRelKnob, filRelLabel, filVelKnob, filVelLabel);

    // Amp ADSR + Velocity
    layoutAdsrWithVel(col2X, ampAttKnob, ampAttLabel, ampDecKnob, ampDecLabel,
                      ampSusKnob, ampSusLabel, ampRelKnob, ampRelLabel, ampVelKnob, ampVelLabel);

    // WaveMod ADSR + Velocity
    layoutAdsrWithVel(col3X, wmodAttKnob, wmodAttLabel, wmodDecKnob, wmodDecLabel,
                      wmodSusKnob, wmodSusLabel, wmodRelKnob, wmodRelLabel, wmodVelKnob, wmodVelLabel);

    // Interactive ADSR Curve Visualizers & Bottom Toggles
    int envCurveGap = 5;
    int envCurveY = cardTopH + envCurveGap;
    int totalBottomH = tabBounds.getHeight() - envCurveY;
    int togAreaH = 46; // 2 rows of toggles
    int curveH = std::max(40, totalBottomH - togAreaH - 6);

    if (filAdsrCurve) filAdsrCurve->setBounds(col1X, envCurveY, colW, curveH);
    if (ampAdsrCurve) ampAdsrCurve->setBounds(col2X, envCurveY, colW, curveH);
    if (wmodAdsrCurve) wmodAdsrCurve->setBounds(col3X, envCurveY, colW, curveH);

    auto layoutBottomEnvToggles = [](int startX, int cardW, int startY,
                                     const std::unique_ptr<juce::ToggleButton> typeToggles[4],
                                     const std::unique_ptr<juce::ToggleButton>& loopToggle)
    {
        int margin = 8;
        int availW = cardW - 2 * margin;
        int colGap = 6;
        int colW = (availW - colGap * 2) / 3;
        int c1X = startX + margin;
        int c2X = c1X + colW + colGap;
        int c3X = startX + cardW - margin - (colW + 4);
        int row1Y = startY;
        int row2Y = startY + 22;
        int togH = 20;

        if (typeToggles[0]) typeToggles[0]->setBounds(c1X, row1Y, colW, togH);
        if (typeToggles[2]) typeToggles[2]->setBounds(c2X, row1Y, colW, togH);
        if (loopToggle)     loopToggle->setBounds(c3X, row1Y, colW + 4, togH);

        if (typeToggles[1]) typeToggles[1]->setBounds(c1X, row2Y, colW, togH);
        if (typeToggles[3]) typeToggles[3]->setBounds(c2X, row2Y, colW, togH);
    };

    int togY = envCurveY + curveH + 6;
    layoutBottomEnvToggles(col2X, colW, togY, ampEnvTypeToggles, ampEnvLoopToggle);
    layoutBottomEnvToggles(col3X, colW, togY, wmodEnvTypeToggles, wmodEnvLoopToggle);
}
