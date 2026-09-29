#include "SynthEngine.h"
#include <cmath>
#include "FilterCalibration.h"
#include "DefaultWaves.h"
#include "../data/DefaultKit.h"
#include "VoiceConfig.h"

void SynthEngine::applyPreparedState(const PreparedState& state, bool preserveMainParameters) {
    // A preset load is prepared on the message thread and reaches the audio
    // engine through panicGeneration.  Retire voices before replacing wave
    // memory or filter/envelope settings, otherwise release tails continue
    // with unrelated preset data and can produce clicks or bursts of noise.
    if (panicGeneration != state.panicGeneration) {
        const float transitionLeft = bus.getLastLeft();
        const float transitionRight = bus.getLastRight();
        reset();
        allocator.clearNoteCVs();
        panicGeneration = state.panicGeneration;
        bus.startPresetTransition(transitionLeft, transitionRight, sampleRate);
    }

    std::array<bool, 16> controlsChanged{};
    std::array<int32_t, 16> cutoffDelta{};
    for (int part = 0; part < 16; ++part) {
        auto& slot = parts[part];
        const auto& source = state.parts[part];
        if (part != 0 || !preserveMainParameters)
            cutoffDelta[part] = static_cast<int32_t>(source.continuous[cpCutoff])
                - static_cast<int32_t>(slot.preset.continuousParams[cpCutoff]);
        if (part != 0 || !preserveMainParameters) {
            controlsChanged[part] = std::memcmp(slot.preset.continuousParams, source.continuous, sizeof(source.continuous)) != 0
                || std::memcmp(slot.preset.steppedParams, source.stepped, sizeof(source.stepped)) != 0;
            std::copy_n(source.continuous, cpCount, slot.preset.continuousParams);
            std::copy_n(source.stepped, spCount, slot.preset.steppedParams);
        }
        controlsChanged[part] |= std::memcmp(slot.preset.voicePattern, source.pattern, sizeof(source.pattern)) != 0;
        std::copy_n(source.matrix, MOD_MATRIX_SLOT_COUNT, slot.preset.modMatrix);
        std::copy_n(source.pattern, SYNTH_VOICE_COUNT, slot.preset.voicePattern);
        // Waves are copied only when their revision changed (0: compare data).
        if (source.waveRevision == 0 || source.waveRevision != slot.waveRevision) {
            for (int wave = 0; wave < abxCount; ++wave) {
                auto* target = slot.waves[wave];
                if (std::memcmp(source.waves[wave], target, sizeof(source.waves[wave])) != 0)
                    std::copy_n(source.waves[wave], WTOSC_SAMPLE_COUNT, target);
            }
            slot.waveRevision = source.waveRevision;
        }
        allocator.route(part) = source.route;
    }
    for (int n = 0; n < 128; ++n) noteMap[n] = static_cast<uint8_t>(state.noteMap[n] % 16);
    std::copy_n(state.faders, SYNTH_VOICE_COUNT, voiceFader);
    bus.setMuted(state.masterMute);
    std::copy_n(state.pans, SYNTH_VOICE_COUNT, voicePan);
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) voicePanCustomized[v] = state.panCustomized[v] != 0;
    allocator.setCustomRouting(state.customRouting);
    if (controlsChanged[0]) applyControls();
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        const int part = allocator.part(v);
        if (part >= 0 && (controlsChanged[0] || controlsChanged[part]))
            configureVoicePart(v, static_cast<uint8_t>(part), midiInput.voice(v).noteOnVelocity);
        // Like a live cutoff edit: sounding voices of the part slew to the new cutoff.
        if (part > 0 && cutoffDelta[part] != 0 && voices[v].isActive())
            allocator.retargetFilter(v, cutoffDelta[part]);
    }
    for (int part = 1; part < 16; ++part)
        if (controlsChanged[part] && (lfoPartsRunning & (1u << part))) configurePartLfos(part);
    for (int s = 0; s < 16; ++s) {
        arpeggiator.setStepPattern(s, state.arpPattern[s]);
        arpeggiator.setStepDegree(s, state.arpDegrees[s]);
    }
    arpeggiator.setTranspose(state.transpose);
}

