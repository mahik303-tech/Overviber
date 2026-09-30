#include "FilterVcaTab.h"
#include "../../data/ParamLabels.h"
#include "../../dsp/SemFilter.h"

#include <algorithm>

FilterVcaTab::FilterVcaTab(ModernTabContext& context)
    : ModernTabModule(context) {}

// LADDER: SSI2144 (24 dB) and the SST ladder's 18/12/6 dB taps; RIPPLES: the
// SEM model's Liquid (Ripples) variant; SEM: the Cytomic SVF; SHELVES: the
// 4-band EQ.
// Other SEM variants and the SST's 24 dB and Shelves' SVF modes stay in the
// engine but are not offered here. Equal filters share a row across the
// families: Lowpass 24 dB in row 1, Bandpass 12 dB in row 2, Lowpass 12 dB
// in row 3.
const std::vector<FilterVcaTab::FilterChoice>& FilterVcaTab::filterChoices(int family) {
    static const std::vector<FilterChoice> choices[kFilterFamilyCount] = {
        { { fmSSI2144, 0, 0, "Lowpass 24 dB" }, { fmSST, 0, 1, "Lowpass 18 dB" },
          { fmSST, 0, 2, "Lowpass 12 dB" }, { fmSST, 0, 3, "Lowpass 6 dB" } },
        { { fmSem, SemFilter::Liquid, 0, "Lowpass 24 dB" }, { fmSem, SemFilter::Liquid, 2, "Bandpass 12 dB" },
          { fmSem, SemFilter::Liquid, 1, "Lowpass 12 dB" } },
        { { fmSem, SemFilter::Cytomic, 2, "Highpass 12 dB" }, { fmSem, SemFilter::Cytomic, 1, "Bandpass 12 dB" },
          { fmSem, SemFilter::Cytomic, 0, "Lowpass 12 dB" }, { fmSem, SemFilter::Cytomic, 3, "Notch" } },
        { { fmEQ, 0, 0, "4-Band Parametric" } },
    };
    return choices[juce::jlimit(0, kFilterFamilyCount - 1, family)];
}

int FilterVcaTab::filterFamilyOf(int model, int semVariant) {
    switch (model) {
        case fmSSI2144: case fmSST: return 0;
        case fmSem: return semVariant == SemFilter::Liquid ? 1 : 2;
        default: return 3;
    }
}

// The entry showing a preset's filter: the one with its model and mode
// (other SEM variants show as the Cytomic entry of their mode). One the UI
// does not offer (SST 24 dB, Shelves SVF) shows as the family's first entry.
int FilterVcaTab::filterEntryOf(int family, int model, int mode) {
    const auto& choices = filterChoices(family);
    for (size_t i = 0; i < choices.size(); ++i)
        if (choices[i].model == model && choices[i].mode == mode) return (int)i;
    return 0;
}

int FilterVcaTab::entryForFamily(int family) const {
    const bool visited = familyVisited[(size_t)family];
    const int remembered = visited ? familyEntryMemory[(size_t)family] : 0;
    if (visited && !matchFilterOnFamilySwitch) return remembered;
    // The same filter (by its label) if the family offers it, else the same
    // type (the label's first word, e.g. Lowpass), else the family's last
    // choice (its first entry when not visited yet).
    const auto& current = filterChoices(selectedFilterFamily);
    const auto& target = filterChoices(family);
    if (selectedFilterEntry < 0 || selectedFilterEntry >= (int)current.size()) return remembered;
    const juce::String label = current[(size_t)selectedFilterEntry].label;
    for (size_t i = 0; i < target.size(); ++i)
        if (label == target[i].label) return (int)i;
    const juce::String type = label.upToFirstOccurrenceOf(" ", false, false);
    for (size_t i = 0; i < target.size(); ++i)
        if (juce::String(target[i].label).startsWith(type + " ")) return (int)i;
    return remembered;
}

