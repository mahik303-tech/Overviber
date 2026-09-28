#include "wtosc.h"

#define CLOCK ((uint32_t)SYNTH_MASTER_CLOCK)
#define MAX_SAMPLERATE (CLOCK / DACSPI_TICK_RATE)

WtOsc::WtOsc() {
    init(0);
}

void WtOsc::init(int8_t ch) {
    channel = ch;
    currentSampleRate = 48000.0f;
    mainData = nullptr;
    crossoverData = nullptr;
    period[0] = period[1] = pendingPeriod[0] = pendingPeriod[1] = 1875;
    increment[0] = increment[1] = pendingIncrement[0] = pendingIncrement[1] = 1;
    counter = 0;
    phase = 0;
    curSample = prevSample = prevSample2 = prevSample3 = HALF_RANGE;
    aliasing = 0;
    folder = UINT16_MAX / 32;
    bitcrush = 1;
    pitch = 0;
    width = HALF_RANGE >> (16 - WIDTH_MOD_BITS);
    crossover = 0;
    wmType = wmOff;
    pendingUpdate = 0;
}

void WtOsc::setSampleRate(float sr) {
    currentSampleRate = std::max(22050.0f, sr);
}

void WtOsc::setSampleData(const uint16_t* mainD, const uint16_t* xovrD) {
    mainData = mainD;
    crossoverData = xovrD;
}

uint32_t WtOsc::cvToFrequency(uint32_t cv) {
    uint32_t v = cv % (12 * WTOSC_CV_SEMITONE);
    v = (v * 21) << 8;
    v = (uint32_t)computeShape(v, oscOctaveCurve, 1) + 32768;
    v = (v << WIDTH_MOD_BITS) >> (12 - (cv / (12 * WTOSC_CV_SEMITONE)));
    return v;
}

void WtOsc::updatePeriodIncrement(int8_t type) {
    if (pendingUpdate >= type) {
        period[0] = pendingPeriod[0];
        period[1] = pendingPeriod[1];
        increment[0] = pendingIncrement[0];
        increment[1] = pendingIncrement[1];
        pendingUpdate = 0;
    }
}

void WtOsc::handlePhaseUnderflow(oscSyncMode_t syncMode, int16_t* syncPosition) {
    if (phase < 0) {
        phase += WTOSC_SAMPLE_COUNT;
        updatePeriodIncrement(1);

        if (syncMode == osmMaster && syncPosition != nullptr) {
            *syncPosition = (int16_t)counter;
        }
    }
}

void WtOsc::handleSlaveSync(int16_t* syncPosition) {
    if (syncPosition != nullptr && *syncPosition > INT16_MIN) {
        phase = 0;
        counter = *syncPosition;
        *syncPosition = INT16_MIN;
    }
}

int32_t WtOsc::handleCounterUnderflow_wmOff(oscSyncMode_t syncMode, int16_t* syncPosition) {
    int32_t curPeriod = period[0];
    int32_t curIncrement = increment[0];

    phase -= curIncrement;
    handlePhaseUnderflow(syncMode, syncPosition);
    counter += curPeriod;

    prevSample3 = prevSample2;
    prevSample2 = prevSample;
    prevSample = curSample;

    if (mainData != nullptr) {
        curSample = mainData[(phase >= 0 && phase < WTOSC_SAMPLE_COUNT) ? phase : 0];
    } else {
        curSample = HALF_RANGE;
    }

    return (curPeriod > 0) ? ((1 << (FRAC_SHIFT * 2)) / curPeriod) : 1;
}

int32_t WtOsc::handleCounterUnderflow_wmAliasing(oscSyncMode_t syncMode, int16_t* syncPosition) {
    int32_t curPeriod = period[0];
    int32_t curIncrement = increment[0];

    phase -= curIncrement;
    handlePhaseUnderflow(syncMode, syncPosition);
    counter += curPeriod;

    if (aliasing) {
        prevSample3 = prevSample2 = curSample;
    } else {
        prevSample3 = prevSample2;
        prevSample2 = prevSample;
    }
    prevSample = curSample;

    if (mainData != nullptr) {
        curSample = mainData[(phase >= 0 && phase < WTOSC_SAMPLE_COUNT) ? phase : 0];
    } else {
        curSample = HALF_RANGE;
    }

    return (curPeriod > 0) ? ((1 << (FRAC_SHIFT * 2)) / curPeriod) : 1;
}

int32_t WtOsc::handleCounterUnderflow_wmWidth(oscSyncMode_t syncMode, int16_t* syncPosition) {
    int32_t curPeriod, curIncrement;
    if (phase >= WTOSC_SAMPLE_COUNT / 2) {
        curPeriod = period[1];
        curIncrement = increment[1];
    } else {
        curPeriod = period[0];
        curIncrement = increment[0];
    }

    phase -= curIncrement;
    handlePhaseUnderflow(syncMode, syncPosition);
    counter += curPeriod;

    prevSample3 = prevSample2;
    prevSample2 = prevSample;
    prevSample = curSample;

    if (mainData != nullptr) {
        curSample = mainData[(phase >= 0 && phase < WTOSC_SAMPLE_COUNT) ? phase : 0];
    } else {
        curSample = HALF_RANGE;
    }

    return (curPeriod > 0) ? ((1 << (FRAC_SHIFT * 2)) / curPeriod) : 1;
}

