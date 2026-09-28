#include "SynthEngine.h"
#include <cmath>
#include "FilterCalibration.h"

void SynthEngine::capturePreparedState(PreparedState& state) const {
    for (int part = 0; part < 16; ++part) {
        const auto& slot = afxKit.getSlot(part);
        auto& target = state.parts[part];
        std::copy_n(slot.preset.continuousParams, cpCount, target.continuous);
        std::copy_n(slot.preset.steppedParams, spCount, target.stepped);
        std::copy_n(slot.preset.modMatrix, MOD_MATRIX_SLOT_COUNT, target.matrix);
        std::copy_n(slot.preset.voicePattern, SYNTH_VOICE_COUNT, target.pattern);
        for (int wave = 0; wave < abxCount; ++wave)
            std::copy_n(slot.waveManager.getWaveData(static_cast<abx_t>(wave)), WTOSC_SAMPLE_COUNT, target.waves[wave]);
        target.route = partRoutes[part];
    }
    std::copy_n(voiceFader, SYNTH_VOICE_COUNT, state.faders);
    std::copy_n(voicePan, SYNTH_VOICE_COUNT, state.pans);
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) state.panCustomized[v] = voicePanCustomized[v] ? 1 : 0;
    for (int n = 0; n < 128; ++n) state.noteMap[n] = afxKit.getSlotForNote(static_cast<uint8_t>(n));
    for (int s = 0; s < 16; ++s) {
        state.arpPattern[s] = arpeggiator.getStepPattern(s);
        state.arpDegrees[s] = arpeggiator.getStepDegree(s);
    }
    state.transpose = arpeggiator.getTranspose();
    state.customRouting = customRouting;
    state.calibratedGain = calibratedGain;
    state.panicGeneration = panicGeneration;
}

void SynthEngine::applyPreparedState(const PreparedState& state, bool preserveMainParameters) {
    // A preset load is prepared on the message thread and reaches the audio
    // engine through panicGeneration.  Retire voices before replacing wave
    // memory or filter/envelope settings, otherwise release tails continue
    // with unrelated preset data and can produce clicks or bursts of noise.
    if (panicGeneration != state.panicGeneration) {
        const float transitionLeft = lastOutputLeft;
        const float transitionRight = lastOutputRight;
        reset();
        panicGeneration = state.panicGeneration;
        presetTransitionLeft = transitionLeft;
        presetTransitionRight = transitionRight;
        presetTransitionSamples = std::max(1, static_cast<int>(sampleRate * 0.003f));
        presetTransitionRemaining = presetTransitionSamples;
    }

    std::array<bool, 16> controlsChanged{};
    for (int part = 0; part < 16; ++part) {
        auto& slot = afxKit.getSlot(part);
        const auto& source = state.parts[part];
        if (part != 0 || !preserveMainParameters) {
            controlsChanged[part] = std::memcmp(slot.preset.continuousParams, source.continuous, sizeof(source.continuous)) != 0
                || std::memcmp(slot.preset.steppedParams, source.stepped, sizeof(source.stepped)) != 0;
            std::copy_n(source.continuous, cpCount, slot.preset.continuousParams);
            std::copy_n(source.stepped, spCount, slot.preset.steppedParams);
        }
        controlsChanged[part] |= std::memcmp(slot.preset.voicePattern, source.pattern, sizeof(source.pattern)) != 0;
        std::copy_n(source.matrix, MOD_MATRIX_SLOT_COUNT, slot.preset.modMatrix);
        std::copy_n(source.pattern, SYNTH_VOICE_COUNT, slot.preset.voicePattern);
        for (int wave = 0; wave < abxCount; ++wave) {
            auto* target = slot.waveManager.getMutableWaveData(static_cast<abx_t>(wave));
            if (std::memcmp(source.waves[wave], target, sizeof(source.waves[wave])) != 0)
                std::copy_n(source.waves[wave], WTOSC_SAMPLE_COUNT, target);
        }
        partRoutes[part] = source.route;
    }
    for (int n = 0; n < 128; ++n) afxKit.setNoteMapping(static_cast<uint8_t>(n), state.noteMap[n]);
    std::copy_n(state.faders, SYNTH_VOICE_COUNT, voiceFader);
    std::copy_n(state.pans, SYNTH_VOICE_COUNT, voicePan);
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) voicePanCustomized[v] = state.panCustomized[v] != 0;
    customRouting = state.customRouting;
    setCalibratedGain(state.calibratedGain);
    if (controlsChanged[0]) applyControls();
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
        if (voiceSlot[v] >= 0 && (controlsChanged[0] || controlsChanged[voiceSlot[v]]))
            configureVoicePart(v, static_cast<uint8_t>(voiceSlot[v]), voiceExpr[v].noteOnVelocity);
    for (int s = 0; s < 16; ++s) {
        arpeggiator.setStepPattern(s, state.arpPattern[s]);
        arpeggiator.setStepDegree(s, state.arpDegrees[s]);
    }
    arpeggiator.setTranspose(state.transpose);
}

SynthEngine::SynthEngine() : waveManager(afxKit.getSlot(0).waveManager), currentPreset(afxKit.getSlot(0).preset) {
    sampleRate = 48000.0f;
    tickStep = (uint32_t)(SYNTH_MASTER_CLOCK / sampleRate);
    currentTick = 0;
    cvSubSampleCounter = 0.0f;
    tickSubSampleCounter = 0.0f;
    arpGateCloseTick = UINT32_MAX;

    benderAmount = 0;
    modwheelAmount = 0;
    pressureAmount = 0;
    timbreAmount = 0;
    glideAmount = 0;
    gliding = 0;

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].init(v);
        voiceSlot[v] = -1;
        oscANoteCV[v] = oscBNoteCV[v] = filterNoteCV[v] = 0;
        oscATargetCV[v] = oscBTargetCV[v] = filterTargetCV[v] = 0;
    }

    lfo[0].init();
    lfo[1].init();

    assigner.init();
    assigner.setCallback([this](uint8_t note, int8_t gate, int8_t voice, uint16_t velocity, uint8_t flags) {
        assignerEvent(note, gate, voice, velocity, flags);
    });

    arpeggiator.init();
    arpeggiator.setNoteAssignCallback([this](uint8_t note, int8_t gate, uint16_t velocity, uint8_t channel) {
        if (gate && customRouting) {
            for (int part = 0; part < 16; ++part) {
                const auto& route = partRoutes[part];
                const auto routingChannel = isMpeMemberChannel(channel) ? 1 : channel;
                if (route.enabled && (route.channel == 0 || route.channel == routingChannel)
                    && note >= route.low && note <= route.high) {
                    pendingPart = part;
                    assigner.assignNote(note, gate, velocity, 0, currentTick, channel, static_cast<uint8_t>(part));
                }
            }
            pendingPart = -1;
        } else assigner.assignNote(note, gate, velocity, 0, currentTick, channel);

        // Queue MIDI Output for DAW live capture (e.g., in Ableton Live)
        uint8_t vel7 = (uint8_t)(((uint32_t)velocity * 127U) / 65535U);
        if (gate && vel7 == 0) vel7 = 1;
        if (!pendingMidiOut.push_back({ note, vel7, channel, gate != 0, currentSampleOffset })) midiOverflow = true;
    });

    currentPreset.setDefaults();
    for (int part = 0; part < 16; ++part) partRoutes[part].channel = static_cast<uint8_t>(part + 1);
    setCalibratedGain(true);
}

void SynthEngine::prepare(float sr) {
    sampleRate = std::max(22050.0f, sr);
    tickStep = (uint32_t)(SYNTH_MASTER_CLOCK / sampleRate);
    const auto filterGains = calibrateFilters(sampleRate);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].setSampleRate(sampleRate);
        voices[v].filterGains = filterGains;
    }
    mackity.setSampleRate(sampleRate);
    consoleX.setSampleRate(sampleRate);

    applyPreset();
}

void SynthEngine::pullPendingMidiOut(std::vector<MidiOutEvent>& outEvents) {
    outEvents.assign(pendingMidiOut.begin(), pendingMidiOut.end());
    pendingMidiOut.clear();
}

static inline int getMPEBendSemitones(uint8_t mpeBendParam) {
    switch (mpeBendParam) {
    case 0: return 2;
    case 1: return 12;
    case 2: return 24;
    case 3: return 48;
    case 4: return 96;
    default: return 24;
    }
}

bool SynthEngine::isMpeMemberChannel(uint8_t channel) const {
    const uint8_t mode = currentPreset.steppedParams[spMPEMode];
    if (mode == 1) return channel >= 2 && channel <= 7;
    if (mode == 2) return channel >= 2 && channel <= 15;
    return false;
}

void SynthEngine::setHostTransport(double ppqPosition, bool playing) {
    if (!std::isfinite(ppqPosition)) return;
    const bool stoppedNow = hostSyncEnabled && !playing
        && (!hostTransportAvailable || hostTransportPlaying);
    hostTransportAvailable = true;
    hostTransportPlaying = playing;
    if (!hostSyncEnabled) return;

    if (stoppedNow) {
        arpeggiator.finishPreviousNote();
        arpGateCloseTick = UINT32_MAX;
    }

    const double tickPosition = std::max(0.0, ppqPosition) * 48.0;
    const double wholeTicks = std::floor(tickPosition);
    currentTick = static_cast<uint32_t>(wholeTicks);
    tickSubSampleCounter = static_cast<float>(tickPosition - wholeTicks);
}

