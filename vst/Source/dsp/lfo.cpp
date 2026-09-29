#include "lfo.h"

LfoModule::LfoModule() {
    init();
}

void LfoModule::init() {
    noise = 0x5a5a5a5a;
    halfPeriodCounter = 0;
    phase = 0;
    speed = 0;
    increment = 0;
    levelCV = 0;
    bpmCV = 0;
    output = 0;
    speedShift = 0;
    shape = lsTri;
    halfPeriodLimit = 0;
}

void LfoModule::reset() {
    halfPeriodCounter = 0;
    phase = 0;
    updateIncrement();
}

void LfoModule::updateIncrement() {
    increment = speed * (1 - (int32_t)(halfPeriodCounter & 1) * 2);
}

void LfoModule::updateSpeed() {
    speed = lfoSpeed(scan_potFrom16bits(bpmCV), speedShift);
}

void LfoModule::handlePhaseOverflow() {
    ++halfPeriodCounter;
    phase = (halfPeriodCounter & 1) ? 0x00ffffff : 0;
    updateIncrement();
}

void LfoModule::setCVs(uint16_t bpm, uint16_t lvl) {
    levelCV = lvl;
    if (bpm != bpmCV) {
        bpmCV = bpm;
        updateSpeed();
        updateIncrement();
    }
}

void LfoModule::setShape(lfoShape_t shp, uint8_t halfPeriods) {
    shape = shp;
    halfPeriodLimit = halfPeriods;
}

void LfoModule::setSpeedShift(int8_t shift) {
    if (shift != speedShift) {
        speedShift = shift;
        updateSpeed();
        updateIncrement();
    }
}

void LfoModule::update() {
    int16_t rawOutput;

    if (phase >> 24) {
        handlePhaseOverflow();
    }

    switch (shape) {
    case lsPulse:
        rawOutput = INT16_MAX;
        break;
    case lsTri:
        rawOutput = (int16_t)std::abs((int16_t)(phase >> 8));
        break;
    case lsRand:
        if (!phase) {
            noise = (noise * 1664525u + 1013904223u);
        }
        rawOutput = (int16_t)((noise & UINT16_MAX) + INT16_MIN);
        break;
    case lsSine:
        rawOutput = computeShape(phase, sineShape, 2);
        break;
    case lsNoise:
        noise = lfsr(noise, (bpmCV >> 12) + 1);
        rawOutput = (int16_t)((noise & UINT16_MAX) + INT16_MIN);
        break;
    case lsSaw:
        rawOutput = (int16_t)(INT16_MIN + (phase >> 9));
        break;
    case lsRevSaw:
        rawOutput = (int16_t)(INT16_MAX - (phase >> 9));
        break;
    default:
        rawOutput = 0;
        break;
    }

    if (halfPeriodCounter & 1) {
        rawOutput = -rawOutput;
    }

    if (!bpmCV) {
        output = levelCV >> 1;
    } else if (!halfPeriodLimit || halfPeriodCounter < halfPeriodLimit) {
        phase += increment;
        output = scaleU16S16(levelCV, rawOutput);
    } else {
        output = 0;
    }
}