void FilterVcaTab::selectFilterChoice(int family, int entry) {
    const bool enteringShelves = family == 3 && selectedFilterFamily != 3;
    const auto& choices = filterChoices(family);
    entry = juce::jlimit(0, (int)choices.size() - 1, entry);
    const auto& choice = choices[(size_t)entry];
    selectedFilterFamily = family;
    selectedFilterEntry = entry;
    familyEntryMemory[(size_t)family] = entry;
    familyVisited[(size_t)family] = true;
    selectedFilterModel = choice.model;
    selectedFilterMode = choice.mode;
    setSteppedParam(spFilterModel, choice.model);
    if (choice.model == fmSem) setSteppedParam(spSemModel, choice.variant);
    setSteppedParam(spFilterMode, choice.mode);
    // In the EQ the cutoff is the mid low band's frequency; the other filters'
    // open cutoff (26 kHz) would put it out of reach, so it starts at 400 Hz,
    // and its Q (the resonance) at the mid bands' default Q 1.0.
    if (enteringShelves) {
        setContinuousParam(cpCutoff, 433.0f);
        // The mid low band's Q is the resonance: start at the mid bands' Q 1.0.
        setContinuousParam(cpResonance, kShelvesDefaultQPot);
    }
    updateFilterModeToggles();
    updateFilterUIState();
    resized();
}

void FilterVcaTab::setup() {
    addAndMakeVisible(filterCard);
    filterCard.toBack();
    createFilterControls();
    createFilterCurve();
    updateFilterUIState();

    addAndMakeVisible(vcaCard);
    addAndMakeVisible(mixerCard);
    vcaCard.toBack();
    mixerCard.toBack();
    createAmplifierControls();
    createMixerControls();

    // Real-time 6-Voice Activity & LM13700 VCA Gain Monitoring Panel
    voiceMeterPanel = std::make_unique<ModernVoiceMeterPanel>(model);
    voiceMeterPanel->onContinuousParam = [this](continuousParameter_t cp, float pot) { setContinuousParam(cp, pot); };
    voiceMeterPanel->onSteppedParam = [this](steppedParameter_t sp, uint8_t v) { setSteppedParam(sp, v); };
    addAndMakeVisible(*voiceMeterPanel);

    assignComponentIDs();
}