void SynthEngine::reset() {
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].reset();
        voiceExpr[v].reset();
    }
    mackity.reset();
    consoleX.reset();
    assigner.panicOff();
    arpeggiator.init();
    arpGateCloseTick = UINT32_MAX;
    benderAmount = 0;
    modwheelAmount = 0;
    pressureAmount = 0;
    timbreAmount = 0;
    lastOutputLeft = lastOutputRight = 0.0f;
    presetTransitionLeft = presetTransitionRight = 0.0f;
    presetTransitionSamples = presetTransitionRemaining = 0;
    beginVoiceMeterBlock();
}

void SynthEngine::noteOn(uint8_t note, uint16_t velocity, uint8_t channel) {
    if (arpeggiator.getMode() != amOff) {
        arpeggiator.assignNote(note, 1, velocity, channel);
    } else {
        if (customRouting) {
            const auto routingChannel = isMpeMemberChannel(channel) ? 1 : channel;
            for (int part = 0; part < 16; ++part) {
                const auto& route = partRoutes[part];
                if (route.enabled && (route.channel == 0 || route.channel == routingChannel)
                    && note >= route.low && note <= route.high) {
                    pendingPart = part;
                    assigner.assignNote(note, 1, velocity, 1, currentTick, channel, static_cast<uint8_t>(part));
                }
            }
            pendingPart = -1;
        } else assigner.assignNote(note, 1, velocity, 1, currentTick, channel);
    }
}

void SynthEngine::noteOff(uint8_t note, uint16_t velocity, uint8_t channel) {
    if (arpeggiator.getMode() != amOff) {
        arpeggiator.assignNote(note, 0, velocity, channel);
    } else {
        // Record release velocity (lift) on matching voice
        for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v)
            if (assigner.voiceMatches(v, note, channel)) voiceExpr[v].noteOffVelocity = velocity;
        assigner.assignNote(note, 0, velocity, 1, currentTick, channel);
    }
}

void SynthEngine::pitchBend(int16_t bend, uint8_t channel) {
    if (isMpeMemberChannel(channel)) {
        // MPE Per-Member-Channel Pitch Bend
        int mpeSemitones = getMPEBendSemitones(currentPreset.steppedParams[spMPEPitchBendRange]);
        int16_t bendCv = (int16_t)(((int32_t)bend * mpeSemitones * WTOSC_CV_SEMITONE) / 8192);

        for (int vi = 0; vi < SYNTH_VOICE_COUNT; ++vi) {
            if (voiceExpr[vi].midiChannel == channel) {
                voiceExpr[vi].pitchBendOffset = bendCv;
                voiceExpr[vi].hasPerVoiceBend = true;
            }
        }
    } else {
        // Standard Global / Master Channel Pitch Bend
        static constexpr int bendRanges[] = {3, 5, 12};
        const int rangeSemitones = bendRanges[std::min<uint8_t>(currentPreset.steppedParams[spBenderRange], 2)];
        benderAmount = (int16_t)(((int32_t)bend * rangeSemitones * WTOSC_CV_SEMITONE) / 8192);
    }
}

void SynthEngine::modWheel(uint16_t mod, uint8_t channel) {
    modwheelAmount = mod;
}

void SynthEngine::channelPressure(uint16_t press, uint8_t channel) {
    if (isMpeMemberChannel(channel)) {
        for (auto& expression : voiceExpr) if (expression.midiChannel == channel) {
            expression.pressure = press;
            expression.hasPerVoicePressure = true;
        }
    } else {
        pressureAmount = press;
    }
}

void SynthEngine::polyAftertouch(uint8_t note, uint16_t press, uint8_t channel) {
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!assigner.voiceMatches(v, note, channel)) continue;
        voiceExpr[v].pressure = press;
        voiceExpr[v].hasPerVoicePressure = true;
    }
}

void SynthEngine::timbreSlide(uint16_t timbre, uint8_t channel) {
    if (isMpeMemberChannel(channel)) {
        for (auto& expression : voiceExpr) if (expression.midiChannel == channel) {
            expression.timbre = timbre;
            expression.hasPerVoiceTimbre = true;
        }
    } else {
        timbreAmount = timbre;
    }
}

void SynthEngine::breathController(uint16_t breath, uint8_t channel) {
    breathAmount = breath;
}

void SynthEngine::expressionController(uint16_t expr, uint8_t channel) {
    expressionAmount = expr;
}

void SynthEngine::controlChange(uint8_t ccNumber, uint8_t value, uint8_t channel) {
    uint16_t val16 = (uint16_t)((uint32_t)value * 65535U / 127U);
    switch (ccNumber) {
    case 1:
        modWheel(val16, channel);
        break;
    case 2:
        breathController(val16, channel);
        break;
    case 11:
        expressionController(val16, channel);
        break;
    case 64:
        holdPedal(value >= 64);
        break;
    case 74:
        timbreSlide(val16, channel);
        break;
    case 120:
    case 123:
        allNotesOff();
        break;
    default:
        break;
    }
}

void SynthEngine::setMatrixSlot(int slotIndex, modSource_t src, modDest_t dest, modSource_t via, int16_t depth, bool enabled) {
    if (slotIndex >= 0 && slotIndex < MOD_MATRIX_SLOT_COUNT) {
        currentPreset.modMatrix[slotIndex].source = (uint8_t)src;
        currentPreset.modMatrix[slotIndex].dest = (uint8_t)dest;
        currentPreset.modMatrix[slotIndex].viaSource = (uint8_t)via;
        currentPreset.modMatrix[slotIndex].depth = (int16_t)std::clamp((int)depth, -100, 100);
        currentPreset.modMatrix[slotIndex].enabled = enabled;
    }
}

float SynthEngine::evaluateModSource(int8_t v, uint8_t src) const {
    if (v < 0 || v >= SYNTH_VOICE_COUNT) return 0.0f;

    switch (src) {
    case modSrcModWheel:
        return (float)modwheelAmount / 65535.0f;
    case modSrcPitchBend:
        if (voiceExpr[v].hasPerVoiceBend) {
            return std::clamp(voiceExpr[v].smoothedBend / (float)(12 * WTOSC_CV_SEMITONE), -1.0f, 1.0f);
        }
        return std::clamp((float)benderAmount / (float)(12 * WTOSC_CV_SEMITONE), -1.0f, 1.0f);
    case modSrcAftertouch:
        if (voiceExpr[v].hasPerVoicePressure) {
            return std::clamp(voiceExpr[v].smoothedPressure / 65535.0f, 0.0f, 1.0f);
        }
        return (float)pressureAmount / 65535.0f;
    case modSrcTimbreSlide:
        if (voiceExpr[v].hasPerVoiceTimbre) {
            return std::clamp(voiceExpr[v].smoothedTimbre / 65535.0f, 0.0f, 1.0f);
        }
        return (float)timbreAmount / 65535.0f;
    case modSrcVelocity:
        return (float)voiceExpr[v].noteOnVelocity / 65535.0f;
    case modSrcReleaseVelocity:
        return (float)voiceExpr[v].noteOffVelocity / 65535.0f;
    case modSrcKeyTrack:
        if (voiceExpr[v].noteNumber != ASSIGNER_NO_NOTE) {
            return std::clamp((float)((int)voiceExpr[v].noteNumber - MIDDLE_C_NOTE) / 60.0f, -1.0f, 1.0f);
        }
        return 0.0f;
    case modSrcBreath:
        return (float)breathAmount / 65535.0f;
    case modSrcExpression:
        return (float)expressionAmount / 65535.0f;
    case modSrcFilterEnv:
        return (float)voices[v].getFilEnv().getOutput() / 65535.0f;
    case modSrcAmpEnv:
        return (float)voices[v].getAmpEnv().getOutput() / 65535.0f;
    case modSrcWaveModEnv:
        return (float)voices[v].getWmodEnv().getOutput() / 65535.0f;
    case modSrcLFO1:
        return (float)lfo[0].getOutput() / 32768.0f;
    case modSrcLFO1_Uni:
        return (float)lfo[0].getLevelCV() / 65535.0f;
    case modSrcLFO2:
        return (float)lfo[1].getOutput() / 32768.0f;
    case modSrcLFO2_Uni:
        return (float)lfo[1].getLevelCV() / 65535.0f;
    case modSrcConstant:
        return 1.0f;
    default:
        return 0.0f;
    }
}

void SynthEngine::holdPedal(bool down) {
    assigner.holdEvent(down ? 1 : 0);
}

void SynthEngine::refreshOscWaves() {
    const uint16_t* waveAMain = waveManager.getWaveData(abxAMain);
    const uint16_t* waveAXOvr = waveManager.getWaveData(abxACrossover);
    const uint16_t* waveBMain = waveManager.getWaveData(abxBMain);
    const uint16_t* waveBXOvr = waveManager.getWaveData(abxBCrossover);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].setOscSampleData(waveAMain, waveAXOvr, waveBMain, waveBXOvr);
    }
}

void SynthEngine::allNotesOff() {
    arpeggiator.allNotesOff();
    assigner.allNotesOff();
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voiceExpr[v].reset();
    }
    pressureAmount = 0;
    benderAmount = 0;
}

void SynthEngine::panic() {
    ++panicGeneration;
    reset();
}

