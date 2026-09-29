// Glide and the LFO start delay run on the firmware's 500 Hz control tick:
// independent of tempo and of a stopped host transport, with the hardware's
// times (dsp/ControlTimes.h).
#include "TestSynth.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace {

constexpr float kRate = 48000.0f;
constexpr int kBlock = 64;
int failures = 0;

void check(bool ok, const std::string& name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name.c_str());
    failures += !ok;
}

std::unique_ptr<TestSynth> makeSynth() {
    auto synth = std::make_unique<TestSynth>();
    synth->prepare(kRate);
    synth->setSteppedParam(spVoiceCount, 0);   // mono: the second note glides
    return synth;
}

// Seconds until oscillator A's note CV of the voice playing `to` reaches the
// target after a legato octave step, or -1 if it does not arrive within 3 s.
float octaveGlideSeconds(TestSynth& synth) {
    float left[kBlock], right[kBlock];
    synth.noteOn(48, 60000);
    for (int b = 0; b < 40; ++b) synth.renderBlock(left, right, kBlock);
    synth.noteOn(60, 60000);
    const int voice = synth.findVoiceByNote(60);
    if (voice < 0) return -1.0f;
    const uint16_t target = synth.getOscATargetCV(voice);
    for (int b = 0; b < static_cast<int>(3.0f * kRate / kBlock); ++b) {
        synth.renderBlock(left, right, kBlock);
        if (synth.getOscANoteCV(voice) == target) return (b + 1) * kBlock / kRate;
    }
    return -1.0f;
}

} // namespace

int main() {
    // Glide: knob 700 is 192 ms per octave on the hardware.
    const uint16_t glide = static_cast<uint16_t>(scan_potTo16bits(700));
    const float expected = controltimes::glideOctaveMilliseconds(glide) / 1000.0f;
    float times[3];
    const float bpms[3] = { 60.0f, 180.0f, 120.0f };
    for (int i = 0; i < 3; ++i) {
        auto synth = makeSynth();
        synth->setContinuousParam(cpGlide, glide);
        synth->setSteppedParam(spArpSync, 1);
        synth->setHostBpm(bpms[i]);
        if (i == 2) synth->setHostTransport(0.0, false);   // DAW stopped
        times[i] = octaveGlideSeconds(*synth);
    }
    std::printf("Octave glide at knob 700: expected %.3f s; 60 BPM %.3f s, 180 BPM %.3f s, stopped %.3f s\n",
                expected, times[0], times[1], times[2]);
    for (float t : times) check(t > 0 && std::fabs(t - expected) < 0.01f, "glide takes the hardware time");
    check(std::fabs(times[0] - times[1]) < 0.002f, "glide does not depend on tempo");

    // Start delay: knob 700 waits 974 ms, then fades in over 974 ms. As in
    // the firmware it acts on the LFO the modwheel does not control.
    const uint16_t delayCv = static_cast<uint16_t>(scan_potTo16bits(700));
    const float delay = controltimes::modDelayMilliseconds(delayCv) / 1000.0f;
    auto levelAfter = [&](uint16_t modDelay, float seconds, uint8_t wheelTarget, int lfo) {
        auto synth = makeSynth();
        synth->setContinuousParam(cpLFOAmt, UINT16_MAX);
        synth->setContinuousParam(cpLFO2Amt, UINT16_MAX);
        synth->setSteppedParam(spModwheelTarget, wheelTarget);
        synth->setContinuousParam(cpModDelay, modDelay);
        float left[kBlock], right[kBlock];
        synth->noteOn(60, 60000);
        for (int b = 0; b < static_cast<int>(seconds * kRate / kBlock); ++b) synth->renderBlock(left, right, kBlock);
        return synth->getLfo(lfo).getLevelCV();
    };
    for (uint8_t wheelTarget = 0; wheelTarget < 2; ++wheelTarget) {
        const int delayed = wheelTarget == 0 ? 1 : 0;
        const std::string name = "LFO " + std::to_string(delayed + 1);
        const uint16_t during = levelAfter(delayCv, delay * 0.8f, wheelTarget, delayed);
        const uint16_t fading = levelAfter(delayCv, delay * 1.5f, wheelTarget, delayed);
        const uint16_t full = levelAfter(delayCv, delay * 2.1f, wheelTarget, delayed);
        const uint16_t wheelLfo = levelAfter(delayCv, delay * 0.8f, wheelTarget, 1 - delayed);
        const uint16_t noDelay = levelAfter(0, 0.01f, wheelTarget, delayed);
        std::printf("Modwheel on LFO %d, %s level with delay %.3f s: at 0.8x %u, 1.5x %u, 2.1x %u; "
                    "without delay %u; wheel LFO during the delay %u\n",
                    2 - delayed, name.c_str(), delay, during, fading, full, noDelay, wheelLfo);
        check(during == 0, name + " is silent during the delay");
        check(fading > 0 && fading < full, name + " fades in after the delay");
        check(full >= UINT16_MAX - 2, name + " reaches full depth after twice the delay");
        check(noDelay == UINT16_MAX, "without delay " + name + " is at full depth at once");
        check(wheelLfo == UINT16_MAX, "the modwheel's LFO is not delayed");
    }

    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
