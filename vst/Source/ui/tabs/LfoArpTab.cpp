#include "LfoArpTab.h"
#include "../../data/ParamLabels.h"
#include "../../dsp/lfo.h"

namespace {
// Depth knobs: component ID part, knob name part, caption.
const char* const kDepthIds[5] = { "Pitch", "WMod", "Fil", "Res", "Amp" };
const char* const kDepthKnobNames[5] = { "Pit", "Wmo", "Fil", "Res", "Amp" };
const char* const kDepthLabels[5] = { "PITCH", "WAVEMOD", "CUTOFF", "RESO", "VOLUME" };

juce::StringArray choiceLabels(const paramlabels::Choice* choices, int count, bool shortLabels) {
    juce::StringArray labels;
    for (int i = 0; i < count; ++i) labels.add(shortLabels ? choices[i].host : choices[i].label);
    return labels;
}

// Card layout (y in the tab; the cards start at 0).
constexpr int kCardHeight = 318;
// Button rows at the top (shape), in the middle (range, trigger) and at the
// bottom (pitch target), the knob rows between them.
constexpr int kSpeedDividerY = 72, kSpeedKnobY = 84, kRangeRowY = 162;
constexpr int kDepthDividerY = 196, kDepthKnobY = 208, kPitchTargetY = 290;
}

const std::array<LfoArpTab::LfoDescriptor, 2> LfoArpTab::kLfos{ {
    { 1, cpLFOFreq, cpLFOAmt, { cpLFOPitchAmt, cpLFOWModAmt, cpLFOFilAmt, cpLFOResAmt, cpLFOAmpAmt },
      spLFOShape, spLFOSpeed, spLFOTargets, spLFOTrig },
    { 2, cpLFO2Freq, cpLFO2Amt, { cpLFO2PitchAmt, cpLFO2WModAmt, cpLFO2FilAmt, cpLFO2ResAmt, cpLFO2AmpAmt },
      spLFO2Shape, spLFO2Speed, spLFO2Targets, spLFO2Trig },
} };

LfoArpTab::LfoArpTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void LfoArpTab::setup() {
    addAndMakeVisible(lfo1Card);
    addAndMakeVisible(lfo2Card);
    lfo1Card.toBack();
    lfo2Card.toBack();

    for (size_t i = 0; i < kLfos.size(); ++i) createLfo(lfos[i], kLfos[i]);

    addAndMakeVisible(arpCard);
    arpCard.toBack();
    createArp();

    assignComponentIDs();
}

void LfoArpTab::paintLfoShape(juce::Graphics& g, juce::Rectangle<float> area, int shape, juce::Colour colour) {
    const float x0 = area.getX(), w = area.getWidth(), top = area.getY(), bottom = area.getBottom();
    const float mid = area.getCentreY();
    auto at = [&](float fx, float level) { return juce::Point<float>(x0 + fx * w, bottom - level * (bottom - top)); };
    juce::Path path;
    switch (shape) {
    case 0:   // pulse / square
        path.startNewSubPath(at(0.0f, 0.0f)); path.lineTo(at(0.0f, 1.0f)); path.lineTo(at(0.5f, 1.0f));
        path.lineTo(at(0.5f, 0.0f)); path.lineTo(at(1.0f, 0.0f)); path.lineTo(at(1.0f, 1.0f));
        break;
    case 1:   // triangle
        path.startNewSubPath(x0, mid); path.lineTo(at(0.25f, 1.0f)); path.lineTo(at(0.75f, 0.0f));
        path.lineTo(at(1.0f, 0.5f));
        break;
    case 2: { // random sample & hold
        const float levels[6] = { 0.35f, 0.9f, 0.15f, 0.65f, 1.0f, 0.45f };
        path.startNewSubPath(at(0.0f, levels[0]));
        for (int i = 0; i < 6; ++i) {
            if (i > 0) path.lineTo(at(i / 6.0f, levels[i]));
            path.lineTo(at((i + 1) / 6.0f, levels[i]));
        }
        break;
    }
    case 3:   // sine
        path.startNewSubPath(x0, mid);
        for (int i = 1; i <= 24; ++i)
            path.lineTo(at(i / 24.0f, 0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * i / 24.0f)));
        break;
    case 4: { // noise
        const float levels[13] = { 0.5f, 0.8f, 0.2f, 0.95f, 0.4f, 0.1f, 0.7f, 0.3f, 1.0f, 0.05f, 0.6f, 0.85f, 0.45f };
        path.startNewSubPath(at(0.0f, levels[0]));
        for (int i = 1; i < 13; ++i) path.lineTo(at(i / 12.0f, levels[i]));
        break;
    }
    case 5:   // sawtooth (rising)
        path.startNewSubPath(at(0.0f, 0.0f)); path.lineTo(at(0.5f, 1.0f)); path.lineTo(at(0.5f, 0.0f));
        path.lineTo(at(1.0f, 1.0f));
        break;
    default:  // inverted saw (falling)
        path.startNewSubPath(at(0.0f, 1.0f)); path.lineTo(at(0.5f, 0.0f)); path.lineTo(at(0.5f, 1.0f));
        path.lineTo(at(1.0f, 0.0f));
        break;
    }
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
}