void SynthEngine::assignerEvent(uint8_t note, int8_t gate, int8_t voice, uint16_t velocity, uint8_t flags) {
    if (voice < 0 || voice >= SYNTH_VOICE_COUNT) return;

    if (gate) {
        voiceExpr[voice].noteNumber = note;
        voiceExpr[voice].midiChannel = assigner.getVoiceChannel(voice);
        voiceExpr[voice].noteOnVelocity = velocity;

        // Set voice pitch targets
        uint16_t baseCutoffRaw = currentPreset.continuousParams[cpCutoff];
        uint16_t baseAPitch = currentPreset.continuousParams[cpAFreq] >> 2;
        uint16_t baseBPitch = currentPreset.continuousParams[cpBFreq] >> 2;
        uint16_t trackRaw = currentPreset.continuousParams[cpFilKbdAmt];

        uint16_t cva = (note * WTOSC_CV_SEMITONE) + baseAPitch;
        uint16_t cvb = (note * WTOSC_CV_SEMITONE) + baseBPitch;

        int32_t trackOffset = (((int8_t)note - MIDDLE_C_NOTE) * (trackRaw >> 8)) >> 8;
        uint16_t cvf = (uint16_t)__USAT((int32_t)baseCutoffRaw + (trackOffset * WTOSC_CV_SEMITONE), 16);

        if (gliding) {
            if (oscANoteCV[voice] == 0) {
                oscANoteCV[voice] = cva;
                oscBNoteCV[voice] = cvb;
                filterNoteCV[voice] = cvf;
            }
            oscATargetCV[voice] = cva;
            oscBTargetCV[voice] = cvb;
            filterTargetCV[voice] = cvf;
        } else {
            oscANoteCV[voice] = cva;
            oscBNoteCV[voice] = cvb;
            filterNoteCV[voice] = cvf;
            filterTargetCV[voice] = cvf;
        }

        // Velocity sensitivity (16-bit high-resolution scaling)
        uint16_t velAmt = currentPreset.continuousParams[cpWModVelocity];
        voices[voice].getWmodEnv().setCVs(0, 0, 0, 0, (UINT16_MAX - velAmt) + scaleU16U16(velocity, velAmt), 0x10);

        velAmt = currentPreset.continuousParams[cpFilVelocity];
        voices[voice].getFilEnv().setCVs(0, 0, 0, 0, (UINT16_MAX - velAmt) + scaleU16U16(velocity, velAmt), 0x10);

        velAmt = currentPreset.continuousParams[cpAmpVelocity];
        voices[voice].getAmpEnv().setCVs(0, 0, 0, 0, (UINT16_MAX - velAmt) + scaleU16U16(velocity, velAmt), 0x10);

        // All modes are routing presets over the same sixteen-part engine.
        {
            auto mode = static_cast<engineMode_t>(currentPreset.steppedParams[spEngineMode]);
            uint8_t channel = voiceExpr[voice].midiChannel;
            uint8_t slotIdx = pendingPart >= 0 ? static_cast<uint8_t>(pendingPart)
                : mode == emAFX ? afxKit.getSlotForNote(note)
                : mode == emSingle || currentPreset.steppedParams[spMPEMode] != 0 ? 0
                : static_cast<uint8_t>(std::clamp<int>(channel, 1, 16) - 1);
            voiceSlot[voice] = slotIdx;
            configureVoicePart(voice, slotIdx, velocity);
        }

        voices[voice].gateOn(note, velocity, flags);
        updateSingleVoice(voice, false);

        // LFO retrigger
        if (currentPreset.steppedParams[spLFOTrig]) lfo[0].reset();
        if (currentPreset.steppedParams[spLFO2Trig]) lfo[1].reset();
    } else {
        // Apply optional release velocity scaling (lift dynamic)
        const PresetData& relPreset = (voiceSlot[voice] >= 0)
            ? afxKit.getSlot(voiceSlot[voice]).preset
            : currentPreset;
        uint8_t relVelAmt = relPreset.steppedParams[spReleaseVelocityAmt];
        if (relVelAmt > 0 && voiceExpr[voice].noteOffVelocity > 0) {
            uint16_t baseRel = relPreset.continuousParams[cpAmpRel];
            // Faster release for higher lift velocity
            uint32_t scaledRel = (baseRel * (65535U - (voiceExpr[voice].noteOffVelocity >> (4 - relVelAmt)))) >> 16;
            voices[voice].getAmpEnv().setCVs(0, 0, 0, (uint16_t)scaledRel, UINT16_MAX, 0x08);
        }
        voices[voice].gateOff();
    }
}

void SynthEngine::applyPreset() {
    // Apply waveforms to all voices
    waveManager.loadWave(abxAMain, currentPreset.oscBank[abxAMain], currentPreset.oscWave[abxAMain]);
    waveManager.loadWave(abxBMain, currentPreset.oscBank[abxBMain], currentPreset.oscWave[abxBMain]);
    waveManager.loadWave(abxACrossover, currentPreset.oscBank[abxACrossover], currentPreset.oscWave[abxACrossover]);
    waveManager.loadWave(abxBCrossover, currentPreset.oscBank[abxBCrossover], currentPreset.oscWave[abxBCrossover]);

    const uint16_t* waveAMain = waveManager.getWaveData(abxAMain);
    const uint16_t* waveAXOvr = waveManager.getWaveData(abxACrossover);
    const uint16_t* waveBMain = waveManager.getWaveData(abxBMain);
    const uint16_t* waveBXOvr = waveManager.getWaveData(abxBCrossover);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].setOscSampleData(waveAMain, waveAXOvr, waveBMain, waveBXOvr);
    }

    applyControls();
}

void SynthEngine::applyControls() {
    // Envelope settings
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].getFilEnv().setCVs(
            currentPreset.continuousParams[cpFilAtt],
            currentPreset.continuousParams[cpFilDec],
            currentPreset.continuousParams[cpFilSus],
            currentPreset.continuousParams[cpFilRel],
            UINT16_MAX, 0x1F
        );
        voices[v].getFilEnv().setShape(currentPreset.steppedParams[spFilEnvLin] ? 0 : 1, currentPreset.steppedParams[spFilEnvLoop]);
        voices[v].getFilEnv().setSpeedShift(currentPreset.steppedParams[spFilEnvSlow] ? 2 : 0);

        voices[v].getAmpEnv().setCVs(
            currentPreset.continuousParams[cpAmpAtt],
            currentPreset.continuousParams[cpAmpDec],
            currentPreset.continuousParams[cpAmpSus],
            currentPreset.continuousParams[cpAmpRel],
            UINT16_MAX, 0x1F
        );
        voices[v].getAmpEnv().setShape(currentPreset.steppedParams[spAmpEnvLin] ? 0 : 1, currentPreset.steppedParams[spAmpEnvLoop]);
        voices[v].getAmpEnv().setSpeedShift(currentPreset.steppedParams[spAmpEnvSlow] ? 2 : 0);

        voices[v].getWmodEnv().setCVs(
            currentPreset.continuousParams[cpWModAtt],
            currentPreset.continuousParams[cpWModDec],
            currentPreset.continuousParams[cpWModSus],
            currentPreset.continuousParams[cpWModRel],
            UINT16_MAX, 0x1F
        );
        voices[v].getWmodEnv().setShape(currentPreset.steppedParams[spWModEnvLin] ? 0 : 1, currentPreset.steppedParams[spWModEnvLoop]);
        voices[v].getWmodEnv().setSpeedShift(currentPreset.steppedParams[spWModEnvSlow] ? 2 : 0);
    }

    // LFO settings
    lfo[0].setShape((lfoShape_t)currentPreset.steppedParams[spLFOShape]);
    lfo[0].setSpeedShift(currentPreset.steppedParams[spLFOSpeed]);
    lfo[0].setCVs(currentPreset.continuousParams[cpLFOFreq], currentPreset.continuousParams[cpLFOAmt]);

    lfo[1].setShape((lfoShape_t)currentPreset.steppedParams[spLFO2Shape]);
    lfo[1].setSpeedShift(currentPreset.steppedParams[spLFO2Speed]);
    lfo[1].setCVs(currentPreset.continuousParams[cpLFO2Freq], currentPreset.continuousParams[cpLFO2Amt]);

    // Glide
    glideAmount = exponentialCourse(currentPreset.continuousParams[cpGlide], 11000.0f, 2100.0f);
    gliding = (glideAmount < 2000);

    // Filter model & mode
    float lsFreq = (float)currentPreset.continuousParams[cpShelvesLsFreq] / 65535.0f;
    float lsGain = ((float)currentPreset.continuousParams[cpShelvesLsGain] - 32768.0f) / 32768.0f;
    float p1Freq = (float)currentPreset.continuousParams[cpCutoff] / 65535.0f;
    float p1Gain = ((float)currentPreset.continuousParams[cpShelvesP1Gain] - 32768.0f) / 32768.0f;
    float p1Q = (float)currentPreset.continuousParams[cpResonance] / 65535.0f;
    float p2Freq = (float)currentPreset.continuousParams[cpShelvesP2Freq] / 65535.0f;
    float p2Gain = ((float)currentPreset.continuousParams[cpShelvesP2Gain] - 32768.0f) / 32768.0f;
    float p2Q = (float)currentPreset.continuousParams[cpShelvesP2Q] / 65535.0f;
    float hsFreq = (float)currentPreset.continuousParams[cpShelvesHsFreq] / 65535.0f;
    float hsGain = ((float)currentPreset.continuousParams[cpShelvesHsGain] - 32768.0f) / 32768.0f;

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].setFilterModelAndMode(
            currentPreset.steppedParams[spFilterModel],
            currentPreset.steppedParams[spFilterMode]
        );
        voices[v].setShelvesEQParams(lsFreq, lsGain, p1Freq, p1Gain, p1Q, p2Freq, p2Gain, p2Q, hsFreq, hsGain);
    }

    // Arpeggiator configuration from preset
    arpeggiator.setMode((arpMode_t)currentPreset.steppedParams[spArpMode], currentPreset.steppedParams[spArpHold]);
    arpeggiator.setOctaves(currentPreset.steppedParams[spArpOctaves] + 1);
    arpeggiator.setRate(currentPreset.steppedParams[spArpRate]);
    float gateVal = (float)scan_potFrom16bits(currentPreset.continuousParams[cpArpGate]) / 999.0f;
    arpeggiator.setGateLength(std::clamp(gateVal, 0.10f, 1.0f));
    float swingVal = 0.50f + ((float)scan_potFrom16bits(currentPreset.continuousParams[cpArpSwing]) - 500.0f) * (0.25f / 250.0f);
    arpeggiator.setSwing(std::clamp(swingVal, 0.50f, 0.75f));

    // Internal BPM and MIDI / Host Tempo Sync
    float bpmVal = 20.0f + ((float)scan_potFrom16bits(currentPreset.continuousParams[cpArpBpm]) / 999.0f) * 280.0f;
    setInternalBpm(bpmVal);
    setHostSyncEnabled(currentPreset.steppedParams[spArpSync] != 0);

    // Voice Count & Mask
    int vCount = currentPreset.steppedParams[spVoiceCount] + 1;
    vCount = std::clamp(vCount, 1, SYNTH_VOICE_COUNT);
    uint8_t mask = (uint8_t)((1 << vCount) - 1);
    assigner.setVoiceMask(mask);
    assigner.setPriority((assignerPriority_t)currentPreset.steppedParams[spAssignerPriority]);
    assigner.setPattern(currentPreset.voicePattern, currentPreset.steppedParams[spUnison]);

    // Master Console Model Configuration
    if (currentPreset.steppedParams[spConsoleModel] == cmMackity) {
        float potTrim = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityInTrim]);
        float potPad = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityOutPad]);
        mackity.setParameters(potTrim, potPad);
    } else if (currentPreset.steppedParams[spConsoleModel] == cmConsoleX) {
        float drive = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityInTrim]) / 999.0f;
        float pad = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityOutPad]) / 999.0f;
        float disc = (float)scan_potFrom16bits(currentPreset.continuousParams[cpConsoleDiscontinuity]) / 999.0f;
        consoleX.setParameters(drive, pad, disc);
    }
}

