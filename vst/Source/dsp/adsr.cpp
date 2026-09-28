#include "adsr.h"

AdsrEnv::AdsrEnv() {
    init();
}

void AdsrEnv::init() {
    stageIncrement = 0;
    phase = 0;
    attackIncrement = decayIncrement = releaseIncrement = 0;
    sustainCV = levelCV = 0;
    attackCV = decayCV = releaseCV = 0;
    stageLevel = stageAdd = stageMul = 0;
    output = 0;
    expOutput = 0;
    gate = 0;
    loop = 0;
    speedShift = 0;
    stage = sWait;
}

void AdsrEnv::reset() {
    gate = 0;
    output = 0;
    phase = 0;
    stageLevel = 0;
    stage = sWait;
    updateStageVars(sWait);
}

uint32_t AdsrEnv::getPhaseInc(uint8_t v) {
    uint32_t r = 0;
    r |= (uint32_t)phaseLookupLo[v];
    r |= (uint32_t)phaseLookupMid[v] << 8;
    r |= (uint32_t)phaseLookupHi[v] << 16;
    return r;
}

void AdsrEnv::updateStageVars(adsrStage_t s) {
    switch (s) {
    case sAttack:
        stageAdd = scaleU16U16(stageLevel, levelCV);
        stageMul = scaleU16U16(UINT16_MAX - stageLevel, levelCV);
        stageIncrement = attackIncrement;
        break;
    case sDecay:
        stageAdd = scaleU16U16(sustainCV, levelCV);
        stageMul = scaleU16U16(UINT16_MAX - sustainCV, levelCV);
        stageIncrement = decayIncrement;
        break;
    case sSustain:
        stageAdd = 0;
        stageMul = levelCV;
        stageIncrement = 0;
        break;
    case sRelease:
        stageAdd = 0;
        stageMul = scaleU16U16(stageLevel, levelCV);
        stageIncrement = releaseIncrement;
        break;
    default:
        stageAdd = 0;
        stageMul = 0;
        stageIncrement = 0;
        break;
    }
}

void AdsrEnv::updateIncrements() {
    uint32_t aInc = getPhaseInc((uint8_t)(attackCV >> 8)) >> speedShift;
    uint32_t dInc = getPhaseInc((uint8_t)(decayCV >> 8)) >> speedShift;
    uint32_t rInc = getPhaseInc((uint8_t)(releaseCV >> 8)) >> speedShift;

    attackIncrement = aInc << 4;
    decayIncrement = dInc << 4;
    releaseIncrement = rInc << 4;

    updateStageVars(stage);
}

uint16_t AdsrEnv::computeOutput(uint32_t ph, const uint16_t lookup[], int8_t isExp) {
    if (isExp) {
        return computeShape(ph, lookup, 2);
    } else {
        return (uint16_t)(ph >> 8);
    }
}

void AdsrEnv::handlePhaseOverflow() {
    phase = 0;
    stageIncrement = 0;

    stage = (adsrStage_t)((int)stage + 1);

    switch (stage) {
    case sDecay:
        output = levelCV;
        updateStageVars(sDecay);
        return;
    case sSustain:
        if (loop) {
            stageLevel = sustainCV;
            stage = sAttack;
            updateStageVars(sAttack);
        } else {
            updateStageVars(sSustain);
        }
        return;
    case sDone:
        stage = sWait;
        output = 0;
        return;
    default:
        break;
    }
}

void AdsrEnv::setCVs(uint16_t atk, uint16_t dec, uint16_t sus, uint16_t rls, uint16_t lvl, uint8_t mask) {
    int8_t m = mask & 0x80;

    if ((mask & 0x01) && attackCV != atk) { m = 1; attackCV = atk; }
    if ((mask & 0x02) && decayCV != dec) { m = 1; decayCV = dec; }
    if ((mask & 0x04) && sustainCV != sus) { m = 1; sustainCV = sus; }
    if ((mask & 0x08) && releaseCV != rls) { m = 1; releaseCV = rls; }
    if ((mask & 0x10) && levelCV != lvl) { m = 1; levelCV = lvl; }

    if (m) updateIncrements();
}

void AdsrEnv::setGate(int8_t g) {
    phase = 0;
    if (levelCV > 0) {
        stageLevel = (uint16_t)(((uint32_t)output << 16) / levelCV);
    } else {
        stageLevel = 0;
    }

    if (g) {
        stage = sAttack;
        updateStageVars(sAttack);
    } else {
        stage = sRelease;
        updateStageVars(sRelease);
    }

    gate = g;
}

void AdsrEnv::setShape(int8_t isExp, int8_t isLoop) {
    expOutput = isExp;
    loop = isLoop;

    if (loop && stage == sSustain) {
        stage = sDecay;
        handlePhaseOverflow();
    }
}

void AdsrEnv::setSpeedShift(int8_t shift) {
    speedShift = shift;
    updateIncrements();
}

void AdsrEnv::update() {
    if (phase >> 24) {
        handlePhaseOverflow();
    }

    uint16_t o = 0;
    switch (stage) {
    case sAttack:
        o = computeOutput(phase, attackCurveLookup, expOutput);
        break;
    case sDecay:
    case sRelease:
        o = UINT16_MAX - computeOutput(phase, decayCurveLookup, expOutput);
        break;
    case sSustain:
        o = sustainCV;
        break;
    default:
        break;
    }

    output = scaleU16U16(o, stageMul) + stageAdd;
    phase += stageIncrement;
}