SynthEngine::SynthEngine() : currentPreset(parts[0].preset) {
    // Until a prepared state arrives: the default kit and the built-in waves.
    static const auto defaultWaves = [] {
        std::array<std::array<uint16_t, WTOSC_SAMPLE_COUNT>, abxCount> waves{};
        for (int abx = 0; abx < abxCount; ++abx) defaultwaves::generate(defaultwaves::forSlot(abx), waves[abx].data());
        return waves;
    }();
    for (int part = 0; part < 16; ++part) {
        defaultkit::partPreset(part, parts[part].preset);
        for (int abx = 0; abx < abxCount; ++abx)
            std::copy(defaultWaves[abx].begin(), defaultWaves[abx].end(), parts[part].waves[abx]);
    }
    for (int note = 0; note < 128; ++note) noteMap[note] = defaultkit::partForNote(note);

    sampleRate = 48000.0f;
    tickStep = (uint32_t)(SYNTH_MASTER_CLOCK / sampleRate);
    currentTick = 0;
    cvSubSampleCounter = 0.0f;
    tickSubSampleCounter = 0.0f;

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) voices[v].init(v);


    assigner.init();
    assigner.setCallback([this](uint8_t note, int8_t gate, int8_t voice, uint16_t velocity, uint8_t flags) {
        assignerEvent(note, gate, voice, velocity, flags);
    });

    arpeggiator.init();
    arpeggiator.setNoteAssignCallback([this](uint8_t note, int8_t gate, uint16_t velocity, uint8_t channel) {
        allocator.assign(assigner, note, gate, velocity, 0, currentTick, channel, isMpeMemberChannel(channel));

        // Queue MIDI Output for DAW live capture (e.g., in Ableton Live)
        uint8_t vel7 = (uint8_t)(((uint32_t)velocity * 127U) / 65535U);
        if (gate && vel7 == 0) vel7 = 1;
        if (!pendingMidiOut.push_back({ note, vel7, channel, gate != 0, currentSampleOffset })) midiOverflow = true;
    });

    currentPreset.setDefaults();
}

void SynthEngine::prepare(float sr) {
    sampleRate = std::max(22050.0f, sr);
    tickStep = (uint32_t)(SYNTH_MASTER_CLOCK / sampleRate);
    const auto filterGains = calibrateFilters(sampleRate);
    const auto semGains = calibrateSemFilters(sampleRate);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].setSampleRate(sampleRate);
        voices[v].filterGains = filterGains;
        voices[v].semGains = semGains;
    }
    bus.prepare(sampleRate);

    applyPreset();
}

void SynthEngine::pullPendingMidiOut(std::vector<MidiOutEvent>& outEvents) {
    outEvents.assign(pendingMidiOut.begin(), pendingMidiOut.end());
    pendingMidiOut.clear();
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

    if (stoppedNow) arpeggiator.stopClock();

    const double tickPosition = std::max(0.0, ppqPosition) * 48.0;
    const double wholeTicks = std::floor(tickPosition);
    currentTick = static_cast<uint32_t>(wholeTicks);
    tickSubSampleCounter = static_cast<float>(tickPosition - wholeTicks);
}

void SynthEngine::reset() {
    for (auto& voice : voices) voice.reset();
    midiInput.reset();
    bus.reset();
    assigner.panicOff();
    arpeggiator.init();
    modDelayStart.fill(UINT32_MAX);
    partKeyHeld.fill(false);
    beginVoiceMeterBlock();
}

void SynthEngine::noteOn(uint8_t note, uint16_t velocity, uint8_t channel) {
    if (arpeggiator.getMode() != amOff) arpeggiator.assignNote(note, 1, velocity, channel);
    else allocator.assign(assigner, note, 1, velocity, 1, currentTick, channel, isMpeMemberChannel(channel));
}

void SynthEngine::noteOff(uint8_t note, uint16_t velocity, uint8_t channel) {
    if (arpeggiator.getMode() != amOff) {
        arpeggiator.assignNote(note, 0, velocity, channel);
    } else {
        // Record release velocity (lift) on matching voice
        for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v)
            if (assigner.voiceMatches(v, note, channel)) midiInput.setReleaseVelocity(v, velocity);
        assigner.assignNote(note, 0, velocity, 1, currentTick, channel);
    }
}

void SynthEngine::pitchBend(int16_t bend, uint8_t channel) {
    if (isMpeMemberChannel(channel))
        midiInput.setChannelPitchBend(channel, bend, currentPreset.steppedParams[spMPEPitchBendRange]);
    else
        midiInput.setPitchBend(bend);
}

void SynthEngine::modWheel(uint16_t mod, uint8_t) { midiInput.setModWheel(mod); }

void SynthEngine::channelPressure(uint16_t press, uint8_t channel) {
    if (isMpeMemberChannel(channel)) midiInput.setChannelPressure(channel, press);
    else midiInput.setPressure(press);
}