void SynthEngine::loadPreset(int presetIndex) {
    PresetData p;
    // Load file from disk / parse string outside the lock to prevent audio thread priority inversion
    if (presetManager.loadPreset(presetIndex, p)) {
        // Presets replace wave and DSP state as one unit.  Do not let voices
        // from the previous preset keep rendering against the new state.
        // panicGeneration carries this reset to the real-time audio engine.
        panic();
        // Browsing sounds must not silently change the user's gain-staging mode.
        currentPreset = p;
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            oscANoteCV[v] = oscBNoteCV[v] = filterNoteCV[v] = 0;
            oscATargetCV[v] = oscBTargetCV[v] = filterTargetCV[v] = 0;
        }
        applyPreset();
    }
}

void SynthEngine::setContinuousParam(continuousParameter_t cp, uint16_t value) {
    if (cp < 0 || cp >= cpCount) return;

    const uint16_t previousValue = currentPreset.continuousParams[cp];
    currentPreset.continuousParams[cp] = value;

    // Fast-path targeted updates without reloading wavetables or rebuilding whole synth
    switch (cp) {
    case cpCutoff: {
        // Cutoff is part of each note's tracked filter CV. Retarget voices that
        // are already sounding; updateCVs() slews them at control rate.
        const int32_t delta = static_cast<int32_t>(value) - static_cast<int32_t>(previousValue);
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0 || !voices[v].isActive()) continue;
            filterTargetCV[v] = static_cast<uint16_t>(__USAT(
                static_cast<int32_t>(filterTargetCV[v]) + delta, 16));
        }

        // In Shelves mode cpCutoff is also parametric band 1's centre.
        if (currentPreset.steppedParams[spFilterModel] == fmEQ) {
            const float p1Freq = static_cast<float>(value) / 65535.0f;
            for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
                if (voiceSlot[v] > 0) continue;
                voices[v].setShelvesEQParams(
                    static_cast<float>(currentPreset.continuousParams[cpShelvesLsFreq]) / 65535.0f,
                    (static_cast<float>(currentPreset.continuousParams[cpShelvesLsGain]) - 32768.0f) / 32768.0f,
                    p1Freq,
                    (static_cast<float>(currentPreset.continuousParams[cpShelvesP1Gain]) - 32768.0f) / 32768.0f,
                    static_cast<float>(currentPreset.continuousParams[cpResonance]) / 65535.0f,
                    static_cast<float>(currentPreset.continuousParams[cpShelvesP2Freq]) / 65535.0f,
                    (static_cast<float>(currentPreset.continuousParams[cpShelvesP2Gain]) - 32768.0f) / 32768.0f,
                    static_cast<float>(currentPreset.continuousParams[cpShelvesP2Q]) / 65535.0f,
                    static_cast<float>(currentPreset.continuousParams[cpShelvesHsFreq]) / 65535.0f,
                    (static_cast<float>(currentPreset.continuousParams[cpShelvesHsGain]) - 32768.0f) / 32768.0f);
            }
        }
        break;
    }

    case cpFilAtt:
    case cpFilDec:
    case cpFilSus:
    case cpFilRel:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getFilEnv().setCVs(
                currentPreset.continuousParams[cpFilAtt],
                currentPreset.continuousParams[cpFilDec],
                currentPreset.continuousParams[cpFilSus],
                currentPreset.continuousParams[cpFilRel],
                UINT16_MAX, 0x1F
            );
        }
        break;

    case cpAmpAtt:
    case cpAmpDec:
    case cpAmpSus:
    case cpAmpRel:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getAmpEnv().setCVs(
                currentPreset.continuousParams[cpAmpAtt],
                currentPreset.continuousParams[cpAmpDec],
                currentPreset.continuousParams[cpAmpSus],
                currentPreset.continuousParams[cpAmpRel],
                UINT16_MAX, 0x1F
            );
        }
        break;

    case cpWModAtt:
    case cpWModDec:
    case cpWModSus:
    case cpWModRel:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getWmodEnv().setCVs(
                currentPreset.continuousParams[cpWModAtt],
                currentPreset.continuousParams[cpWModDec],
                currentPreset.continuousParams[cpWModSus],
                currentPreset.continuousParams[cpWModRel],
                UINT16_MAX, 0x1F
            );
        }
        break;

    case cpLFOFreq:
    case cpLFOAmt:
        lfo[0].setCVs(currentPreset.continuousParams[cpLFOFreq], currentPreset.continuousParams[cpLFOAmt]);
        break;

    case cpLFO2Freq:
    case cpLFO2Amt:
        lfo[1].setCVs(currentPreset.continuousParams[cpLFO2Freq], currentPreset.continuousParams[cpLFO2Amt]);
        break;

    case cpGlide:
        glideAmount = exponentialCourse(currentPreset.continuousParams[cpGlide], 11000.0f, 2100.0f);
        gliding = (glideAmount < 2000);
        break;

    case cpShelvesLsFreq:
    case cpShelvesLsGain:
    case cpShelvesP1Gain:
    case cpShelvesP2Freq:
    case cpShelvesP2Gain:
    case cpShelvesP2Q:
    case cpShelvesHsFreq:
    case cpShelvesHsGain: {
        float lsFreq = (float)currentPreset.continuousParams[cpShelvesLsFreq] / 65535.0f;
        float lsGain = ((float)currentPreset.continuousParams[cpShelvesLsGain] - 32768.0f) / 32768.0f;
        float p1Freq = (float)currentPreset.continuousParams[cpCutoff] / 65535.0f;
        float p1Gain = ((float)currentPreset.continuousParams[cpShelvesP1Gain] - 32768.0f) / 32768.0f;
        float p1Q = (float)currentPreset.continuousParams[cpResonance] / 65535.0f;
        float p2Freq = (float)currentPreset.continuousParams[cpShelvesP2Freq] / 65535.0f;
        float p2Gain = ((float)currentPreset.continuousParams[cpShelvesP2Gain] - 32768.0f) / 32768.0f;
        float p2Q = (float)currentPreset.continuousParams[cpShelvesP2Q] / 65535.0f;
        float hsFreq = (float)currentPreset.continuousParams[cpShelvesHsFreq] / 65535.0f;
        float hsGain = ((float)currentPreset.continuousParams[cpShelvesHsGain] - 32768.0f) / 32768.0f;

        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].setShelvesEQParams(lsFreq, lsGain, p1Freq, p1Gain, p1Q, p2Freq, p2Gain, p2Q, hsFreq, hsGain);
        }
        break;
    }

    case cpArpGate: {
        float gateVal = (float)scan_potFrom16bits(currentPreset.continuousParams[cpArpGate]) / 999.0f;
        arpeggiator.setGateLength(std::clamp(gateVal, 0.10f, 1.0f));
        break;
    }

    case cpArpSwing: {
        float swingVal = 0.50f + ((float)scan_potFrom16bits(currentPreset.continuousParams[cpArpSwing]) - 500.0f) * (0.25f / 250.0f);
        arpeggiator.setSwing(std::clamp(swingVal, 0.50f, 0.75f));
        break;
    }

    case cpArpBpm: {
        float bpmVal = 20.0f + ((float)scan_potFrom16bits(currentPreset.continuousParams[cpArpBpm]) / 999.0f) * 280.0f;
        setInternalBpm(bpmVal);
        break;
    }

    case cpMackityInTrim:
    case cpMackityOutPad:
    case cpConsoleDiscontinuity:
        if (currentPreset.steppedParams[spConsoleModel] == cmMackity) {
            float potTrim = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityInTrim]);
            float potPad = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityOutPad]);
            mackity.setParameters(potTrim, potPad);
        } else if (currentPreset.steppedParams[spConsoleModel] == cmConsoleX) {
            float drive = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityInTrim]) / 999.0f;
            float pad = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityOutPad]) / 999.0f;
            float disc = (float)scan_potFrom16bits(currentPreset.continuousParams[cpConsoleDiscontinuity]) / 999.0f;
            consoleX.setParameters(drive, pad, disc);
        }
        break;

    default:
        break;
    }
}