void LfoArpTab::createLfo(LfoSection& lfo, const LfoDescriptor& d) {
    const juce::String n(d.number);
    auto knob = [this](std::unique_ptr<juce::Slider>& slider, std::unique_ptr<juce::Label>& label,
                       const juce::String& name, int init, KnobMode mode, continuousParameter_t cp,
                       const juce::String& caption) {
        slider = createKnob(name, 0, 999, init, mode);
        auto* raw = slider.get();
        slider->onValueChange = [this, raw, cp]() { setContinuousParam(cp, (float)raw->getValue()); };
        addAndMakeVisible(*slider);
        label = createLabel(caption, *this);
    };

    knob(lfo.speedKnob, lfo.speedLabel, n + "Spd", 250, KnobMode::LfoSpeedHz, d.speed, "SPEED / RATE");
    knob(lfo.amountKnob, lfo.amountLabel, n + "Amt", 0, KnobMode::Percent, d.amount, "MOD DEPTH");
    // The start delay (one parameter) sits in the card of the LFO it acts
    // on: as in the firmware, the LFO the mod wheel does not control.
    knob(lfo.delayKnob, lfo.delayLabel, "MDly", 0, KnobMode::TimeMs, cpModDelay, "START DELAY");
    lfo.delayKnob->textFromValueFunction = [](double value) { return formatModDelayTime(value); };
    lfo.delayKnob->updateText();
    lfo.delayKnob->setTooltip("Fades this LFO in after a note starts. It acts on the LFO the mod wheel does not "
                              "control (MOD MATRIX, mod wheel destination).");

    const int shapeCount = (int)std::size(paramlabels::kLfoShapes);
    lfo.shapeChoice = std::make_unique<ModernChoiceButtons>(choiceLabels(paramlabels::kLfoShapes, shapeCount, true));
    lfo.shapeChoice->setTooltips(choiceLabels(paramlabels::kLfoShapes, shapeCount, false));
    lfo.shapeChoice->setGlyphPainter(&LfoArpTab::paintLfoShape);
    lfo.shapeChoice->onSelect = [this, &lfo, d](int shape) {
        setSteppedParam(d.shape, (uint8_t)shape);
        if (lfo.preview) lfo.preview->setShape(shape);
    };
    addAndMakeVisible(*lfo.shapeChoice);

    lfo.rangeChoice = std::make_unique<ModernChoiceButtons>(juce::StringArray{ "x1", "x2", "x4", "x8" });
    lfo.rangeChoice->setTooltips({ "Speed range: normal", "Speed range: 2x faster", "Speed range: 4x faster",
                                   "Speed range: 8x faster" });
    lfo.rangeChoice->onSelect = [this, &lfo, d](int range) {
        setSteppedParam(d.speedRange, (uint8_t)range);
        if (lfo.speedKnob) lfo.speedKnob->updateText();
    };
    addAndMakeVisible(*lfo.rangeChoice);

    lfo.triggerChoice = std::make_unique<ModernChoiceButtons>(juce::StringArray{ "FREE RUN", "KEY SYNC" });
    lfo.triggerChoice->setTooltips({ "The LFO runs freely", "Every note restarts the LFO" });
    lfo.triggerChoice->onSelect = [this, d](int trigger) { setSteppedParam(d.trigger, (uint8_t)trigger); };
    addAndMakeVisible(*lfo.triggerChoice);

    // Depths of the LFO's fixed routings
    for (int i = 0; i < kDepths; ++i)
        knob(lfo.depthKnobs[(size_t)i], lfo.depthLabels[(size_t)i], n + kDepthKnobNames[i], 0, KnobMode::Percent,
             d.depths[(size_t)i], kDepthLabels[i]);

    // Which oscillators the PITCH depth reaches.
    lfo.pitchTargetLabel = createLabel("PITCH TO", *this);
    lfo.pitchTargetLabel->setJustificationType(juce::Justification::centredLeft);
    lfo.pitchTargetChoice = std::make_unique<ModernChoiceButtons>(juce::StringArray{ "NONE", "OSC A", "OSC B", "A + B" });
    lfo.pitchTargetChoice->setTooltips({ "The PITCH depth reaches no oscillator", "PITCH moves oscillator A",
                                         "PITCH moves oscillator B", "PITCH moves both oscillators" });
    lfo.pitchTargetChoice->onSelect = [this, d](int target) { setSteppedParam(d.targets, (uint8_t)target); };
    addAndMakeVisible(*lfo.pitchTargetChoice);

    lfo.preview = std::make_unique<LfoWavePreviewComponent>(model, d.number);
    addAndMakeVisible(*lfo.preview);

    // The speed knob shows the real cycle frequency including the speed range.
    const auto range = d.speedRange;
    lfo.speedKnob->textFromValueFunction = [this, range](double value) {
        return formatLfoSpeed(value, model.getCurrentPreset().steppedParams[range]);
    };
    lfo.speedKnob->updateText();
}