int32_t WtOsc::handleCounterUnderflow_wmCrossOver(oscSyncMode_t syncMode, int16_t* syncPosition) {
    int32_t curPeriod = period[0];
    int32_t curIncrement = increment[0];

    phase -= curIncrement;
    handlePhaseUnderflow(syncMode, syncPosition);
    counter += curPeriod;

    prevSample3 = prevSample2;
    prevSample2 = prevSample;
    prevSample = curSample;

    if (mainData != nullptr) {
        int p = (phase >= 0 && phase < WTOSC_SAMPLE_COUNT) ? phase : 0;
        uint16_t m = mainData[p];
        uint16_t x = (crossoverData != nullptr) ? crossoverData[p] : m;
        curSample = lerp16(m, x, crossover);
    } else {
        curSample = HALF_RANGE;
    }

    return (curPeriod > 0) ? ((1 << (FRAC_SHIFT * 2)) / curPeriod) : 1;
}

int32_t WtOsc::handleCounterUnderflow_wmFolder(oscSyncMode_t syncMode, int16_t* syncPosition) {
    int32_t curPeriod = period[0];
    int32_t curIncrement = increment[0];

    phase -= curIncrement;
    handlePhaseUnderflow(syncMode, syncPosition);
    counter += curPeriod;

    prevSample3 = prevSample2;
    prevSample2 = prevSample;
    prevSample = curSample;

    if (mainData != nullptr) {
        int32_t smp = mainData[(phase >= 0 && phase < WTOSC_SAMPLE_COUNT) ? phase : 0];
        smp += INT16_MIN;
        smp *= folder;
        smp = (smp >> 2) + (1 << 24);
        smp -= (smp + (1 << 25)) & 0xfc000000;
        smp ^= (smp >> 31);
        smp >>= 9;
        curSample = smp;
    } else {
        curSample = HALF_RANGE;
    }

    return (curPeriod > 0) ? ((1 << (FRAC_SHIFT * 2)) / curPeriod) : 1;
}

int32_t WtOsc::handleCounterUnderflow_wmBitCrush(oscSyncMode_t syncMode, int16_t* syncPosition) {
    int32_t curPeriod = period[0];
    int32_t curIncrement = increment[0];

    phase -= curIncrement;
    handlePhaseUnderflow(syncMode, syncPosition);
    counter += curPeriod;

    prevSample3 = prevSample2;
    prevSample2 = prevSample;
    prevSample = curSample;

    if (mainData != nullptr) {
        int32_t smp = mainData[(phase >= 0 && phase < WTOSC_SAMPLE_COUNT) ? phase : 0];
        if (bitcrush >= 0) smp += INT16_MIN;
        smp = (smp << 1) + 1;
        if (bitcrush != 0) {
            smp /= bitcrush;
            smp *= bitcrush;
        }
        smp >>= 1;
        if (bitcrush >= 0) smp -= INT16_MIN;
        curSample = smp;
    } else {
        curSample = HALF_RANGE;
    }

    return (curPeriod > 0) ? ((1 << (FRAC_SHIFT * 2)) / curPeriod) : 1;
}

