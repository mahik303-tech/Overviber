#include "FilterVcaTab.h"
#include "../components/ModernGlyphs.h"
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
    updateFilterChoiceButtons();
    updateFilterUIState();
    resized();
}

void FilterVcaTab::setup() {
    addAndMakeVisible(filterCard);
    filterCard.toBack();
    createFilterControls();
    createFilterCurve();
    updateFilterUIState();

    addAndMakeVisible(mixerCard);
    addAndMakeVisible(outputCard);
    mixerCard.toBack();
    outputCard.toBack();
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
    // Filter families and their entries (filterChoices()); only the selected
    // family's entries are shown.
    filterFamilyChoice = std::make_unique<ModernChoiceButtons>(
        juce::StringArray{ "LADDER", "RIPPLES", "SEM", "SHELVES" });
    filterFamilyChoice->setTooltips({ "SSI2144 and SST ladder", "SEM model, Liquid (Ripples) variant",
                                      "Cytomic state variable filter", "4-band parametric EQ" });
    filterFamilyChoice->onSelect = [this](int family) { selectFilterChoice(family, entryForFamily(family)); };
    addAndMakeVisible(*filterFamilyChoice);

    // Each filter as its response symbol and slope (the notch: its name)
    for (int family = 0; family < kFilterFamilyCount - 1; ++family) {
        juce::StringArray labels;
        for (const auto& choice : filterChoices(family)) labels.add(juce::String(choice.label).toUpperCase());
        auto& group = filterEntryChoices[(size_t)family];
        group = std::make_unique<ModernChoiceButtons>(labels, 2);
        group->setTooltips(labels);
        group->setGlyphPainter([this, labels](juce::Graphics& g, juce::Rectangle<float> area, int i, juce::Colour colour) {
            using modernglyphs::Response;
            const juce::String& label = labels[i];
            const auto type = label.startsWith("HIGHPASS") ? Response::highpass
                            : label.startsWith("BANDPASS") ? Response::bandpass
                            : label.startsWith("NOTCH") ? Response::notch : Response::lowpass;
            const int slope = label.upToFirstOccurrenceOf(" DB", false, false).getTrailingIntValue();
            const juce::String text = slope > 0 ? juce::String(slope) + " DB" : label;
            modernglyphs::drawWithText(g, area, text, modernLnf.getCustomFont(10.5f, juce::Font::bold), colour,
                                       [type, slope](juce::Rectangle<float> r) {
                                           return modernglyphs::response(r, type, (float)(slope - 6) / 18.0f);
                                       });
        });
        group->onSelect = [this, family](int entry) { selectFilterChoice(family, entry); };
        addChildComponent(*group);
    }

    // The Shelves EQ's band: what the knobs edit
    eqBandChoice = std::make_unique<ModernChoiceButtons>(juce::StringArray{ "LOW", "MID LOW", "MID HIGH", "HIGH" }, 2);
    eqBandChoice->setTooltips({ "Low shelf", "Mid low peak (the filter's cutoff and resonance)", "Mid high peak",
                                "High shelf" });
    eqBandChoice->setGlyphPainter([this](juce::Graphics& g, juce::Rectangle<float> area, int band, juce::Colour colour) {
        using modernglyphs::Response;
        static const char* const names[4] = { "LOW", "MID LOW", "MID HIGH", "HIGH" };
        const Response shapes[4] = { Response::lowShelf, Response::peak, Response::peak, Response::highShelf };
        modernglyphs::drawWithText(g, area, names[band], modernLnf.getCustomFont(10.5f, juce::Font::bold), colour,
                                   [shape = shapes[band]](juce::Rectangle<float> r) { return modernglyphs::response(r, shape); });
    });
    eqBandChoice->onSelect = [this](int band) { selectEQBand(band); };
    addChildComponent(*eqBandChoice);
    updateFilterChoiceButtons();

    cutoffKnob = createParamKnob(cutoffLabel, "CUTOFF", "Cutoff", 0, 999, 999, KnobMode::CutoffHz, cpCutoff);
    resoKnob = createParamKnob(resoLabel, "RESONANCE", "Reso", 0, 999, 100, KnobMode::Percent, cpResonance);
    filKbdKnob = createParamKnob(filKbdLabel, "KEY TRACK", "FKbd", 0, 999, 500, KnobMode::Percent, cpFilKbdAmt);

    eqQKnob = createKnob("EqQ", 0, 999, 300, KnobMode::Raw);
    addChildComponent(*eqQKnob);
    eqQLabel = createLabel("Q", *this);
    eqQLabel->setVisible(false);

    filEnvAmtKnob = createParamKnob(filEnvAmtLabel, "ENV DEPTH", "FEnv", -499, 499, 0, KnobMode::BipolarPercent,
                                    cpFilEnvAmt, 500.0f);
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

// Output stages in signal order: VCA level, ConsoleX bus, Mackity
void FilterVcaTab::createAmplifierControls() {
    ampLevelKnob = createParamKnob(ampLevelLabel, "LEVEL", "Level", 0, 999, 500, KnobMode::Percent, cpAmpLevel);

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

// Tuning & voices: pitch (tune, glide, quantize), unison, voice allocation
void FilterVcaTab::createMixerControls() {
    masterTuneKnob = createParamKnob(masterTuneLabel, "MASTER TUNE", "MTune", -499, 499, 0, KnobMode::TuneCents,
                                     cpMasterTune, 500.0f);
    glideKnob = createParamKnob(glideLabel, "GLIDE", "Glide", 0, 999, 0, KnobMode::TimeMs, cpGlide);
    glideKnob->textFromValueFunction = [](double value) { return formatGlideTime(value); };
    glideKnob->updateText();

    // The steps of the oscillators' pitch knobs
    pitchQuantizeChoice = std::make_unique<ModernChoiceButtons>(juce::StringArray{ "FREE", "SEMITONES", "OCTAVES" });
    pitchQuantizeChoice->setTooltips({ "Pitch knobs turn freely", "Pitch knobs step in semitones",
                                       "Pitch knobs step in octaves" });
    pitchQuantizeChoice->onSelect = [this](int i) { setSteppedParam(spChromaticPitch, (uint8_t)i); };
    addAndMakeVisible(*pitchQuantizeChoice);

    unisonDetuneKnob = createParamKnob(unisonDetuneLabel, "SPREAD", "MDet", 0, 999, 10, KnobMode::Percent, cpUnisonDetune);
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

    // Voices: count 1 .. 6 and the note priority when all are in use.
    voiceCountSlider = std::make_unique<juce::Slider>("VoiceCount");
    voiceCountSlider->setSliderStyle(juce::Slider::LinearBar);
    voiceCountSlider->setRange(1, SYNTH_VOICE_COUNT, 1.0);
    voiceCountSlider->textFromValueFunction = [](double value) -> juce::String {
        const int voices = (int)std::round(value);
        return voices == 1 ? "1 VOICE  MONO" : juce::String(voices) + " VOICES  POLY";
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
    voicePriorityChoice = std::make_unique<ModernChoiceButtons>(juce::StringArray{ "LAST", "LOW", "HIGH" });
    voicePriorityChoice->setTooltips({ "A new note takes the oldest voice", "The lowest notes keep their voices",
                                       "The highest notes keep their voices" });
    voicePriorityChoice->onSelect = [this](int i) { setSteppedParam(spAssignerPriority, (uint8_t)i); };
    addAndMakeVisible(*voicePriorityChoice);
}

void FilterVcaTab::assignComponentIDs() {
    filterCard.setComponentID("filterCard");
    mixerCard.setComponentID("mixerCard");
    outputCard.setComponentID("outputCard");

    // Tab 2: Filter
    if (filterFamilyChoice) filterFamilyChoice->setIdPrefix("filterFamilyButton");
    const char* entryPrefixes[kFilterFamilyCount - 1] = { "ladderModeButton", "ripplesModeButton", "semModeButton" };
    for (size_t f = 0; f < filterEntryChoices.size(); ++f)
        if (filterEntryChoices[f]) filterEntryChoices[f]->setIdPrefix(entryPrefixes[f]);
    if (eqBandChoice) eqBandChoice->setIdPrefix("eqBandButton");
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
    if (filterCurve) filterCurve->setComponentID("filterCurve");

    // Tab 2: output & bus, tuning & voices
    if (ampLevelKnob) ampLevelKnob->setComponentID("ampLevelKnob");
    if (ampLevelLabel) ampLevelLabel->setComponentID("ampLevelLabel");
    if (consoleDriveKnob) consoleDriveKnob->setComponentID("consoleDriveKnob");
    if (consoleDriveLabel) consoleDriveLabel->setComponentID("consoleDriveLabel");
    if (consoleDiscontinuityKnob) consoleDiscontinuityKnob->setComponentID("consoleDiscontinuityKnob");
    if (consoleDiscontinuityLabel) consoleDiscontinuityLabel->setComponentID("consoleDiscontinuityLabel");
    if (mackityDriveKnob) mackityDriveKnob->setComponentID("mackityDriveKnob");
    if (mackityDriveLabel) mackityDriveLabel->setComponentID("mackityDriveLabel");
    if (masterTuneKnob) masterTuneKnob->setComponentID("masterTuneKnob");
    if (masterTuneLabel) masterTuneLabel->setComponentID("masterTuneLabel");
    if (glideKnob) glideKnob->setComponentID("glideKnob");
    if (glideLabel) glideLabel->setComponentID("glideLabel");
    if (pitchQuantizeChoice) pitchQuantizeChoice->setIdPrefix("pitchQuantizeButton");
    if (unisonDetuneKnob) unisonDetuneKnob->setComponentID("unisonDetuneKnob");
    if (unisonDetuneLabel) unisonDetuneLabel->setComponentID("unisonDetuneLabel");
    if (unisonToggle) unisonToggle->setComponentID("unisonToggle");
    if (voiceCountSlider) voiceCountSlider->setComponentID("voiceCountSlider");
    if (voicePriorityChoice) voicePriorityChoice->setIdPrefix("voicePriorityButton");
    if (voiceMeterPanel) voiceMeterPanel->setComponentID("voiceMeterPanel");
}

void FilterVcaTab::selectEQBand(int band) {
    setCurrentEQBand(band);
    applyEQBandSelection();
}

void FilterVcaTab::applyEQBandSelection() {
    if (eqBandChoice) eqBandChoice->setSelected(getCurrentEQBand());
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
// and gain, the Q knob the mid bands' Q; KEY TRACK and ENV DEPTH stay. The
// band buttons name the band, the captions only the control.
void FilterVcaTab::updateEQKnobsForCurrentBand() {
    const auto binding = getActiveEqBandBinding();
    formatEqFrequency(*cutoffKnob);
    bindPotKnob(*cutoffKnob, binding.frequency, 0, true);
    syncPotKnob(*cutoffKnob, binding.frequency, 0);
    if (cutoffLabel) cutoffLabel->setText("FREQ", juce::dontSendNotification);

    formatEqGain(*resoKnob);
    bindPotKnob(*resoKnob, binding.gain, 500, true);
    syncPotKnob(*resoKnob, binding.gain, 500);
    if (resoLabel) resoLabel->setText("GAIN", juce::dontSendNotification);

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
        if (eqQLabel) eqQLabel->setText("Q", juce::dontSendNotification);
    }
    if (eqQKnob) eqQKnob->setVisible(hasQ);
    if (eqQLabel) eqQLabel->setVisible(hasQ);

    formatBipolarPercent(*filEnvAmtKnob);
    bindPotKnob(*filEnvAmtKnob, cpFilEnvAmt, 500, false);
    syncPotKnob(*filEnvAmtKnob, cpFilEnvAmt, 500);
    if (filEnvAmtLabel) filEnvAmtLabel->setText("ENV DEPTH", juce::dontSendNotification);
}

void FilterVcaTab::updateFilterChoiceButtons() {
    if (filterFamilyChoice) filterFamilyChoice->setSelected(selectedFilterFamily);
    const auto options = getFilterModeOptions();
    for (size_t f = 0; f < filterEntryChoices.size(); ++f) {
        auto* group = filterEntryChoices[f].get();
        if (group == nullptr) continue;
        const bool shown = (int)f == selectedFilterFamily;
        group->setVisible(shown);
        if (shown) group->setSelected(options.selectedIndex);
    }
}

void FilterVcaTab::updateFilterUIState() {
    const bool isShelvesEQ = isShelvesEqActive();

    if (eqBandChoice) {
        eqBandChoice->setVisible(isShelvesEQ);
        eqBandChoice->setSelected(getCurrentEQBand());
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
        { cpShelvesLsFreq, cpShelvesLsGain, cpFilKbdAmt, EqThirdControl::None },
        { cpCutoff, cpShelvesP1Gain, cpResonance, EqThirdControl::Q },
        { cpShelvesP2Freq, cpShelvesP2Gain, cpShelvesP2Q, EqThirdControl::Q },
        { cpShelvesHsFreq, cpShelvesHsGain, cpFilKbdAmt, EqThirdControl::None },
    } };
    return bands;
}

FilterVcaTab::EqBandBinding FilterVcaTab::getActiveEqBandBinding() const {
    return eqBands()[(size_t)juce::jlimit(0, 3, currentEQBand)];
}

void FilterVcaTab::updateFromEngine() {
    const auto& preset = model.getCurrentPreset();

    // Tuning & voices
    safeSetKnob(masterTuneKnob.get(), scan_potFrom16bits(preset.continuousParams[cpMasterTune]) - 500);
    safeSetKnob(glideKnob.get(), scan_potFrom16bits(preset.continuousParams[cpGlide]));
    if (pitchQuantizeChoice) pitchQuantizeChoice->setSelected(preset.steppedParams[spChromaticPitch]);
    safeSetKnob(unisonDetuneKnob.get(), scan_potFrom16bits(preset.continuousParams[cpUnisonDetune]));
    safeSetToggle(unisonToggle.get(), preset.steppedParams[spUnison] != 0);
    if (unisonToggle && unisonToggle->onStateChange) unisonToggle->onStateChange(); // ON / OFF caption
    safeSetKnob(voiceCountSlider.get(), juce::jlimit(1, SYNTH_VOICE_COUNT, preset.steppedParams[spVoiceCount] + 1));
    if (voicePriorityChoice) voicePriorityChoice->setSelected(preset.steppedParams[spAssignerPriority]);

    // Filter & VCA
    uint8_t fModel = preset.steppedParams[spFilterModel];
    uint8_t fMode = preset.steppedParams[spFilterMode];
    selectedFilterModel = fModel;
    selectedFilterMode = fMode;
    selectedFilterFamily = filterFamilyOf(fModel, preset.steppedParams[spSemModel]);
    selectedFilterEntry = filterEntryOf(selectedFilterFamily, fModel, fMode);
    familyEntryMemory[(size_t)selectedFilterFamily] = selectedFilterEntry;
    familyVisited[(size_t)selectedFilterFamily] = true;
    updateFilterChoiceButtons();
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

    // Output & bus
    safeSetKnob(ampLevelKnob.get(), scan_potFrom16bits(preset.continuousParams[cpAmpLevel]));
    safeSetKnob(consoleDriveKnob.get(), scan_potFrom16bits(preset.continuousParams[cpConsoleDrive]));
    safeSetKnob(consoleDiscontinuityKnob.get(), scan_potFrom16bits(preset.continuousParams[cpConsoleDiscontinuity]));
    safeSetKnob(mackityDriveKnob.get(), scan_potFrom16bits(preset.continuousParams[cpMackityDrive]));

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
    // Card 1: FILTER (VCF). The family on top, the knobs in the middle, the
    // family's filters (the Shelves EQ: its band) at the bottom.
    // ------------------------------------------
    filterCard.setBounds(col1X, 0, col1W, cardTopH);
    filterCard.clearDividers();

    const int innerX = col1X + 12, innerW = col1W - 24;
    if (filterFamilyChoice) filterFamilyChoice->setBounds(innerX, 34, innerW, 24);

    int knobSz = getStandardKnobSize(); // 55px hardware standard
    constexpr int sectionDivY = 68;
    constexpr int filKnobY = 80;
    constexpr int choiceDivY = 160, choiceY = 170, choiceH = 50;   // two rows
    const int knobsBottom = choiceDivY - 4;

    filterCard.addDivider(choiceDivY, isEQ ? "EQ BAND" : "FILTER TYPE");
    for (auto& group : filterEntryChoices)
        if (group) group->setBounds(innerX, choiceY, innerW, choiceH);
    if (eqBandChoice) eqBandChoice->setBounds(innerX, choiceY, innerW, choiceH);

    if (isEQ) {
        // The band's frequency, gain and Q, then key track and envelope depth
        const int slotW = innerW / 5;
        auto slotX = [&](int slot) { return innerX + slot * slotW + (slotW - knobSz) / 2; };
        filterCard.addDivider(6, sectionDivY, slotW * 3 + 2, "BAND");
        filterCard.addDivider(12 + slotW * 3 + 4, sectionDivY, slotW * 2 - 4, "MODULATION");
        filterCard.addVerticalDivider(12 + slotW * 3, sectionDivY + 10, knobsBottom);
        layoutKnob(cutoffKnob, cutoffLabel, slotX(0), filKnobY, knobSz);
        layoutKnob(resoKnob, resoLabel, slotX(1), filKnobY, knobSz);
        layoutKnob(eqQKnob, eqQLabel, slotX(2), filKnobY, knobSz);
        layoutKnob(filKbdKnob, filKbdLabel, slotX(3), filKnobY, knobSz);
        layoutKnob(filEnvAmtKnob, filEnvAmtLabel, slotX(4), filKnobY, knobSz);
    } else {
        if (eqQKnob) eqQKnob->setVisible(false);
        if (eqQLabel) eqQLabel->setVisible(false);
        const int slotW = innerW / 4;
        auto slotX = [&](int slot) { return innerX + slot * slotW + (slotW - knobSz) / 2; };
        filterCard.addDivider(6, sectionDivY, slotW * 2 - 4, "CUTOFF & RESONANCE");
        filterCard.addDivider(12 + slotW * 2 + 4, sectionDivY, slotW * 2 - 4, "MODULATION");
        filterCard.addVerticalDivider(12 + slotW * 2, sectionDivY + 10, knobsBottom);
        layoutKnob(cutoffKnob, cutoffLabel, slotX(0), filKnobY, knobSz);
        layoutKnob(resoKnob, resoLabel, slotX(1), filKnobY, knobSz);
        layoutKnob(filKbdKnob, filKbdLabel, slotX(2), filKnobY, knobSz);
        layoutKnob(filEnvAmtKnob, filEnvAmtLabel, slotX(3), filKnobY, knobSz);
    }

    // Row 1, columns 2-3: the response curve
    if (filterCurve) filterCurve->setBounds(col2X, 0, totalW - col2X, cardTopH);

    // ------------------------------------------
    // Row 2: mixerCard | outputCard | voiceMeterPanel
    // ------------------------------------------
    const int row2Y = cardTopH + colGap;
    const int row2H = tabBounds.getHeight() - row2Y;
    if (row2H <= 40) return;

    const int cardW = 220; // mixerCard and outputCard share one width
    const int mixerX = 0;
    const int outputX = mixerX + cardW + colGap;
    const int meterX = outputX + cardW + colGap;
    constexpr int knobCellH = 71; // standard knob + caption below
    const int colW = (cardW - 24) / 2;
    auto columnKnob = [&](int cardX, auto& knob, auto& label, int column, int y) {
        layoutKnob(knob, label, cardX + 12 + column * colW + (colW - knobSz) / 2, row2Y + y, knobSz);
    };
    auto centredKnob = [&](int cardX, auto& knob, auto& label, int y) {
        layoutKnob(knob, label, cardX + (cardW - knobSz) / 2, row2Y + y, knobSz);
    };

    // Card: TUNING & VOICES. PITCH (master tune, glide), the pitch knobs'
    // QUANTIZE steps, UNISON (spread knob, on/off), VOICES (count, priority).
    mixerCard.setBounds(mixerX, row2Y, cardW, row2H);
    mixerCard.clearDividers();
    mixerCard.addDivider(38, "PITCH");
    columnKnob(mixerX, masterTuneKnob, masterTuneLabel, 0, 48);
    columnKnob(mixerX, glideKnob, glideLabel, 1, 48);
    const int quantizeDivY = 48 + knobCellH + 8;
    mixerCard.addDivider(quantizeDivY, "QUANTIZE");
    if (pitchQuantizeChoice) pitchQuantizeChoice->setBounds(mixerX + 12, row2Y + quantizeDivY + 10, cardW - 24, 20);
    const int unisonDivY = quantizeDivY + 40;
    mixerCard.addDivider(unisonDivY, "UNISON");
    const int spreadY = unisonDivY + 10;
    columnKnob(mixerX, unisonDetuneKnob, unisonDetuneLabel, 0, spreadY);
    if (unisonToggle)
        unisonToggle->setBounds(mixerX + 12 + colW + 8, row2Y + spreadY + (knobSz - 22) / 2, colW - 8, 22);
    const int voicesDivY = spreadY + knobCellH + 8;
    mixerCard.addDivider(voicesDivY, "VOICES");
    if (voiceCountSlider) voiceCountSlider->setBounds(mixerX + 12, row2Y + voicesDivY + 10, cardW - 24, 20);
    if (voicePriorityChoice) voicePriorityChoice->setBounds(mixerX + 12, row2Y + voicesDivY + 34, cardW - 24, 20);

    // Card: OUTPUT & BUS in signal order: VCA level, ConsoleX bus (drive,
    // air), Mackity drive (its send is on the mixer's master strip).
    outputCard.setBounds(outputX, row2Y, cardW, row2H);
    outputCard.clearDividers();
    const int outStep = juce::jlimit(knobCellH + 18, 100, (row2H - 48 - knobCellH - 12) / 2);
    outputCard.addDivider(38, "VCA");
    centredKnob(outputX, ampLevelKnob, ampLevelLabel, 48);
    outputCard.addDivider(38 + outStep, "CONSOLEX BUS");
    columnKnob(outputX, consoleDriveKnob, consoleDriveLabel, 0, 48 + outStep);
    columnKnob(outputX, consoleDiscontinuityKnob, consoleDiscontinuityLabel, 1, 48 + outStep);
    outputCard.addDivider(38 + 2 * outStep, "MACKITY");
    centredKnob(outputX, mackityDriveKnob, mackityDriveLabel, 48 + 2 * outStep);

    if (auto* meter = getVoiceMeterPanel()) meter->setBounds(meterX, row2Y, totalW - meterX, row2H);
}