void LfoArpTab::createArp() {
    const int modeCount = (int)std::size(paramlabels::kArpModes);
    arpModeChoice = std::make_unique<ModernChoiceButtons>(
        juce::StringArray{ "OFF", "UP", "DOWN", "UP/DOWN", "RANDOM", "PLAYED", "CHORD", "CONVERGE", "DEGREE", "STRUM" }, 5);
    arpModeChoice->setTooltips(choiceLabels(paramlabels::kArpModes, modeCount, false));
    arpModeChoice->onSelect = [this](int mode) { setSteppedParam(spArpMode, (uint8_t)mode); };
    addAndMakeVisible(*arpModeChoice);

    arpOctaveChoice = std::make_unique<ModernChoiceButtons>(
        juce::StringArray{ "1 OCTAVE", "2 OCTAVES", "3 OCTAVES", "4 OCTAVES" });
    arpOctaveChoice->onSelect = [this](int octaves) { setSteppedParam(spArpOctaves, (uint8_t)octaves); };
    addAndMakeVisible(*arpOctaveChoice);

    arpRateChoice = std::make_unique<ModernChoiceButtons>(
        juce::StringArray{ "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" });
    arpRateChoice->setTooltips({ "Quarter notes", "Eighth notes", "Eighth-note triplets (1/12)", "Sixteenth notes",
                                 "Sixteenth-note triplets (1/24)", "Thirty-second notes" });
    arpRateChoice->onSelect = [this](int rate) { setSteppedParam(spArpRate, (uint8_t)rate); };
    addAndMakeVisible(*arpRateChoice);

    arpGateKnob = createKnob("ArpGate", 100, 999, 833, KnobMode::Percent);
    arpGateKnob->onValueChange = [this]() { setContinuousParam(cpArpGate, (float)arpGateKnob->getValue()); };
    addAndMakeVisible(*arpGateKnob);
    arpGateLabel = createLabel("GATE LEN", *this);

    arpSwingKnob = createKnob("ArpSwing", 500, 750, 500, KnobMode::Raw);
    arpSwingKnob->textFromValueFunction = [](double val) -> juce::String {
        int pct = (int)std::round(val / 10.0);
        if (pct == 50) return "50% (Off)";
        return juce::String(pct) + " %";
    };
    arpSwingKnob->onValueChange = [this]() { setContinuousParam(cpArpSwing, (float)arpSwingKnob->getValue()); };
    addAndMakeVisible(*arpSwingKnob);
    arpSwingLabel = createLabel("SWING", *this);

    arpBpmKnob = createKnob("ArpBpm", 0, 999, 357, KnobMode::Raw);
    arpBpmKnob->textFromValueFunction = [](double val) -> juce::String {
        int bpm = (int)std::round(20.0 + (val / 999.0) * 280.0);
        return juce::String(bpm) + " BPM";
    };
    arpBpmKnob->onValueChange = [this]() { setContinuousParam(cpArpBpm, (float)arpBpmKnob->getValue()); };
    addAndMakeVisible(*arpBpmKnob);
    arpBpmLabel = createLabel("FREE BPM", *this);

    arpHoldToggle = createToggle("LATCH / HOLD");
    arpHoldToggle->onClick = [this]() {
        setSteppedParam(spArpHold, arpHoldToggle->getToggleState() ? 1 : 0);
    };
    addAndMakeVisible(*arpHoldToggle);

    arpSyncToggle = createToggle("HOST SYNC (DAW)");
    arpSyncToggle->onClick = [this]() {
        setSteppedParam(spArpSync, arpSyncToggle->getToggleState() ? 1 : 0);
    };
    addAndMakeVisible(*arpSyncToggle);

    arpVisualizer = std::make_unique<ArpVisualizerComponent>(model);
    addAndMakeVisible(*arpVisualizer);
}