void SynthEngine::polyAftertouch(uint8_t note, uint16_t press, uint8_t channel) {
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v)
        if (assigner.voiceMatches(v, note, channel)) midiInput.setVoicePressure(v, press);
}

void SynthEngine::timbreSlide(uint16_t timbre, uint8_t channel) {
    if (isMpeMemberChannel(channel)) midiInput.setChannelTimbre(channel, timbre);
    else midiInput.setTimbre(timbre);
}

void SynthEngine::breathController(uint16_t breath, uint8_t) { midiInput.setBreath(breath); }

void SynthEngine::expressionController(uint16_t expr, uint8_t) { midiInput.setExpression(expr); }

void SynthEngine::setMatrixSlot(int slotIndex, modSource_t src, modDest_t dest, modSource_t via, int16_t depth, bool enabled) {
    if (slotIndex >= 0 && slotIndex < MOD_MATRIX_SLOT_COUNT) {
        currentPreset.modMatrix[slotIndex].source = (uint8_t)src;
        currentPreset.modMatrix[slotIndex].dest = (uint8_t)dest;
        currentPreset.modMatrix[slotIndex].viaSource = (uint8_t)via;
        currentPreset.modMatrix[slotIndex].depth = (int16_t)std::clamp((int)depth, -100, 100);
        currentPreset.modMatrix[slotIndex].enabled = enabled;
    }
}

ModulationInputs SynthEngine::modulationInputs(int v) const {
    return ModulationInputs{
        voicePreset(v), partLfos[voiceLfoPart(v)][0], partLfos[voiceLfoPart(v)][1], voices[v], midiInput.voice(v), v,
        midiInput.getPitchBend(), midiInput.getModWheel(), midiInput.getPressure(), midiInput.getTimbre(),
        midiInput.getBreath(), midiInput.getExpression(),
        allocator.oscANote(v), allocator.oscBNote(v), allocator.filterNote(v)};
}

float SynthEngine::evaluateModSource(int8_t v, uint8_t src) const {
    if (v < 0 || v >= SYNTH_VOICE_COUNT) return 0.0f;
    return modulation::source(modulationInputs(v), src);
}

void SynthEngine::holdPedal(bool down) {
    assigner.holdEvent(down ? 1 : 0);
}

void SynthEngine::refreshOscWaves() {
    const auto& waves = parts[0].waves;
    const uint16_t* waveAMain = waves[abxAMain];
    const uint16_t* waveAXOvr = waves[abxACrossover];
    const uint16_t* waveBMain = waves[abxBMain];
    const uint16_t* waveBXOvr = waves[abxBCrossover];

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].setOscSampleData(waveAMain, waveAXOvr, waveBMain, waveBXOvr);
    }
}

void SynthEngine::allNotesOff() {
    arpeggiator.allNotesOff();
    assigner.allNotesOff();
    midiInput.releaseAll();
}

void SynthEngine::panic() {
    ++panicGeneration;
    reset();
    allocator.clearNoteCVs();
}

void SynthEngine::assignerEvent(uint8_t note, int8_t gate, int8_t voice, uint16_t velocity, uint8_t flags) {
    if (voice < 0 || voice >= SYNTH_VOICE_COUNT) return;

    if (gate) {
        const uint8_t channel = assigner.getVoiceChannel(voice);
        midiInput.noteStarted(voice, note, channel, velocity);

        // All modes are routing presets over the same sixteen-part engine.
        const uint8_t part = allocator.partForNewVoice(note, channel, currentPreset, noteMap);
        const PresetData& partPreset = parts[part].preset;
        allocator.setPart(voice, part);
        allocator.startNote(voice, note, partPreset);
        configureVoicePart(voice, part, velocity);

        voices[voice].gateOn(note, velocity, flags);
        updateSingleVoice(voice, false);

        // LFO retrigger
        if (partPreset.steppedParams[spLFOTrig]) partLfos[part][0].reset();
        if (partPreset.steppedParams[spLFO2Trig]) partLfos[part][1].reset();
    } else {
        voiceconfig::applyReleaseVelocity(voices[voice], voicePreset(voice), midiInput.voice(voice).noteOffVelocity);
        voices[voice].gateOff();
    }
}

void SynthEngine::applyPreset() {
    refreshOscWaves();
    applyControls();
}

