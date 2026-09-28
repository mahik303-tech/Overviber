#pragma once
#include "Ssi2144Filter.h"
#include "SstLadderFilter.h"
#include "SemFilter.h"
#include "audible/ShelvesFilter.h"
#include <array>

// Small-signal passband calibration, performed only in prepare(). This does
// not compensate away intentional bandpass/EQ response or large-signal drive.
template<class Filter> inline float measurePassband(Filter& filter, float rate) {
    filter.setSampleRate(rate); filter.setCV(65535, 0);
    double inputEnergy = 0, outputEnergy = 0;
    for (int i = 0; i < 8192; ++i) {
        const float input = 0.001f * std::sin(6.283185307179586 * 1000.0 * i / rate);
        const float output = filter.processSample(input);
        if (i >= 4096) { inputEnergy += input * input; outputEnergy += output * output; }
    }
    const double gain = outputEnergy > 1e-15 ? std::sqrt(inputEnergy / outputEnergy) : 1.0;
    return static_cast<float>(std::clamp(gain, 0.125, 8.0));
}
// Per filter model; the SEM model (index 1) is calibrated per variant below.
inline std::array<float, 4> calibrateFilters(float rate) {
    Ssi2144Filter ssi; ShelvesFilter shelves; SstLadderFilter sst;
    return {measurePassband(ssi, rate), 1.0f, measurePassband(shelves, rate), measurePassband(sst, rate)};
}
inline std::array<float, SemFilter::VariantCount> calibrateSemFilters(float rate) {
    std::array<float, SemFilter::VariantCount> gains{};
    for (int variant = 0; variant < SemFilter::VariantCount; ++variant) {
        SemFilter sem;
        sem.setVariant(static_cast<uint8_t>(variant));
        gains[variant] = measurePassband(sem, rate);
    }
    return gains;
}
