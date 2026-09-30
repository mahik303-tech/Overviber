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
        retireVoices();
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
    cvIncrement = static_cast<uint32_t>(std::llround((double)DACSPI_UPDATE_HZ / sampleRate * 4294967296.0));

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) voices[v].init(v);


    assigner.init();
    assigner.setCallback([this](uint8_t note, int8_t gate, int8_t voice, uint16_t velocity, uint8_t flags) {
        assignerEvent(note, gate, voice, velocity, flags);
    });

    arpeggiator.init();
    arpeggiator.setNoteAssignCallback([this](uint8_t note, int8_t gate, uint16_t velocity, uint8_t channel) {
        allocator.assign(assigner, note, gate, velocity, 0, ++noteSerial, channel, isMpeMemberChannel(channel));

        // Queue MIDI Output for DAW live capture (e.g., in Ableton Live)
        uint8_t vel7 = (uint8_t)(((uint32_t)velocity * 127U) / 65535U);
        if (gate && vel7 == 0) vel7 = 1;
        if (!pendingMidiOut.push_back({ note, vel7, channel, gate != 0, currentSampleOffset })) midiOverflow = true;
    });

    currentPreset.setDefaults();
}

void SynthEngine::prepare(float sr) {
    sampleRate = std::max(22050.0f, sr);
    oversampling = sampleRate < 100000.0f ? 2 : 1;
    const float voiceRate = sampleRate * static_cast<float>(oversampling);
    tickStep = (uint32_t)(SYNTH_MASTER_CLOCK / voiceRate);
    cvIncrement = static_cast<uint32_t>(std::llround((double)DACSPI_UPDATE_HZ / sampleRate * 4294967296.0));
    const auto filterGains = calibrateFilters(voiceRate);
    const auto semGains = calibrateSemFilters(voiceRate);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voices[v].setSampleRate(sampleRate, oversampling);
        voices[v].filterGains = filterGains;
        voices[v].semGains = semGains;
    }
    bus.prepare(sampleRate, oversampling);
    mixSmoothing = 1.0f - std::exp(-1.0f / (kMixSmoothingSeconds * voiceRate));

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

// Called at each host block start. While the transport plays, a position
// that continues the running clock only corrects it (the arp keeps what it
// played, nothing is skipped or doubled at the block boundary); a start or
// a jump relocates the arp, so a step exactly on the start position plays.
// A stop ends the sounding step. While stopped the host position is ignored:
// the clock runs on at the host tempo (renderBlock), so held keys keep the
// arp going and a key pressed while stopped starts it at once.
void SynthEngine::setHostTransport(double ppqPosition, bool playing) {
    if (!std::isfinite(ppqPosition)) return;
    const bool stoppedNow = hostSyncEnabled && !playing
        && (!hostTransportAvailable || hostTransportPlaying);
    hostTransportAvailable = true;
    hostTransportPlaying = playing;
    if (!hostSyncEnabled) return;

    if (stoppedNow) arpeggiator.stopClock();
    if (!playing) {
        clockLocked = false;
        return;
    }

    const double position = std::max(0.0, ppqPosition) * 48.0;
    if (!clockLocked || std::abs(position - clockPosition) > kClockLockTolerance) {
        arpeggiator.relocate(position);
        clockLocked = true;
    }
    clockPosition = position;
    currentTick = static_cast<uint32_t>(position);
}

void SynthEngine::reset() {
    retireVoices();
    midiInput.reset();
}

// Voices, per-note expression, bus, assigner and arp. The channel-wide
// controllers (mod wheel, bend, pressure, timbre, breath, expression) keep
// their values: a hardware controller keeps its position across a preset
// change and sends nothing new.
void SynthEngine::retireVoices() {
    for (auto& voice : voices) voice.reset();
    mixIdle.fill(true);
    midiInput.resetNotes();
    bus.reset();
    assigner.panicOff();
    arpeggiator.init();
    modDelayStart.fill(UINT32_MAX);
    partKeyHeld.fill(false);
    beginVoiceMeterBlock();
}

void SynthEngine::noteOn(uint8_t note, uint16_t velocity, uint8_t channel) {
    if (arpeggiator.getMode() != amOff) arpeggiator.assignNote(note, 1, velocity, channel);
    else allocator.assign(assigner, note, 1, velocity, 1, ++noteSerial, channel, isMpeMemberChannel(channel));
}