void LfoArpTab::assignComponentIDs() {
    lfo1Card.setComponentID("lfo1Card");
    lfo2Card.setComponentID("lfo2Card");
    arpCard.setComponentID("arpCard");
    for (size_t i = 0; i < kLfos.size(); ++i) assignComponentIDs(lfos[i], kLfos[i]);

    if (arpModeChoice) arpModeChoice->setIdPrefix("arpModeButton");
    if (arpOctaveChoice) arpOctaveChoice->setIdPrefix("arpOctaveButton");
    if (arpRateChoice) arpRateChoice->setIdPrefix("arpRateButton");
    if (arpGateKnob) arpGateKnob->setComponentID("arpGateKnob");
    if (arpGateLabel) arpGateLabel->setComponentID("arpGateLabel");
    if (arpSwingKnob) arpSwingKnob->setComponentID("arpSwingKnob");
    if (arpSwingLabel) arpSwingLabel->setComponentID("arpSwingLabel");
    if (arpBpmKnob) arpBpmKnob->setComponentID("arpBpmKnob");
    if (arpBpmLabel) arpBpmLabel->setComponentID("arpBpmLabel");
    if (arpHoldToggle) arpHoldToggle->setComponentID("arpHoldToggle");
    if (arpSyncToggle) arpSyncToggle->setComponentID("arpSyncToggle");
    if (arpVisualizer) arpVisualizer->setComponentID("arpVisualizer");
}

void LfoArpTab::assignComponentIDs(LfoSection& lfo, const LfoDescriptor& d) {
    const juce::String prefix = "lfo" + juce::String(d.number);
    if (lfo.shapeChoice) lfo.shapeChoice->setIdPrefix(prefix + "ShapeButton");
    if (lfo.rangeChoice) lfo.rangeChoice->setIdPrefix(prefix + "SpeedButton");
    if (lfo.triggerChoice) lfo.triggerChoice->setIdPrefix(prefix + "TrigButton");
    if (lfo.pitchTargetChoice) lfo.pitchTargetChoice->setIdPrefix(prefix + "TargetsButton");
    if (lfo.pitchTargetLabel) lfo.pitchTargetLabel->setComponentID(prefix + "TargetsLabel");
    auto ids = [&](const std::unique_ptr<juce::Slider>& knob, const std::unique_ptr<juce::Label>& label, const juce::String& part) {
        if (knob) knob->setComponentID(prefix + part + "Knob");
        if (label) label->setComponentID(prefix + part + "Label");
    };
    ids(lfo.speedKnob, lfo.speedLabel, "Freq");
    ids(lfo.amountKnob, lfo.amountLabel, "Amt");
    ids(lfo.delayKnob, lfo.delayLabel, "Delay");
    for (int i = 0; i < kDepths; ++i) ids(lfo.depthKnobs[(size_t)i], lfo.depthLabels[(size_t)i], kDepthIds[i]);
    if (lfo.preview) lfo.preview->setComponentID(prefix + "WavePreview");
}

void LfoArpTab::advancePreviewAnimation() {
    // Phase in cycles at the LFO's real frequency (the editor timer runs at 30 Hz).
    for (size_t i = 0; i < kLfos.size(); ++i) {
        auto& lfo = lfos[i];
        const int pot = scan_potFrom16bits(scan_potTo16bits((int)std::round(lfo.speedKnob->getValue())));
        const float hz = lfoCycleHz(pot, (int8_t)model.getCurrentPreset().steppedParams[kLfos[i].speedRange]);
        lfo.phase = std::fmod(lfo.phase + hz * 0.033f, LfoWavePreviewComponent::kPhaseWrap);
        if (lfo.preview) lfo.preview->setPhase(lfo.phase);
    }
}

