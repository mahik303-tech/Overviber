// Parameter use as in the Overcycler firmware (synth.c): envelope speed,
// keyboard tracking of the filter and the WaveMod type "Frequency".
#include "TestSynth.h"
#include "dsp/Modulation.h"
#include <cmath>
#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

// Cutoff in Hz of the SEM/SSI exponential range for a cutoff CV.
double cutoffHz(double cv) { return 20.0 * std::pow(1300.0, cv / 65535.0); }

modulation::VoiceControls controls(const PresetData& part) {
    static LfoModule lfo1, lfo2;
    static Voice voice;
    static VoiceExpressionState expression;
    const uint16_t note = 60 * WTOSC_CV_SEMITONE;
    ModulationInputs in{part, lfo1, lfo2, voice, expression, 0, 0, 0, 0, 0, 0, 0, note, note, 32768};
    return modulation::computeVoiceControls(in);
}

// Seconds the amp envelope of a new note spends in its attack stage.
float attackSeconds(uint16_t attackCv, bool slow) {
    auto synth = std::make_unique<TestSynth>();
    synth->prepare(48000.0f);
    synth->setContinuousParam(cpAmpAtt, attackCv);
    synth->setSteppedParam(spAmpEnvSlow, slow ? 1 : 0);
    float left[64], right[64];
    synth->noteOn(60, 60000);
    const int voice = synth->findVoiceByNote(60);
    for (int b = 0; b < 48000 * 60 / 64; ++b) {
        synth->renderBlock(left, right, 64);
        if (synth->getVoice(voice).getAmpEnv().getStage() != sAttack) return (b + 1) * 64 / 48000.0f;
    }
    return -1.0f;
}

} // namespace

int main() {
    // Envelope speed: the firmware's speed shift 2 (normal) and 4 (slow).
    // Knob 500 is 125 ms on the hardware, 500 ms in the slow range.
    {
        const uint16_t cv = static_cast<uint16_t>(scan_potTo16bits(500));
        const float normal = attackSeconds(cv, false), slow = attackSeconds(cv, true);
        const float expected = adsrStageMilliseconds(cv) / 1000.0f;
        std::printf("Amp attack at knob 500: normal %.3f s, slow %.3f s (expected %.3f s / %.3f s)\n",
                    normal, slow, expected, expected * 4);
        check(std::fabs(expected - 0.125f) < 0.005f, "knob 500 is 125 ms as on the hardware");
        check(std::fabs(normal - expected) < 0.003f, "attack takes the hardware time");
        check(std::fabs(slow - 4 * expected) < 0.005f, "slow range is four times longer");
    }

    // Full keyboard tracking: the cutoff follows the keys one octave per octave.
    {
        auto synth = std::make_unique<TestSynth>();
        synth->prepare(48000.0f);
        synth->setContinuousParam(cpFilKbdAmt, UINT16_MAX);
        synth->setContinuousParam(cpCutoff, 20000);
        uint16_t cv[2];
        const uint8_t notes[2] = { 48, 72 };
        for (int i = 0; i < 2; ++i) {
            synth->noteOn(notes[i], 60000);
            cv[i] = synth->getFilterNoteCV(synth->findVoiceByNote(notes[i]));
            synth->noteOff(notes[i], 0);
        }
        // Like the firmware, full tracking (255/256) truncates to 11 semitones up
        // and 12 down for one octave.
        const double ratio = cutoffHz(cv[1]) / cutoffHz(cv[0]);
        const double expected = std::pow(2.0, 23.0 / 12.0);
        std::printf("Full tracking, notes 48 -> 72: cutoff ratio %.3f (expected %.3f)\n", ratio, expected);
        check(std::fabs(ratio / expected - 1.0) < 0.01, "full tracking follows the keys in semitones");
    }

    // WaveMod "Frequency": half the base amount goes to the pitch.
    {
        PresetData part;   // defaults, then no LFO, performance or matrix modulation
        part.steppedParams[spLFOTargets] = part.steppedParams[spLFO2Targets] = 0;
        part.steppedParams[spModwheelTarget] = part.steppedParams[spPressureTarget] = part.steppedParams[spTimbreTarget] = modNone;
        part.continuousParams[cpUnisonDetune] = 0;
        for (auto& slot : part.modMatrix) slot = ModMatrixSlot{};
        part.continuousParams[cpABaseWMod] = UINT16_MAX;
        part.continuousParams[cpBBaseWMod] = UINT16_MAX;
        part.continuousParams[cpDetune] = HALF_RANGE;
        part.continuousParams[cpMasterTune] = HALF_RANGE;
        part.continuousParams[cpWModAEnv] = HALF_RANGE;
        part.continuousParams[cpWModBEnv] = HALF_RANGE;
        part.steppedParams[spAWModType] = wmFrequency;
        part.steppedParams[spBWModType] = wmWidth;
        const modulation::VoiceControls c = controls(part);
        const int shiftA = (int)c.pitchA - 60 * WTOSC_CV_SEMITONE;
        const int shiftB = (int)c.pitchB - 60 * WTOSC_CV_SEMITONE;
        std::printf("Base WaveMod max: osc A (Frequency) wmod %u, pitch %+d; osc B (Width) wmod %u, pitch %+d\n",
                    c.wmodA, shiftA, c.wmodB, shiftB);
        check(c.wmodA == 49151, "Frequency halves the base WaveMod");
        check(shiftA == 16383, "Frequency adds the WaveMod to the pitch");
        check(c.wmodB == UINT16_MAX && shiftB == 0, "other WaveMod types leave the pitch alone");
    }

    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