void SynthEngine::noteOff(uint8_t note, uint16_t velocity, uint8_t channel) {
    if (arpeggiator.getMode() != amOff) {
        arpeggiator.assignNote(note, 0, velocity, channel);
    } else {
        // Record release velocity (lift) on matching voice
        for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v)
            if (assigner.voiceMatches(v, note, channel)) midiInput.setReleaseVelocity(v, velocity);
        assigner.assignNote(note, 0, velocity, 1, ++noteSerial, channel);
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
        restartVoiceGrid(voice);

        // LFO retrigger
        if (partPreset.steppedParams[spLFOTrig]) partLfos[part][0].reset();
        if (partPreset.steppedParams[spLFO2Trig]) partLfos[part][1].reset();
    } else {
        voiceconfig::applyReleaseVelocity(voices[voice], voicePreset(voice), midiInput.voice(voice).noteOffVelocity);
        voices[voice].gateOff();
        restartVoiceGrid(voice);
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
    arpeggiator.setGateLength(arpGateFraction(currentPreset.continuousParams[cpArpGate]));
    arpeggiator.setSwing(arpSwingFraction(currentPreset.continuousParams[cpArpSwing]));

    // Internal BPM and MIDI / Host Tempo Sync
    setInternalBpm(arpInternalBpm(currentPreset.continuousParams[cpArpBpm]));
    setHostSyncEnabled(currentPreset.steppedParams[spArpSync] != 0);

    // Voice count, priority and pattern
    assigner.setVoiceMask(voiceMask());
    assigner.setPriority((assignerPriority_t)currentPreset.steppedParams[spAssignerPriority]);
    assigner.setPattern(currentPreset.voicePattern, currentPreset.steppedParams[spUnison]);

    applyMasterBusParameters();
}