void LfoArpTab::updateFromEngine() {
    const auto& preset = model.getCurrentPreset();
    // As in the firmware the start delay acts on the LFO the mod wheel does
    // not control (mod wheel on LFO 1 depth -> delay on LFO 2).
    const bool delayOnLfo2 = preset.steppedParams[spModwheelTarget] == 0;
    if (lfos[0].showsDelay == delayOnLfo2 || lfos[1].showsDelay != delayOnLfo2) {
        lfos[0].showsDelay = !delayOnLfo2;
        lfos[1].showsDelay = delayOnLfo2;
        resized();
    }
    for (size_t i = 0; i < kLfos.size(); ++i) updateLfo(lfos[i], kLfos[i]);

    if (arpModeChoice) arpModeChoice->setSelected(preset.steppedParams[spArpMode]);
    if (arpOctaveChoice) arpOctaveChoice->setSelected(preset.steppedParams[spArpOctaves]);
    if (arpRateChoice) arpRateChoice->setSelected(preset.steppedParams[spArpRate]);
    safeSetToggle(arpHoldToggle.get(), preset.steppedParams[spArpHold] != 0);
    safeSetToggle(arpSyncToggle.get(), preset.steppedParams[spArpSync] != 0);
    safeSetKnob(arpGateKnob.get(), scan_potFrom16bits(preset.continuousParams[cpArpGate]));
    safeSetKnob(arpSwingKnob.get(), scan_potFrom16bits(preset.continuousParams[cpArpSwing]));
    safeSetKnob(arpBpmKnob.get(), scan_potFrom16bits(preset.continuousParams[cpArpBpm]));
}

void LfoArpTab::updateLfo(LfoSection& lfo, const LfoDescriptor& d) {
    const auto& preset = model.getCurrentPreset();
    auto pot = [&](continuousParameter_t cp) { return scan_potFrom16bits(preset.continuousParams[cp]); };
    safeSetKnob(lfo.speedKnob.get(), pot(d.speed));
    safeSetKnob(lfo.amountKnob.get(), pot(d.amount));
    safeSetKnob(lfo.delayKnob.get(), pot(cpModDelay));
    if (lfo.shapeChoice) lfo.shapeChoice->setSelected(preset.steppedParams[d.shape]);
    if (lfo.rangeChoice) lfo.rangeChoice->setSelected(preset.steppedParams[d.speedRange]);
    if (lfo.speedKnob) lfo.speedKnob->updateText();   // the range changes the frequency text
    if (lfo.pitchTargetChoice) lfo.pitchTargetChoice->setSelected(preset.steppedParams[d.targets]);
    if (lfo.triggerChoice) lfo.triggerChoice->setSelected(preset.steppedParams[d.trigger]);
    for (int i = 0; i < kDepths; ++i) safeSetKnob(lfo.depthKnobs[(size_t)i].get(), pot(d.depths[(size_t)i]));
    if (lfo.preview) lfo.preview->setShape(preset.steppedParams[d.shape]);
}

void LfoArpTab::resized() {
    const auto tabBounds = getLocalBounds();

    int colGap = 5;
    int availableW = tabBounds.getWidth();
    int colW = (availableW - colGap * 2) / 3;
    const int colX[3] = { 0, colW + colGap, 2 * (colW + colGap) };
    int colW3 = availableW - colX[2];

    ModernSectionCard* lfoCards[2] = { &lfo1Card, &lfo2Card };
    for (size_t i = 0; i < kLfos.size(); ++i) {
        auto& card = *lfoCards[i];
        card.setBounds(colX[i], 0, colW, kCardHeight);
        card.clearDividers();
        card.addDivider(kSpeedDividerY, "SPEED & AMOUNT");
        card.addDivider(kDepthDividerY, "DESTINATIONS");
        layoutLfo(lfos[i], colX[i], colW);
    }

    arpCard.setBounds(colX[2], 0, colW3, kCardHeight);
    arpCard.clearDividers();
    arpCard.addDivider(94, "GATE, GROOVE & TEMPO");
    arpCard.addDivider(186, "OCTAVES & RATE");
    arpCard.addDivider(258, "LATCH & TEMPO SYNC");
    layoutArp(colX[2], colW3);

    // Real-time visualizers (separate bottom cards with 5px gap)
    int previewGap = 5;
    int previewY = kCardHeight + previewGap;
    int previewH = tabBounds.getHeight() - previewY;
    if (previewH > 40) {
        for (size_t i = 0; i < kLfos.size(); ++i)
            if (lfos[i].preview) lfos[i].preview->setBounds(colX[i], previewY, colW, previewH);
        if (arpVisualizer) arpVisualizer->setBounds(colX[2], previewY, colW3, previewH);
    }
}

