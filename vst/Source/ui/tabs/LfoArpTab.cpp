#include "LfoArpTab.h"

LfoArpTab::LfoArpTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void LfoArpTab::setup() {
    addAndMakeVisible(lfo1Card);
    addAndMakeVisible(lfo2Card);
    lfo1Card.toBack();
    lfo2Card.toBack();

    const char* lfoShapes[] = { "Pulse / Square", "Triangle", "Random S&H", "Sine", "Noise", "Sawtooth", "Inverted Saw" };

    // LFO 1
    lfo1FreqKnob = createKnob("1Spd", 0, 999, 250, KnobMode::LfoSpeedHz);
    lfo1FreqKnob->onValueChange = [this]() { setContinuousParam(cpLFOFreq, (float)lfo1FreqKnob->getValue()); };
    addAndMakeVisible(*lfo1FreqKnob);
    lfo1FreqLabel = createLabel("SPEED / RATE", *this);

    lfo1AmtKnob = createKnob("1Amt", 0, 999, 0, KnobMode::Percent);
    lfo1AmtKnob->onValueChange = [this]() { setContinuousParam(cpLFOAmt, (float)lfo1AmtKnob->getValue()); };
    addAndMakeVisible(*lfo1AmtKnob);
    lfo1AmtLabel = createLabel("MOD DEPTH", *this);

    lfo1DelayKnob = createKnob("MDly", 0, 999, 0, KnobMode::TimeMs);
    lfo1DelayKnob->onValueChange = [this]() { setContinuousParam(cpModDelay, (float)lfo1DelayKnob->getValue()); };
    addAndMakeVisible(*lfo1DelayKnob);
    lfo1DelayLabel = createLabel("START DELAY", *this);

    for (int i = 0; i < 7; ++i) lfo1ShapeCombo.addItem(lfoShapes[i], i + 1);
    lfo1ShapeCombo.onChange = [this]() {
        int shape = lfo1ShapeCombo.getSelectedId() - 1;
        setSteppedParam(spLFOShape, (uint8_t)shape);
        if (lfo1WavePreview) lfo1WavePreview->setShape(shape);
    };
    addAndMakeVisible(lfo1ShapeCombo);

    lfo1SpeedCombo.addItem("Normal (x1)", 1);
    lfo1SpeedCombo.addItem("Fast (x2)", 2);
    lfo1SpeedCombo.addItem("High (x4)", 3);
    lfo1SpeedCombo.addItem("Ultra (x8)", 4);
    lfo1SpeedCombo.onChange = [this]() { setSteppedParam(spLFOSpeed, (uint8_t)(lfo1SpeedCombo.getSelectedId() - 1)); };
    addAndMakeVisible(lfo1SpeedCombo);

    lfo1TargetsCombo.addItem("Pitch: None", 1);
    lfo1TargetsCombo.addItem("Pitch: Osc A Only", 2);
    lfo1TargetsCombo.addItem("Pitch: Osc B Only", 3);
    lfo1TargetsCombo.addItem("Pitch: Both (A & B)", 4);
    lfo1TargetsCombo.onChange = [this]() { setSteppedParam(spLFOTargets, (uint8_t)(lfo1TargetsCombo.getSelectedId() - 1)); };
    addAndMakeVisible(lfo1TargetsCombo);

    lfo1TrigCombo.addItem("Free-Running", 1);
    lfo1TrigCombo.addItem("Key-Sync", 2);
    lfo1TrigCombo.onChange = [this]() { setSteppedParam(spLFOTrig, (uint8_t)(lfo1TrigCombo.getSelectedId() - 1)); };
    addAndMakeVisible(lfo1TrigCombo);

    // LFO 1 Depths
    lfo1PitchKnob = createKnob("1Pit", 0, 999, 0, KnobMode::Percent);
    lfo1PitchKnob->onValueChange = [this]() { setContinuousParam(cpLFOPitchAmt, (float)lfo1PitchKnob->getValue()); };
    addAndMakeVisible(*lfo1PitchKnob);
    lfo1PitchLabel = createLabel("PITCH", *this);

    lfo1WModKnob = createKnob("1Wmo", 0, 999, 0, KnobMode::Percent);
    lfo1WModKnob->onValueChange = [this]() { setContinuousParam(cpLFOWModAmt, (float)lfo1WModKnob->getValue()); };
    addAndMakeVisible(*lfo1WModKnob);
    lfo1WModLabel = createLabel("WAVEMOD", *this);

    lfo1FilKnob = createKnob("1Fil", 0, 999, 0, KnobMode::Percent);
    lfo1FilKnob->onValueChange = [this]() { setContinuousParam(cpLFOFilAmt, (float)lfo1FilKnob->getValue()); };
    addAndMakeVisible(*lfo1FilKnob);
    lfo1FilLabel = createLabel("CUTOFF", *this);

    lfo1ResKnob = createKnob("1Res", 0, 999, 0, KnobMode::Percent);
    lfo1ResKnob->onValueChange = [this]() { setContinuousParam(cpLFOResAmt, (float)lfo1ResKnob->getValue()); };
    addAndMakeVisible(*lfo1ResKnob);
    lfo1ResLabel = createLabel("RESO", *this);

    lfo1AmpKnob = createKnob("1Amp", 0, 999, 0, KnobMode::Percent);
    lfo1AmpKnob->onValueChange = [this]() { setContinuousParam(cpLFOAmpAmt, (float)lfo1AmpKnob->getValue()); };
    addAndMakeVisible(*lfo1AmpKnob);
    lfo1AmpLabel = createLabel("VOLUME", *this);

    // LFO 1 Wave Preview
    lfo1WavePreview = std::make_unique<LfoWavePreviewComponent>(engine, 1);
    addAndMakeVisible(*lfo1WavePreview);

    // LFO 2
    lfo2FreqKnob = createKnob("2Spd", 0, 999, 250, KnobMode::LfoSpeedHz);
    lfo2FreqKnob->onValueChange = [this]() { setContinuousParam(cpLFO2Freq, (float)lfo2FreqKnob->getValue()); };
    addAndMakeVisible(*lfo2FreqKnob);
    lfo2FreqLabel = createLabel("SPEED / RATE", *this);

    lfo2AmtKnob = createKnob("2Amt", 0, 999, 0, KnobMode::Percent);
    lfo2AmtKnob->onValueChange = [this]() { setContinuousParam(cpLFO2Amt, (float)lfo2AmtKnob->getValue()); };
    addAndMakeVisible(*lfo2AmtKnob);
    lfo2AmtLabel = createLabel("MOD DEPTH", *this);

    lfo2DelayKnob = createKnob("2Dly", 0, 999, 0, KnobMode::TimeMs);
    lfo2DelayKnob->onValueChange = [this]() { setContinuousParam(cpModDelay, (float)lfo2DelayKnob->getValue()); };
    addAndMakeVisible(*lfo2DelayKnob);
    lfo2DelayLabel = createLabel("START DELAY", *this);

    for (int i = 0; i < 7; ++i) lfo2ShapeCombo.addItem(lfoShapes[i], i + 1);
    lfo2ShapeCombo.onChange = [this]() {
        int shape = lfo2ShapeCombo.getSelectedId() - 1;
        setSteppedParam(spLFO2Shape, (uint8_t)shape);
        if (lfo2WavePreview) lfo2WavePreview->setShape(shape);
    };
    addAndMakeVisible(lfo2ShapeCombo);

    lfo2SpeedCombo.addItem("Normal (x1)", 1);
    lfo2SpeedCombo.addItem("Fast (x2)", 2);
    lfo2SpeedCombo.addItem("High (x4)", 3);
    lfo2SpeedCombo.addItem("Ultra (x8)", 4);
    lfo2SpeedCombo.onChange = [this]() { setSteppedParam(spLFO2Speed, (uint8_t)(lfo2SpeedCombo.getSelectedId() - 1)); };
    addAndMakeVisible(lfo2SpeedCombo);

    lfo2TargetsCombo.addItem("Pitch: None", 1);
    lfo2TargetsCombo.addItem("Pitch: Osc A Only", 2);
    lfo2TargetsCombo.addItem("Pitch: Osc B Only", 3);
    lfo2TargetsCombo.addItem("Pitch: Both (A & B)", 4);
    lfo2TargetsCombo.onChange = [this]() { setSteppedParam(spLFO2Targets, (uint8_t)(lfo2TargetsCombo.getSelectedId() - 1)); };
    addAndMakeVisible(lfo2TargetsCombo);

    lfo2TrigCombo.addItem("Free-Running", 1);
    lfo2TrigCombo.addItem("Key-Sync", 2);
    lfo2TrigCombo.onChange = [this]() { setSteppedParam(spLFO2Trig, (uint8_t)(lfo2TrigCombo.getSelectedId() - 1)); };
    addAndMakeVisible(lfo2TrigCombo);

    // LFO 2 Depths
    lfo2PitchKnob = createKnob("2Pit", 0, 999, 0, KnobMode::Percent);
    lfo2PitchKnob->onValueChange = [this]() { setContinuousParam(cpLFO2PitchAmt, (float)lfo2PitchKnob->getValue()); };
    addAndMakeVisible(*lfo2PitchKnob);
    lfo2PitchLabel = createLabel("PITCH", *this);

    lfo2WModKnob = createKnob("2Wmo", 0, 999, 0, KnobMode::Percent);
    lfo2WModKnob->onValueChange = [this]() { setContinuousParam(cpLFO2WModAmt, (float)lfo2WModKnob->getValue()); };
    addAndMakeVisible(*lfo2WModKnob);
    lfo2WModLabel = createLabel("WAVEMOD", *this);

    lfo2FilKnob = createKnob("2Fil", 0, 999, 0, KnobMode::Percent);
    lfo2FilKnob->onValueChange = [this]() { setContinuousParam(cpLFO2FilAmt, (float)lfo2FilKnob->getValue()); };
    addAndMakeVisible(*lfo2FilKnob);
    lfo2FilLabel = createLabel("CUTOFF", *this);

    lfo2ResKnob = createKnob("2Res", 0, 999, 0, KnobMode::Percent);
    lfo2ResKnob->onValueChange = [this]() { setContinuousParam(cpLFO2ResAmt, (float)lfo2ResKnob->getValue()); };
    addAndMakeVisible(*lfo2ResKnob);
    lfo2ResLabel = createLabel("RESO", *this);

    lfo2AmpKnob = createKnob("2Amp", 0, 999, 0, KnobMode::Percent);
    lfo2AmpKnob->onValueChange = [this]() { setContinuousParam(cpLFO2AmpAmt, (float)lfo2AmpKnob->getValue()); };
    addAndMakeVisible(*lfo2AmpKnob);
    lfo2AmpLabel = createLabel("VOLUME", *this);

    // LFO 2 Wave Preview
    lfo2WavePreview = std::make_unique<LfoWavePreviewComponent>(engine, 2);
    addAndMakeVisible(*lfo2WavePreview);

    addAndMakeVisible(arpCard);
    arpCard.toBack();

    arpModeCombo.addItem("Off (Disabled)", 1);
    arpModeCombo.addItem("Up", 2);
    arpModeCombo.addItem("Down", 3);
    arpModeCombo.addItem("Up / Down", 4);
    arpModeCombo.addItem("Random", 5);
    arpModeCombo.addItem("As Played", 6);
    arpModeCombo.addItem("Chord (All Voices)", 7);
    arpModeCombo.addItem("Converge (Outside-In)", 8);
    arpModeCombo.addItem("Chord Degree (Arpligner)", 9);
    arpModeCombo.addItem("Poly Strum (Arpligner)", 10);
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

    arpVisualizer = std::make_unique<ArpVisualizerComponent>(engine);
    addAndMakeVisible(*arpVisualizer);

    assignComponentIDs();
}