void SynthEngine::applyControls() {
    // Envelope settings
    for (auto& voice : voices) voiceconfig::applyEnvelopes(voice, currentPreset);

    configurePartLfos(0);

    // Glide
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
        if (followsMainPart(v)) allocator.setGlide(v, currentPreset.continuousParams[cpGlide]);

    // Filter model & mode, then the Shelves bands (see VoiceConfig.h)
    for (auto& voice : voices) {
        voiceconfig::applyFilterModel(voice, currentPreset);
        voiceconfig::applyShelves(voice, currentPreset);
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

    applyMasterBusParameters();
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
            if (!followsMainPart(v) || !voices[v].isActive()) continue;
            allocator.retargetFilter(v, delta);
        }

        // In Shelves mode cpCutoff is also parametric band 1's centre.
        if (currentPreset.steppedParams[spFilterModel] == fmEQ)
            forEachMainPartVoice([this](Voice& voice) { voiceconfig::applyShelves(voice, currentPreset); });
        break;
    }

    case cpFilAtt:
    case cpFilDec:
    case cpFilSus:
    case cpFilRel:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeTimes(voice.getFilEnv(), currentPreset, voiceconfig::kFilterEnvelope);
        });
        break;

    case cpAmpAtt:
    case cpAmpDec:
    case cpAmpSus:
    case cpAmpRel:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeTimes(voice.getAmpEnv(), currentPreset, voiceconfig::kAmpEnvelope);
        });
        break;

    case cpWModAtt:
    case cpWModDec:
    case cpWModSus:
    case cpWModRel:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeTimes(voice.getWmodEnv(), currentPreset, voiceconfig::kWaveModEnvelope);
        });
        break;

    case cpLFOFreq:
    case cpLFOAmt:
    case cpLFO2Freq:
    case cpLFO2Amt:
        applyPartLfoAmounts(0);
        break;

    case cpGlide:
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
            if (followsMainPart(v)) allocator.setGlide(v, currentPreset.continuousParams[cpGlide]);
        break;

    case cpShelvesLsFreq:
    case cpShelvesLsGain:
    case cpShelvesP1Gain:
    case cpShelvesP2Freq:
    case cpShelvesP2Gain:
    case cpShelvesP2Q:
    case cpShelvesHsFreq:
    case cpShelvesHsGain:
        forEachMainPartVoice([this](Voice& voice) { voiceconfig::applyShelves(voice, currentPreset); });
        break;

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

    case cpConsoleDrive:
    case cpConsolePad:
    case cpConsoleDiscontinuity:
    case cpMackityDrive:
        applyMasterBusParameters();
        break;

    default:
        break;
    }
}

void SynthEngine::setSteppedParam(steppedParameter_t sp, uint8_t value) {
    if (sp < 0 || sp >= spCount) return;
    if (sp == spEngineMode && value >= emCount) value = emMultiChannel;

    const arpMode_t previousArpMode = arpeggiator.getMode();
    currentPreset.steppedParams[sp] = value;

    switch (sp) {
    case spFilterModel:
    case spFilterMode:
    case spSemModel:
        forEachMainPartVoice([this](Voice& voice) { voiceconfig::applyFilterModel(voice, currentPreset); });
        break;

    case spFilEnvLin:
    case spFilEnvLoop:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeShape(voice.getFilEnv(), currentPreset, voiceconfig::kFilterEnvelope);
        });
        break;

    case spFilEnvSlow:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeSpeed(voice.getFilEnv(), currentPreset, voiceconfig::kFilterEnvelope);
        });
        break;

    case spAmpEnvLin:
    case spAmpEnvLoop:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeShape(voice.getAmpEnv(), currentPreset, voiceconfig::kAmpEnvelope);
        });
        break;

    case spAmpEnvSlow:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeSpeed(voice.getAmpEnv(), currentPreset, voiceconfig::kAmpEnvelope);
        });
        break;

    case spWModEnvLin:
    case spWModEnvLoop:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeShape(voice.getWmodEnv(), currentPreset, voiceconfig::kWaveModEnvelope);
        });
        break;

    case spWModEnvSlow:
        forEachMainPartVoice([this](Voice& voice) {
            voiceconfig::applyEnvelopeSpeed(voice.getWmodEnv(), currentPreset, voiceconfig::kWaveModEnvelope);
        });
        break;

    case spLFOShape:
    case spLFOSpeed:
    case spLFO2Shape:
    case spLFO2Speed:
        applyPartLfoShapes(0);
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

    modulation::apply(voices[v], modulation::computeVoiceControls(modulationInputs(v)));
}