void FilterVcaTab::createFilterControls() {
    // Filter families and their entries (filterChoices())
    const char* familyNames[kFilterFamilyCount] = { "Ladder", "Ripples", "SEM", "Shelves" };
    for (int i = 0; i < kFilterFamilyCount; ++i) {
        filterModelToggles[i] = createToggle(familyNames[i]);
        filterModelToggles[i]->setRadioGroupId(1201);
        filterModelToggles[i]->onClick = [this, i]() { selectFilterChoice(i, entryForFamily(i)); };
        addAndMakeVisible(*filterModelToggles[i]);
    }

    for (int i = 0; i < 4; ++i) {
        filterModeToggles[i] = createToggle("");
        filterModeToggles[i]->setRadioGroupId(1202);
        filterModeToggles[i]->onClick = [this, i]() { selectFilterChoice(selectedFilterFamily, i); };
        addAndMakeVisible(*filterModeToggles[i]);
    }
    updateFilterModeToggles();

    // 4-Band Shelves EQ Band Selector Buttons
    const char* bandNames[] = { "LOW", "MID LOW", "MID HIGH", "HIGH" };
    for (int i = 0; i < 4; ++i) {
        eqBandButtons[i].setButtonText(bandNames[i]);
        eqBandButtons[i].setClickingTogglesState(false);
        eqBandButtons[i].setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        eqBandButtons[i].getProperties().set("compactFont", true);
        eqBandButtons[i].onClick = [this, i]() { selectEQBand(i); };
        addChildComponent(eqBandButtons[i]);
    }

    cutoffKnob = createParamKnob(cutoffLabel, "CUTOFF", "Cutoff", 0, 999, 999, KnobMode::CutoffHz, cpCutoff);
    resoKnob = createParamKnob(resoLabel, "RESONANCE", "Reso", 0, 999, 100, KnobMode::Percent, cpResonance);
    filKbdKnob = createParamKnob(filKbdLabel, "KEY TRACK", "FKbd", 0, 999, 500, KnobMode::Percent, cpFilKbdAmt);

    eqQKnob = createKnob("EqQ", 0, 999, 300, KnobMode::Raw);
    addChildComponent(*eqQKnob);
    eqQLabel = createLabel("Q", *this);
    eqQLabel->setVisible(false);

    filEnvAmtKnob = createParamKnob(filEnvAmtLabel, "ENV DEPTH", "FEnv", -499, 499, 0, KnobMode::BipolarPercent,
                                    cpFilEnvAmt, 500.0f);

    for (int i = 0; i < 4; ++i) {
        filEnvTypeToggles[i] = createToggle(paramlabels::kEnvelopeTypes[i]);
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
}

// Interactive frequency response; in the Shelves EQ its band handles set
// the bands' frequency, gain and Q.
void FilterVcaTab::createFilterCurve() {
    filterCurve = std::make_unique<FilterCurveComponent>(model, *cutoffKnob, *resoKnob);
    filterCurve->onBandSelected = [this](int b) { selectEQBand(b); };
    filterCurve->onBandParamChanged = [this](int band, float potFreq, float potGain) {
        if (band < 0 || band >= (int)eqBands().size()) return;
        const auto& b = eqBands()[(size_t)band];
        setContinuousParam(b.frequency, potFreq);
        setContinuousParam(b.gain, potGain);
        if (band == getCurrentEQBand()) {
            if (cutoffKnob && !cutoffKnob->isMouseButtonDown())
                cutoffKnob->setValue((int)std::round(potFreq), juce::dontSendNotification);
            if (resoKnob && !resoKnob->isMouseButtonDown())
                resoKnob->setValue((int)std::round(potGain) - 500, juce::dontSendNotification);
        }
    };
    filterCurve->onBandQChanged = [this](int band, float deltaQ) {
        if (band < 0 || band >= (int)eqBands().size()) return;
        const auto& b = eqBands()[(size_t)band];
        if (b.thirdControl != EqThirdControl::Q) return;
        const float current = (float)scan_potFrom16bits(model.getCurrentPreset().continuousParams[b.third]);
        const float next = std::clamp(current + deltaQ * 40.0f, 0.0f, 999.0f);
        setContinuousParam(b.third, next);
        if (getCurrentEQBand() == band && eqQKnob && !eqQKnob->isMouseButtonDown())
            eqQKnob->setValue((int)std::round(next), juce::dontSendNotification);
    };
    addAndMakeVisible(*filterCurve);
}

void FilterVcaTab::createAmplifierControls() {
    ampLevelKnob = createParamKnob(ampLevelLabel, "LEVEL", "Level", 0, 999, 500, KnobMode::Percent, cpAmpLevel);
    glideKnob = createParamKnob(glideLabel, "GLIDE", "Glide", 0, 999, 0, KnobMode::TimeMs, cpGlide);
    glideKnob->textFromValueFunction = [](double value) { return formatGlideTime(value); };
    glideKnob->updateText();

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
    consoleDriveKnob = createParamKnob(consoleDriveLabel, "DRIVE", "ConsoleDrive", 0, 999, 100, KnobMode::Raw, cpConsoleDrive);
    consoleDriveKnob->textFromValueFunction = [](double val) -> juce::String {
        // ConsoleX maps the pot linearly to 0.7x .. 3.7x (100 = 1.0x = 0 dB).
        const float gain = 0.7f + 3.0f * (float)val / 999.0f;
        const float db = 20.0f * std::log10(gain);
        return (db >= 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
    };
    consoleDriveKnob->updateText();

    consoleDiscontinuityKnob = createParamKnob(consoleDiscontinuityLabel, "AIR", "Discontinuity", 0, 999, 17,
                                               KnobMode::Percent, cpConsoleDiscontinuity);

    mackityDriveKnob = createParamKnob(mackityDriveLabel, "DRIVE", "MackityDrive", 0, 999, 300, KnobMode::Raw, cpMackityDrive);
    mackityDriveKnob->textFromValueFunction = [](double val) -> juce::String {
        // Airwindows Mackity input trim: gain = (a * 10)^2, 100 = 0 dB.
        const float a = (float)val / 999.0f;
        const float gain = (a * 10.0f) * (a * 10.0f);
        if (gain <= 0.0001f) return "-inf dB";
        const float db = 20.0f * std::log10(gain);
        return (db >= 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
    };
    mackityDriveKnob->updateText();
}

// Mixer & tuning: two columns, each knob under its own named divider
void FilterVcaTab::createMixerControls() {
    noiseVolKnob = createParamKnob(noiseVolLabel, "NOISE LEVEL", "Noise", 0, 999, 0, KnobMode::Percent, cpNoiseVol);
    masterTuneKnob = createParamKnob(masterTuneLabel, "MASTER TUNE", "MTune", -499, 499, 0, KnobMode::TuneCents,
                                     cpMasterTune, 500.0f);
    unisonDetuneKnob = createParamKnob(unisonDetuneLabel, "SPREAD", "MDet", 0, 999, 10, KnobMode::Percent, cpUnisonDetune);

    const char* chromaticPitchNames[3] = { "Free", "Semitones", "Octaves" };
    for (int i = 0; i < 3; ++i) {
        chromaticPitchToggles[i] = createToggle(chromaticPitchNames[i]);
        chromaticPitchToggles[i]->setRadioGroupId(1205);
        chromaticPitchToggles[i]->onClick = [this, i]() { setSteppedParam(spChromaticPitch, (uint8_t)i); };
        addAndMakeVisible(*chromaticPitchToggles[i]);
    }

    // Voices: count 1 .. 6 and the note priority when all are in use.
    voiceCountSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    voiceCountSlider->setName("VoiceCount");
    voiceCountSlider->setRange(1, SYNTH_VOICE_COUNT, 1.0);
    voiceCountSlider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 20);
    voiceCountSlider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffb0bec5));
    voiceCountSlider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    voiceCountSlider->textFromValueFunction = [](double value) -> juce::String {
        const int voices = (int)std::round(value);
        return voices == 1 ? "Mono" : juce::String(voices) + " Poly";
    };
    voiceCountSlider->valueFromTextFunction = [](const juce::String& text) -> double {
        return text.containsIgnoreCase("mono") ? 1 : std::clamp(text.getIntValue(), 1, SYNTH_VOICE_COUNT);
    };
    voiceCountSlider->setValue(SYNTH_VOICE_COUNT, juce::dontSendNotification);
    voiceCountSlider->updateText();
    voiceCountSlider->onValueChange = [this]() {
        setSteppedParam(spVoiceCount, (uint8_t)std::clamp((int)std::round(voiceCountSlider->getValue()) - 1, 0,
                                                          SYNTH_VOICE_COUNT - 1));
    };
    addAndMakeVisible(*voiceCountSlider);
    const char* priorityNames[3] = { "Last", "Low", "High" };
    for (int i = 0; i < 3; ++i) {
        assignerPrioToggles[i] = createToggle(priorityNames[i]);
        assignerPrioToggles[i]->setRadioGroupId(1204);
        assignerPrioToggles[i]->setTooltip(i == 0 ? "A new note takes the oldest voice"
                                          : i == 1 ? "The lowest notes keep their voices"
                                                   : "The highest notes keep their voices");
        assignerPrioToggles[i]->onClick = [this, i]() { setSteppedParam(spAssignerPriority, (uint8_t)i); };
        addAndMakeVisible(*assignerPrioToggles[i]);
    }
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
    if (eqQKnob) eqQKnob->setComponentID("eqQKnob");
    if (eqQLabel) eqQLabel->setComponentID("eqQLabel");
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
    if (voiceCountSlider) voiceCountSlider->setComponentID("voiceCountSlider");
    for (int i = 0; i < 3; ++i)
        if (assignerPrioToggles[i]) assignerPrioToggles[i]->setComponentID("assignerPrioToggle[" + juce::String(i) + "]");
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
    applyEQBandSelection();
}

