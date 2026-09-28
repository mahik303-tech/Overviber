#include "FilterVcaTab.h"

#include <algorithm>

FilterVcaTab::FilterVcaTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void FilterVcaTab::setup() {
    addAndMakeVisible(filterCard);
    filterCard.toBack();

    // Filter Model & Dynamic Mode Toggles (Vertical ToggleButton Groups)
    const char* filterModelNames[4] = { "SSI2144 Ladder", "Liquid Ripples", "Shelves EQ / SVF", "SST Vintage Moog" };
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
            resized();
        };
        addAndMakeVisible(*filterModelToggles[i]);
    }

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
    const char* bandNames[] = { "1: LOW", "2: MID 1", "3: MID 2", "4: HIGH" };
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

    const char* filEnvTypeNames[4] = { "Fast Exponential", "Slow Exponential (x4)", "Fast Linear", "Slow Linear (x4)" };
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
    filterCurve = std::make_unique<FilterCurveComponent>(engine, *cutoffKnob, *resoKnob);
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
            float cur = (float)scan_potFrom16bits(engine.getCurrentPreset().continuousParams[cpResonance]);
            float next = std::clamp(cur + deltaQ * 40.0f, 0.0f, 999.0f);
            setContinuousParam(cpResonance, next);
            if (getCurrentEQBand() == 1 && filKbdKnob && !filKbdKnob->isMouseButtonDown())
                filKbdKnob->setValue((int)std::round(next), juce::dontSendNotification);
        } else if (band == 2) {
            float cur = (float)scan_potFrom16bits(engine.getCurrentPreset().continuousParams[cpShelvesP2Q]);
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

    unisonToggle = std::make_unique<juce::ToggleButton>("UNISON");
    unisonToggle->onClick = [this]() {
        setSteppedParam(spUnison, unisonToggle->getToggleState() ? 1 : 0);
    };
    addAndMakeVisible(*unisonToggle);

    // Labels follow the persisted numeric IDs: Console=0, Clean=1, Mackity=2.
    const char* consoleModelNames[3] = { "Overviber Console", "Clean (Legacy)", "Mackity (Legacy)" };
    for (int i = 0; i < 3; ++i) {
        consoleModelToggles[i] = createToggle(consoleModelNames[i]);
        consoleModelToggles[i]->setRadioGroupId(1206);
        consoleModelToggles[i]->onClick = [this, i]() {
            setSteppedParam(spConsoleModel, (uint8_t)i);
            if (mackityToggle) {
                mackityToggle->setToggleState(i == cmMackity, juce::dontSendNotification);
            }
        };
        addAndMakeVisible(*consoleModelToggles[i]);
    }

    mackityToggle = createToggle("MACKITY");
    mackityToggle->onClick = [this]() {
        uint8_t m = mackityToggle->getToggleState() ? cmMackity : cmConsoleX;
        setSteppedParam(spConsoleModel, m);
        for (int i = 0; i < 3; ++i) {
            if (consoleModelToggles[i]) consoleModelToggles[i]->setToggleState(i == m, juce::dontSendNotification);
        }
    };

    mackityInTrimKnob = createKnob("InTrim", 0, 999, 100, KnobMode::Raw);
    mackityInTrimKnob->textFromValueFunction = [](double val) -> juce::String {
        float a = (float)val / 999.0f;
        float gain = (a * 10.0f) * (a * 10.0f);
        if (gain <= 0.0001f) return "-inf dB";
        float db = 20.0f * std::log10(gain);
        return (db >= 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
    };
    mackityInTrimKnob->updateText();
    mackityInTrimKnob->onValueChange = [this]() {
        setContinuousParam(cpMackityInTrim, (float)mackityInTrimKnob->getValue());
    };
    addAndMakeVisible(*mackityInTrimKnob);
    mackityInTrimLabel = createLabel("DRIVE", *this);

    mackityOutPadKnob = createKnob("OutPad", 0, 999, 999, KnobMode::Percent);
    mackityOutPadKnob->onValueChange = [this]() {
        setContinuousParam(cpMackityOutPad, (float)mackityOutPadKnob->getValue());
    };
    addAndMakeVisible(*mackityOutPadKnob);
    mackityOutPadLabel = createLabel("LEVEL", *this);

    consoleDiscontinuityKnob = createKnob("Discontinuity", 0, 999, 500, KnobMode::Percent);
    consoleDiscontinuityKnob->onValueChange = [this]() {
        setContinuousParam(cpConsoleDiscontinuity, (float)consoleDiscontinuityKnob->getValue());
    };
    addAndMakeVisible(*consoleDiscontinuityKnob);
    consoleDiscontinuityLabel = createLabel("AIR", *this);

    // Multitimbral & AFX Mode (Horizontal Toggles)
    const char* engineModeNames[3] = { "Single", "AFX (Key)", "Multi-Ch" };
    for (int i = 0; i < 3; ++i) {
        engineModeToggles[i] = createToggle(engineModeNames[i]);
        engineModeToggles[i]->setRadioGroupId(1207);
        engineModeToggles[i]->onClick = [this, i]() {
            setSteppedParam(spEngineMode, (uint8_t)i);
        };
        addAndMakeVisible(*engineModeToggles[i]);
    }

    // Master Mixer & Tuning
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
    unisonDetuneLabel = createLabel("UNISON SPREAD", *this);

    const char* chromaticPitchNames[3] = { "Continuous (Free)", "Chromatic (Semitones)", "Octaves" };
    for (int i = 0; i < 3; ++i) {
        chromaticPitchToggles[i] = createToggle(chromaticPitchNames[i]);
        chromaticPitchToggles[i]->setRadioGroupId(1205);
        chromaticPitchToggles[i]->onClick = [this, i]() {
            setSteppedParam(spChromaticPitch, (uint8_t)i);
        };
        addAndMakeVisible(*chromaticPitchToggles[i]);
    }

    // Real-time 6-Voice Activity & LM13700 VCA Gain Monitoring Panel
    voiceMeterPanel = std::make_unique<ModernVoiceMeterPanel>(engine);
    addAndMakeVisible(*voiceMeterPanel);

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
    for (int i = 0; i < 3; ++i) {
        if (consoleModelToggles[i]) consoleModelToggles[i]->setComponentID("consoleModelToggle[" + juce::String(i) + "]");
    }
    if (mackityToggle) mackityToggle->setComponentID("mackityToggle");
    if (mackityInTrimKnob) mackityInTrimKnob->setComponentID("mackityInTrimKnob");
    if (mackityInTrimLabel) mackityInTrimLabel->setComponentID("mackityInTrimLabel");
    if (mackityOutPadKnob) mackityOutPadKnob->setComponentID("mackityOutPadKnob");
    if (mackityOutPadLabel) mackityOutPadLabel->setComponentID("mackityOutPadLabel");
    if (consoleDiscontinuityKnob) consoleDiscontinuityKnob->setComponentID("consoleDiscontinuityKnob");
    if (consoleDiscontinuityLabel) consoleDiscontinuityLabel->setComponentID("consoleDiscontinuityLabel");
    for (int i = 0; i < 3; ++i) {
        if (engineModeToggles[i]) engineModeToggles[i]->setComponentID("engineModeToggle[" + juce::String(i) + "]");
    }
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
    const auto& preset = engine.getCurrentPreset();

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

void FilterVcaTab::updateFilterModeToggles(int model) {
    setSelectedFilterModel(model);
    const auto options = getFilterModeOptions();
    setSelectedFilterMode(options.selectedIndex);
    for (int i = 0; i < 4; ++i) {
        filterModeToggles[i]->setButtonText(options.labels[static_cast<size_t>(i)]);
        filterModeToggles[i]->setVisible(i < options.visibleCount);
        filterModeToggles[i]->setToggleState(i == options.selectedIndex, juce::dontSendNotification);
    }
}

void FilterVcaTab::updateFilterUIState(int model, int mode) {
    const bool isShelvesEQ = isShelvesEqActive();

    for (int i = 0; i < 4; ++i) {
        eqBandButtons[i].setVisible(isShelvesEQ);
        eqBandButtons[i].setToggleState(i == getCurrentEQBand(), juce::dontSendNotification);
    }

    // Keep static header and badge so filter model/type only appears once in the GUI
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

        const auto& preset = engine.getCurrentPreset();
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

        if ((model == 1 && mode == 2) || (model == 2 && mode == 2)) { // Band-Pass modes
            if (cutoffLabel) cutoffLabel->setText("CENTER FREQ", juce::dontSendNotification);
            if (resoLabel) resoLabel->setText("RESONANCE (Q)", juce::dontSendNotification);
        } else if (model == 2 && mode == 3) { // Shelves SVF High-Pass
            if (cutoffLabel) cutoffLabel->setText("CUTOFF (HP)", juce::dontSendNotification);
            if (resoLabel) resoLabel->setText("RESONANCE (Q)", juce::dontSendNotification);
        } else { // Low-Pass modes (SSI2144, Ripples LP4/LP2, Shelves SVF LP)
            if (cutoffLabel) cutoffLabel->setText("CUTOFF", juce::dontSendNotification);
            if (resoLabel) resoLabel->setText((model == 0 || (model == 1 && mode <= 1)) ? "RESONANCE" : "RESONANCE (Q)", juce::dontSendNotification);
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
            options.labels = { "4-Pole Lowpass (24 dB)", "2-Pole Lowpass (12 dB)", "2-Pole Bandpass (12 dB)", "" };
            options.visibleCount = 3;
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
        case 1: return { cpCutoff, cpShelvesP1Gain, cpResonance, EqThirdControl::Q, "MID 1 FREQ", "MID 1 GAIN", "MID 1 Q" };
        case 2: return { cpShelvesP2Freq, cpShelvesP2Gain, cpShelvesP2Q, EqThirdControl::Q, "MID 2 FREQ", "MID 2 GAIN", "MID 2 Q" };
        default: return { cpShelvesHsFreq, cpShelvesHsGain, cpFilKbdAmt, EqThirdControl::Percent, "HIGH FREQ", "HIGH GAIN", "KEY TRACK" };
    }
}

void FilterVcaTab::updateFromEngine() {
    const auto& preset = engine.getCurrentPreset();

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

    uint8_t cModel = preset.steppedParams[spConsoleModel];
    for (int i = 0; i < 3; ++i) {
        if (consoleModelToggles[i])
            consoleModelToggles[i]->setToggleState(i == cModel, juce::dontSendNotification);
    }
    safeSetToggle(mackityToggle.get(), preset.steppedParams[spConsoleModel] != 0);
    safeSetKnob(mackityInTrimKnob.get(), scan_potFrom16bits(preset.continuousParams[cpMackityInTrim]));
    safeSetKnob(mackityOutPadKnob.get(), scan_potFrom16bits(preset.continuousParams[cpMackityOutPad]));
    safeSetKnob(consoleDiscontinuityKnob.get(), scan_potFrom16bits(preset.continuousParams[cpConsoleDiscontinuity]));
    uint8_t eMode = preset.steppedParams[spEngineMode];
    for (int i = 0; i < 3; ++i) {
        if (engineModeToggles[i])
            engineModeToggles[i]->setToggleState(i == eMode, juce::dontSendNotification);
    }

    if (filterCurve) filterCurve->repaint();
}

void FilterVcaTab::resized() {
    const auto tabBounds = getLocalBounds();

    int colGap = 5;
    int totalW = tabBounds.getWidth();

    int colW = (totalW - colGap * 2) / 3;
    int col1W = colW;
    int col2W = colW;
    int col3W = totalW - colGap * 2 - col1W - col2W;

    int col1X = 0;
    int col2X = col1X + col1W + colGap;
    int col3X = col2X + col2W + colGap;

    const bool isEQ = isShelvesEqActive();
    int cardTopH = 328; // Increased hardware height across Row 1 cards

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
    for (int i = 0; i < 4; ++i) {
        if (filterModeToggles[i])
            filterModeToggles[i]->setBounds(col1X + midX + 8, filCtrlStartY + i * toggleStep, subColW - 6, toggleH);
    }

    // Middle Section: Cutoff, Reso & Modulation (or EQ Band selector)
    int knobSz = getStandardKnobSize(); // 55px hardware standard
    int filKnobSlotW = (col1W - 24) / 4;

    if (isEQ) {
        filterCard.addDivider(130, "EQ BAND SELECTOR & PARAMETERS");
        filterCard.addVerticalDivider(12 + filKnobSlotW * 2, 140, cardTopH - 12);
        int btnGap = 4;
        int btnW = (col1W - 24 - btnGap * 3) / 4;
        for (int i = 0; i < 4; ++i) {
            eqBandButtons[i].setVisible(true);
            eqBandButtons[i].setBounds(col1X + 12 + i * (btnW + btnGap), 146, btnW, 22);
        }
        int eqKnobSz = 48;
        int filKnobY = 194;
        layoutKnob(cutoffKnob, cutoffLabel, col1X + 12 + (filKnobSlotW - eqKnobSz) / 2, filKnobY, eqKnobSz);
        layoutKnob(resoKnob, resoLabel, col1X + 12 + filKnobSlotW + (filKnobSlotW - eqKnobSz) / 2, filKnobY, eqKnobSz);
        layoutKnob(filKbdKnob, filKbdLabel, col1X + 12 + filKnobSlotW * 2 + (filKnobSlotW - eqKnobSz) / 2, filKnobY, eqKnobSz);
        layoutKnob(filEnvAmtKnob, filEnvAmtLabel, col1X + 12 + filKnobSlotW * 3 + (filKnobSlotW - eqKnobSz) / 2, filKnobY, eqKnobSz);
    } else {
        for (int i = 0; i < 4; ++i) {
            eqBandButtons[i].setVisible(false);
        }
        int halfSlotW = filKnobSlotW * 2;
        filterCard.addDivider(6, 130, halfSlotW - 4, "CUTOFF & RESONANCE");
        filterCard.addDivider(12 + halfSlotW + 4, 130, halfSlotW - 4, "MODULATION");
        filterCard.addVerticalDivider(12 + halfSlotW, 140, cardTopH - 12);
        int filKnobY = 184;
        layoutKnob(cutoffKnob, cutoffLabel, col1X + 12 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(resoKnob, resoLabel, col1X + 12 + filKnobSlotW + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(filKbdKnob, filKbdLabel, col1X + 12 + filKnobSlotW * 2 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(filEnvAmtKnob, filEnvAmtLabel, col1X + 12 + filKnobSlotW * 3 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
    }

    // ------------------------------------------
    // Card 2: AMPLIFIER (VCA)
    // ------------------------------------------
    vcaCard.setBounds(col2X, 0, col2W, cardTopH);
    vcaCard.clearDividers();
    vcaCard.addDivider(26, "AMPLIFIER LEVEL & GLIDE");

    int vcaSlotW = (col2W - 24) / 3;
    int col1SlotX = col2X + 12;
    int col2SlotX = col2X + 12 + vcaSlotW;
    int col3SlotX = col2X + 12 + vcaSlotW * 2;

    int knobY1 = 38;
    int glideKnobX = col2SlotX + (vcaSlotW - knobSz) / 2;
    int unisonX = col3SlotX + 4;
    int unisonW = (col2X + col2W - 12) - unisonX;

    layoutKnob(ampLevelKnob, ampLevelLabel, col1SlotX + (vcaSlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(glideKnob, glideLabel, glideKnobX, knobY1, knobSz);
    if (unisonToggle) {
        unisonToggle->setBounds(unisonX, knobY1 + (knobSz - 24) / 2, unisonW, 24);
    }

    // Sektion 2: CONSOLE MODEL & DRIVE (Voice allocation moved to AFX tab)
    vcaCard.addDivider(130, "CONSOLE MODEL (AIRWINDOWS CONSOLEX)");
    int cStartY = 148;
    int cToggleStep = 24;
    for (int i = 0; i < 3; ++i) {
        if (consoleModelToggles[i]) {
            consoleModelToggles[i]->setBounds(col1SlotX, cStartY + i * cToggleStep, vcaSlotW - 6, 20);
        }
    }

    int mackKnobSz = knobSz;
    int mackKnobY = 160;
    layoutKnob(mackityInTrimKnob, mackityInTrimLabel, glideKnobX, mackKnobY, mackKnobSz);
    layoutKnob(mackityOutPadKnob, mackityOutPadLabel, unisonX, mackKnobY, mackKnobSz);

    int discSz = mackKnobSz;
    int discY = 236;
    layoutKnob(consoleDiscontinuityKnob, consoleDiscontinuityLabel, col1SlotX + (vcaSlotW - discSz) / 2, discY, discSz);

    // ------------------------------------------
    // Card 3: MASTER MIXER & TUNING (MIXER)
    // ------------------------------------------
    mixerCard.setBounds(col3X, 0, col3W, cardTopH);
    mixerCard.clearDividers();
    mixerCard.addDivider(26, "LEVELS & MASTER TUNING");

    int mixSlotW = (col3W - 24) / 3;
    layoutKnob(noiseVolKnob.get(), noiseVolLabel, col3X + 12 + (mixSlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(masterTuneKnob.get(), masterTuneLabel, col3X + 12 + mixSlotW + (mixSlotW - knobSz) / 2, knobY1, knobSz);
    layoutKnob(unisonDetuneKnob.get(), unisonDetuneLabel, col3X + 12 + mixSlotW * 2 + (mixSlotW - knobSz) / 2, knobY1, knobSz);

    mixerCard.addDivider(130, "KEYBOARD PITCH QUANTIZATION");
    int pitchToggleH = 18;
    int pitchToggleStep = 21;
    int pitchStartY = 146;
    for (int i = 0; i < 3; ++i) {
        if (chromaticPitchToggles[i])
            chromaticPitchToggles[i]->setBounds(col3X + 12, pitchStartY + i * pitchToggleStep, col3W - 24, pitchToggleH);
    }

    mixerCard.addDivider(236, "SYNTH ENGINE MODE");
    int engGap = 6;
    int engBtnW = (col3W - 24 - engGap * 2) / 3;
    int engBtnH = 26;
    int engY = 256;
    for (int i = 0; i < 3; ++i) {
        if (engineModeToggles[i])
            engineModeToggles[i]->setBounds(col3X + 12 + i * (engBtnW + engGap), engY, engBtnW, engBtnH);
    }

    // ------------------------------------------
    // Row 2: filterCurve, Envelope Routing & voiceMeterPanel
    // ------------------------------------------
    int row2Gap = 5;
    int row2Y = cardTopH + row2Gap;
    int row2H = tabBounds.getHeight() - row2Y;

    if (row2H > 40) {
        int curveW = (int)std::round((totalW - row2Gap) * 0.52f);
        int meterW = totalW - row2Gap - curveW;

        int envRoutingH = 48;
        int curveH = std::max(40, row2H - envRoutingH - 6);

        if (filterCurve) {
            filterCurve->setBounds(0, row2Y, curveW, curveH);
        }

        int envStartY = row2Y + curveH + 6;
        int margin = 8;
        int totalAvailW = curveW - 2 * margin;
        int envColW = (totalAvailW - 2 * 10) / 3;
        int col1LeftX = margin;
        int col2MidX = margin + envColW + 10;
        int col3RightX = curveW - margin - (envColW + 8);
        int envTogH = 18;

        if (filEnvTypeToggles[0]) filEnvTypeToggles[0]->setBounds(col1LeftX, envStartY, envColW, envTogH);
        if (filEnvTypeToggles[2]) filEnvTypeToggles[2]->setBounds(col2MidX, envStartY, envColW, envTogH);
        if (filEnvLoopToggle) filEnvLoopToggle->setBounds(col3RightX, envStartY, envColW + 8, 20);

        if (filEnvTypeToggles[1]) filEnvTypeToggles[1]->setBounds(col1LeftX, envStartY + 22, envColW, envTogH);
        if (filEnvTypeToggles[3]) filEnvTypeToggles[3]->setBounds(col2MidX, envStartY + 22, envColW, envTogH);

        if (auto* meter = getVoiceMeterPanel()) {
            meter->setBounds(curveW + row2Gap, row2Y, meterW, row2H);
        }
    }
}
