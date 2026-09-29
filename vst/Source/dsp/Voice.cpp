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
    // Distinct noise per voice; identical seeds would give every voice of a
    // chord the same bow/blow noise.
    oscElements->setRandomSeed(0x9E3779B9u * static_cast<uint32_t>(vIdx + 1));
    oscElements->setSampleRate(48000.0f);
    oscElements->reset();

    filEnv.init();
    ampEnv.init();
    wmodEnv.init();

    filterSSI.reset();
    filterSem.reset();
    filterEQ.reset();
    filterSST.reset();
    vca.setSampleRate(48000.0f);
    vca.reset();
    gainA = 0.5f;
    gainB = 0.5f;
    gainNoise = 0.0f;
    noiseLfsr = 0x12345678 + vIdx * 0x10101010;
}

void Voice::setSampleRate(float baseRate, int newOversampling) {
    oversampling = std::clamp(newOversampling, 1, 2);
    const float sr = baseRate * static_cast<float>(oversampling);
    filterFadeStep = 1.0f / std::max(1.0f, sr * 0.010f);
    filterSSI.setSampleRate(sr);
    filterSem.setSampleRate(sr);
    filterEQ.setSampleRate(sr);
    filterSST.setSampleRate(sr);
    oscA.setSampleRate(sr);
    oscB.setSampleRate(sr);
    if (oscElements) {
        oscElements->setSampleRate(baseRate);
    }
    vca.setSampleRate(sr);
    dcBlockCoeff = std::exp(-2.0f * 3.14159265f * kDcBlockHz / std::max(1.0f, sr));
    // White noise at the higher rate spreads over twice the bandwidth.
    noiseScale = std::sqrt(static_cast<float>(oversampling));
    elementsUpsampler.reset();
    subsample = 0;
    updateFilterCV();
}

void Voice::setFilterModelAndMode(uint8_t model, uint8_t mode, uint8_t semVariant) {
    model = std::min<uint8_t>(model, 3);
    mode = std::min<uint8_t>(mode, 3);
    semVariant = std::min<uint8_t>(semVariant, SemFilter::VariantCount - 1);
    if (requestedFilter == model && requestedMode == mode && requestedVariant == semVariant) return;
    requestedFilter = model; requestedMode = mode; requestedVariant = semVariant;
    if (isActive()) filterFadingOut = true;
    else { commitFilter(); filterFade = 1.0f; filterFadingOut = false; }
}

void Voice::commitFilter() {
    filterModel = requestedFilter; filterMode = requestedMode; filterVariant = requestedVariant;
    switch (filterModel) {
        case fmSem: filterSem.setVariant(filterVariant); filterSem.reset(); filterSem.setMode(filterMode); break;
        case fmEQ: filterEQ.reset(); filterEQ.setMode(filterMode); break;
        case fmSST: filterSST.reset(); filterSST.setMode(filterMode); break;
        default: filterSSI.reset(); break;
    }
    updateFilterCV();
}

void Voice::updateFilterCV() {
    switch (filterModel) {
        case fmSem: filterSem.setCV(lastCutoff, lastResonance); break;
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
    gated = true;
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
    gated = false;
    wmodEnv.setGate(0);
    filEnv.setGate(0);
    ampEnv.setGate(0);

    if (oscElements) {
        oscElements->gateOff();
    }
}

void Voice::reset() {
    filterFade = 1.0f; filterFadingOut = false;
    filterModel = requestedFilter; filterMode = requestedMode; filterVariant = requestedVariant;
    active = false;
    gated = false;
    wmodEnv.reset();
    filEnv.reset();
    ampEnv.reset();
    filterSSI.reset();
    filterSem.setVariant(filterVariant);
    filterSem.reset();
    filterEQ.reset();
    filterSST.reset();
    vca.reset();
    dcBlockIn = dcBlockOut = 0.0f;
    elementsUpsampler.reset();
    elementsSecond = 0.0f;
    subsample = 0;
    filterSem.setMode(filterMode); filterEQ.setMode(filterMode); filterSST.setMode(filterMode);
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

void Voice::updateElementsParams(const ElementsControls& c) {
    if (!oscElements) return;

    oscElements->setModel(c.model);
    oscElements->setPitch(c.pitchMidiNote);
    oscElements->setGeometry(c.geometry);
    oscElements->setBrightness(c.brightness);
    oscElements->setDamping(c.damping);
    oscElements->setPosition(c.position);
    oscElements->setSpace(c.space);
    oscElements->setContour(c.contour);
    oscElements->setBowLevel(c.bow);
    oscElements->setBlowLevel(c.blow);
    oscElements->setStrikeLevel(c.strike);
    oscElements->setBlowMeta(c.flow);
    oscElements->setStrikeMeta(c.mallet);
    oscElements->setBowTimbre(c.bowTimbre);
    oscElements->setBlowTimbre(c.blowTimbre);
    oscElements->setStrikeTimbre(c.strikeTimbre);
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

int Voice::process(float* out, int count, uint32_t tickStep) {
    for (int i = 0; i < count; ++i) {
        if (!isActive()) return i;
        for (int k = 0; k < oversampling; ++k) out[i * oversampling + k] = processSample(tickStep);
    }
    return count;
}

float Voice::processSample(uint32_t tickStep) {
    const bool firstSubsample = subsample == 0;
    subsample = (subsample + 1) % oversampling;
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
        // Elements runs at the base rate: one sample per base-rate sample,
        // interpolated to the voice's rate. In hybrid mode, Osc A physically
        // excites the Elements modal resonator (its first sample of the pair).
        if (firstSubsample) {
            float exciterIn = (oscEngine == oeHybrid) ? (sA * 0.5f) : 0.0f;
            sElements = oscElements->processSample(exciterIn);
            if (oversampling == 2) elementsUpsampler.process(sElements, sElements, elementsSecond);
        } else {
            sElements = elementsSecond;
        }
    }

    noiseLfsr = lfsr(noiseLfsr, 1);
    float sNoise = ((float)(int16_t)(noiseLfsr & 0xFFFF)) * (noiseScale / 32768.0f);

    float mixed = 0.0f;
    if (oscEngine == oeWavetable) {
        mixed = (sA * gainA) + (sB * gainB) + (sNoise * gainNoise);
    } else if (oscEngine == oeElements) {
        mixed = (sElements * gainA) + (sNoise * gainNoise);
    } else { // oeHybrid
        mixed = (sA * gainA * 0.5f) + (sB * gainB * 0.5f) + (sElements * gainA * 0.7f) + (sNoise * gainNoise);
    }

    float filtered = 0.0f;
    // Keep nonlinear cores in a comparable nominal input range and restore the
    // linear gain after filtering, corrected by the measured filter gain.
    const float filterInput = mixed * kFilterInputPad;
    switch (filterModel) {
        case fmSem:
            filtered = filterSem.processSample(filterInput);
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
    filtered *= kFilterMakeup * (filterModel == fmSem ? semGains[filterVariant]
                                                      : filterGains[std::min<int>(filterModel, 3)]);
    if (filterFadingOut) {
        filterFade = std::max(0.0f, filterFade - filterFadeStep);
    } else filterFade = std::min(1.0f, filterFade + filterFadeStep);
    const float smoothFade = filterFade * filterFade * (3.0f - 2.0f * filterFade);
    // Coupling capacitor before the VCA (see kDcBlockHz).
    const float coupled = filtered - dcBlockIn + dcBlockCoeff * dcBlockOut;
    dcBlockIn = filtered;
    dcBlockOut = coupled;
    float out = vca.processSample(coupled * smoothFade);
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