void FilterVcaTab::applyEQBandSelection() {
    for (int i = 0; i < 4; ++i) {
        eqBandButtons[i].setToggleState(i == getCurrentEQBand(), juce::dontSendNotification);
    }
    if (filterCurve) {
        filterCurve->setActiveBand(getCurrentEQBand());
    }
    updateEQKnobsForCurrentBand();
}

// Knob <-> parameter: the knob shows pot - offset and sets pot = value +
// offset; repaintCurve for the parameters the response curve shows.
void FilterVcaTab::bindPotKnob(juce::Slider& knob, continuousParameter_t cp, int offset, bool repaintCurve) {
    auto* raw = &knob;
    knob.onValueChange = [this, raw, cp, offset, repaintCurve]() {
        setContinuousParam(cp, (float)raw->getValue() + (float)offset);
        if (repaintCurve && filterCurve) filterCurve->repaint();
    };
}

void FilterVcaTab::syncPotKnob(juce::Slider& knob, continuousParameter_t cp, int offset) {
    if (!knob.isMouseButtonDown())
        knob.setValue(scan_potFrom16bits(model.getCurrentPreset().continuousParams[cp]) - offset, juce::dontSendNotification);
    knob.updateText();
}

// Shelves EQ frequency: 20 Hz x 1000 (20 Hz .. 20 kHz).
void FilterVcaTab::formatEqFrequency(juce::Slider& s) {
    s.setRange(0, 999, 1.0);
    s.textFromValueFunction = [](double val) -> juce::String {
        float hz = 20.0f * std::pow(10.0f, ((float)val / 999.0f) * 3.0f);
        if (hz >= 1000.0f) return juce::String(hz / 1000.0f, 2) + " kHz";
        return juce::String((int)std::round(hz)) + " Hz";
    };
    s.valueFromTextFunction = [](const juce::String& text) -> double {
        juce::String t = text.trim().toLowerCase();
        float mul = t.contains("k") ? 1000.0f : 1.0f;
        float hz = t.replace("khz", "").replace("hz", "").replace("k", "").trim().getFloatValue() * mul;
        hz = std::clamp(hz, 20.0f, 20000.0f);
        return (std::log10(hz / 20.0f) / 3.0f) * 999.0;
    };
}