void SynthEngine::setSteppedParam(steppedParameter_t sp, uint8_t value) {
    if (sp < 0 || sp >= spCount) return;

    const arpMode_t previousArpMode = arpeggiator.getMode();
    currentPreset.steppedParams[sp] = value;

    switch (sp) {
    case spFilterModel:
    case spFilterMode:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].setFilterModelAndMode(
                currentPreset.steppedParams[spFilterModel],
                currentPreset.steppedParams[spFilterMode]
            );
        }
        break;

    case spFilEnvLin:
    case spFilEnvLoop:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getFilEnv().setShape(currentPreset.steppedParams[spFilEnvLin] ? 0 : 1, currentPreset.steppedParams[spFilEnvLoop]);
        }
        break;

    case spFilEnvSlow:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getFilEnv().setSpeedShift(currentPreset.steppedParams[spFilEnvSlow] ? 2 : 0);
        }
        break;

    case spAmpEnvLin:
    case spAmpEnvLoop:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getAmpEnv().setShape(currentPreset.steppedParams[spAmpEnvLin] ? 0 : 1, currentPreset.steppedParams[spAmpEnvLoop]);
        }
        break;

    case spAmpEnvSlow:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getAmpEnv().setSpeedShift(currentPreset.steppedParams[spAmpEnvSlow] ? 2 : 0);
        }
        break;

    case spWModEnvLin:
    case spWModEnvLoop:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getWmodEnv().setShape(currentPreset.steppedParams[spWModEnvLin] ? 0 : 1, currentPreset.steppedParams[spWModEnvLoop]);
        }
        break;

    case spWModEnvSlow:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voiceSlot[v] > 0) continue;
            voices[v].getWmodEnv().setSpeedShift(currentPreset.steppedParams[spWModEnvSlow] ? 2 : 0);
        }
        break;

    case spLFOShape:
        lfo[0].setShape((lfoShape_t)currentPreset.steppedParams[spLFOShape]);
        break;
    case spLFOSpeed:
        lfo[0].setSpeedShift(currentPreset.steppedParams[spLFOSpeed]);
        break;

    case spLFO2Shape:
        lfo[1].setShape((lfoShape_t)currentPreset.steppedParams[spLFO2Shape]);
        break;
    case spLFO2Speed:
        lfo[1].setSpeedShift(currentPreset.steppedParams[spLFO2Speed]);
        break;

    case spArpMode: {
        const auto newArpMode = (arpMode_t)currentPreset.steppedParams[spArpMode];
        // Notes started while the arp was disabled belong directly to the
        // voice assigner. Once enabled, their physical note-offs are routed
        // to the arp instead, so release those direct gates at the boundary.
        if (previousArpMode == amOff && newArpMode != amOff)
            assigner.allKeysOff();
        arpeggiator.setMode(newArpMode, currentPreset.steppedParams[spArpHold]);
        break;
    }
    case spArpHold:
        arpeggiator.setMode((arpMode_t)currentPreset.steppedParams[spArpMode], currentPreset.steppedParams[spArpHold]);
        break;
    case spArpOctaves:
        arpeggiator.setOctaves(currentPreset.steppedParams[spArpOctaves] + 1);
        break;
    case spArpRate:
        arpeggiator.setRate(currentPreset.steppedParams[spArpRate]);
        break;
    case spArpSync:
        setHostSyncEnabled(currentPreset.steppedParams[spArpSync] != 0);
        break;

    case spVoiceCount: {
        int vCount = currentPreset.steppedParams[spVoiceCount] + 1;
        vCount = std::clamp(vCount, 1, SYNTH_VOICE_COUNT);
        uint8_t mask = (uint8_t)((1 << vCount) - 1);
        assigner.setVoiceMask(mask);
        break;
    }
    case spAssignerPriority:
        assigner.setPriority((assignerPriority_t)currentPreset.steppedParams[spAssignerPriority]);
        break;
    case spUnison:
        assigner.setPattern(currentPreset.voicePattern, currentPreset.steppedParams[spUnison]);
        break;

    case spConsoleModel:
        if (currentPreset.steppedParams[spConsoleModel] == cmMackity) {
            float potTrim = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityInTrim]);
            float potPad = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityOutPad]);
            mackity.setParameters(potTrim, potPad);
        } else if (currentPreset.steppedParams[spConsoleModel] == cmConsoleX) {
            float drive = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityInTrim]) / 999.0f;
            float pad = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityOutPad]) / 999.0f;
            float disc = (float)scan_potFrom16bits(currentPreset.continuousParams[cpConsoleDiscontinuity]) / 999.0f;
            consoleX.setParameters(drive, pad, disc);
        }
        break;

    case spEngineMode:
        if (currentPreset.steppedParams[spEngineMode] == emSingle) {
            for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
                voiceSlot[v] = 0;
                configureVoicePart(v, 0, voiceExpr[v].noteOnVelocity);
            }
            applyControls();
        }
        break;

    default:
        break;
    }
}