void SynthEngine::updateCVs() {
    if (++cvUpdatesSinceTick == controltimes::kCvUpdatesPerTick) {
        cvUpdatesSinceTick = 0;
        controlTickEvent();
    }

    for (int part = 0; part < 16; ++part) {
        if (!(lfoPartsRunning & (1u << part))) continue;
        partLfos[part][0].update();
        partLfos[part][1].update();
    }

    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!allocator.isGliding(v) && voices[v].isActive()) allocator.slewFilter(v);
        midiInput.smooth(v);
        updateSingleVoice(v, true);
    }
}

void SynthEngine::tickTimerEvent(uint8_t phase) {
    ++currentTick;
    arpeggiator.clock(currentTick);
}

// Constant-power compensation keeps stacked unison voices from overdriving
// the summing bus while retaining the perceived lift of unison.
float SynthEngine::unisonCompensation() const {
    if (currentPreset.steppedParams[spUnison] == 0) return 1.0f;
    int unisonVoices = 0;
    while (unisonVoices < SYNTH_VOICE_COUNT
           && currentPreset.voicePattern[unisonVoices] != ASSIGNER_NO_NOTE) ++unisonVoices;
    return unisonVoices > 1 ? 1.0f / std::sqrt(static_cast<float>(unisonVoices)) : 1.0f;
}

void SynthEngine::renderBlock(float* leftOut, float* rightOut, int numSamples, int hostOffset) {
    // Control rate (~4 kHz CV updates) and the 48 PPQ clock ticker.
    const float cvStep = (float)DACSPI_UPDATE_HZ / sampleRate;
    const float tickerHz = getEffectiveBpm() * 0.8f; // 48 ticks per quarter note
    const bool clockRunning = !hostSyncEnabled || !hostTransportAvailable || hostTransportPlaying;
    const float tickStepRate = clockRunning ? tickerHz / sampleRate : 0.0f;

    // Voice mixer settings are constant within a block: note events split blocks.
    const float unisonGain = unisonCompensation();
    float panLeft[SYNTH_VOICE_COUNT], panRight[SYNTH_VOICE_COUNT];
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        const float pan = getVoicePan(v);
        panLeft[v] = 0.5f * (1.0f - pan);
        panRight[v] = 0.5f * (1.0f + pan);
    }
    applyMasterBusParameters();

    for (int i = 0; i < numSamples;) {
        // Clock and control events of the segment's first sample. The event
        // handlers see the sample position for sample-accurate MIDI out.
        currentSampleOffset = hostOffset + i;
        cvSubSampleCounter += cvStep;
        if (cvSubSampleCounter >= 1.0f) {
            cvSubSampleCounter -= 1.0f;
            updateCVs();
        }
        tickSubSampleCounter += tickStepRate;
        if (tickSubSampleCounter >= 1.0f) {
            tickSubSampleCounter -= 1.0f;
            tickTimerEvent(0);
        }

        // The segment continues until the sample on which the next event
        // fires. The counters advance with the same additions per sample.
        int length = 1;
        while (i + length < numSamples && length < kMaxSegment) {
            const float nextCv = cvSubSampleCounter + cvStep;
            const float nextTick = tickSubSampleCounter + tickStepRate;
            if (nextCv >= 1.0f || nextTick >= 1.0f) break;
            cvSubSampleCounter = nextCv;
            tickSubSampleCounter = nextTick;
            ++length;
        }

        int rendered[SYNTH_VOICE_COUNT];
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
            rendered[v] = voices[v].process(voiceBuffer[v], length, tickStep);

        for (int s = 0; s < length; ++s) {
            for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
                if (s >= rendered[v]) continue;
                const float smp = voiceBuffer[v][s] * voiceFader[v] * unisonGain;
                voiceMeterPeaks[v] = std::max(voiceMeterPeaks[v], std::abs(smp));
                voiceLoadPeaks[v] = std::max(voiceLoadPeaks[v], bus.addVoice(smp, panLeft[v], panRight[v]));
            }
            bus.process(leftOut[i + s], rightOut[i + s]);
            outputPeaks[0] = std::max(outputPeaks[0], std::abs(leftOut[i + s]));
            outputPeaks[1] = std::max(outputPeaks[1], std::abs(rightOut[i + s]));
        }
        i += length;
    }
}

int32_t SynthEngine::getVoiceAmpLevel(int voiceIndex) {
    if (voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT && voices[voiceIndex].isActive())
        return voices[voiceIndex].getAmpEnv().getOutput();
    return 0;
}