// Shelves EQ gain: +-18 dB around pot 500.
void FilterVcaTab::formatEqGain(juce::Slider& s) {
    s.setRange(-499, 499, 1.0);
    s.textFromValueFunction = [](double val) -> juce::String {
        float db = ((float)val / 499.0f) * 18.0f;
        int r = (int)std::round(db);
        return (r > 0 ? "+" : "") + juce::String(r) + " dB";
    };
    s.valueFromTextFunction = [](const juce::String& text) -> double {
        float db = text.replace("db", "").replace("+", "").trim().getFloatValue();
        return std::clamp((db / 18.0f) * 499.0f, -499.0f, 499.0f);
    };
}

// Shelves EQ Q of the mid bands (shelvesQ, 0.5 .. 40).
void FilterVcaTab::formatEqQ(juce::Slider& s) {
    s.setRange(0, 999, 1.0);
    s.textFromValueFunction = [](double val) -> juce::String {
        const float q = shelvesQ((float)val);
        return "Q " + juce::String(q, 2);
    };
    s.valueFromTextFunction = [](const juce::String& text) -> double {
        float q = text.replace("q", "").trim().getFloatValue();
        q = std::clamp(q, 0.5f, 40.0f);
        return shelvesQPot(q);
    };
}

// The filters' cutoff: 20 Hz x 1300 (20 Hz .. 26 kHz).
void FilterVcaTab::formatFilterCutoff(juce::Slider& s) {
    s.setRange(0, 999, 1.0);
    s.textFromValueFunction = [](double val) -> juce::String {
        float hz = 20.0f * std::pow(1300.0f, (float)val / 999.0f);
        if (hz >= 1000.0f) return juce::String(hz / 1000.0f, 2) + " kHz";
        return juce::String((int)std::round(hz)) + " Hz";
    };
    s.valueFromTextFunction = [](const juce::String& text) -> double {
        juce::String t = text.trim().toLowerCase();
        float mul = t.contains("k") ? 1000.0f : 1.0f;
        float hz = t.replace("khz", "").replace("hz", "").replace("k", "").trim().getFloatValue() * mul;
        hz = std::clamp(hz, 20.0f, 26000.0f);
        return (std::log(hz / 20.0f) / std::log(1300.0f)) * 999.0;
    };
}

