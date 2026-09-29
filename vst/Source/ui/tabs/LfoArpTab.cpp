#include "LfoArpTab.h"
#include "../../data/ParamLabels.h"
#include "../../dsp/lfo.h"

namespace {
// Depth knobs: component ID part, knob name part, caption.
const char* const kDepthIds[5] = { "Pitch", "WMod", "Fil", "Res", "Amp" };
const char* const kDepthKnobNames[5] = { "Pit", "Wmo", "Fil", "Res", "Amp" };
const char* const kDepthLabels[5] = { "PITCH", "WAVEMOD", "CUTOFF", "RESO", "VOLUME" };
}

const std::array<LfoArpTab::LfoDescriptor, 2> LfoArpTab::kLfos{ {
    { 1, cpLFOFreq, cpLFOAmt, { cpLFOPitchAmt, cpLFOWModAmt, cpLFOFilAmt, cpLFOResAmt, cpLFOAmpAmt },
      spLFOShape, spLFOSpeed, spLFOTargets, spLFOTrig, true },
    { 2, cpLFO2Freq, cpLFO2Amt, { cpLFO2PitchAmt, cpLFO2WModAmt, cpLFO2FilAmt, cpLFO2ResAmt, cpLFO2AmpAmt },
      spLFO2Shape, spLFO2Speed, spLFO2Targets, spLFO2Trig, false },
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
    if (d.hasStartDelay) {
        knob(lfo.delayKnob, lfo.delayLabel, "MDly", 0, KnobMode::TimeMs, cpModDelay, "START DELAY");
        lfo.delayKnob->textFromValueFunction = [](double value) { return formatModDelayTime(value); };
        lfo.delayKnob->updateText();
    }

    for (int i = 0; i < (int)std::size(paramlabels::kLfoShapes); ++i) lfo.shapeCombo.addItem(paramlabels::kLfoShapes[i].label, i + 1);
    lfo.shapeCombo.onChange = [this, &lfo, d]() {
        const int shape = lfo.shapeCombo.getSelectedId() - 1;
        setSteppedParam(d.shape, (uint8_t)shape);
        if (lfo.preview) lfo.preview->setShape(shape);
    };
    addAndMakeVisible(lfo.shapeCombo);

    lfo.speedCombo.addItem("Normal (x1)", 1);
    lfo.speedCombo.addItem("Fast (x2)", 2);
    lfo.speedCombo.addItem("High (x4)", 3);
    lfo.speedCombo.addItem("Ultra (x8)", 4);
    lfo.speedCombo.onChange = [this, &lfo, d]() {
        setSteppedParam(d.speedRange, (uint8_t)(lfo.speedCombo.getSelectedId() - 1));
        if (lfo.speedKnob) lfo.speedKnob->updateText();
    };
    addAndMakeVisible(lfo.speedCombo);

    lfo.targetsCombo.addItem("Pitch: None", 1);
    lfo.targetsCombo.addItem("Pitch: Osc A Only", 2);
    lfo.targetsCombo.addItem("Pitch: Osc B Only", 3);
    lfo.targetsCombo.addItem("Pitch: Both (A & B)", 4);
    lfo.targetsCombo.onChange = [this, &lfo, d]() { setSteppedParam(d.targets, (uint8_t)(lfo.targetsCombo.getSelectedId() - 1)); };
    addAndMakeVisible(lfo.targetsCombo);

    lfo.triggerCombo.addItem("Free-Running", 1);
    lfo.triggerCombo.addItem("Key-Sync", 2);
    lfo.triggerCombo.onChange = [this, &lfo, d]() { setSteppedParam(d.trigger, (uint8_t)(lfo.triggerCombo.getSelectedId() - 1)); };
    addAndMakeVisible(lfo.triggerCombo);

    // Depths of the LFO's fixed routings
    for (int i = 0; i < kDepths; ++i)
        knob(lfo.depthKnobs[(size_t)i], lfo.depthLabels[(size_t)i], n + kDepthKnobNames[i], 0, KnobMode::Percent,
             d.depths[(size_t)i], kDepthLabels[i]);

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
    for (int i = 0; i < (int)std::size(paramlabels::kArpModes); ++i) arpModeCombo.addItem(paramlabels::kArpModes[i].label, i + 1);
    arpModeCombo.onChange = [this]() {
        setSteppedParam(spArpMode, (uint8_t)(arpModeCombo.getSelectedId() - 1));
    };
    addAndMakeVisible(arpModeCombo);

    arpOctaveCombo.addItem("1 Octave", 1);
    arpOctaveCombo.addItem("2 Octaves", 2);
    arpOctaveCombo.addItem("3 Octaves", 3);
    arpOctaveCombo.addItem("4 Octaves", 4);
    arpOctaveCombo.onChange = [this]() {
        setSteppedParam(spArpOctaves, (uint8_t)(arpOctaveCombo.getSelectedId() - 1));
    };
    addAndMakeVisible(arpOctaveCombo);

    arpRateCombo.addItem("1/4 Note", 1);
    arpRateCombo.addItem("1/8 Note", 2);
    arpRateCombo.addItem("1/8 Triplet (1/12)", 3);
    arpRateCombo.addItem("1/16 Note", 4);
    arpRateCombo.addItem("1/16 Triplet (1/24)", 5);
    arpRateCombo.addItem("1/32 Note", 6);
    arpRateCombo.onChange = [this]() {
        setSteppedParam(spArpRate, (uint8_t)(arpRateCombo.getSelectedId() - 1));
    };
    addAndMakeVisible(arpRateCombo);

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

    arpModeCombo.setComponentID("arpModeCombo");
    arpOctaveCombo.setComponentID("arpOctaveCombo");
    arpRateCombo.setComponentID("arpRateCombo");
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
    lfo.shapeCombo.setComponentID(prefix + "ShapeCombo");
    lfo.speedCombo.setComponentID(prefix + "SpeedCombo");
    lfo.targetsCombo.setComponentID(prefix + "TargetsCombo");
    lfo.triggerCombo.setComponentID(prefix + "TrigCombo");
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
    for (size_t i = 0; i < kLfos.size(); ++i) updateLfo(lfos[i], kLfos[i]);

    safeSetCombo(arpModeCombo, preset.steppedParams[spArpMode] + 1);
    safeSetCombo(arpOctaveCombo, preset.steppedParams[spArpOctaves] + 1);
    safeSetCombo(arpRateCombo, preset.steppedParams[spArpRate] + 1);
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
    if (d.hasStartDelay) {
        safeSetKnob(lfo.delayKnob.get(), pot(cpModDelay));
        // As in the firmware the start delay acts on the LFO the modwheel does
        // not control (modwheel on LFO 1 -> delay on LFO 2).
        if (lfo.delayLabel)
            lfo.delayLabel->setText(preset.steppedParams[spModwheelTarget] == 0 ? "DELAY LFO 2" : "DELAY LFO 1",
                                    juce::dontSendNotification);
    }
    safeSetCombo(lfo.shapeCombo, preset.steppedParams[d.shape] + 1);
    safeSetCombo(lfo.speedCombo, preset.steppedParams[d.speedRange] + 1);
    if (lfo.speedKnob) lfo.speedKnob->updateText();   // the range changes the frequency text
    safeSetCombo(lfo.targetsCombo, preset.steppedParams[d.targets] + 1);
    safeSetCombo(lfo.triggerCombo, preset.steppedParams[d.trigger] + 1);
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
    int cardTopH = 315;

    ModernSectionCard* lfoCards[2] = { &lfo1Card, &lfo2Card };
    for (size_t i = 0; i < kLfos.size(); ++i) {
        auto& card = *lfoCards[i];
        card.setBounds(colX[i], 0, colW, cardTopH);
        card.clearDividers();
        card.addDivider(114, "SPEED & AMOUNT");
        card.addDivider(218, "MODULATION MATRIX ROUTING");
        layoutLfo(lfos[i], kLfos[i], colX[i], colW);
    }

    arpCard.setBounds(colX[2], 0, colW3, cardTopH);
    arpCard.clearDividers();
    arpCard.addDivider(114, "GATE, GROOVE & TEMPO");
    arpCard.addDivider(218, "LATCH & TEMPO SYNC");
    layoutArp(colX[2], colW3);

    // Real-time visualizers (separate bottom cards with 5px gap)
    int previewGap = 5;
    int previewY = cardTopH + previewGap;
    int previewH = tabBounds.getHeight() - previewY;
    if (previewH > 40) {
        for (size_t i = 0; i < kLfos.size(); ++i)
            if (lfos[i].preview) lfos[i].preview->setBounds(colX[i], previewY, colW, previewH);
        if (arpVisualizer) arpVisualizer->setBounds(colX[2], previewY, colW3, previewH);
    }
}

// Combos in two rows, speed/amount/delay knobs, then the five depth knobs.
void LfoArpTab::layoutLfo(LfoSection& lfo, const LfoDescriptor& d, int x, int colW) {
    int comboW = (colW - 30) / 2;
    lfo.shapeCombo.setBounds(x + 10, 40, comboW, 25);
    lfo.speedCombo.setBounds(x + 20 + comboW, 40, comboW, 25);
    lfo.targetsCombo.setBounds(x + 10, 72, comboW, 25);
    lfo.triggerCombo.setBounds(x + 20 + comboW, 72, comboW, 25);

    const int knobSz = getStandardKnobSize();
    const int knob3W = (colW - 30) / 3;
    const int slotW = d.hasStartDelay ? knob3W : knob3W * 3 / 2;   // two knobs share the row without the delay
    layoutKnob(lfo.speedKnob.get(), lfo.speedLabel, x + 15 + (slotW - knobSz) / 2, 126, knobSz);
    layoutKnob(lfo.amountKnob.get(), lfo.amountLabel, x + 15 + slotW + (slotW - knobSz) / 2, 126, knobSz);
    if (d.hasStartDelay)
        layoutKnob(lfo.delayKnob.get(), lfo.delayLabel, x + 15 + slotW * 2 + (slotW - knobSz) / 2, 126, knobSz);

    const int knob5W = (colW - 20) / 5;
    for (int i = 0; i < kDepths; ++i)
        layoutKnob(lfo.depthKnobs[(size_t)i].get(), lfo.depthLabels[(size_t)i],
                   x + 10 + knob5W * i + (knob5W - knobSz) / 2, 238, knobSz);
}

void LfoArpTab::layoutArp(int x, int colW) {
    arpModeCombo.setBounds(x + 10, 40, colW - 20, 25);
    int halfW = (colW - 26) / 2;
    arpOctaveCombo.setBounds(x + 10, 72, halfW, 25);
    arpRateCombo.setBounds(x + 16 + halfW, 72, halfW, 25);

    int knob3W = (colW - 20) / 3;
    int knobSz = getStandardKnobSize();
    layoutKnob(arpGateKnob.get(), arpGateLabel, x + 10 + (knob3W - knobSz) / 2, 126, knobSz);
    layoutKnob(arpSwingKnob.get(), arpSwingLabel, x + 10 + knob3W + (knob3W - knobSz) / 2, 126, knobSz);
    layoutKnob(arpBpmKnob.get(), arpBpmLabel, x + 10 + knob3W * 2 + (knob3W - knobSz) / 2, 126, knobSz);

    int toggleH = 18;
    int toggleStep = 21;
    int startY = 230;
    if (arpHoldToggle) arpHoldToggle->setBounds(x + 12, startY, colW - 24, toggleH);
    if (arpSyncToggle) arpSyncToggle->setBounds(x + 12, startY + toggleStep, colW - 24, toggleH);
}
