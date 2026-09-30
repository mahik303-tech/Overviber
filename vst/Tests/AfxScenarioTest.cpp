// AFX mode (sound per key): a note must sound as long as the same part played
// through its MIDI channel in Multi-Channel mode, and every part of the
// default kit must be audible for at least 50 ms, not a click. (The kit's envelope times
// were once written as milliseconds on a scale where 250 means 3 ms.)
#include "TestSynth.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

constexpr float kRate = 48000.0f;
constexpr int kBlock = 64;

// Seconds the output stays above -40 dB of its peak, for one note held `hold`
// seconds and followed by `tail` seconds of release.
float soundingSeconds(bool afx, int part, float hold, float tail, float* peakOut = nullptr) {
    auto synth = std::make_unique<TestSynth>();
    synth->prepare(kRate);
    synth->setSteppedParam(spEngineMode, afx ? emAFX : emMultiChannel);
    const uint8_t note = static_cast<uint8_t>(part * 8 + 4);         // default map: 8 notes per part
    const uint8_t channel = static_cast<uint8_t>(afx ? 1 : part + 1);  // Multi-Channel: channel N plays part N
    const int holdFrames = static_cast<int>(hold * kRate), total = static_cast<int>((hold + tail) * kRate);
    std::vector<float> level;
    float left[kBlock], right[kBlock];
    synth->noteOn(note, 60000, channel);
    for (int frame = 0; frame < total; frame += kBlock) {
        if (frame == holdFrames - holdFrames % kBlock) synth->noteOff(note, 0, channel);
        synth->renderBlock(left, right, kBlock);
        float blockPeak = 0;
        for (int i = 0; i < kBlock; ++i) blockPeak = std::fmax(blockPeak, std::fabs(left[i]) + std::fabs(right[i]));
        level.push_back(blockPeak);
    }
    float peak = 0;
    for (float l : level) peak = std::fmax(peak, l);
    if (peakOut) *peakOut = peak;
    int last = -1;
    for (int b = 0; b < static_cast<int>(level.size()); ++b) if (level[b] > peak * 0.01f) last = b;
    return (last + 1) * kBlock / kRate;
}

} // namespace

int main() {
    int failures = 0;
    std::printf("Part  AFX sounding  Multi-Channel sounding  (held 1.0 s)\n");
    for (int part = 0; part < 16; ++part) {
        float afxPeak = 0, mcPeak = 0;
        const float afx = soundingSeconds(true, part, 1.0f, 1.5f, &afxPeak);
        const float mc = soundingSeconds(false, part, 1.0f, 1.5f, &mcPeak);
        const bool ok = std::fabs(afx - mc) < 0.02f && afxPeak > 0 && afx >= 0.05f;
        std::printf("%4d  %8.3f s  %8.3f s  peak %.3f / %.3f  %s\n", part + 1, afx, mc, afxPeak, mcPeak, ok ? "" : "FAIL");
        failures += !ok;
    }
    std::printf("%d parts failed\n", failures);

    // The engine reports the parts that sound (the AFX pads light up): a
    // chord over parts 3 and 10, then silence after the release.
    {
        auto synth = std::make_unique<TestSynth>();
        synth->prepare(kRate);
        synth->setSteppedParam(spEngineMode, emAFX);
        float left[kBlock], right[kBlock];
        synth->noteOn(2 * 8 + 4, 60000);
        synth->noteOn(9 * 8 + 4, 60000);
        synth->renderBlock(left, right, kBlock);
        const uint16_t playing = synth->getSoundingParts();
        synth->noteOff(2 * 8 + 4, 0);
        synth->noteOff(9 * 8 + 4, 0);
        for (int b = 0; b < static_cast<int>(3.0f * kRate) / kBlock; ++b) synth->renderBlock(left, right, kBlock);
        const bool ok = playing == ((1u << 2) | (1u << 9)) && synth->getSoundingParts() == 0;
        std::printf("sounding parts: 0x%04x while held, 0x%04x after the release  %s\n", playing,
                    synth->getSoundingParts(), ok ? "" : "FAIL");
        failures += !ok;
    }
    return failures ? 1 : 0;
}