void LfoArpTab::assignComponentIDs() {
    lfo1Card.setComponentID("lfo1Card");
    lfo2Card.setComponentID("lfo2Card");
    arpCard.setComponentID("arpCard");

    // Tab 5: LFO 1
    lfo1ShapeCombo.setComponentID("lfo1ShapeCombo");
    lfo1SpeedCombo.setComponentID("lfo1SpeedCombo");
    lfo1TargetsCombo.setComponentID("lfo1TargetsCombo");
    lfo1TrigCombo.setComponentID("lfo1TrigCombo");
    if (lfo1FreqKnob) lfo1FreqKnob->setComponentID("lfo1FreqKnob");
    if (lfo1FreqLabel) lfo1FreqLabel->setComponentID("lfo1FreqLabel");
    if (lfo1AmtKnob) lfo1AmtKnob->setComponentID("lfo1AmtKnob");
    if (lfo1AmtLabel) lfo1AmtLabel->setComponentID("lfo1AmtLabel");
    if (lfo1DelayKnob) lfo1DelayKnob->setComponentID("lfo1DelayKnob");
    if (lfo1DelayLabel) lfo1DelayLabel->setComponentID("lfo1DelayLabel");
    if (lfo1PitchKnob) lfo1PitchKnob->setComponentID("lfo1PitchKnob");
    if (lfo1PitchLabel) lfo1PitchLabel->setComponentID("lfo1PitchLabel");
    if (lfo1WModKnob) lfo1WModKnob->setComponentID("lfo1WModKnob");
    if (lfo1WModLabel) lfo1WModLabel->setComponentID("lfo1WModLabel");
    if (lfo1FilKnob) lfo1FilKnob->setComponentID("lfo1FilKnob");
    if (lfo1FilLabel) lfo1FilLabel->setComponentID("lfo1FilLabel");
    if (lfo1ResKnob) lfo1ResKnob->setComponentID("lfo1ResKnob");
    if (lfo1ResLabel) lfo1ResLabel->setComponentID("lfo1ResLabel");
    if (lfo1AmpKnob) lfo1AmpKnob->setComponentID("lfo1AmpKnob");
    if (lfo1AmpLabel) lfo1AmpLabel->setComponentID("lfo1AmpLabel");
    if (lfo1WavePreview) lfo1WavePreview->setComponentID("lfo1WavePreview");

    // Tab 5: LFO 2
    lfo2ShapeCombo.setComponentID("lfo2ShapeCombo");
    lfo2SpeedCombo.setComponentID("lfo2SpeedCombo");
    lfo2TargetsCombo.setComponentID("lfo2TargetsCombo");
    lfo2TrigCombo.setComponentID("lfo2TrigCombo");
    if (lfo2FreqKnob) lfo2FreqKnob->setComponentID("lfo2FreqKnob");
    if (lfo2FreqLabel) lfo2FreqLabel->setComponentID("lfo2FreqLabel");
    if (lfo2AmtKnob) lfo2AmtKnob->setComponentID("lfo2AmtKnob");
    if (lfo2AmtLabel) lfo2AmtLabel->setComponentID("lfo2AmtLabel");
    if (lfo2DelayKnob) lfo2DelayKnob->setComponentID("lfo2DelayKnob");
    if (lfo2DelayLabel) lfo2DelayLabel->setComponentID("lfo2DelayLabel");
    if (lfo2PitchKnob) lfo2PitchKnob->setComponentID("lfo2PitchKnob");
    if (lfo2PitchLabel) lfo2PitchLabel->setComponentID("lfo2PitchLabel");
    if (lfo2WModKnob) lfo2WModKnob->setComponentID("lfo2WModKnob");
    if (lfo2WModLabel) lfo2WModLabel->setComponentID("lfo2WModLabel");
    if (lfo2FilKnob) lfo2FilKnob->setComponentID("lfo2FilKnob");
    if (lfo2FilLabel) lfo2FilLabel->setComponentID("lfo2FilLabel");
    if (lfo2ResKnob) lfo2ResKnob->setComponentID("lfo2ResKnob");
    if (lfo2ResLabel) lfo2ResLabel->setComponentID("lfo2ResLabel");
    if (lfo2AmpKnob) lfo2AmpKnob->setComponentID("lfo2AmpKnob");
    if (lfo2AmpLabel) lfo2AmpLabel->setComponentID("lfo2AmpLabel");
    if (lfo2WavePreview) lfo2WavePreview->setComponentID("lfo2WavePreview");

    // Tab 5: Arp
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

void LfoArpTab::advancePreviewAnimation() {
    float lfo1Speed = 0.05f * std::pow(1000.0f, (float)lfo1FreqKnob->getValue() / 999.0f);
    float lfo2Speed = 0.05f * std::pow(1000.0f, (float)lfo2FreqKnob->getValue() / 999.0f);
    lfo1Phase += lfo1Speed * 0.033f;
    lfo2Phase += lfo2Speed * 0.033f;
    if (lfo1Phase > 1.0f) lfo1Phase -= 1.0f;
    if (lfo2Phase > 1.0f) lfo2Phase -= 1.0f;

    if (lfo1WavePreview) lfo1WavePreview->setPhase(lfo1Phase);
    if (lfo2WavePreview) lfo2WavePreview->setPhase(lfo2Phase);
}

void LfoArpTab::updateFromEngine() {
    const auto& preset = engine.getCurrentPreset();

    safeSetKnob(lfo1FreqKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFOFreq]));
    safeSetKnob(lfo1AmtKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFOAmt]));
    safeSetKnob(lfo1DelayKnob.get(), scan_potFrom16bits(preset.continuousParams[cpModDelay]));
    safeSetCombo(lfo1ShapeCombo, preset.steppedParams[spLFOShape] + 1);
    safeSetCombo(lfo1SpeedCombo, preset.steppedParams[spLFOSpeed] + 1);
    safeSetCombo(lfo1TargetsCombo, preset.steppedParams[spLFOTargets] + 1);
    safeSetCombo(lfo1TrigCombo, preset.steppedParams[spLFOTrig] + 1);
    safeSetKnob(lfo1PitchKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFOPitchAmt]));
    safeSetKnob(lfo1WModKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFOWModAmt]));
    safeSetKnob(lfo1FilKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFOFilAmt]));
    safeSetKnob(lfo1ResKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFOResAmt]));
    safeSetKnob(lfo1AmpKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFOAmpAmt]));

    safeSetKnob(lfo2FreqKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFO2Freq]));
    safeSetKnob(lfo2AmtKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFO2Amt]));
    safeSetKnob(lfo2DelayKnob.get(), scan_potFrom16bits(preset.continuousParams[cpModDelay]));
    safeSetCombo(lfo2ShapeCombo, preset.steppedParams[spLFO2Shape] + 1);
    safeSetCombo(lfo2SpeedCombo, preset.steppedParams[spLFO2Speed] + 1);
    safeSetCombo(lfo2TargetsCombo, preset.steppedParams[spLFO2Targets] + 1);
    safeSetCombo(lfo2TrigCombo, preset.steppedParams[spLFO2Trig] + 1);
    safeSetKnob(lfo2PitchKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFO2PitchAmt]));
    safeSetKnob(lfo2WModKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFO2WModAmt]));
    safeSetKnob(lfo2FilKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFO2FilAmt]));
    safeSetKnob(lfo2ResKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFO2ResAmt]));
    safeSetKnob(lfo2AmpKnob.get(), scan_potFrom16bits(preset.continuousParams[cpLFO2AmpAmt]));

    safeSetCombo(arpModeCombo, preset.steppedParams[spArpMode] + 1);
    safeSetCombo(arpOctaveCombo, preset.steppedParams[spArpOctaves] + 1);
    safeSetCombo(arpRateCombo, preset.steppedParams[spArpRate] + 1);
    safeSetToggle(arpHoldToggle.get(), preset.steppedParams[spArpHold] != 0);
    safeSetToggle(arpSyncToggle.get(), preset.steppedParams[spArpSync] != 0);
    safeSetKnob(arpGateKnob.get(), scan_potFrom16bits(preset.continuousParams[cpArpGate]));
    safeSetKnob(arpSwingKnob.get(), scan_potFrom16bits(preset.continuousParams[cpArpSwing]));
    safeSetKnob(arpBpmKnob.get(), scan_potFrom16bits(preset.continuousParams[cpArpBpm]));

    if (lfo1WavePreview) lfo1WavePreview->setShape(preset.steppedParams[spLFOShape]);
    if (lfo2WavePreview) lfo2WavePreview->setShape(preset.steppedParams[spLFO2Shape]);
}

