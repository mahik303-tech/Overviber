// Filter resonance calibration: as on the hardware ("self-oscillation can be
// heard in the last third of amount", Overcycler manual), the filter models
// that self-oscillate start to do so at about two thirds of the knob
// (kFilterResonanceOnset). Prints, per model and cutoff, the lowest knob
// position at which the filter keeps ringing after an impulse.
#include "dsp/FilterCalibration.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

// The filters' running rate in the engine at a 48 kHz output: the voices
// run at twice the output rate (SynthEngine::prepare).
constexpr float kRate = 96000.0f;
int failures = 0;

void check(bool ok, const std::string& name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name.c_str());
    failures += !ok;
}

// Excites the filter with a short burst and compares the RMS in 1.75..2 s
// with that in 0.25..0.5 s. A filter that keeps oscillating stays near 1 or
// grows; below the threshold the ringing decays. A quiet late signal (noise,
// DC, slow decay) does not count as oscillation.
template <class Filter> float ringRatio(Filter& filter, uint16_t cutoff, uint16_t resonance) {
    filter.setSampleRate(kRate);
    filter.reset();
    filter.setCV(cutoff, resonance);
    double early = 0, late = 0;
    const int n = static_cast<int>(2.0f * kRate);
    for (int i = 0; i < n; ++i) {
        const float out = filter.processSample(i < 16 ? 0.2f : 0.0f);
        if (!std::isfinite(out)) return 0.0f;
        if (i >= kRate * 0.25f && i < kRate * 0.5f) early += out * out;
        if (i >= kRate * 1.75f) late += out * out;
    }
    if (std::sqrt(late / (0.25 * kRate)) < 0.01) return 0.0f;
    return static_cast<float>(std::sqrt(late / std::max(early, 1e-20)));
}

struct Model {
    const char* name;
    bool selfOscillates;
    std::function<float(uint16_t cutoff, uint16_t resonance)> ring;
};

std::vector<Model> models() {
    auto sem = [](uint8_t variant, uint8_t mode) {
        return [=](uint16_t c, uint16_t r) { SemFilter f; f.setVariant(variant); f.setMode(mode); return ringRatio(f, c, r); };
    };
    auto sst = [](uint8_t mode) {
        return [=](uint16_t c, uint16_t r) { SstLadderFilter f; f.setMode(mode); return ringRatio(f, c, r); };
    };
    return {
        { "SSI2144", true, [](uint16_t c, uint16_t r) { Ssi2144Filter f; return ringRatio(f, c, r); } },
        { "SST LP4", true, sst(0) },
        { "SST LP2", true, sst(2) },
        { "Liquid LP4", true, sem(SemFilter::Liquid, 0) },
        { "Liquid LP2", true, sem(SemFilter::Liquid, 1) },
        { "SEM OB-Xd", false, sem(SemFilter::ObXd, 0) },
        { "SEM Cytomic", false, sem(SemFilter::Cytomic, 0) },
        { "Shelves SVF LP", false, [](uint16_t c, uint16_t r) { ShelvesFilter f; f.setMode(1); return ringRatio(f, c, r); } },
    };
}

// Lowest resonance CV (step 512, 0.008 of the knob) at which the filter
// keeps ringing, or -1. The ringing only grows with the resonance, so a
// bisection over the steps finds the same step as a scan.
int threshold(const Model& m, uint16_t cutoff) {
    auto rings = [&](int step) { return m.ring(cutoff, static_cast<uint16_t>(std::min(step * 512, 65535))) > 0.5f; };
    int low = 0, high = 128;             // steps; 128 is CV 65535
    if (!rings(high)) return -1;
    if (rings(low)) return 0;
    while (high - low > 1) {             // rings(high), !rings(low)
        const int mid = (low + high) / 2;
        (rings(mid) ? high : low) = mid;
    }
    return std::min(high * 512, 65535);
}

} // namespace

int main() {
    const uint16_t cutoffs[3] = { 30000, 40000, 48000 };   // about 0.5, 1.5 and 4 kHz
    for (const auto& m : models()) {
        std::printf("%-15s", m.name);
        int onsets[3];
        for (int i = 0; i < 3; ++i) {
            onsets[i] = threshold(m, cutoffs[i]);
            std::printf("  cutoff %5u: knob %6.3f", cutoffs[i], onsets[i] < 0 ? -1.0 : onsets[i] / 65535.0);
        }
        std::printf("\n");
        for (int t : onsets) {
            if (m.selfOscillates)
                // The SST ladder feeds back with one sample delay, so its limit
                // moves with the cutoff (0.65 .. 0.74 of the knob).
                check(t >= 0 && std::fabs(t / 65535.0 - kFilterResonanceOnset) < 0.08,
                      std::string(m.name) + " self-oscillates from about two thirds of the knob");
            else
                check(t < 0, std::string(m.name) + " does not self-oscillate");
        }
    }
    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