// Shape symbols, the speed/amount(/delay) knobs with the speed range and
// trigger below them, the five depth knobs and the oscillators the pitch
// depth reaches.
void LfoArpTab::layoutLfo(LfoSection& lfo, int x, int colW) {
    if (lfo.shapeChoice) lfo.shapeChoice->setBounds(x + 10, 34, colW - 20, 26);
    const int halfW = (colW - 30) / 2;
    if (lfo.rangeChoice) lfo.rangeChoice->setBounds(x + 10, kRangeRowY, halfW, 22);
    if (lfo.triggerChoice) lfo.triggerChoice->setBounds(x + 20 + halfW, kRangeRowY, colW - 30 - halfW, 22);

    const int knobSz = getStandardKnobSize();
    const int knob3W = (colW - 30) / 3;
    const int slotW = lfo.showsDelay ? knob3W : knob3W * 3 / 2;   // two knobs share the row without the delay
    layoutKnob(lfo.speedKnob.get(), lfo.speedLabel, x + 15 + (slotW - knobSz) / 2, kSpeedKnobY, knobSz);
    layoutKnob(lfo.amountKnob.get(), lfo.amountLabel, x + 15 + slotW + (slotW - knobSz) / 2, kSpeedKnobY, knobSz);
    layoutKnob(lfo.delayKnob.get(), lfo.delayLabel, x + 15 + slotW * 2 + (slotW - knobSz) / 2, kSpeedKnobY, knobSz);
    if (lfo.delayKnob) lfo.delayKnob->setVisible(lfo.showsDelay);
    if (lfo.delayLabel) lfo.delayLabel->setVisible(lfo.showsDelay);

    const int knob5W = (colW - 20) / 5;
    for (int i = 0; i < kDepths; ++i)
        layoutKnob(lfo.depthKnobs[(size_t)i].get(), lfo.depthLabels[(size_t)i],
                   x + 10 + knob5W * i + (knob5W - knobSz) / 2, kDepthKnobY, knobSz);

    if (lfo.pitchTargetLabel) lfo.pitchTargetLabel->setBounds(x + 10, kPitchTargetY, 64, 20);
    if (lfo.pitchTargetChoice) lfo.pitchTargetChoice->setBounds(x + 76, kPitchTargetY, colW - 86, 20);
}

// The mode grid on top, the gate/swing/tempo knobs, octaves and rate in the
// middle, the two switches at the bottom.
void LfoArpTab::layoutArp(int x, int colW) {
    if (arpModeChoice) arpModeChoice->setBounds(x + 10, 34, colW - 20, 48);

    int knob3W = (colW - 20) / 3;
    int knobSz = getStandardKnobSize();
    layoutKnob(arpGateKnob.get(), arpGateLabel, x + 10 + (knob3W - knobSz) / 2, 106, knobSz);
    layoutKnob(arpSwingKnob.get(), arpSwingLabel, x + 10 + knob3W + (knob3W - knobSz) / 2, 106, knobSz);
    layoutKnob(arpBpmKnob.get(), arpBpmLabel, x + 10 + knob3W * 2 + (knob3W - knobSz) / 2, 106, knobSz);

    if (arpOctaveChoice) arpOctaveChoice->setBounds(x + 10, 196, colW - 20, 22);
    if (arpRateChoice) arpRateChoice->setBounds(x + 10, 224, colW - 20, 22);

    const int halfW = (colW - 24) / 2;
    if (arpHoldToggle) arpHoldToggle->setBounds(x + 12, 272, halfW, 20);
    if (arpSyncToggle) arpSyncToggle->setBounds(x + 12 + halfW, 272, halfW, 20);
}