void SynthEngine::updateSingleVoice(int8_t v, bool advanceEnv) {
    if (v < 0 || v >= SYNTH_VOICE_COUNT) return;

    if (advanceEnv) {
        voices[v].updateEnvelopes();
    }

    if (!voices[v].isActive()) {
        voices[v].getVca().setCV(0);
        return;
    }

    const PresetData& preset = (voiceSlot[v] >= 0)
        ? afxKit.getSlot(voiceSlot[v]).preset
        : currentPreset;

    int32_t resVal = preset.continuousParams[cpResonance];
    resVal += scaleU16S16(preset.continuousParams[cpLFOResAmt], lfo[0].getOutput());
    resVal += scaleU16S16(preset.continuousParams[cpLFO2ResAmt], lfo[1].getOutput());
    resVal = __USAT(resVal, 16);

    int32_t resoFactor = (35 * (int32_t)UINT16_MAX + 170 * std::max(0, (int)resVal - 2500)) / (100 * 256);
    float rf = calibratedGain ? 1.0f : (float)resoFactor / 256.0f;

    float gainA = ((float)preset.continuousParams[cpAVol] / 65535.0f) * rf;
    float gainB = ((float)preset.continuousParams[cpBVol] / 65535.0f) * rf;
    float gainNoise = ((float)preset.continuousParams[cpNoiseVol] / 65535.0f) * rf * 0.35f;

    // ---------------------------------------------------------
    // Modulation Matrix Evaluation (Slots 0 .. 7)
    // ---------------------------------------------------------
    float modPitchAll = 0.0f;
    float modPitchA = 0.0f;
    float modPitchB = 0.0f;
    float modDetune = 0.0f;
    float modWaveModAll = 0.0f;
    float modWaveModA = 0.0f;
    float modWaveModB = 0.0f;
    float modVolOscA = 0.0f;
    float modVolOscB = 0.0f;
    float modNoiseVol = 0.0f;
    float modCutoff = 0.0f;
    float modResonance = 0.0f;
    float modAmpLevel = 0.0f;
    float modElementsGeometry = 0.0f;
    float modElementsBrightness = 0.0f;
    float modElementsDamping = 0.0f;
    float modElementsPosition = 0.0f;
    float modElementsSpace = 0.0f;
    float modElementsBow = 0.0f;
    float modElementsBlow = 0.0f;
    float modElementsStrike = 0.0f;

    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        const auto& slot = preset.modMatrix[s];
        if (slot.enabled && slot.source != modSrcNone && slot.dest != modDestNone && slot.depth != 0) {
            float srcVal = evaluateModSource(v, slot.source);
            float viaVal = 1.0f;
            if (slot.viaSource != modSrcNone) {
                viaVal = evaluateModSource(v, slot.viaSource);
            }
            float depthNorm = (float)slot.depth / 100.0f;
            float delta = srcVal * viaVal * depthNorm;

            switch (slot.dest) {
            case modDestPitchAll: modPitchAll += delta; break;
            case modDestPitchOscA: modPitchA += delta; break;
            case modDestPitchOscB: modPitchB += delta; break;
            case modDestDetune: modDetune += delta; break;
            case modDestWaveModAll: modWaveModAll += delta; break;
            case modDestWaveModOscA: modWaveModA += delta; break;
            case modDestWaveModOscB: modWaveModB += delta; break;
            case modDestVolOscA: modVolOscA += delta; break;
            case modDestVolOscB: modVolOscB += delta; break;
            case modDestNoiseVol: modNoiseVol += delta; break;
            case modDestCutoff: modCutoff += delta; break;
            case modDestResonance: modResonance += delta; break;
            case modDestAmpLevel: modAmpLevel += delta; break;
            case modDestElementsGeometry: modElementsGeometry += delta; break;
            case modDestElementsBrightness: modElementsBrightness += delta; break;
            case modDestElementsDamping: modElementsDamping += delta; break;
            case modDestElementsPosition: modElementsPosition += delta; break;
            case modDestElementsSpace: modElementsSpace += delta; break;
            case modDestElementsBow: modElementsBow += delta; break;
            case modDestElementsBlow: modElementsBlow += delta; break;
            case modDestElementsStrike: modElementsStrike += delta; break;
            default: break;
            }
        }
    }

    resVal = std::clamp((int32_t)(resVal + (int32_t)(modResonance * 65535.0f)), 0, 65535);
    gainA = std::clamp(gainA + modVolOscA * rf, 0.0f, 2.0f);
    gainB = std::clamp(gainB + modVolOscB * rf, 0.0f, 2.0f);
    gainNoise = std::clamp(gainNoise + modNoiseVol * rf * 0.35f, 0.0f, 1.0f);

    // Pitch modulation
    int32_t pitchAVal = 0, pitchBVal = 0;
    int32_t lfo1Pitch = scaleU16S16(preset.continuousParams[cpLFOPitchAmt], lfo[0].getOutput() >> 1);
    if (preset.steppedParams[spLFOTargets] & otA) pitchAVal += lfo1Pitch;
    if (preset.steppedParams[spLFOTargets] & otB) pitchBVal += lfo1Pitch;

    int32_t lfo2Pitch = scaleU16S16(preset.continuousParams[cpLFO2PitchAmt], lfo[1].getOutput() >> 1);
    if (preset.steppedParams[spLFO2Targets] & otA) pitchAVal += lfo2Pitch;
    if (preset.steppedParams[spLFO2Targets] & otB) pitchBVal += lfo2Pitch;

    // Pitch Bend (Per-Voice MPE or Global Master)
    int16_t voiceBend = voiceExpr[v].hasPerVoiceBend ? (int16_t)std::round(voiceExpr[v].smoothedBend) : benderAmount;
    pitchAVal += voiceBend;
    pitchBVal += voiceBend;

    // Modulation Matrix Pitch Offsets
    pitchAVal += (int32_t)((modPitchAll + modPitchA) * (12.0f * (float)WTOSC_CV_SEMITONE));
    pitchBVal += (int32_t)((modPitchAll + modPitchB) * (12.0f * (float)WTOSC_CV_SEMITONE));
    pitchAVal -= (int32_t)(modDetune * 256.0f);
    pitchBVal += (int32_t)(modDetune * 256.0f);

    // Per-Voice Expression values (Pressure & Timbre CC74)
    static const int8_t pr[] = { 5, 3, 1, 0 };
    int8_t pShift = pr[std::clamp((int)preset.steppedParams[spPressureRange], 0, 3)];
    uint16_t rawPress = voiceExpr[v].hasPerVoicePressure ? (uint16_t)std::clamp((int)std::round(voiceExpr[v].smoothedPressure), 0, 65535) : pressureAmount;
    int32_t pressAmt = (rawPress >> pShift);

    uint16_t rawTimbre = voiceExpr[v].hasPerVoiceTimbre ? (uint16_t)std::clamp((int)std::round(voiceExpr[v].smoothedTimbre), 0, 65535) : timbreAmount;
    int32_t timbreBipolar = ((int32_t)rawTimbre - 32768);

    // Mod Wheel Modulation
    if (preset.steppedParams[spModwheelTarget] == modPitch) {
        int32_t mwPitch = scaleU16S16(modwheelAmount, lfo[0].getOutput() >> 1);
        pitchAVal += mwPitch;
        pitchBVal += mwPitch;
    }

    // Pressure & Timbre to Pitch
    if (preset.steppedParams[spPressureTarget] == modPitch) {
        pitchAVal -= (pressAmt >> 2);
        pitchBVal -= (pressAmt >> 2);
    }
    if (preset.steppedParams[spTimbreTarget] == modPitch) {
        int32_t tPitch = timbreBipolar >> 5;
        pitchAVal += tPitch;
        pitchBVal += tPitch;
    }

    int32_t detuneRaw = preset.continuousParams[cpDetune];
    int32_t detune = (detuneRaw >> 8) + INT8_MIN;
    pitchAVal -= (detune >> 1);
    pitchBVal += (detune >> 1);

    int32_t mTuneRaw = currentPreset.continuousParams[cpMasterTune];
    int32_t mTune = (mTuneRaw >> 7) + INT8_MIN * 2;
    pitchAVal += mTune;
    pitchBVal += mTune;

    // Filter modulation
    int32_t filterMod = scaleU16S16(preset.continuousParams[cpLFOFilAmt], lfo[0].getOutput());
    filterMod += scaleU16S16(preset.continuousParams[cpLFO2FilAmt], lfo[1].getOutput());
    filterMod += (int32_t)(modCutoff * 65535.0f);

    if (preset.steppedParams[spModwheelTarget] == modFilter) {
        filterMod += (modwheelAmount >> 2);
    }
    if (preset.steppedParams[spPressureTarget] == modFilter) {
        filterMod += pressAmt;
    }
    if (preset.steppedParams[spTimbreTarget] == modFilter) {
        filterMod += (timbreBipolar >> 1);
    }

    // Amp modulation
    int32_t ampVal = UINT16_MAX;
    ampVal -= scaleU16U16(preset.continuousParams[cpLFOAmpAmt], lfo[0].getLevelCV() >> 1);
    ampVal += scaleU16S16(preset.continuousParams[cpLFOAmpAmt], lfo[0].getOutput());
    ampVal -= scaleU16U16(preset.continuousParams[cpLFO2AmpAmt], lfo[1].getLevelCV() >> 1);
    ampVal += scaleU16S16(preset.continuousParams[cpLFO2AmpAmt], lfo[1].getOutput());

    if (preset.steppedParams[spPressureTarget] == modVolume) {
        ampVal = std::clamp(ampVal + (pressAmt >> 1), 0, 65535);
    }
    if (preset.steppedParams[spTimbreTarget] == modVolume) {
        ampVal = std::clamp(ampVal + (timbreBipolar >> 2), 0, 65535);
    }
    ampVal = std::clamp((int32_t)(ampVal + (int32_t)(modAmpLevel * 65535.0f)), 0, 65535);
    ampVal = scaleU16U16((uint16_t)__USAT(ampVal, 16), currentPreset.continuousParams[cpAmpLevel]);

    // WaveMod
    int32_t filEnvAmt = (int32_t)preset.continuousParams[cpFilEnvAmt] + INT16_MIN;
    int32_t wmodAEnvAmt = (int32_t)preset.continuousParams[cpWModAEnv] + INT16_MIN;
    int32_t wmodBEnvAmt = (int32_t)preset.continuousParams[cpWModBEnv] + INT16_MIN;

    int32_t wmodAVal = preset.continuousParams[cpABaseWMod];
    if (preset.steppedParams[spLFOTargets] & otA)
        wmodAVal += scaleU16S16(preset.continuousParams[cpLFOWModAmt], lfo[0].getOutput());
    if (preset.steppedParams[spLFO2Targets] & otA)
        wmodAVal += scaleU16S16(preset.continuousParams[cpLFO2WModAmt], lfo[1].getOutput());

    int32_t wmodBVal = preset.continuousParams[cpBBaseWMod];
    if (preset.steppedParams[spLFOTargets] & otB)
        wmodBVal += scaleU16S16(preset.continuousParams[cpLFOWModAmt], lfo[0].getOutput());
    if (preset.steppedParams[spLFO2Targets] & otB)
        wmodBVal += scaleU16S16(preset.continuousParams[cpLFO2WModAmt], lfo[1].getOutput());

    if (preset.steppedParams[spModwheelTarget] == modWaveMod) {
        wmodAVal += (modwheelAmount >> 2);
        wmodBVal += (modwheelAmount >> 2);
    }
    if (preset.steppedParams[spPressureTarget] == modWaveMod) {
        wmodAVal += pressAmt;
        wmodBVal += pressAmt;
    }
    if (preset.steppedParams[spTimbreTarget] == modWaveMod) {
        wmodAVal += (timbreBipolar >> 1);
        wmodBVal += (timbreBipolar >> 1);
    }

    wmodAVal += (int32_t)((modWaveModAll + modWaveModA) * 65535.0f);
    wmodBVal += (int32_t)((modWaveModAll + modWaveModB) * 65535.0f);

    int16_t unisonDetuneRaw = currentPreset.continuousParams[cpUnisonDetune];
    bool hardSync = preset.steppedParams[spOscSync] != 0;

    // Filter cutoff
    int32_t vf = filterMod;
    vf += scaleU16S16(voices[v].getFilEnv().getOutput(), filEnvAmt);
    vf += filterNoteCV[v];
    uint16_t cutoffCV = (uint16_t)__USAT(vf, 16);

    // WaveMod A & B
    int32_t vma = wmodAVal + scaleU16S16(voices[v].getWmodEnv().getOutput(), wmodAEnvAmt);
    uint16_t finalWmodA = (uint16_t)__USAT(vma, 16);

    int32_t vmb = wmodBVal + scaleU16S16(voices[v].getWmodEnv().getOutput(), wmodBEnvAmt);
    uint16_t finalWmodB = (uint16_t)__USAT(vmb, 16);

    // Pitches
    int32_t vpa = pitchAVal + oscANoteCV[v];
    int32_t vpb = pitchBVal + oscBNoteCV[v];

    int16_t uDetune = (int16_t)((1 + (v >> 1)) * (v & 1 ? -1 : 1) * (unisonDetuneRaw >> 9));
    vpa += uDetune;
    vpb += uDetune;

    uint16_t finalPitchA = (uint16_t)__USAT(vpa, 16);
    uint16_t finalPitchB = (uint16_t)__USAT(vpb, 16);

    // Amplitude
    uint16_t finalAmp = scaleU16U16(voices[v].getAmpEnv().getOutput(), (uint16_t)ampVal);

    voices[v].updateVoiceCVs(
        finalPitchA, finalPitchB,
        (oscWModTarget_t)preset.steppedParams[spAWModType], finalWmodA,
        (oscWModTarget_t)preset.steppedParams[spBWModType], finalWmodB,
        cutoffCV, (uint16_t)resVal, finalAmp,
        gainA, gainB, gainNoise, hardSync
    );

    uint8_t oscEngineMode = preset.steppedParams[spOscEngine];
    voices[v].setOscEngine(oscEngineMode);

    if (oscEngineMode != oeWavetable) {
        float geom = std::clamp(((float)preset.continuousParams[cpElementsGeometry] / 65535.0f) + modElementsGeometry, 0.0f, 1.0f);
        float bright = std::clamp(((float)preset.continuousParams[cpElementsBrightness] / 65535.0f) + modElementsBrightness, 0.0f, 1.0f);
        float damp = std::clamp(((float)preset.continuousParams[cpElementsDamping] / 65535.0f) + modElementsDamping, 0.0f, 1.0f);
        float pos = std::clamp(((float)preset.continuousParams[cpElementsPosition] / 65535.0f) + modElementsPosition, 0.0f, 1.0f);
        float space = std::clamp(((float)preset.continuousParams[cpElementsSpace] / 65535.0f) + modElementsSpace, 0.0f, 1.0f);
        float bow = std::clamp(((float)preset.continuousParams[cpElementsBow] / 65535.0f) + modElementsBow, 0.0f, 1.0f);
        float blow = std::clamp(((float)preset.continuousParams[cpElementsBlow] / 65535.0f) + modElementsBlow, 0.0f, 1.0f);
        float strike = std::clamp(((float)preset.continuousParams[cpElementsStrike] / 65535.0f) + modElementsStrike, 0.0f, 1.0f);
        float mallet = std::clamp((float)preset.continuousParams[cpElementsMallet] / 65535.0f, 0.0f, 1.0f);
        uint8_t model = preset.steppedParams[spElementsModel];
        float pitchMidiNote = (float)finalPitchA / (float)WTOSC_CV_SEMITONE;

        voices[v].updateElementsParams(model, geom, bright, damp, pos, space, bow, blow, strike, mallet, pitchMidiNote);
    }
}

