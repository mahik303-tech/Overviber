#include "FilterVcaTab.h"

#include <algorithm>

FilterVcaTab::FilterVcaTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void FilterVcaTab::setup() {
    addAndMakeVisible(filterCard);
    filterCard.toBack();

    // Filter Model & Dynamic Mode Toggles (Vertical ToggleButton Groups)
    const char* filterModelNames[4] = { "SSI2144 Ladder", "SEM", "Shelves EQ / SVF", "SST Vintage Moog" };
    for (int i = 0; i < 4; ++i) {
        filterModelToggles[i] = createToggle(filterModelNames[i]);
        filterModelToggles[i]->setRadioGroupId(1201);
        filterModelToggles[i]->onClick = [this, i]() {
            setSelectedFilterModel(i);
            setSteppedParam(spFilterModel, (uint8_t)i);
            updateFilterModeToggles(i);
            int mode = getSelectedFilterMode();
            setSteppedParam(spFilterMode, (uint8_t)mode);
            updateFilterUIState(i, mode);
            if (semVariantCombo) semVariantCombo->setEnabled(i == fmSem);
            resized();
        };
        addAndMakeVisible(*filterModelToggles[i]);
    }

    // SEM variants (dsp/SemFilter.h); Liquid is the former Ripples filter.
    semVariantCombo = createCombo();
    const char* semVariantNames[] = { "OB-Xd 12 dB", "Oberheim", "Vult SVF", "Cytomic SVF", "Liquid" };
    for (int v = 0; v < 5; ++v) semVariantCombo->addItem(semVariantNames[v], v + 1);
    semVariantCombo->setSelectedId(1, juce::dontSendNotification);
    semVariantCombo->setTooltip("SEM filter model");
    semVariantCombo->onChange = [this]() {
        const int variant = semVariantCombo->getSelectedId() - 1;
        if (variant < 0) return;
        selectedSemVariant = variant;
        setSteppedParam(spSemModel, (uint8_t)variant);
        // Liquid offers other modes than the SVF variants.
        updateFilterModeToggles(getSelectedFilterModel());
        const int mode = getSelectedFilterMode();
        setSteppedParam(spFilterMode, (uint8_t)mode);
        updateFilterUIState(getSelectedFilterModel(), mode);
    };
    addAndMakeVisible(*semVariantCombo);

    for (int i = 0; i < 4; ++i) {
        filterModeToggles[i] = createToggle("");
        filterModeToggles[i]->setRadioGroupId(1202);
        filterModeToggles[i]->onClick = [this, i]() {
            setSelectedFilterMode(i);
            setSteppedParam(spFilterMode, (uint8_t)i);
            int m = getSelectedFilterModel();
            updateFilterUIState(m, i);
            resized();
        };
        addAndMakeVisible(*filterModeToggles[i]);
    }
    updateFilterModeToggles(0);

    // 4-Band Shelves EQ Band Selector Buttons
    const char* bandNames[] = { "LOW", "MID LOW", "MID HIGH", "HIGH" };
    for (int i = 0; i < 4; ++i) {
        eqBandButtons[i].setButtonText(bandNames[i]);
        eqBandButtons[i].setClickingTogglesState(false);
        eqBandButtons[i].setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        eqBandButtons[i].onClick = [this, i]() {
            selectEQBand(i);
        };
        addChildComponent(eqBandButtons[i]);
    }

    cutoffKnob = createKnob("Cutoff", 0, 999, 999, KnobMode::CutoffHz);
    cutoffKnob->onValueChange = [this]() { setContinuousParam(cpCutoff, (float)cutoffKnob->getValue()); };
    addAndMakeVisible(*cutoffKnob);
    cutoffLabel = createLabel("CUTOFF", *this);

    resoKnob = createKnob("Reso", 0, 999, 100, KnobMode::Percent);
    resoKnob->onValueChange = [this]() { setContinuousParam(cpResonance, (float)resoKnob->getValue()); };
    addAndMakeVisible(*resoKnob);
    resoLabel = createLabel("RESONANCE", *this);

    filKbdKnob = createKnob("FKbd", 0, 999, 500, KnobMode::Percent);
    filKbdKnob->onValueChange = [this]() { setContinuousParam(cpFilKbdAmt, (float)filKbdKnob->getValue()); };
    addAndMakeVisible(*filKbdKnob);
    filKbdLabel = createLabel("KEY TRACK", *this);

    filEnvAmtKnob = createKnob("FEnv", -499, 499, 0, KnobMode::BipolarPercent);
    filEnvAmtKnob->onValueChange = [this]() {
        setContinuousParam(cpFilEnvAmt, (float)filEnvAmtKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*filEnvAmtKnob);
    filEnvAmtLabel = createLabel("ENV DEPTH", *this);

    const char* filEnvTypeNames[4] = { "Fast Exp", "Slow Exp x4", "Fast Lin", "Slow Lin x4" };
    for (int i = 0; i < 4; ++i) {
        filEnvTypeToggles[i] = createToggle(filEnvTypeNames[i]);
        filEnvTypeToggles[i]->setRadioGroupId(1203);
        filEnvTypeToggles[i]->onClick = [this, i]() {
            setSteppedParam(spFilEnvSlow, (i & 1) ? 1 : 0);
            setSteppedParam(spFilEnvLin, (i & 2) ? 1 : 0);
        };
        addAndMakeVisible(*filEnvTypeToggles[i]);
    }

    filEnvLoopToggle = std::make_unique<juce::ToggleButton>("LOOP ENVELOPE");
    filEnvLoopToggle->onClick = [this]() {
        setSteppedParam(spFilEnvLoop, filEnvLoopToggle->getToggleState() ? 1 : 0);
    };
    addAndMakeVisible(*filEnvLoopToggle);

    // Interactive Filter Frequency Response Curve
    filterCurve = std::make_unique<FilterCurveComponent>(model, *cutoffKnob, *resoKnob);
    filterCurve->onBandSelected = [this](int b) {
        selectEQBand(b);
    };
    filterCurve->onBandParamChanged = [this](int band, float potFreq, float potGain) {
        switch (band) {
            case 0:
                setContinuousParam(cpShelvesLsFreq, potFreq);
                setContinuousParam(cpShelvesLsGain, potGain);
                break;
            case 1:
                setContinuousParam(cpCutoff, potFreq);
                setContinuousParam(cpShelvesP1Gain, potGain);
                break;
            case 2:
                setContinuousParam(cpShelvesP2Freq, potFreq);
                setContinuousParam(cpShelvesP2Gain, potGain);
                break;
            case 3:
                setContinuousParam(cpShelvesHsFreq, potFreq);
                setContinuousParam(cpShelvesHsGain, potGain);
                break;
        }
        if (band == getCurrentEQBand()) {
            if (cutoffKnob && !cutoffKnob->isMouseButtonDown())
                cutoffKnob->setValue((int)std::round(potFreq), juce::dontSendNotification);
            if (resoKnob && !resoKnob->isMouseButtonDown())
                resoKnob->setValue((int)std::round(potGain) - 500, juce::dontSendNotification);
        }
    };
    filterCurve->onBandQChanged = [this](int band, float deltaQ) {
        if (band == 1) {
            float cur = (float)scan_potFrom16bits(model.getCurrentPreset().continuousParams[cpResonance]);
            float next = std::clamp(cur + deltaQ * 40.0f, 0.0f, 999.0f);
            setContinuousParam(cpResonance, next);
            if (getCurrentEQBand() == 1 && filKbdKnob && !filKbdKnob->isMouseButtonDown())
                filKbdKnob->setValue((int)std::round(next), juce::dontSendNotification);
        } else if (band == 2) {
            float cur = (float)scan_potFrom16bits(model.getCurrentPreset().continuousParams[cpShelvesP2Q]);
            float next = std::clamp(cur + deltaQ * 40.0f, 0.0f, 999.0f);
            setContinuousParam(cpShelvesP2Q, next);
            if (getCurrentEQBand() == 2 && filKbdKnob && !filKbdKnob->isMouseButtonDown())
                filKbdKnob->setValue((int)std::round(next), juce::dontSendNotification);
        }
    };
    addAndMakeVisible(*filterCurve);

    updateFilterUIState(0, 0);

    addAndMakeVisible(vcaCard);
    addAndMakeVisible(mixerCard);
    vcaCard.toBack();
    mixerCard.toBack();

    // VCA
    ampLevelKnob = createKnob("Level", 0, 999, 500, KnobMode::Percent);
    ampLevelKnob->onValueChange = [this]() { setContinuousParam(cpAmpLevel, (float)ampLevelKnob->getValue()); };
    addAndMakeVisible(*ampLevelKnob);
    ampLevelLabel = createLabel("LEVEL", *this);

    glideKnob = createKnob("Glide", 0, 999, 0, KnobMode::TimeMs);
    glideKnob->onValueChange = [this]() { setContinuousParam(cpGlide, (float)glideKnob->getValue()); };
    addAndMakeVisible(*glideKnob);
    glideLabel = createLabel("GLIDE", *this);

    // Caption shows the state (the UNISON divider names the control).
    // onStateChange fires for clicks; updateFromEngine() calls it after syncing.
    unisonToggle = std::make_unique<juce::ToggleButton>("OFF");
    unisonToggle->onStateChange = [this]() {
        const juce::String text = unisonToggle->getToggleState() ? "ON" : "OFF";
        if (unisonToggle->getButtonText() != text) unisonToggle->setButtonText(text);
    };
    unisonToggle->onClick = [this]() {
        setSteppedParam(spUnison, unisonToggle->getToggleState() ? 1 : 0);
    };
    addAndMakeVisible(*unisonToggle);

    // Console & saturation
    consoleDriveKnob = createKnob("ConsoleDrive", 0, 999, 100, KnobMode::Raw);
    consoleDriveKnob->textFromValueFunction = [](double val) -> juce::String {
        // ConsoleX maps the pot linearly to 0.7x .. 3.7x (100 = 1.0x = 0 dB).
        const float gain = 0.7f + 3.0f * (float)val / 999.0f;
        const float db = 20.0f * std::log10(gain);
        return (db >= 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
    };
    consoleDriveKnob->updateText();
    consoleDriveKnob->onValueChange = [this]() {
        setContinuousParam(cpConsoleDrive, (float)consoleDriveKnob->getValue());
    };
    addAndMakeVisible(*consoleDriveKnob);
    consoleDriveLabel = createLabel("DRIVE", *this);

    consoleDiscontinuityKnob = createKnob("Discontinuity", 0, 999, 500, KnobMode::Percent);
    consoleDiscontinuityKnob->onValueChange = [this]() {
        setContinuousParam(cpConsoleDiscontinuity, (float)consoleDiscontinuityKnob->getValue());
    };
    addAndMakeVisible(*consoleDiscontinuityKnob);
    consoleDiscontinuityLabel = createLabel("AIR", *this);

    mackityDriveKnob = createKnob("MackityDrive", 0, 999, 300, KnobMode::Raw);
    mackityDriveKnob->textFromValueFunction = [](double val) -> juce::String {
        // Airwindows Mackity input trim: gain = (a * 10)^2, 100 = 0 dB.
        const float a = (float)val / 999.0f;
        const float gain = (a * 10.0f) * (a * 10.0f);
        if (gain <= 0.0001f) return "-inf dB";
        const float db = 20.0f * std::log10(gain);
        return (db >= 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
    };
    mackityDriveKnob->updateText();
    mackityDriveKnob->onValueChange = [this]() {
        setContinuousParam(cpMackityDrive, (float)mackityDriveKnob->getValue());
    };
    addAndMakeVisible(*mackityDriveKnob);
    mackityDriveLabel = createLabel("DRIVE", *this);

    // Mixer & Tuning: two columns, each knob under its own named divider
    noiseVolKnob = createKnob("Noise", 0, 999, 0, KnobMode::Percent);
    noiseVolKnob->onValueChange = [this]() { setContinuousParam(cpNoiseVol, (float)noiseVolKnob->getValue()); };
    addAndMakeVisible(*noiseVolKnob);
    noiseVolLabel = createLabel("NOISE LEVEL", *this);

    masterTuneKnob = createKnob("MTune", -499, 499, 0, KnobMode::PitchSemitones);
    masterTuneKnob->onValueChange = [this]() {
        setContinuousParam(cpMasterTune, (float)masterTuneKnob->getValue() + 500.0f);
    };
    addAndMakeVisible(*masterTuneKnob);
    masterTuneLabel = createLabel("MASTER TUNE", *this);

    unisonDetuneKnob = createKnob("MDet", 0, 999, 10, KnobMode::Percent);
    unisonDetuneKnob->onValueChange = [this]() { setContinuousParam(cpUnisonDetune, (float)unisonDetuneKnob->getValue()); };
    addAndMakeVisible(*unisonDetuneKnob);
    unisonDetuneLabel = createLabel("SPREAD", *this);

    const char* chromaticPitchNames[3] = { "Free", "Semitones", "Octaves" };
    for (int i = 0; i < 3; ++i) {
        chromaticPitchToggles[i] = createToggle(chromaticPitchNames[i]);
        chromaticPitchToggles[i]->setRadioGroupId(1205);
        chromaticPitchToggles[i]->onClick = [this, i]() {
            setSteppedParam(spChromaticPitch, (uint8_t)i);
        };
        addAndMakeVisible(*chromaticPitchToggles[i]);
    }

    afxModeToggle = createToggle("AFX Mode (Sound per Key)");
    afxModeToggle->onClick = [this]() {
        setSteppedParam(spEngineMode, afxModeToggle->getToggleState() ? emAFX : emMultiChannel);
    };
    addAndMakeVisible(*afxModeToggle);

    // Real-time 6-Voice Activity & LM13700 VCA Gain Monitoring Panel
    voiceMeterPanel = std::make_unique<ModernVoiceMeterPanel>(model);
    voiceMeterPanel->onContinuousParam = [this](continuousParameter_t cp, float pot) { setContinuousParam(cp, pot); };
    voiceMeterPanel->onSteppedParam = [this](steppedParameter_t sp, uint8_t v) { setSteppedParam(sp, v); };
    addAndMakeVisible(*voiceMeterPanel);
    afxModeToggle->toFront(false); // sits in the voice mixer's footer row

    assignComponentIDs();
}

void FilterVcaTab::assignComponentIDs() {
    filterCard.setComponentID("filterCard");
    vcaCard.setComponentID("vcaCard");
    mixerCard.setComponentID("mixerCard");

    // Tab 2: Filter
    for (int i = 0; i < 4; ++i) {
        if (filterModelToggles[i]) filterModelToggles[i]->setComponentID("filterModelToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 4; ++i) {
        if (filterModeToggles[i]) filterModeToggles[i]->setComponentID("filterModeToggle[" + juce::String(i) + "]");
    }
    if (semVariantCombo) semVariantCombo->setComponentID("semVariantCombo");
    for (int i = 0; i < 4; ++i) eqBandButtons[i].setComponentID("eqBandButtons[" + juce::String(i) + "]");
    if (cutoffKnob) cutoffKnob->setComponentID("cutoffKnob");
    if (cutoffLabel) cutoffLabel->setComponentID("cutoffLabel");
    if (resoKnob) resoKnob->setComponentID("resoKnob");
    if (resoLabel) resoLabel->setComponentID("resoLabel");
    if (filKbdKnob) filKbdKnob->setComponentID("filKbdKnob");
    if (filKbdLabel) filKbdLabel->setComponentID("filKbdLabel");
    if (filEnvAmtKnob) filEnvAmtKnob->setComponentID("filEnvAmtKnob");
    if (filEnvAmtLabel) filEnvAmtLabel->setComponentID("filEnvAmtLabel");
    for (int i = 0; i < 4; ++i) {
        if (filEnvTypeToggles[i]) filEnvTypeToggles[i]->setComponentID("filEnvTypeToggle[" + juce::String(i) + "]");
    }
    if (filEnvLoopToggle) filEnvLoopToggle->setComponentID("filEnvLoopToggle");
    if (filterCurve) filterCurve->setComponentID("filterCurve");

    // Tab 2: VCA & Master Mixer
    if (ampLevelKnob) ampLevelKnob->setComponentID("ampLevelKnob");
    if (ampLevelLabel) ampLevelLabel->setComponentID("ampLevelLabel");
    if (glideKnob) glideKnob->setComponentID("glideKnob");
    if (glideLabel) glideLabel->setComponentID("glideLabel");
    if (unisonToggle) unisonToggle->setComponentID("unisonToggle");
    if (consoleDriveKnob) consoleDriveKnob->setComponentID("consoleDriveKnob");
    if (consoleDriveLabel) consoleDriveLabel->setComponentID("consoleDriveLabel");
    if (consoleDiscontinuityKnob) consoleDiscontinuityKnob->setComponentID("consoleDiscontinuityKnob");
    if (consoleDiscontinuityLabel) consoleDiscontinuityLabel->setComponentID("consoleDiscontinuityLabel");
    if (mackityDriveKnob) mackityDriveKnob->setComponentID("mackityDriveKnob");
    if (mackityDriveLabel) mackityDriveLabel->setComponentID("mackityDriveLabel");
    if (afxModeToggle) afxModeToggle->setComponentID("engineModeToggle[1]");
    if (noiseVolKnob) noiseVolKnob->setComponentID("noiseVolKnob");
    if (noiseVolLabel) noiseVolLabel->setComponentID("noiseVolLabel");
    if (masterTuneKnob) masterTuneKnob->setComponentID("masterTuneKnob");
    if (masterTuneLabel) masterTuneLabel->setComponentID("masterTuneLabel");
    if (unisonDetuneKnob) unisonDetuneKnob->setComponentID("unisonDetuneKnob");
    if (unisonDetuneLabel) unisonDetuneLabel->setComponentID("unisonDetuneLabel");
    for (int i = 0; i < 3; ++i) {
        if (chromaticPitchToggles[i]) chromaticPitchToggles[i]->setComponentID("chromaticPitchToggle[" + juce::String(i) + "]");
    }
    if (voiceMeterPanel) voiceMeterPanel->setComponentID("voiceMeterPanel");
}

void FilterVcaTab::selectEQBand(int band) {
    setCurrentEQBand(band);
    applyEQBandSelection(currentEQBand);
}

void FilterVcaTab::applyEQBandSelection(int band) {
    for (int i = 0; i < 4; ++i) {
        eqBandButtons[i].setToggleState(i == getCurrentEQBand(), juce::dontSendNotification);
    }
    if (filterCurve) {
        filterCurve->setActiveBand(getCurrentEQBand());
    }
    updateEQKnobsForCurrentBand();
}

void FilterVcaTab::updateEQKnobsForCurrentBand() {
    const auto& preset = model.getCurrentPreset();

    auto setFreqKnob = [this, &preset](juce::Slider* s, continuousParameter_t cp, const juce::String& lblText) {
        if (!s) return;
        s->setRange(0, 999, 1.0);
        s->textFromValueFunction = [](double val) -> juce::String {
            float hz = 20.0f * std::pow(10.0f, ((float)val / 999.0f) * 3.0f);
            if (hz >= 1000.0f) return juce::String(hz / 1000.0f, 2) + " kHz";
            return juce::String((int)std::round(hz)) + " Hz";
        };
        s->valueFromTextFunction = [](const juce::String& text) -> double {
            juce::String t = text.trim().toLowerCase();
            float mul = t.contains("k") ? 1000.0f : 1.0f;
            float hz = t.replace("khz", "").replace("hz", "").replace("k", "").trim().getFloatValue() * mul;
            hz = std::clamp(hz, 20.0f, 20000.0f);
            return (std::log10(hz / 20.0f) / 3.0f) * 999.0;
        };
        if (!s->isMouseButtonDown())
            s->setValue(scan_potFrom16bits(preset.continuousParams[cp]), juce::dontSendNotification);
        s->updateText();
        s->onValueChange = [this, s, cp]() {
            setContinuousParam(cp, (float)s->getValue());
            if (filterCurve) filterCurve->repaint();
        };
        if (cutoffLabel) cutoffLabel->setText(lblText, juce::dontSendNotification);
    };

    auto setGainKnob = [this, &preset](juce::Slider* s, continuousParameter_t cp, const juce::String& lblText) {
        if (!s) return;
        s->setRange(-499, 499, 1.0);
        s->textFromValueFunction = [](double val) -> juce::String {
            float db = ((float)val / 499.0f) * 18.0f;
            int r = (int)std::round(db);
            return (r > 0 ? "+" : "") + juce::String(r) + " dB";
        };
        s->valueFromTextFunction = [](const juce::String& text) -> double {
            float db = text.replace("db", "").replace("+", "").trim().getFloatValue();
            return std::clamp((db / 18.0f) * 499.0f, -499.0f, 499.0f);
        };
        if (!s->isMouseButtonDown())
            s->setValue(scan_potFrom16bits(preset.continuousParams[cp]) - 500, juce::dontSendNotification);
        s->updateText();
        s->onValueChange = [this, s, cp]() {
            setContinuousParam(cp, (float)s->getValue() + 500.0f);
            if (filterCurve) filterCurve->repaint();
        };
        if (resoLabel) resoLabel->setText(lblText, juce::dontSendNotification);
    };

    auto setQKnob = [this, &preset](juce::Slider* s, continuousParameter_t cp, const juce::String& lblText) {
        if (!s) return;
        s->setRange(0, 999, 1.0);
        s->textFromValueFunction = [](double val) -> juce::String {
            float q = 0.5f + ((float)val / 999.0f) * 9.5f;
            return "Q " + juce::String(q, 2);
        };
        s->valueFromTextFunction = [](const juce::String& text) -> double {
            float q = text.replace("q", "").trim().getFloatValue();
            q = std::clamp(q, 0.5f, 10.0f);
            return ((q - 0.5f) / 9.5f) * 999.0;
        };
        if (!s->isMouseButtonDown())
            s->setValue(scan_potFrom16bits(preset.continuousParams[cp]), juce::dontSendNotification);
        s->updateText();
        s->onValueChange = [this, s, cp]() {
            setContinuousParam(cp, (float)s->getValue());
            if (filterCurve) filterCurve->repaint();
        };
        if (filKbdLabel) filKbdLabel->setText(lblText, juce::dontSendNotification);
    };

    auto setPercentKnob = [this, &preset](juce::Slider* s, continuousParameter_t cp, const juce::String& lblText) {
        if (!s) return;
        s->setRange(0, 999, 1.0);
        s->textFromValueFunction = [](double val) -> juce::String {
            return juce::String((int)std::round((val / 999.0) * 100.0)) + " %";
        };
        s->valueFromTextFunction = [](const juce::String& text) -> double {
            float p = text.replace("%", "").trim().getFloatValue();
            return std::clamp((p / 100.0f) * 999.0f, 0.0f, 999.0f);
        };
        if (!s->isMouseButtonDown())
            s->setValue(scan_potFrom16bits(preset.continuousParams[cp]), juce::dontSendNotification);
        s->updateText();
        s->onValueChange = [this, s, cp]() {
            setContinuousParam(cp, (float)s->getValue());
        };
        if (filKbdLabel) filKbdLabel->setText(lblText, juce::dontSendNotification);
    };

    const auto binding = getActiveEqBandBinding();
    setFreqKnob(cutoffKnob.get(), binding.frequency, binding.frequencyLabel);
    setGainKnob(resoKnob.get(), binding.gain, binding.gainLabel);
    if (binding.thirdControl == FilterVcaTab::EqThirdControl::Q)
        setQKnob(filKbdKnob.get(), binding.third, binding.thirdLabel);
    else
        setPercentKnob(filKbdKnob.get(), binding.third, binding.thirdLabel);

    if (filEnvAmtKnob) {
        filEnvAmtKnob->setRange(-499, 499, 1.0);
        filEnvAmtKnob->textFromValueFunction = [](double val) -> juce::String {
            int pct = (int)std::round((val / 499.0) * 100.0);
            return (pct > 0 ? "+" : "") + juce::String(pct) + " %";
        };
        filEnvAmtKnob->valueFromTextFunction = [](const juce::String& text) -> double {
            float pct = text.replace("%", "").replace("+", "").trim().getFloatValue();
            return std::clamp((pct / 100.0f) * 499.0f, -499.0f, 499.0f);
        };
        if (!filEnvAmtKnob->isMouseButtonDown())
            filEnvAmtKnob->setValue(scan_potFrom16bits(preset.continuousParams[cpFilEnvAmt]) - 500, juce::dontSendNotification);
        filEnvAmtKnob->updateText();
        filEnvAmtKnob->onValueChange = [this]() {
            setContinuousParam(cpFilEnvAmt, (float)filEnvAmtKnob->getValue() + 500.0f);
        };
    }
    if (filEnvAmtLabel) filEnvAmtLabel->setText("ENV DEPTH", juce::dontSendNotification);
}

void FilterVcaTab::updateFilterModeToggles(int filterModel) {
    setSelectedFilterModel(filterModel);
    const auto options = getFilterModeOptions();
    setSelectedFilterMode(options.selectedIndex);
    for (int i = 0; i < 4; ++i) {
        filterModeToggles[i]->setButtonText(options.labels[static_cast<size_t>(i)]);
        filterModeToggles[i]->setVisible(i < options.visibleCount);
        filterModeToggles[i]->setToggleState(i == options.selectedIndex, juce::dontSendNotification);
    }
}

void FilterVcaTab::updateFilterUIState(int filterModel, int mode) {
    const bool isShelvesEQ = isShelvesEqActive();

    for (int i = 0; i < 4; ++i) {
        eqBandButtons[i].setVisible(isShelvesEQ);
        eqBandButtons[i].setToggleState(i == getCurrentEQBand(), juce::dontSendNotification);
    }

    // Keep static header and badge so filter filterModel/type only appears once in the GUI
    filterCard.setHeader("FILTER", "VCF");

    // 3. Dynamic parameter labels & knob configurations
    if (isShelvesEQ) {
        updateEQKnobsForCurrentBand();
    } else {
        // Restore standard knob ranges & formatters
        cutoffKnob->setRange(0, 999, 1.0);
        cutoffKnob->textFromValueFunction = [](double val) -> juce::String {
            float hz = 20.0f * std::pow(10.0f, ((float)val / 999.0f) * 3.0f);
            if (hz >= 1000.0f) return juce::String(hz / 1000.0f, 2) + " kHz";
            return juce::String((int)std::round(hz)) + " Hz";
        };
        cutoffKnob->valueFromTextFunction = [](const juce::String& text) -> double {
            juce::String t = text.trim().toLowerCase();
            float mul = t.contains("k") ? 1000.0f : 1.0f;
            float hz = t.replace("khz", "").replace("hz", "").replace("k", "").trim().getFloatValue() * mul;
            hz = std::clamp(hz, 20.0f, 20000.0f);
            return (std::log10(hz / 20.0f) / 3.0f) * 999.0;
        };
        cutoffKnob->onValueChange = [this]() {
            setContinuousParam(cpCutoff, (float)cutoffKnob->getValue());
            if (filterCurve) filterCurve->repaint();
        };

        resoKnob->setRange(0, 999, 1.0);
        resoKnob->textFromValueFunction = [](double val) -> juce::String {
            return juce::String((int)std::round((val / 999.0) * 100.0)) + " %";
        };
        resoKnob->valueFromTextFunction = [](const juce::String& text) -> double {
            float pct = text.replace("%", "").trim().getFloatValue();
            return std::clamp((pct / 100.0f) * 999.0f, 0.0f, 999.0f);
        };
        resoKnob->onValueChange = [this]() {
            setContinuousParam(cpResonance, (float)resoKnob->getValue());
            if (filterCurve) filterCurve->repaint();
        };

        filKbdKnob->setRange(0, 999, 1.0);
        filKbdKnob->textFromValueFunction = [](double val) -> juce::String {
            return juce::String((int)std::round((val / 999.0) * 100.0)) + " %";
        };
        filKbdKnob->valueFromTextFunction = [](const juce::String& text) -> double {
            float pct = text.replace("%", "").trim().getFloatValue();
            return std::clamp((pct / 100.0f) * 999.0f, 0.0f, 999.0f);
        };
        filKbdKnob->onValueChange = [this]() {
            setContinuousParam(cpFilKbdAmt, (float)filKbdKnob->getValue());
        };

        filEnvAmtKnob->setRange(-499, 499, 1.0);
        filEnvAmtKnob->textFromValueFunction = [](double val) -> juce::String {
            int pct = (int)std::round((val / 499.0) * 100.0);
            return (pct > 0 ? "+" : "") + juce::String(pct) + " %";
        };
        filEnvAmtKnob->valueFromTextFunction = [](const juce::String& text) -> double {
            float pct = text.replace("%", "").replace("+", "").trim().getFloatValue();
            return std::clamp((pct / 100.0f) * 499.0f, -499.0f, 499.0f);
        };
        filEnvAmtKnob->onValueChange = [this]() {
            setContinuousParam(cpFilEnvAmt, (float)filEnvAmtKnob->getValue() + 500.0f);
        };

        const auto& preset = model.getCurrentPreset();
        if (cutoffKnob && !cutoffKnob->isMouseButtonDown()) {
            cutoffKnob->setValue(scan_potFrom16bits(preset.continuousParams[cpCutoff]), juce::dontSendNotification);
            cutoffKnob->updateText();
        }
        if (resoKnob && !resoKnob->isMouseButtonDown()) {
            resoKnob->setValue(scan_potFrom16bits(preset.continuousParams[cpResonance]), juce::dontSendNotification);
            resoKnob->updateText();
        }
        if (filKbdKnob && !filKbdKnob->isMouseButtonDown()) {
            filKbdKnob->setValue(scan_potFrom16bits(preset.continuousParams[cpFilKbdAmt]), juce::dontSendNotification);
            filKbdKnob->updateText();
        }
        if (filEnvAmtKnob && !filEnvAmtKnob->isMouseButtonDown()) {
            filEnvAmtKnob->setValue(scan_potFrom16bits(preset.continuousParams[cpFilEnvAmt]) - 500, juce::dontSendNotification);
            filEnvAmtKnob->updateText();
        }

        if ((filterModel == 1 && mode == 2) || (filterModel == 2 && mode == 2)) { // Band-Pass modes
            if (cutoffLabel) cutoffLabel->setText("CENTER FREQ", juce::dontSendNotification);
            if (resoLabel) resoLabel->setText("RESONANCE (Q)", juce::dontSendNotification);
        } else if (filterModel == 2 && mode == 3) { // Shelves SVF High-Pass
            if (cutoffLabel) cutoffLabel->setText("CUTOFF (HP)", juce::dontSendNotification);
            if (resoLabel) resoLabel->setText("RESONANCE (Q)", juce::dontSendNotification);
        } else { // Low-Pass modes (SSI2144, Ripples LP4/LP2, Shelves SVF LP)
            if (cutoffLabel) cutoffLabel->setText("CUTOFF", juce::dontSendNotification);
            if (resoLabel) resoLabel->setText((filterModel == 0 || (filterModel == 1 && mode <= 1)) ? "RESONANCE" : "RESONANCE (Q)", juce::dontSendNotification);
        }

        if (filKbdLabel) filKbdLabel->setText("KEY TRACK", juce::dontSendNotification);
        if (filEnvAmtLabel) filEnvAmtLabel->setText("ENV DEPTH", juce::dontSendNotification);
    }

    if (filterCurve) filterCurve->repaint();
}

FilterVcaTab::FilterModeOptions FilterVcaTab::getFilterModeOptions() const {
    FilterModeOptions options;
    switch (selectedFilterModel) {
        case 1:
            if (selectedSemVariant == 4) { // Liquid (Ripples)
                options.labels = { "4-Pole Lowpass (24 dB)", "2-Pole Lowpass (12 dB)", "2-Pole Bandpass (12 dB)", "" };
                options.visibleCount = 3;
            } else {
                options.labels = { "Lowpass (12 dB)", "Bandpass (12 dB)", "Highpass (12 dB)", "Notch" };
                options.visibleCount = 4;
            }
            break;
        case 2:
            options.labels = { "4-Band Parametric EQ", "SVF Lowpass (12 dB)", "SVF Bandpass (12 dB)", "SVF Highpass (12 dB)" };
            options.visibleCount = 4;
            break;
        case 3:
            options.labels = { "Vintage LP (24 dB)", "Vintage LP (18 dB)", "Vintage LP (12 dB)", "Vintage LP (6 dB)" };
            options.visibleCount = 4;
            break;
        default:
            options.labels = { "24 dB Lowpass", "", "", "" };
            options.visibleCount = 1;
            break;
    }
    // A mode the new model does not offer falls back to its first mode.
    options.selectedIndex = selectedFilterMode < options.visibleCount ? selectedFilterMode : 0;
    return options;
}

FilterVcaTab::EqBandBinding FilterVcaTab::getActiveEqBandBinding() const {
    switch (currentEQBand) {
        case 0: return { cpShelvesLsFreq, cpShelvesLsGain, cpFilKbdAmt, EqThirdControl::Percent, "LOW FREQ", "LOW GAIN", "KEY TRACK" };
        case 1: return { cpCutoff, cpShelvesP1Gain, cpResonance, EqThirdControl::Q, "MID LOW FREQ", "MID LOW GAIN", "MID LOW Q" };
        case 2: return { cpShelvesP2Freq, cpShelvesP2Gain, cpShelvesP2Q, EqThirdControl::Q, "MID HIGH FREQ", "MID HIGH GAIN", "MID HIGH Q" };
        default: return { cpShelvesHsFreq, cpShelvesHsGain, cpFilKbdAmt, EqThirdControl::Percent, "HIGH FREQ", "HIGH GAIN", "KEY TRACK" };
    }
}

void FilterVcaTab::updateFromEngine() {
    const auto& preset = model.getCurrentPreset();

    // Master mixer & tuning (FILTER / VCA tab)
    safeSetKnob(noiseVolKnob.get(), scan_potFrom16bits(preset.continuousParams[cpNoiseVol]));
    safeSetKnob(masterTuneKnob.get(), scan_potFrom16bits(preset.continuousParams[cpMasterTune]) - 500);
    safeSetKnob(unisonDetuneKnob.get(), scan_potFrom16bits(preset.continuousParams[cpUnisonDetune]));
    uint8_t cPitch = preset.steppedParams[spChromaticPitch];
    for (int i = 0; i < 3; ++i) {
        if (chromaticPitchToggles[i])
            chromaticPitchToggles[i]->setToggleState(i == cPitch, juce::dontSendNotification);
    }

    // Filter & VCA
    uint8_t fModel = preset.steppedParams[spFilterModel];
    uint8_t fMode = preset.steppedParams[spFilterMode];
    selectedSemVariant = std::min<int>(preset.steppedParams[spSemModel], 4);
    if (semVariantCombo) {
        safeSetCombo(*semVariantCombo, selectedSemVariant + 1);
        semVariantCombo->setEnabled(fModel == fmSem);
    }
    setSelectedFilterModel(fModel);
    setSelectedFilterMode(fMode);
    for (int i = 0; i < 4; ++i) {
        if (filterModelToggles[i])
            filterModelToggles[i]->setToggleState(i == fModel, juce::dontSendNotification);
    }
    updateFilterModeToggles(fModel);
    for (int i = 0; i < 4; ++i) {
        if (filterModeToggles[i])
            filterModeToggles[i]->setToggleState(i == fMode, juce::dontSendNotification);
    }
    updateFilterUIState(fModel, fMode);
    // Card layout (dividers, EQ row) depends on model/mode; re-lay out when a
    // preset or the host changed them rather than a click on the toggles.
    if (fModel != laidOutFilterModel || fMode != laidOutFilterMode) resized();

    if (fModel != 2 || fMode != 0) {
        safeSetKnob(cutoffKnob.get(), scan_potFrom16bits(preset.continuousParams[cpCutoff]));
        safeSetKnob(resoKnob.get(), scan_potFrom16bits(preset.continuousParams[cpResonance]));
        safeSetKnob(filKbdKnob.get(), scan_potFrom16bits(preset.continuousParams[cpFilKbdAmt]));
    }
    safeSetKnob(filEnvAmtKnob.get(), scan_potFrom16bits(preset.continuousParams[cpFilEnvAmt]) - 500);
    safeSetToggle(filEnvLoopToggle.get(), preset.steppedParams[spFilEnvLoop] != 0);
    int fLin = preset.steppedParams[spFilEnvLin] ? 2 : 0;
    int fSlow = preset.steppedParams[spFilEnvSlow] ? 1 : 0;
    int envTypeId = fLin + fSlow;
    for (int i = 0; i < 4; ++i) {
        if (filEnvTypeToggles[i])
            filEnvTypeToggles[i]->setToggleState(i == envTypeId, juce::dontSendNotification);
    }

    safeSetKnob(ampLevelKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAmpLevel]));
    safeSetKnob(glideKnob.get(), scan_potFrom16bits(preset.continuousParams[cpGlide]));
    safeSetToggle(unisonToggle.get(), preset.steppedParams[spUnison] != 0);
    if (unisonToggle && unisonToggle->onStateChange) unisonToggle->onStateChange(); // ON / OFF caption

    safeSetKnob(consoleDriveKnob.get(), scan_potFrom16bits(preset.continuousParams[cpConsoleDrive]));
    safeSetKnob(consoleDiscontinuityKnob.get(), scan_potFrom16bits(preset.continuousParams[cpConsoleDiscontinuity]));
    safeSetKnob(mackityDriveKnob.get(), scan_potFrom16bits(preset.continuousParams[cpMackityDrive]));
    safeSetToggle(afxModeToggle.get(), preset.steppedParams[spEngineMode] == emAFX);

    if (filterCurve) filterCurve->repaint();
}

void FilterVcaTab::resized() {
    const auto tabBounds = getLocalBounds();

    int colGap = 5;
    int totalW = tabBounds.getWidth();

    // Row 1: the filter card takes a third, the response curve the rest.
    int col1W = (totalW - colGap * 2) / 3;
    int col1X = 0;
    int col2X = col1X + col1W + colGap;

    const bool isEQ = isShelvesEqActive();
    laidOutFilterModel = getSelectedFilterModel();
    laidOutFilterMode = getSelectedFilterMode();
    // Row 1 is kept compact so row 2 has room for full-size encoder columns.
    int cardTopH = 236;

    // ------------------------------------------
    // Card 1: FILTER (VCF)
    // ------------------------------------------
    filterCard.setBounds(col1X, 0, col1W, cardTopH);
    filterCard.clearDividers();

    // Top Section: Filter Model (4 Models) & Filter Mode (4 Modes)
    int subColW = (col1W - 24 - 8) / 2;
    int midX = 12 + subColW + 4;
    filterCard.addDivider(6, 26, subColW, "FILTER MODEL");
    filterCard.addDivider(midX + 4, 26, subColW, "FILTER MODE");
    filterCard.addVerticalDivider(midX, 28, 122);

    int toggleH = 18;
    int toggleStep = 21;
    int filCtrlStartY = 38;

    for (int i = 0; i < 4; ++i) {
        if (filterModelToggles[i])
            filterModelToggles[i]->setBounds(col1X + 12, filCtrlStartY + i * toggleStep, subColW - 6, toggleH);
    }
    // The SEM variant selector shares the SEM toggle's row.
    constexpr int semToggleW = 52;
    if (filterModelToggles[fmSem]) filterModelToggles[fmSem]->setSize(semToggleW, toggleH);
    if (semVariantCombo)
        semVariantCombo->setBounds(col1X + 12 + semToggleW, filCtrlStartY + fmSem * toggleStep - 1,
                                   subColW - 6 - semToggleW, toggleH + 2);
    for (int i = 0; i < 4; ++i) {
        if (filterModeToggles[i])
            filterModeToggles[i]->setBounds(col1X + midX + 8, filCtrlStartY + i * toggleStep, subColW - 6, toggleH);
    }

    // Middle Section: Cutoff, Reso & Modulation (or EQ Band selector)
    int knobSz = getStandardKnobSize(); // 55px hardware standard
    int filKnobSlotW = (col1W - 24) / 4;
    constexpr int sectionDivY = 134; // below the model/mode toggles

    if (isEQ) {
        // 4-band EQ: the four knobs edit one band, so no vertical split
        filterCard.addDivider(sectionDivY, "EQ BAND SELECTOR & PARAMETERS");
        // Compact band selector: small buttons, closely spaced, centred as a group
        constexpr int btnW = 64, btnH = 16, btnGap = 4;
        const int groupX = col1X + (col1W - (4 * btnW + 3 * btnGap)) / 2;
        for (int i = 0; i < 4; ++i) {
            eqBandButtons[i].setVisible(true);
            eqBandButtons[i].setBounds(groupX + i * (btnW + btnGap), sectionDivY + 9, btnW, btnH);
        }
        int filKnobY = 162;
        layoutKnob(cutoffKnob, cutoffLabel, col1X + 12 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(resoKnob, resoLabel, col1X + 12 + filKnobSlotW + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(filKbdKnob, filKbdLabel, col1X + 12 + filKnobSlotW * 2 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(filEnvAmtKnob, filEnvAmtLabel, col1X + 12 + filKnobSlotW * 3 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
    } else {
        for (int i = 0; i < 4; ++i) {
            eqBandButtons[i].setVisible(false);
        }
        int halfSlotW = filKnobSlotW * 2;
        filterCard.addDivider(6, sectionDivY, halfSlotW - 4, "CUTOFF & RESONANCE");
        filterCard.addDivider(12 + halfSlotW + 4, sectionDivY, halfSlotW - 4, "MODULATION");
        filterCard.addVerticalDivider(12 + halfSlotW, sectionDivY + 10, cardTopH - 8);
        int filKnobY = 152;
        layoutKnob(cutoffKnob, cutoffLabel, col1X + 12 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(resoKnob, resoLabel, col1X + 12 + filKnobSlotW + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(filKbdKnob, filKbdLabel, col1X + 12 + filKnobSlotW * 2 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(filEnvAmtKnob, filEnvAmtLabel, col1X + 12 + filKnobSlotW * 3 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
    }

    // ------------------------------------------
    // Row 1, columns 2-3: filterCurve with the filter envelope routing below
    // ------------------------------------------
    {
        const int curveX = col2X;
        const int curveW = totalW - curveX;
        constexpr int envRoutingH = 48;
        const int curveH = std::max(40, cardTopH - envRoutingH - 6);
        if (filterCurve) filterCurve->setBounds(curveX, 0, curveW, curveH);

        // Same widths and spacing as the amp envelope toggles on the ENV tab
        const int envStartY = curveH + 6;
        constexpr int envTogW = 108, envLoopW = 112, envTogH = 20, envRowStep = 22;
        const int envCol1X = curveX + 8;
        const int envCol2X = envCol1X + envTogW + 6;
        const int envCol3X = envCol2X + envTogW + 2;

        if (filEnvTypeToggles[0]) filEnvTypeToggles[0]->setBounds(envCol1X, envStartY, envTogW, envTogH);
        if (filEnvTypeToggles[2]) filEnvTypeToggles[2]->setBounds(envCol2X, envStartY, envTogW, envTogH);
        if (filEnvLoopToggle) filEnvLoopToggle->setBounds(envCol3X, envStartY, envLoopW, envTogH);
        if (filEnvTypeToggles[1]) filEnvTypeToggles[1]->setBounds(envCol1X, envStartY + envRowStep, envTogW, envTogH);
        if (filEnvTypeToggles[3]) filEnvTypeToggles[3]->setBounds(envCol2X, envStartY + envRowStep, envTogW, envTogH);
    }

    // ------------------------------------------
    // Row 2: vcaCard | mixerCard | voiceMeterPanel
    // ------------------------------------------
    const int row2Y = cardTopH + colGap;
    const int row2H = tabBounds.getHeight() - row2Y;
    if (row2H <= 40) return;

    const int cardW = 220; // vcaCard and mixerCard share one width
    const int vcaX = 0;
    const int mixerX = vcaX + cardW + colGap;
    const int meterX = mixerX + cardW + colGap;
    constexpr int knobCellH = 71; // standard knob + caption below
    const int colW = (cardW - 24) / 2;
    // Two-column card grid: named divider per column, knob centred below it.
    auto columnDivider = [&](ModernSectionCard& card, int column, int y, const juce::String& name) {
        card.addDivider(column == 0 ? 6 : 12 + colW + 4, y, column == 0 ? colW - 2 : colW - 6, name);
    };
    auto columnKnob = [&](int cardX, auto& knob, auto& label, int column, int y) {
        layoutKnob(knob, label, cardX + 12 + column * colW + (colW - knobSz) / 2, row2Y + y, knobSz);
    };

    // Card: AMPLIFIER. AMP (level, glide) | CONSOLE (drive, air, Mackity)
    vcaCard.setBounds(vcaX, row2Y, cardW, row2H);
    vcaCard.clearDividers();
    columnDivider(vcaCard, 0, 38, "AMP");
    columnDivider(vcaCard, 1, 38, "CONSOLE");
    vcaCard.addVerticalDivider(12 + colW, 30, row2H - 8);
    const int vcaStep = juce::jlimit(knobCellH + 4, 88, (row2H - 56) / 3);
    columnKnob(vcaX, ampLevelKnob, ampLevelLabel, 0, 48);
    columnKnob(vcaX, glideKnob, glideLabel, 0, 48 + vcaStep);
    columnKnob(vcaX, consoleDriveKnob, consoleDriveLabel, 1, 48);
    columnKnob(vcaX, consoleDiscontinuityKnob, consoleDiscontinuityLabel, 1, 48 + vcaStep);
    // Third row: NOISE (under GLIDE) | MACKITY (under AIR)
    const int thirdRowDivY = 48 + 2 * vcaStep - 8;
    columnDivider(vcaCard, 0, thirdRowDivY, "NOISE");
    columnKnob(vcaX, noiseVolKnob, noiseVolLabel, 0, thirdRowDivY + 10);
    columnDivider(vcaCard, 1, thirdRowDivY, "MACKITY");
    columnKnob(vcaX, mackityDriveKnob, mackityDriveLabel, 1, thirdRowDivY + 10);

    // Card: TUNING & UNISON. TUNE | PITCH quantize toggles, then UNISON
    // across both columns: spread knob left, on/off toggle right.
    mixerCard.setBounds(mixerX, row2Y, cardW, row2H);
    mixerCard.clearDividers();
    columnDivider(mixerCard, 0, 38, "TUNE");
    columnKnob(mixerX, masterTuneKnob, masterTuneLabel, 0, 48);
    columnDivider(mixerCard, 1, 38, "PITCH");
    constexpr int pitchToggleH = 18, pitchToggleStep = 21;
    for (int i = 0; i < 3; ++i)
        if (chromaticPitchToggles[i])
            chromaticPitchToggles[i]->setBounds(mixerX + 12 + colW + 8, row2Y + 50 + i * pitchToggleStep,
                                                colW - 8, pitchToggleH);

    const int unisonDivY = 48 + knobCellH + 20; // extra air below the TUNE caption
    mixerCard.addDivider(unisonDivY, "UNISON");
    const int spreadY = unisonDivY + 10;
    columnKnob(mixerX, unisonDetuneKnob, unisonDetuneLabel, 0, spreadY);
    if (unisonToggle)
        unisonToggle->setBounds(mixerX + 12 + colW + 8, row2Y + spreadY + (knobSz - 22) / 2, colW - 8, 22);

    if (auto* meter = getVoiceMeterPanel()) {
        meter->setBounds(meterX, row2Y, totalW - meterX, row2H);
        // AFX lives in the voice mixer's footer row, bottom left.
        if (afxModeToggle) {
            const auto area = meter->getFooterControlArea();
            afxModeToggle->setBounds(meterX + area.getX(), row2Y + area.getY(),
                                     std::min(area.getWidth(), 200), area.getHeight());
        }
    }
}
