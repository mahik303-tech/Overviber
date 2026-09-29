// Parameter use as in the Overcycler firmware (synth.c): envelope speed,
// keyboard tracking of the filter, the WaveMod type "Frequency", bender,
// modwheel and pressure targets, the mixer's resonance compensation and the
// chromatic oscillator pitch.
#include "TestSynth.h"
#include "dsp/Modulation.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name.c_str());
    failures += !ok;
}

// Cutoff in Hz of the SEM/SSI exponential range for a cutoff CV.
double cutoffHz(double cv) { return 20.0 * std::pow(1300.0, cv / 65535.0); }

modulation::VoiceControls controls(const PresetData& part, int16_t bend = 0, uint16_t pressure = 0) {
    static LfoModule lfo1, lfo2;
    static Voice voice;
    static VoiceExpressionState expression;
    const uint16_t note = 60 * WTOSC_CV_SEMITONE;
    ModulationInputs in{part, lfo1, lfo2, voice, expression, 0, bend, 0, pressure, 32768, 0, 0, note, note, 32768};
    return modulation::computeVoiceControls(in);
}

// Defaults without LFO, performance or matrix modulation.
PresetData plainPart() {
    PresetData part;
    part.steppedParams[spLFOTargets] = part.steppedParams[spLFO2Targets] = 0;
    part.steppedParams[spBenderTarget] = part.steppedParams[spPressureTarget] = modNone;
    part.steppedParams[spTimbreTarget] = modNone;
    part.continuousParams[cpUnisonDetune] = 0;
    part.continuousParams[cpDetune] = part.continuousParams[cpMasterTune] = HALF_RANGE;
    part.continuousParams[cpResonance] = 0;
    for (auto& slot : part.modMatrix) slot = ModMatrixSlot{};
    return part;
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

    // Bender: +-4/7/12 semitones on the pitch, four times that on the filter,
    // bend/12 x range on volume and WaveMod.
    {
        PresetData part = plainPart();
        const int ranges[3] = { 4, 7, 12 };
        bool pitchOk = true, filterOk = true, levelOk = true;
        for (uint8_t r = 0; r < 3; ++r) {
            part.steppedParams[spBenderRange] = r;
            part.steppedParams[spBenderTarget] = modPitch;
            pitchOk &= std::abs(modulation::benderAmount(part, 32764) - ranges[r] * WTOSC_CV_SEMITONE) <= 1;
            part.steppedParams[spBenderTarget] = modFilter;
            filterOk &= std::abs(modulation::benderAmount(part, 32764) - ranges[r] * 4 * FILTER_CV_SEMITONE) <= 4;
            part.steppedParams[spBenderTarget] = modWaveMod;
            levelOk &= modulation::benderAmount(part, 32764) == 32764 / 12 * ranges[r];
        }
        check(pitchOk, "bender pitch range 4/7/12 semitones");
        check(filterOk, "bender filter range four times the pitch range");
        check(levelOk, "bender volume/WaveMod bend/12 x range");

        part.steppedParams[spBenderRange] = 2;
        part.steppedParams[spBenderTarget] = modVolume;
        const auto rest = controls(part), down = controls(part, -32768), up = controls(part, 32764);
        std::printf("Bender on volume: osc A gain %.3f down, %.3f at rest, %.3f up\n", down.gainA, rest.gainA, up.gainA);
        check(down.gainA < 0.001f && std::fabs(up.gainA / rest.gainA - 2.0f) < 0.01f,
              "bender on volume scales the mixer levels 0 .. 2x");
        check(up.amp == rest.amp, "bender on volume leaves the VCA alone");
    }

    // Resonance compensation of the mixer levels for the ladder filters.
    {
        PresetData part = plainPart();
        part.steppedParams[spFilterModel] = fmSSI2144;
        const float open = controls(part).gainA;
        part.continuousParams[cpResonance] = UINT16_MAX;
        const float ladder = controls(part).gainA;
        part.steppedParams[spFilterModel] = fmSem;
        const float sem = controls(part).gainA;
        std::printf("Osc A gain at full resonance: SSI2144 %.3f (%.2fx), SEM %.3f\n", ladder, ladder / open, sem);
        check(std::fabs(ladder / open - 5.67f) < 0.05f, "ladder mixer level rises with the resonance (5.7x)");
        check(sem == open, "SEM mixer level does not depend on the resonance");
    }

    // Modwheel on LFO 1 or LFO 2 depth, pressure on an LFO, start delay on
    // the other LFO.
    {
        PresetData part = plainPart();
        part.continuousParams[cpLFOAmt] = part.continuousParams[cpLFO2Amt] = 1000;
        part.steppedParams[spModwheelRange] = 1;   // >> 3
        part.steppedParams[spModwheelTarget] = 0;
        auto a = modulation::lfoAmounts(part, 65535, 0, 32768, UINT16_MAX);
        check(a[0] == 1000 + (65535 >> 3) && a[1] == 1000, "modwheel adds to LFO 1 depth (range >> 3)");
        a = modulation::lfoAmounts(part, 65535, 0, 32768, 0);
        check(a[0] == 1000 + (65535 >> 3) && a[1] == 0, "start delay scales the LFO the wheel does not control");
        part.steppedParams[spModwheelTarget] = 1;
        part.steppedParams[spPressureTarget] = modLFO1;
        part.steppedParams[spPressureRange] = 3;   // >> 0
        a = modulation::lfoAmounts(part, 65535, 20000, 32768, UINT16_MAX);
        check(a[1] == 1000 + (65535 >> 3) && a[0] == 21000, "modwheel on LFO 2, pressure on LFO 1");
    }

    // Chromatic pitch: free, semitones, octaves.
    {
        const uint16_t freq = static_cast<uint16_t>((((19 << 8) + 100) << 2));   // 19 semitones + 100/256
        uint16_t cv[3];
        for (uint8_t mode = 0; mode < 3; ++mode) {
            auto synth = std::make_unique<TestSynth>();
            synth->prepare(48000.0f);
            synth->setContinuousParam(cpAFreq, freq);
            synth->setSteppedParam(spChromaticPitch, mode);
            synth->noteOn(48, 60000);
            cv[mode] = synth->getOscANoteCV(synth->findVoiceByNote(48));
        }
        const int base = 48 * WTOSC_CV_SEMITONE;
        std::printf("Chromatic pitch, base 19 semitones + 100/256: free %+d, semitones %+d, octaves %+d\n",
                    cv[0] - base, cv[1] - base, cv[2] - base);
        check(cv[0] - base == (19 << 8) + 100, "free pitch keeps the fine tuning");
        check(cv[1] - base == 19 << 8, "semitones drop the fine tuning");
        check(cv[2] - base == 12 << 8, "octaves round down to whole octaves");
    }

    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
