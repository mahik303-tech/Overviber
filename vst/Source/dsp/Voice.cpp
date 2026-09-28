#include "Voice.h"
#include "audible/ElementsOsc.h"

Voice::Voice() {
    init(0);
}

Voice::~Voice() = default;

void Voice::init(int8_t vIdx) {
    voiceIndex = vIdx;
    currentNote = 60;
    active = false;
    syncEnabled = false;
    syncPosition = INT16_MIN;
    oscEngine = oeWavetable;

    oscA.init(vIdx * 2);
    oscB.init(vIdx * 2 + 1);
    oscA.setSampleRate(48000.0f);
    oscB.setSampleRate(48000.0f);

    if (!oscElements) {
        oscElements = std::make_unique<ElementsOsc>();
    }
    oscElements->setSampleRate(48000.0f);
    oscElements->reset();

    filEnv.init();
    ampEnv.init();
    wmodEnv.init();

    filterSSI.reset();
    filterLiquid.reset();
    filterEQ.reset();
    filterSST.reset();
    vca.setSampleRate(48000.0f);
    vca.reset();
    gainA = 0.5f;
    gainB = 0.5f;
    gainNoise = 0.0f;
    noiseLfsr = 0x12345678 + vIdx * 0x10101010;
}

void Voice::setSampleRate(float sr) {
    filterFadeStep = 1.0f / std::max(1.0f, sr * 0.010f);
    filterSSI.setSampleRate(sr);
    filterLiquid.setSampleRate(sr);
    filterEQ.setSampleRate(sr);
    filterSST.setSampleRate(sr);
    oscA.setSampleRate(sr);
    oscB.setSampleRate(sr);
    if (oscElements) {
        oscElements->setSampleRate(sr);
    }
    vca.setSampleRate(sr);
    updateFilterCV();
}

void Voice::setFilterModelAndMode(uint8_t model, uint8_t mode) {
    model = std::min<uint8_t>(model, 3);
    mode = std::min<uint8_t>(mode, 3);
    if (requestedFilter == model && requestedMode == mode) return;
    requestedFilter = model; requestedMode = mode;
    if (isActive()) filterFadingOut = true;
    else { commitFilter(); filterFade = 1.0f; filterFadingOut = false; }
}

void Voice::commitFilter() {
    filterModel = requestedFilter; filterMode = requestedMode;
    switch (filterModel) {
        case fmLiquid: filterLiquid.reset(); filterLiquid.setMode(filterMode); break;
        case fmEQ: filterEQ.reset(); filterEQ.setMode(filterMode); break;
        case fmSST: filterSST.reset(); filterSST.setMode(filterMode); break;
        default: filterSSI.reset(); break;
    }
    updateFilterCV();
}

void Voice::updateFilterCV() {
    switch (filterModel) {
        case fmLiquid: filterLiquid.setCV(lastCutoff, lastResonance); break;
        case fmEQ: filterEQ.setCV(lastCutoff, lastResonance); break;
        case fmSST: filterSST.setCV(lastCutoff, lastResonance); break;
        default: filterSSI.setCV(lastCutoff, lastResonance); break;
    }
}

void Voice::setOscSampleData(const uint16_t* aMain, const uint16_t* aXovr, const uint16_t* bMain, const uint16_t* bXovr) {
    oscA.setSampleData(aMain, aXovr);
    oscB.setSampleData(bMain, bXovr);
}

void Voice::gateOn(uint8_t note, uint16_t velocity, uint8_t flags) {
    if (!active) {
        vca.reset();
    }
    currentNote = note;
    active = true;
    syncPosition = INT16_MIN;

    if (!(flags & ASSIGNER_EVENT_FLAG_LEGATO)) {
        wmodEnv.setGate(1);
        filEnv.setGate(1);
        ampEnv.setGate(1);
    }

    if (oscElements) {
        float strength = std::clamp((float)velocity / 65535.0f, 0.05f, 1.0f);
        oscElements->gateOn(strength);
    }
}

void Voice::gateOff() {
    wmodEnv.setGate(0);
    filEnv.setGate(0);
    ampEnv.setGate(0);

    if (oscElements) {
        oscElements->gateOff();
    }
}

void Voice::reset() {
    filterFade = 1.0f; filterFadingOut = false;
    filterModel = requestedFilter; filterMode = requestedMode;
    active = false;
    wmodEnv.reset();
    filEnv.reset();
    ampEnv.reset();
    filterSSI.reset();
    filterLiquid.reset();
    filterEQ.reset();
    filterSST.reset();
    vca.reset();
    filterLiquid.setMode(filterMode); filterEQ.setMode(filterMode); filterSST.setMode(filterMode);
    updateFilterCV();
    syncPosition = INT16_MIN;

    if (oscElements) {
        oscElements->reset();
    }
}

bool Voice::isActive() const {
    // The envelope may finish before the smoothed analog VCA. Rendering must
    // continue until that gain has decayed, otherwise a zero release cuts audio.
    bool baseActive = active && (ampEnv.getStage() != sWait || vca.getCurrentGain() > 0.00001f);
    if ((oscEngine == oeElements || oscEngine == oeHybrid) && oscElements) {
        return baseActive || oscElements->isActive();
    }
    return baseActive;
}