void LfoArpTab::resized() {
    const auto tabBounds = getLocalBounds();

    int colGap = 5;
    int availableW = tabBounds.getWidth();
    int colW = (availableW - colGap * 2) / 3;
    int col1X = 0;
    int col2X = col1X + colW + colGap;
    int col3X = col2X + colW + colGap;
    int colW3 = availableW - col3X;
    int cardTopH = 315;

    // Column 1: LFO 1
    lfo1Card.setBounds(col1X, 0, colW, cardTopH);
    lfo1Card.clearDividers();
    lfo1Card.addDivider(114, "SPEED & AMOUNT");
    lfo1Card.addDivider(218, "MODULATION MATRIX ROUTING");

    // Column 2: LFO 2
    lfo2Card.setBounds(col2X, 0, colW, cardTopH);
    lfo2Card.clearDividers();
    lfo2Card.addDivider(114, "SPEED & AMOUNT");
    lfo2Card.addDivider(218, "MODULATION MATRIX ROUTING");

    // Column 3: ARPEGGIATOR
    arpCard.setBounds(col3X, 0, colW3, cardTopH);
    arpCard.clearDividers();
    arpCard.addDivider(38, "PLAYBACK PATTERN MODE");
    arpCard.addDivider(114, "PLAYBACK CONTROLS");
    arpCard.addDivider(218, "PLAYBACK MATRIX PREVIEW");

    int comboW = (colW - 30) / 2;

    // LFO 1 Controls
    lfo1ShapeCombo.setBounds(col1X + 10, 40, comboW, 25);
    lfo1SpeedCombo.setBounds(col1X + 20 + comboW, 40, comboW, 25);
    lfo1TargetsCombo.setBounds(col1X + 10, 72, comboW, 25);
    lfo1TrigCombo.setBounds(col1X + 20 + comboW, 72, comboW, 25);

    int knob3W = (colW - 30) / 3;
    int knobSzLfo = getStandardKnobSize();
    layoutKnob(lfo1FreqKnob.get(), lfo1FreqLabel, col1X + 15 + (knob3W - knobSzLfo) / 2, 126, knobSzLfo);
    layoutKnob(lfo1AmtKnob.get(), lfo1AmtLabel, col1X + 15 + knob3W + (knob3W - knobSzLfo) / 2, 126, knobSzLfo);
    layoutKnob(lfo1DelayKnob.get(), lfo1DelayLabel, col1X + 15 + knob3W * 2 + (knob3W - knobSzLfo) / 2, 126, knobSzLfo);

    int knob5W = (colW - 20) / 5;
    int knobSz5 = getStandardKnobSize();
    layoutKnob(lfo1PitchKnob.get(), lfo1PitchLabel, col1X + 10 + (knob5W - knobSz5) / 2, 238, knobSz5);
    layoutKnob(lfo1WModKnob.get(), lfo1WModLabel, col1X + 10 + knob5W + (knob5W - knobSz5) / 2, 238, knobSz5);
    layoutKnob(lfo1FilKnob.get(), lfo1FilLabel, col1X + 10 + knob5W * 2 + (knob5W - knobSz5) / 2, 238, knobSz5);
    layoutKnob(lfo1ResKnob.get(), lfo1ResLabel, col1X + 10 + knob5W * 3 + (knob5W - knobSz5) / 2, 238, knobSz5);
    layoutKnob(lfo1AmpKnob.get(), lfo1AmpLabel, col1X + 10 + knob5W * 4 + (knob5W - knobSz5) / 2, 238, knobSz5);

    // LFO 2 Controls
    lfo2ShapeCombo.setBounds(col2X + 10, 40, comboW, 25);
    lfo2SpeedCombo.setBounds(col2X + 20 + comboW, 40, comboW, 25);
    lfo2TargetsCombo.setBounds(col2X + 10, 72, comboW, 25);
    lfo2TrigCombo.setBounds(col2X + 20 + comboW, 72, comboW, 25);

    layoutKnob(lfo2FreqKnob.get(), lfo2FreqLabel, col2X + 15 + (knob3W - knobSzLfo) / 2, 126, knobSzLfo);
    layoutKnob(lfo2AmtKnob.get(), lfo2AmtLabel, col2X + 15 + knob3W + (knob3W - knobSzLfo) / 2, 126, knobSzLfo);
    layoutKnob(lfo2DelayKnob.get(), lfo2DelayLabel, col2X + 15 + knob3W * 2 + (knob3W - knobSzLfo) / 2, 126, knobSzLfo);

    layoutKnob(lfo2PitchKnob.get(), lfo2PitchLabel, col2X + 10 + (knob5W - knobSz5) / 2, 238, knobSz5);
    layoutKnob(lfo2WModKnob.get(), lfo2WModLabel, col2X + 10 + knob5W + (knob5W - knobSz5) / 2, 238, knobSz5);
    layoutKnob(lfo2FilKnob.get(), lfo2FilLabel, col2X + 10 + knob5W * 2 + (knob5W - knobSz5) / 2, 238, knobSz5);
    layoutKnob(lfo2ResKnob.get(), lfo2ResLabel, col2X + 10 + knob5W * 3 + (knob5W - knobSz5) / 2, 238, knobSz5);
    layoutKnob(lfo2AmpKnob.get(), lfo2AmpLabel, col2X + 10 + knob5W * 4 + (knob5W - knobSz5) / 2, 238, knobSz5);

    // Column 3: ARPEGGIATOR
    arpCard.setBounds(col3X, 0, colW3, cardTopH);
    arpCard.clearDividers();
    arpCard.addDivider(114, "GATE, GROOVE & TEMPO");
    arpCard.addDivider(218, "LATCH & TEMPO SYNC");

    arpModeCombo.setBounds(col3X + 10, 40, colW3 - 20, 25);
    int halfW = (colW3 - 26) / 2;
    arpOctaveCombo.setBounds(col3X + 10, 72, halfW, 25);
    arpRateCombo.setBounds(col3X + 16 + halfW, 72, halfW, 25);

    int knob3WArp = (colW3 - 20) / 3;
    int knobSzArp = getStandardKnobSize();
    layoutKnob(arpGateKnob.get(), arpGateLabel, col3X + 10 + (knob3WArp - knobSzArp) / 2, 126, knobSzArp);
    layoutKnob(arpSwingKnob.get(), arpSwingLabel, col3X + 10 + knob3WArp + (knob3WArp - knobSzArp) / 2, 126, knobSzArp);
    layoutKnob(arpBpmKnob.get(), arpBpmLabel, col3X + 10 + knob3WArp * 2 + (knob3WArp - knobSzArp) / 2, 126, knobSzArp);

    int arpToggleH = 18;
    int arpToggleStep = 21;
    int arpStartY = 230;
    if (arpHoldToggle) arpHoldToggle->setBounds(col3X + 12, arpStartY, colW3 - 24, arpToggleH);
    if (arpSyncToggle) arpSyncToggle->setBounds(col3X + 12, arpStartY + arpToggleStep, colW3 - 24, arpToggleH);

    // Real-time Visualizers (separate bottom cards with 5px gap)
    int previewGap = 5;
    int previewY = cardTopH + previewGap;
    int previewH = tabBounds.getHeight() - previewY;
    if (previewH > 40) {
        if (lfo1WavePreview) lfo1WavePreview->setBounds(col1X, previewY, colW, previewH);
        if (lfo2WavePreview) lfo2WavePreview->setBounds(col2X, previewY, colW, previewH);
        if (arpVisualizer) arpVisualizer->setBounds(col3X, previewY, colW3, previewH);
    }
}