// Meter values scaled by 65535 (up to 16x): the bus load is at most one per
// voice before the console's bus saturation.
static int32_t meterValue(float value) {
    return static_cast<int32_t>(std::round(std::clamp(value, 0.0f, 16.0f) * 65535.0f));
}

int32_t SynthEngine::getVoiceBusLoad(int voiceIndex) const {
    return voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT ? meterValue(voiceLoadPeaks[voiceIndex]) : 0;
}

int32_t SynthEngine::getOutputPeak(int channel) const {
    return channel == 0 || channel == 1 ? meterValue(outputPeaks[channel]) : 0;
}

int32_t SynthEngine::getVoicePeakLevel(int voiceIndex) const {
    if (voiceIndex >= 0 && voiceIndex < SYNTH_VOICE_COUNT)
        return static_cast<int32_t>(std::round(std::clamp(voiceMeterPeaks[voiceIndex], 0.0f, 16.0f) * 65535.0f));
    return 0;
}

void SynthEngine::configureVoicePart(int voice, uint8_t slotIdx, uint16_t velocity) {
    auto& slot = parts[slotIdx];
    voices[voice].setOscSampleData(slot.waves[abxAMain], slot.waves[abxACrossover],
                                   slot.waves[abxBMain], slot.waves[abxBCrossover]);
    voiceconfig::configureVoice(voices[voice], slot.preset, velocity);
    configurePartLfos(slotIdx);
    lfoPartsRunning |= static_cast<uint16_t>(1u << slotIdx);
}

void SynthEngine::controlTickEvent() {
    ++controlTick;
    allocator.glideTick();

    // Modulation delay (firmware refreshLfoSettings/refreshModulationDelay),
    // per part: wait N ticks after the first key press, then fade in the LFO
    // the modwheel does not control over N ticks along the attack curve.
    // The LFO amounts also follow modwheel, pressure and timbre here.
    for (int part = 0; part < 16; ++part) {
        if (!(lfoPartsRunning & (1u << part))) continue;
        bool held = false, sounding = false;
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            if (std::max<int>(0, allocator.part(v)) != part) continue;
            held |= voices[v].isGated();
            sounding |= voices[v].isActive();
        }
        if (!sounding) modDelayStart[part] = UINT32_MAX;
        if (held && !partKeyHeld[part]) modDelayStart[part] = controlTick;
        partKeyHeld[part] = held;

        const PresetData& p = parts[part].preset;
        if (controltimes::modDelayEnabled(p.continuousParams[cpModDelay])) {
            const uint32_t ticks = controltimes::modDelayTicks(p.continuousParams[cpModDelay]);
            uint16_t level = 0;
            if (ticks == 0) {
                level = UINT16_MAX;
            } else if (modDelayStart[part] != UINT32_MAX && controlTick >= modDelayStart[part] + ticks) {
                const uint32_t elapsed = controlTick - (modDelayStart[part] + ticks);
                level = elapsed >= ticks ? UINT16_MAX : attackCurveLookup[(elapsed << 8) / ticks];
            }
            modDelayLevel[part] = level;
        }
        applyPartLfoAmounts(part);
    }
}

// LFO frequencies and amounts of a part (modulation::lfoAmounts); the start
// delay only counts when it is enabled.
void SynthEngine::applyPartLfoAmounts(int part) {
    const PresetData& p = parts[part].preset;
    const uint16_t delay = controltimes::modDelayEnabled(p.continuousParams[cpModDelay]) ? modDelayLevel[part] : UINT16_MAX;
    const auto amounts = modulation::lfoAmounts(p, midiInput.getModWheel(), midiInput.getPressure(),
                                                midiInput.getTimbre(), delay);
    partLfos[part][0].setCVs(p.continuousParams[cpLFOFreq], amounts[0]);
    partLfos[part][1].setCVs(p.continuousParams[cpLFO2Freq], amounts[1]);
}

// LFO shapes and speed ranges of a part (x1 .. x8).
void SynthEngine::applyPartLfoShapes(int part) {
    const PresetData& p = parts[part].preset;
    auto& lfo = partLfos[part];
    lfo[0].setShape((lfoShape_t)p.steppedParams[spLFOShape]);
    lfo[0].setSpeedShift(p.steppedParams[spLFOSpeed]);
    lfo[1].setShape((lfoShape_t)p.steppedParams[spLFO2Shape]);
    lfo[1].setSpeedShift(p.steppedParams[spLFO2Speed]);
}

void SynthEngine::configurePartLfos(int part) {
    applyPartLfoShapes(part);
    applyPartLfoAmounts(part);
}