void FilterVcaTab::formatPercent(juce::Slider& s) {
    s.setRange(0, 999, 1.0);
    applyKnobFormat(s, KnobMode::Percent, 0, 999);
}

void FilterVcaTab::formatBipolarPercent(juce::Slider& s) {
    s.setRange(-499, 499, 1.0);
    applyKnobFormat(s, KnobMode::BipolarPercent, -499, 499);
}

// Shelves EQ: cutoff and resonance knobs edit the selected band's frequency
// and gain, the Q knob the mid bands' Q; KEY TRACK and ENV DEPTH stay.
void FilterVcaTab::updateEQKnobsForCurrentBand() {
    const auto binding = getActiveEqBandBinding();
    formatEqFrequency(*cutoffKnob);
    bindPotKnob(*cutoffKnob, binding.frequency, 0, true);
    syncPotKnob(*cutoffKnob, binding.frequency, 0);
    if (cutoffLabel) cutoffLabel->setText(binding.frequencyLabel, juce::dontSendNotification);

    formatEqGain(*resoKnob);
    bindPotKnob(*resoKnob, binding.gain, 500, true);
    syncPotKnob(*resoKnob, binding.gain, 500);
    if (resoLabel) resoLabel->setText(binding.gainLabel, juce::dontSendNotification);

    // KEY TRACK for all bands; the third knob is the mid bands' Q.
    formatPercent(*filKbdKnob);
    bindPotKnob(*filKbdKnob, cpFilKbdAmt, 0, false);
    syncPotKnob(*filKbdKnob, cpFilKbdAmt, 0);
    if (filKbdLabel) filKbdLabel->setText("KEY TRACK", juce::dontSendNotification);

    const bool hasQ = binding.thirdControl == EqThirdControl::Q;
    if (hasQ) {
        formatEqQ(*eqQKnob);
        bindPotKnob(*eqQKnob, binding.third, 0, true);
        syncPotKnob(*eqQKnob, binding.third, 0);
        if (eqQLabel) eqQLabel->setText(binding.thirdLabel, juce::dontSendNotification);
    }
    if (eqQKnob) eqQKnob->setVisible(hasQ);
    if (eqQLabel) eqQLabel->setVisible(hasQ);

    formatBipolarPercent(*filEnvAmtKnob);
    bindPotKnob(*filEnvAmtKnob, cpFilEnvAmt, 500, false);
    syncPotKnob(*filEnvAmtKnob, cpFilEnvAmt, 500);
    if (filEnvAmtLabel) filEnvAmtLabel->setText("ENV DEPTH", juce::dontSendNotification);
}