void Voice::updateEnvelopes() {
    wmodEnv.update();
    filEnv.update();
    ampEnv.update();

    if (ampEnv.getStage() == sWait && active) {
        vca.setCV(0);
        if (vca.getCurrentGain() <= 0.00001f
            && (oscEngine == oeWavetable || (oscElements && !oscElements->isActive()))) {
            active = false;
        }
    }
}

void Voice::updateElementsParams(uint8_t model,
                                 float geometry, float brightness, float damping,
                                 float position, float space,
                                 float bow, float blow, float strike, float mallet,
                                 float pitchMidiNote) {
    if (!oscElements) return;

    oscElements->setModel(model);
    oscElements->setPitch(pitchMidiNote);
    oscElements->setGeometry(geometry);
    oscElements->setBrightness(brightness);
    oscElements->setDamping(damping);
    oscElements->setPosition(position);
    oscElements->setSpace(space);
    oscElements->setBowLevel(bow);
    oscElements->setBlowLevel(blow);
    oscElements->setStrikeLevel(strike);
    oscElements->setContour(mallet);
}

void Voice::updateVoiceCVs(uint16_t pitchA, uint16_t pitchB,
                           oscWModTarget_t wmodTypeA, uint16_t wmodA,
                           oscWModTarget_t wmodTypeB, uint16_t wmodB,
                           uint16_t cutoffCV, uint16_t resonanceCV,
                           uint16_t ampCV,
                           float oscAGain, float oscBGain, float noiseGain,
                           bool hardSync) {
    oscA.setParameters(pitchA, wmodTypeA, wmodA);
    oscB.setParameters(pitchB, wmodTypeB, wmodB);

    if (cutoffCV != lastCutoff || resonanceCV != lastResonance) {
        lastCutoff = cutoffCV; lastResonance = resonanceCV;
        updateFilterCV();
    }
    vca.setCV(ampCV);

    gainA = oscAGain;
    gainB = oscBGain;
    gainNoise = noiseGain;
    syncEnabled = hardSync;
}

float Voice::processSample(uint32_t tickStep) {
    if (!isActive()) {
        return 0.0f;
    }

    float sA = 0.0f;
    float sB = 0.0f;
    float sElements = 0.0f;

    if (oscEngine == oeWavetable || oscEngine == oeHybrid) {
        oscSyncMode_t syncModeMaster = syncEnabled ? osmMaster : osmNone;
        oscSyncMode_t syncModeSlave = syncEnabled ? osmSlave : osmNone;

        uint16_t rawA = oscA.processSample(tickStep, syncModeMaster, &syncPosition);
        uint16_t rawB = oscB.processSample(tickStep, syncModeSlave, &syncPosition);

        sA = ((float)rawA - 32768.0f) * (1.0f / 32768.0f);
        sB = ((float)rawB - 32768.0f) * (1.0f / 32768.0f);
    }

    if ((oscEngine == oeElements || oscEngine == oeHybrid) && oscElements) {
        // In hybrid mode, Osc A physically excites the Elements modal resonator!
        float exciterIn = (oscEngine == oeHybrid) ? (sA * 0.5f) : 0.0f;
        sElements = oscElements->processSample(exciterIn);
    }

    noiseLfsr = lfsr(noiseLfsr, 1);
    float sNoise = ((float)(int16_t)(noiseLfsr & 0xFFFF)) * (1.0f / 32768.0f);

    float mixed = 0.0f;
    if (oscEngine == oeWavetable) {
        mixed = (sA * gainA) + (sB * gainB) + (sNoise * gainNoise);
    } else if (oscEngine == oeElements) {
        mixed = (sElements * gainA) + (sNoise * gainNoise);
    } else { // oeHybrid
        mixed = (sA * gainA * 0.5f) + (sB * gainB * 0.5f) + (sElements * gainA * 0.7f) + (sNoise * gainNoise);
    }

    float filtered = 0.0f;
    // Keep nonlinear cores in a comparable nominal input range. Restore the
    // linear gain after filtering; legacy mode retains its original drive.
    const float filterInput = mixed * (calibratedGain ? 0.25f : 1.0f);
    switch (filterModel) {
        case fmLiquid:
            filtered = filterLiquid.processSample(filterInput);
            break;
        case fmEQ:
            filtered = filterEQ.processSample(filterInput);
            break;
        case fmSST:
            filtered = filterSST.processSample(filterInput);
            break;
        case fmSSI2144:
        default:
            filtered = filterSSI.processSample(filterInput);
            break;
    }
    if (calibratedGain) filtered *= 4.0f * filterGains[std::min<int>(filterModel, 3)];
    if (filterFadingOut) {
        filterFade = std::max(0.0f, filterFade - filterFadeStep);
    } else filterFade = std::min(1.0f, filterFade + filterFadeStep);
    const float smoothFade = filterFade * filterFade * (3.0f - 2.0f * filterFade);
    float out = vca.processSample(filtered * smoothFade);
    if (filterFadingOut && filterFade == 0.0f) {
        commitFilter(); filterFadingOut = false;
    }
#ifdef OVERVIBER_DIAGNOSTICS
    if (diagnostics) {
        diagnostics->oscillator.add(oscEngine == oeWavetable ? sA : sElements);
        diagnostics->mixed.add(mixed);
        diagnostics->filtered.add(filtered);
        diagnostics->vca.add(out);
    }
#endif

    return out;
}