void SynthEngine::setContinuousParam(continuousParameter_t cp, uint16_t value) {
    if (cp < 0 || cp >= cpCount) return;

    const uint16_t previousValue = currentPreset.continuousParams[cp];
    currentPreset.continuousParams[cp] = value;

    // Targeted updates of the voices following the main part, without
    // reloading waves or rebuilding the whole synth.
    if (const auto* env = voiceconfig::envelopeWithTime(cp)) {
        forEachMainPartVoice([this, env](Voice& voice) {
            voiceconfig::applyEnvelopeTimes((voice.*env->envelope)(), currentPreset, *env);
        });
        return;
    }

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

    case cpArpGate: arpeggiator.setGateLength(arpGateFraction(value)); break;
    case cpArpSwing: arpeggiator.setSwing(arpSwingFraction(value)); break;
    case cpArpBpm: setInternalBpm(arpInternalBpm(value)); break;

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

    if (const auto* env = voiceconfig::envelopeWithShape(sp)) {
        forEachMainPartVoice([this, env](Voice& voice) {
            voiceconfig::applyEnvelopeShape((voice.*env->envelope)(), currentPreset, *env);
        });
        return;
    }
    if (const auto* env = voiceconfig::envelopeWithSpeed(sp)) {
        forEachMainPartVoice([this, env](Voice& voice) {
            voiceconfig::applyEnvelopeSpeed((voice.*env->envelope)(), currentPreset, *env);
        });
        return;
    }

    switch (sp) {
    case spFilterModel:
    case spFilterMode:
    case spSemModel:
        forEachMainPartVoice([this](Voice& voice) { voiceconfig::applyFilterModel(voice, currentPreset); });
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

    case spVoiceCount:
        assigner.setVoiceMask(voiceMask());
        break;
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

// Voices 1 .. spVoiceCount + 1 take notes.
uint8_t SynthEngine::voiceMask() const {
    const int count = std::clamp(currentPreset.steppedParams[spVoiceCount] + 1, 1, SYNTH_VOICE_COUNT);
    return (uint8_t)((1 << count) - 1);
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

// The global control grid: the 500 Hz control tick and the LFOs.
void SynthEngine::updateCVs() {
    if (++cvUpdatesSinceTick == controltimes::kCvUpdatesPerTick) {
        cvUpdatesSinceTick = 0;
        controlTickEvent();
    }
    allocator.glideStep();
    midiInput.smoothControllers();

    for (int part = 0; part < 16; ++part) {
        if (!(lfoPartsRunning & (1u << part))) continue;
        partLfos[part][0].update();
        partLfos[part][1].update();
    }
}

// A voice's control grid: its envelopes, filter slew, per-note expression
// and modulation.
void SynthEngine::updateVoiceCVs(int v) {
    if (!allocator.isGliding(v) && voices[v].isActive()) allocator.slewFilter(v);
    midiInput.smooth(v);
    updateSingleVoice(static_cast<int8_t>(v), true);
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
    // The arp clock (48 ticks per quarter note, see clockPosition) and the
    // control grids (updateCVs, updateVoiceCVs). The clock follows a playing
    // host transport; otherwise it runs free on the internal tempo, or on the
    // host tempo while synced to a stopped (or absent) transport.
    const bool freeRunning = !hostSyncEnabled || !hostTransportAvailable || !hostTransportPlaying;
    const double ticksPerSample = static_cast<double>(getEffectiveBpm()) * 0.8 / sampleRate;
    const double blockPosition = clockPosition;
    const double halfSample = 0.5 * ticksPerSample;   // arp events on their nearest sample
    arpeggiator.setFreeRunning(freeRunning);
    // Samples from a phase (after this sample's step) to its next wrap.
    const auto samplesToWrap = [this](uint32_t phase) {
        return static_cast<int>(std::min<uint64_t>(kMaxSegment,
            ((1ull << 32) - phase + cvIncrement - 1) / cvIncrement));
    };

    // Voice mixer targets of this block; the mix glides to them (mixSmoothing).
    // Constant-power pan law, the centre at 0.5 per side as before: a voice
    // panned hard sits 3 dB lower than with the former linear law (1.0), so
    // the pan no longer changes its loudness.
    const float unisonGain = unisonCompensation();
    float gainTarget[SYNTH_VOICE_COUNT], leftTarget[SYNTH_VOICE_COUNT], rightTarget[SYNTH_VOICE_COUNT];
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        const float angle = (getVoicePan(v) + 1.0f) * 0.25f * 3.14159265f;
        gainTarget[v] = voiceFader[v] * unisonGain;
        leftTarget[v] = 0.70710678f * std::cos(angle);
        rightTarget[v] = 0.70710678f * std::sin(angle);
    }
    applyMasterBusParameters();

    for (int i = 0; i < numSamples;) {
        // Events of the segment's first sample: the arp (it may start and end
        // notes), the global control grid, then the voices' grids. The event
        // handlers see the sample position for sample-accurate MIDI out.
        currentSampleOffset = hostOffset + i;
        const double position = blockPosition + i * ticksPerSample;
        currentTick = static_cast<uint32_t>(position);
        arpeggiator.advance(position, halfSample);
        if ((cvPhase += cvIncrement) < cvIncrement) updateCVs();
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
            if ((voiceCvPhase[v] += cvIncrement) < cvIncrement) updateVoiceCVs(v);

        // The segment continues until the sample of the next event.
        int length = std::min(numSamples - i, samplesToWrap(cvPhase));
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) length = std::min(length, samplesToWrap(voiceCvPhase[v]));
        const double samples = std::ceil((arpeggiator.nextEvent() - Arpeggiator::kTimeTolerance - halfSample
                                          - blockPosition) / ticksPerSample) - i;
        if (samples < length) length = std::max(1, static_cast<int>(samples));
        const uint32_t skipped = static_cast<uint32_t>(length - 1) * cvIncrement;
        cvPhase += skipped;
        for (auto& phase : voiceCvPhase) phase += skipped;

        int rendered[SYNTH_VOICE_COUNT];
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            rendered[v] = voices[v].process(voiceBuffer[v], length, tickStep);
            // A voice that starts takes the mix settings at once: a note
            // starts where its fader and pan are.
            if (mixIdle[v] && rendered[v] > 0) {
                mixGain[v] = gainTarget[v];
                mixLeft[v] = leftTarget[v];
                mixRight[v] = rightTarget[v];
            }
            mixIdle[v] = rendered[v] < length;
        }

        for (int s = 0; s < length; ++s) {
            for (int k = 0; k < oversampling; ++k) {
                for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
                    if (s >= rendered[v]) continue;
                    mixGain[v] += (gainTarget[v] - mixGain[v]) * mixSmoothing;
                    mixLeft[v] += (leftTarget[v] - mixLeft[v]) * mixSmoothing;
                    mixRight[v] += (rightTarget[v] - mixRight[v]) * mixSmoothing;
                    const float smp = voiceBuffer[v][s * oversampling + k] * mixGain[v];
                    voiceMeterPeaks[v] = std::max(voiceMeterPeaks[v], std::abs(smp));
                    voiceLoadPeaks[v] = std::max(voiceLoadPeaks[v], bus.addVoice(smp, mixLeft[v], mixRight[v]));
                }
                bus.endSubsample();
            }
            bus.process(leftOut[i + s], rightOut[i + s]);
            outputPeaks[0] = std::max(outputPeaks[0], std::abs(leftOut[i + s]));
            outputPeaks[1] = std::max(outputPeaks[1], std::abs(rightOut[i + s]));
        }
        i += length;
    }
    clockPosition = blockPosition + numSamples * ticksPerSample;
    currentTick = static_cast<uint32_t>(clockPosition);
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