void FilterVcaTab::updateFilterModeToggles() {
    const auto options = getFilterModeOptions();
    for (int i = 0; i < kFilterFamilyCount; ++i)
        if (filterModelToggles[i])
            filterModelToggles[i]->setToggleState(i == selectedFilterFamily, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i) {
        filterModeToggles[i]->setButtonText(options.labels[static_cast<size_t>(i)]);
        filterModeToggles[i]->setVisible(i < options.visibleCount);
        filterModeToggles[i]->setToggleState(i == options.selectedIndex, juce::dontSendNotification);
    }
}

void FilterVcaTab::updateFilterUIState() {
    const bool isShelvesEQ = isShelvesEqActive();

    for (int i = 0; i < 4; ++i) {
        eqBandButtons[i].setVisible(isShelvesEQ);
        eqBandButtons[i].setToggleState(i == getCurrentEQBand(), juce::dontSendNotification);
    }

    // Keep static header and badge so filter filterModel/type only appears once in the GUI
    filterCard.setHeader("FILTER", "VCF");

    if (isShelvesEQ) {
        updateEQKnobsForCurrentBand();
    } else {
        // The filter knobs: cutoff, resonance, key track and envelope depth
        formatFilterCutoff(*cutoffKnob);
        bindPotKnob(*cutoffKnob, cpCutoff, 0, true);
        formatPercent(*resoKnob);
        bindPotKnob(*resoKnob, cpResonance, 0, true);
        formatPercent(*filKbdKnob);
        bindPotKnob(*filKbdKnob, cpFilKbdAmt, 0, false);
        formatBipolarPercent(*filEnvAmtKnob);
        bindPotKnob(*filEnvAmtKnob, cpFilEnvAmt, 500, false);
        syncPotKnob(*cutoffKnob, cpCutoff, 0);
        syncPotKnob(*resoKnob, cpResonance, 0);
        syncPotKnob(*filKbdKnob, cpFilKbdAmt, 0);
        syncPotKnob(*filEnvAmtKnob, cpFilEnvAmt, 500);

        // From the selected entry of the filter table: bandpass and notch
        // filters show FREQ (their centre), the others CUTOFF.
        const auto& choices = filterChoices(selectedFilterFamily);
        const juce::String entry = selectedFilterEntry >= 0 && selectedFilterEntry < (int)choices.size()
            ? juce::String(choices[(size_t)selectedFilterEntry].label) : juce::String();
        const bool centred = entry.startsWith("Bandpass") || entry.startsWith("Notch");
        if (cutoffLabel) cutoffLabel->setText(centred ? "FREQ" : "CUTOFF", juce::dontSendNotification);
        if (resoLabel) resoLabel->setText("RESONANCE", juce::dontSendNotification);

        if (filKbdLabel) filKbdLabel->setText("KEY TRACK", juce::dontSendNotification);
        if (filEnvAmtLabel) filEnvAmtLabel->setText("ENV DEPTH", juce::dontSendNotification);
    }

    if (filterCurve) filterCurve->repaint();
}

FilterVcaTab::FilterModeOptions FilterVcaTab::getFilterModeOptions() const {
    FilterModeOptions options;
    const auto& choices = filterChoices(selectedFilterFamily);
    options.visibleCount = (int)choices.size();
    for (size_t i = 0; i < choices.size(); ++i) options.labels[i] = choices[i].label;
    options.selectedIndex = juce::jlimit(0, options.visibleCount - 1, selectedFilterEntry);
    return options;
}

// The four Shelves bands: low shelf, mid low (the filter's cutoff and
// resonance), mid high, high shelf.
const std::array<FilterVcaTab::EqBandBinding, 4>& FilterVcaTab::eqBands() {
    static const std::array<EqBandBinding, 4> bands{ {
        { cpShelvesLsFreq, cpShelvesLsGain, cpFilKbdAmt, EqThirdControl::None, "LOW FREQ", "LOW GAIN", "" },
        { cpCutoff, cpShelvesP1Gain, cpResonance, EqThirdControl::Q, "MID LOW FREQ", "MID LOW GAIN", "MID LOW Q" },
        { cpShelvesP2Freq, cpShelvesP2Gain, cpShelvesP2Q, EqThirdControl::Q, "MID HIGH FREQ", "MID HIGH GAIN", "MID HIGH Q" },
        { cpShelvesHsFreq, cpShelvesHsGain, cpFilKbdAmt, EqThirdControl::None, "HIGH FREQ", "HIGH GAIN", "" },
    } };
    return bands;
}