void SynthEngine::updateCVs() {
    lfo[0].update();
    lfo[1].update();

    const float alpha = 0.12f;
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!gliding && voices[v].isActive() && filterNoteCV[v] != filterTargetCV[v]) {
            const int32_t difference = static_cast<int32_t>(filterTargetCV[v]) - filterNoteCV[v];
            const int32_t magnitude = std::abs(difference);
            const int32_t step = std::max<int32_t>(1, magnitude / 32);
            filterNoteCV[v] = static_cast<uint16_t>(static_cast<int32_t>(filterNoteCV[v])
                + (difference > 0 ? std::min(step, difference) : std::max(-step, difference)));
        }
        voiceExpr[v].smoothedBend += alpha * ((float)voiceExpr[v].pitchBendOffset - voiceExpr[v].smoothedBend);
        voiceExpr[v].smoothedPressure += alpha * ((float)voiceExpr[v].pressure - voiceExpr[v].smoothedPressure);
        voiceExpr[v].smoothedTimbre += alpha * ((float)voiceExpr[v].timbre - voiceExpr[v].smoothedTimbre);
        updateSingleVoice(v, true);
    }
}

static inline void computeGlide(uint16_t& out, uint16_t target, uint16_t amount) {
    if (out < target) {
        uint16_t diff = target - out;
        out += std::min(amount, diff);
    } else if (out > target) {
        uint16_t diff = out - target;
        out -= std::min(amount, diff);
    }
}

void SynthEngine::tickTimerEvent(uint8_t phase) {
    ++currentTick;

    // Glide computation
    if (gliding) {
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            computeGlide(oscANoteCV[v], oscATargetCV[v], (uint16_t)glideAmount);
            computeGlide(oscBNoteCV[v], oscBTargetCV[v], (uint16_t)glideAmount);
            computeGlide(filterNoteCV[v], filterTargetCV[v], (uint16_t)glideAmount);
        }
    }

    if (arpeggiator.getMode() != amOff) {
        uint32_t baseTicks = arpeggiator.getStepDivisionTicks();
        if (baseTicks < 4) baseTicks = 4;

        int stepIdx = arpeggiator.getStepCount();
        float swing = arpeggiator.getSwing();
        int swingOffset = 0;
        if (stepIdx % 2 == 1 && swing > 0.501f) {
            swingOffset = (int)std::round((swing - 0.5f) * 2.0f * ((float)baseTicks * 0.40f));
        }

        uint32_t stepCycleTicks = baseTicks * 2;
        uint32_t stepPhase = currentTick % stepCycleTicks;

        uint32_t trigger0 = 0;
        uint32_t trigger1 = baseTicks + (uint32_t)swingOffset;

        float gate = arpeggiator.getGateLength();
        uint32_t gateDuration = std::max((uint32_t)1, (uint32_t)std::round((float)baseTicks * gate));
        // A latched 100% gate otherwise closes and retriggers in the exact
        // same control tick. Dense MIDI then repeatedly steals a voice before
        // its release has advanced, producing a metallic release rattle.
        // Keep one 48-PPQ control tick for a real release transition in Hold.
        const bool heldFullGate = arpeggiator.getHold() != 0 && gate >= 0.98f;
        if (heldFullGate && baseTicks > 1) gateDuration = std::min(gateDuration, baseTicks - 1);

        if (stepPhase == trigger0 || stepPhase == trigger1) {
            arpeggiator.clockTick();
            if ((gate < 0.98f || heldFullGate) && arpeggiator.isGateActive() && !arpeggiator.isNextStepTie())
                arpGateCloseTick = currentTick + gateDuration;
            else
                arpGateCloseTick = UINT32_MAX;
        } else if (arpGateCloseTick != UINT32_MAX
                   && static_cast<int32_t>(currentTick - arpGateCloseTick) >= 0) {
            arpeggiator.finishPreviousNote();
            arpGateCloseTick = UINT32_MAX;
        }
    }
}