void WtOsc::setParameters(uint16_t newPitch, oscWModTarget_t newWmType, uint16_t wmAmount) {
    newPitch = std::min((uint16_t)(WTOSC_HIGHEST_NOTE * WTOSC_CV_SEMITONE), newPitch);

    uint16_t newWidth = HALF_RANGE >> (16 - WIDTH_MOD_BITS);
    int32_t aliasing_s = 0;
    int32_t crossover_s = 0;
    int32_t folder_s = UINT16_MAX / 32;
    int32_t bitcrush_s = 1;

    switch (newWmType) {
    case wmWidth: {
        uint16_t wlim = newPitch >> 2;
        newWidth = wmAmount;
        newWidth = std::max(wlim, newWidth);
        newWidth = std::min((uint16_t)(UINT16_MAX - wlim), newWidth);
        newWidth >>= (16 - WIDTH_MOD_BITS);
        break;
    }
    case wmAliasing: {
        aliasing_s = (int32_t)wmAmount + INT16_MIN;
        if (aliasing_s >= 0) {
            aliasing_s >>= 8;
        } else {
            aliasing_s = -aliasing_s;
            aliasing_s >>= 4;
        }
        break;
    }
    case wmCrossOver: {
        crossover_s = (int32_t)wmAmount + INT16_MIN;
        crossover_s = std::abs(crossover_s);
        crossover_s = __USAT(crossover_s << 1, 16);
        break;
    }
    case wmBitCrush: {
        bitcrush_s = (int32_t)wmAmount + INT16_MIN;
        if (!bitcrush_s) bitcrush_s = 1;
        break;
    }
    case wmFolder: {
        folder_s = (int32_t)wmAmount + INT16_MIN;
        folder_s = std::abs(folder_s);
        folder_s = __USAT((folder_s << 1) + UINT16_MAX / 32, 16);
        break;
    }
    default:
        break;
    }

    if (newPitch != pitch || newWidth != width || aliasing_s != aliasing) {
        uint64_t frequency = (uint64_t)cvToFrequency(newPitch) * (WTOSC_SAMPLE_COUNT / 2);

        uint32_t wDenominator0 = (1 << WIDTH_MOD_BITS) - newWidth;
        uint32_t wDenominator1 = newWidth > 0 ? newWidth : 1;
        if (wDenominator0 == 0) wDenominator0 = 1;

        uint32_t sampleRate0 = (uint32_t)(frequency / wDenominator0);
        uint32_t sampleRate1 = (uint32_t)(frequency / wDenominator1);

        uint32_t maxSR = (uint32_t)currentSampleRate;
        if (maxSR == 0) maxSR = 48000;

        int32_t inc0 = sampleRate0 / maxSR;
        int32_t inc1 = sampleRate1 / maxSR;

        if (inc0 >= WTOSC_SAMPLE_COUNT / 2) inc0 = WTOSC_SAMPLE_COUNT / 2 - 1;
        if (inc1 >= WTOSC_SAMPLE_COUNT / 2) inc1 = WTOSC_SAMPLE_COUNT / 2 - 1;

        if (newWmType == wmAliasing) {
            inc0 = oscIncModLUT[inc0];
            inc1 = oscIncModLUT[inc1];
            inc0 = std::min(WTOSC_SAMPLE_COUNT, inc0 + aliasing_s);
            inc1 = std::min(WTOSC_SAMPLE_COUNT, inc1 + aliasing_s);
        }

        if (inc0 <= 0) inc0 = 1;
        if (inc1 <= 0) inc1 = 1;

        uint32_t srDivInc0 = sampleRate0 / inc0;
        uint32_t srDivInc1 = sampleRate1 / inc1;
        if (srDivInc0 == 0) srDivInc0 = 1;
        if (srDivInc1 == 0) srDivInc1 = 1;

        pendingPeriod[0] = CLOCK / srDivInc0;
        pendingPeriod[1] = CLOCK / srDivInc1;
        pendingIncrement[0] = inc0;
        pendingIncrement[1] = inc1;

        pendingUpdate = (newPitch == pitch && aliasing_s == aliasing) ? 1 : 2;

        pitch = newPitch;
        width = newWidth;
        aliasing = aliasing_s;
    }

    crossover = (uint16_t)crossover_s;
    folder = folder_s;
    bitcrush = bitcrush_s;
    wmType = newWmType;
}

uint16_t WtOsc::processSample(uint32_t tickStep, oscSyncMode_t syncMode, int16_t* syncPosition) {
    if (mainData == nullptr) {
        return HALF_RANGE;
    }

    updatePeriodIncrement(2);

    counter -= tickStep;

    if (syncMode == osmSlave) {
        handleSlaveSync(syncPosition);
    }

    int32_t curHalf = (phase >= WTOSC_SAMPLE_COUNT / 2) ? 1 : 0;
    int32_t p = (wmType == wmWidth) ? period[curHalf] : period[0];
    if (p <= 0) p = 1;
    int32_t alphaDiv = (1 << (FRAC_SHIFT * 2)) / p;

    while (counter < 0) {
        switch (wmType) {
        case wmAliasing:
            alphaDiv = handleCounterUnderflow_wmAliasing(syncMode, syncPosition);
            break;
        case wmWidth:
            alphaDiv = handleCounterUnderflow_wmWidth(syncMode, syncPosition);
            break;
        case wmCrossOver:
            alphaDiv = handleCounterUnderflow_wmCrossOver(syncMode, syncPosition);
            break;
        case wmFolder:
            alphaDiv = handleCounterUnderflow_wmFolder(syncMode, syncPosition);
            break;
        case wmBitCrush:
            alphaDiv = handleCounterUnderflow_wmBitCrush(syncMode, syncPosition);
            break;
        case wmOff:
        case wmFrequency:
        default:
            alphaDiv = handleCounterUnderflow_wmOff(syncMode, syncPosition);
            break;
        }
    }

    if (counter < 0) counter = 0;

    int32_t alpha = (counter * alphaDiv) >> FRAC_SHIFT;
    if (alpha < 0) alpha = 0;
    else if (alpha > (1 << FRAC_SHIFT)) alpha = (1 << FRAC_SHIFT);

    return herp(alpha, curSample, prevSample, prevSample2, prevSample3, FRAC_SHIFT);
}