FilterVcaTab::EqBandBinding FilterVcaTab::getActiveEqBandBinding() const {
    return eqBands()[(size_t)juce::jlimit(0, 3, currentEQBand)];
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
    selectedFilterModel = fModel;
    selectedFilterMode = fMode;
    selectedFilterFamily = filterFamilyOf(fModel, preset.steppedParams[spSemModel]);
    selectedFilterEntry = filterEntryOf(selectedFilterFamily, fModel, fMode);
    familyEntryMemory[(size_t)selectedFilterFamily] = selectedFilterEntry;
    familyVisited[(size_t)selectedFilterFamily] = true;
    updateFilterModeToggles();
    updateFilterUIState();
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
    safeSetKnob(voiceCountSlider.get(), juce::jlimit(1, SYNTH_VOICE_COUNT, preset.steppedParams[spVoiceCount] + 1));
    for (int i = 0; i < 3; ++i)
        safeSetToggle(assignerPrioToggles[i].get(), i == preset.steppedParams[spAssignerPriority]);

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
    laidOutFilterModel = selectedFilterModel;
    laidOutFilterMode = selectedFilterMode;
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
        // The knobs edit the selected band; the compact band selector sits
        // below them, under the cutoff (frequency) knob and its neighbours.
        int filKnobY = 143;   // labels end 2 px above the band buttons
        constexpr int btnW = 64, btnH = 14, btnGap = 4;
        const int groupX = col1X + (col1W - (4 * btnW + 3 * btnGap)) / 2;
        for (int i = 0; i < 4; ++i) {
            eqBandButtons[i].setVisible(true);
            eqBandButtons[i].setBounds(groupX + i * (btnW + btnGap), cardTopH - btnH - 6, btnW, btnH);
        }
        layoutKnob(cutoffKnob, cutoffLabel, col1X + 12 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(resoKnob, resoLabel, col1X + 12 + filKnobSlotW + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(eqQKnob, eqQLabel, col1X + 12 + filKnobSlotW * 2 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        layoutKnob(filEnvAmtKnob, filEnvAmtLabel, col1X + 12 + filKnobSlotW * 3 + (filKnobSlotW - knobSz) / 2, filKnobY, knobSz);
        // KEY TRACK in the mode column below the (single) EQ mode, flush
        // above ENV DEPTH.
        layoutKnob(filKbdKnob, filKbdLabel, col1X + 12 + filKnobSlotW * 3 + (filKnobSlotW - knobSz) / 2,
                   filCtrlStartY + toggleStep - 2, knobSz);
    } else {
        if (eqQKnob) eqQKnob->setVisible(false);
        if (eqQLabel) eqQLabel->setVisible(false);
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

    // Card: TUNING & VOICES. TUNE | PITCH quantize toggles, then UNISON
    // across both columns (spread knob left, on/off toggle right), then
    // VOICES: the voice count and the note priority.
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

    const int voicesDivY = spreadY + knobCellH + 6;
    mixerCard.addDivider(voicesDivY, "VOICES");
    if (voiceCountSlider) voiceCountSlider->setBounds(mixerX + 12, row2Y + voicesDivY + 12, cardW - 24, 22);
    const int prioW = (cardW - 24) / 3;
    for (int i = 0; i < 3; ++i)
        if (assignerPrioToggles[i])
            assignerPrioToggles[i]->setBounds(mixerX + 12 + i * prioW, row2Y + voicesDivY + 38, prioW, 20);

    if (auto* meter = getVoiceMeterPanel()) meter->setBounds(meterX, row2Y, totalW - meterX, row2H);
}