void SynthEngine::renderBlock(float* leftOut, float* rightOut, int numSamples, int hostOffset) {
    // Thread safety: serialize parameter and preset updates with audio processing

    // Timing increments for DAC SPI CV updates (~4 kHz) and beat-synchronous sequencer ticker
    const float cvStep = (float)DACSPI_UPDATE_HZ / sampleRate;
    const float effectiveBpm = getEffectiveBpm();
    const float tickerHz = effectiveBpm * 0.8f; // 48 ticks per quarter note
    const bool clockRunning = !hostSyncEnabled || !hostTransportAvailable || hostTransportPlaying;
    const float tickStepRate = clockRunning ? tickerHz / sampleRate : 0.0f;

    // Check Master Console Model: 0 = Clean/Bypass, 1 = Airwindows Mackity, 2 = Airwindows ConsoleX
    const uint8_t consoleModel = currentPreset.steppedParams[spConsoleModel];
    const bool mackityOn = (consoleModel == cmMackity);
    const bool consoleXOn = (consoleModel == cmConsoleX);
    float unisonGain = 1.0f;
    if (calibratedGain && currentPreset.steppedParams[spUnison] != 0) {
        int unisonVoices = 0;
        while (unisonVoices < SYNTH_VOICE_COUNT
               && currentPreset.voicePattern[unisonVoices] != ASSIGNER_NO_NOTE) ++unisonVoices;
        // Constant-power compensation keeps stacked voices from overdriving
        // the summing bus while retaining the perceived lift of unison.
        if (unisonVoices > 1) unisonGain = 1.0f / std::sqrt(static_cast<float>(unisonVoices));
    }
    if (mackityOn) {
        float potTrim = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityInTrim]);
        float potPad = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityOutPad]);
        mackity.setParameters(potTrim, potPad);
    } else if (consoleXOn) {
        float drive = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityInTrim]) / 999.0f;
        float pad = (float)scan_potFrom16bits(currentPreset.continuousParams[cpMackityOutPad]) / 999.0f;
        float disc = (float)scan_potFrom16bits(currentPreset.continuousParams[cpConsoleDiscontinuity]) / 999.0f;
        consoleX.setParameters(drive, pad, disc);
    }

    // Main Sample-by-Sample Audio Rendering Loop
    for (int i = 0; i < numSamples; ++i) {
        // Track sample position within block for sample-accurate MIDI out events
        currentSampleOffset = hostOffset + i;

        // 1. Sub-sample accurate control voltage (CV) updates (~4000 Hz)
        cvSubSampleCounter += cvStep;
        if (cvSubSampleCounter >= 1.0f) {
            cvSubSampleCounter -= 1.0f;
            updateCVs();
        }

        // 2. Hardware ticker clock event for envelopes, arpeggiator & LFO (~250 Hz)
        tickSubSampleCounter += tickStepRate;
        if (tickSubSampleCounter >= 1.0f) {
            tickSubSampleCounter -= 1.0f;
            tickTimerEvent(0);
        }

        // 3. Render and accumulate all 6 polyphonic voices across the stereo field
        float leftAcc = 0.0f;
        float rightAcc = 0.0f;

        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (voices[v].isActive()) {
                float smp = voices[v].processSample(tickStep) * voiceFader[v] * unisonGain;
                voiceMeterPeaks[v] = std::max(voiceMeterPeaks[v], std::abs(smp));
                float pan = getVoicePan(v);
                float panL = 0.5f * (1.0f - pan);
                float panR = 0.5f * (1.0f + pan);

                float vL = smp * panL;
                float vR = smp * panR;

                if (consoleXOn) {
                    float encL = 0.0f, encR = 0.0f;
                    consoleX.encodeVoice(vL, vR, encL, encR);
                    leftAcc += encL;
                    rightAcc += encR;
                } else {
                    leftAcc += vL;
                    rightAcc += vR;
                }
            }
        }

        // 4. Master volume / headroom scaling and master console stage
        float outL = 0.0f;
        float outR = 0.0f;

        if (consoleXOn) {
            // ConsoleX master bus decoding (Phi expansion + Discontinuity + Ultrasonic filtering)
            consoleX.decodeMaster(leftAcc, rightAcc, outL, outR);
#ifdef OVERVIBER_DIAGNOSTICS
            if (diagnostics) {
                diagnostics->consoleLeft.add(outL);
                diagnostics->consoleRight.add(outR);
            }
#endif
            outL *= 0.45f;
            outR *= 0.45f;
        } else {
            outL = leftAcc * 0.45f;
            outR = rightAcc * 0.45f;
            if (mackityOn) {
                mackity.processSample(outL, outR);
            }
        }

        // 5. Write final stereo audio samples to DAW output buffers
        if (calibratedGain) {
            auto ceiling = [](float x) {
                return std::abs(x) <= 0.9f ? x
                    : std::copysign(0.9f + 0.08f * std::tanh((std::abs(x) - 0.9f) / 0.08f), x);
            };
            outL = ceiling(outL); outR = ceiling(outR);
        }
        if (presetTransitionRemaining > 0) {
            const float oldWeight = static_cast<float>(presetTransitionRemaining)
                / static_cast<float>(presetTransitionSamples);
            outL = presetTransitionLeft * oldWeight + outL * (1.0f - oldWeight);
            outR = presetTransitionRight * oldWeight + outR * (1.0f - oldWeight);
            --presetTransitionRemaining;
        }
        lastOutputLeft = outL;
        lastOutputRight = outR;
#ifdef OVERVIBER_DIAGNOSTICS
        if (diagnostics) {
            diagnostics->busLeft.add(leftAcc);
            diagnostics->busRight.add(rightAcc);
            diagnostics->outputLeft.add(outL);
            diagnostics->outputRight.add(outR);
        }
#endif
        leftOut[i] = outL;
        rightOut[i] = outR;
    }
}

int32_t SynthEngine::getVoiceAmpLevel(int voiceIndex) {
    if (useDisplayLevels && voiceIndex >= 0 && voiceIndex < 6) return displayLevels[voiceIndex];
    if (voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT && voices[voiceIndex].isActive())
        return voices[voiceIndex].getAmpEnv().getOutput();
    return 0;
}

int32_t SynthEngine::getVoicePeakLevel(int voiceIndex) const {
    if (voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT)
        return static_cast<int32_t>(std::round(std::clamp(voiceMeterPeaks[voiceIndex], 0.0f, 16.0f) * 65535.0f));
    return 0;
}

void SynthEngine::configureVoicePart(int voice, uint8_t slotIdx, uint16_t velocity) {
            auto& slot = afxKit.getSlot(slotIdx);

            const uint16_t* wAMain = slot.waveManager.getWaveData(abxAMain);
            const uint16_t* wAXOvr = slot.waveManager.getWaveData(abxACrossover);
            const uint16_t* wBMain = slot.waveManager.getWaveData(abxBMain);
            const uint16_t* wBXOvr = slot.waveManager.getWaveData(abxBCrossover);
            if (!wAMain) wAMain = waveManager.getWaveData(abxAMain);
            if (!wAXOvr) wAXOvr = waveManager.getWaveData(abxACrossover);
            if (!wBMain) wBMain = waveManager.getWaveData(abxBMain);
            if (!wBXOvr) wBXOvr = waveManager.getWaveData(abxBCrossover);

            voices[voice].setOscSampleData(wAMain, wAXOvr, wBMain, wBXOvr);
            voices[voice].setFilterModelAndMode(
                slot.preset.steppedParams[spFilterModel],
                slot.preset.steppedParams[spFilterMode]
            );

            // Per-slot ADSR Envelope configuration
            voices[voice].getFilEnv().setCVs(
                slot.preset.continuousParams[cpFilAtt],
                slot.preset.continuousParams[cpFilDec],
                slot.preset.continuousParams[cpFilSus],
                slot.preset.continuousParams[cpFilRel],
                UINT16_MAX, 0x1F
            );
            voices[voice].getFilEnv().setShape(slot.preset.steppedParams[spFilEnvLin] ? 0 : 1, slot.preset.steppedParams[spFilEnvLoop]);
            voices[voice].getFilEnv().setSpeedShift(slot.preset.steppedParams[spFilEnvSlow] ? 2 : 0);

            voices[voice].getAmpEnv().setCVs(
                slot.preset.continuousParams[cpAmpAtt],
                slot.preset.continuousParams[cpAmpDec],
                slot.preset.continuousParams[cpAmpSus],
                slot.preset.continuousParams[cpAmpRel],
                UINT16_MAX, 0x1F
            );
            voices[voice].getAmpEnv().setShape(slot.preset.steppedParams[spAmpEnvLin] ? 0 : 1, slot.preset.steppedParams[spAmpEnvLoop]);
            voices[voice].getAmpEnv().setSpeedShift(slot.preset.steppedParams[spAmpEnvSlow] ? 2 : 0);

            voices[voice].getWmodEnv().setCVs(
                slot.preset.continuousParams[cpWModAtt],
                slot.preset.continuousParams[cpWModDec],
                slot.preset.continuousParams[cpWModSus],
                slot.preset.continuousParams[cpWModRel],
                UINT16_MAX, 0x1F
            );
            voices[voice].getWmodEnv().setShape(slot.preset.steppedParams[spWModEnvLin] ? 0 : 1, slot.preset.steppedParams[spWModEnvLoop]);
            voices[voice].getWmodEnv().setSpeedShift(slot.preset.steppedParams[spWModEnvSlow] ? 2 : 0);

            // Per-slot Velocity Sensitivity
            uint16_t slotVelAmt = slot.preset.continuousParams[cpWModVelocity];
            voices[voice].getWmodEnv().setCVs(0, 0, 0, 0, (UINT16_MAX - slotVelAmt) + scaleU16U16(velocity, slotVelAmt), 0x10);
            slotVelAmt = slot.preset.continuousParams[cpFilVelocity];
            voices[voice].getFilEnv().setCVs(0, 0, 0, 0, (UINT16_MAX - slotVelAmt) + scaleU16U16(velocity, slotVelAmt), 0x10);
            slotVelAmt = slot.preset.continuousParams[cpAmpVelocity];
            voices[voice].getAmpEnv().setCVs(0, 0, 0, 0, (UINT16_MAX - slotVelAmt) + scaleU16U16(velocity, slotVelAmt), 0x10);

            // Per-slot Shelves Parametric EQ
            float lsFreq = (float)slot.preset.continuousParams[cpShelvesLsFreq] / 65535.0f;
            float lsGain = ((float)slot.preset.continuousParams[cpShelvesLsGain] - 32768.0f) / 32768.0f;
            float p1Freq = (float)slot.preset.continuousParams[cpCutoff] / 65535.0f;
            float p1Gain = ((float)slot.preset.continuousParams[cpShelvesP1Gain] - 32768.0f) / 32768.0f;
            float p1Q = (float)slot.preset.continuousParams[cpResonance] / 65535.0f;
            float p2Freq = (float)slot.preset.continuousParams[cpShelvesP2Freq] / 65535.0f;
            float p2Gain = ((float)slot.preset.continuousParams[cpShelvesP2Gain] - 32768.0f) / 32768.0f;
            float p2Q = (float)slot.preset.continuousParams[cpShelvesP2Q] / 65535.0f;
            float hsFreq = (float)slot.preset.continuousParams[cpShelvesHsFreq] / 65535.0f;
            float hsGain = ((float)slot.preset.continuousParams[cpShelvesHsGain] - 32768.0f) / 32768.0f;
            voices[voice].setShelvesEQParams(lsFreq, lsGain, p1Freq, p1Gain, p1Q, p2Freq, p2Gain, p2Q, hsFreq, hsGain);

}
