#include "Modulation.h"
#include <algorithm>
#include <cmath>

// The arithmetic below is the hardware-derived CV computation of the
// Overcycler: 16-bit fixed-point CVs plus float matrix offsets. Keep the order
// and types of the operations; the reference renders compare it bit-exactly.

namespace modulation {

float source(const ModulationInputs& in, uint8_t src) {
    const auto& e = in.expression;
    switch (src) {
    case modSrcModWheel:
        return (float)in.modwheel / 65535.0f;
    case modSrcPitchBend:
        if (e.hasPerVoiceBend) {
            return std::clamp(e.smoothedBend / (float)(12 * WTOSC_CV_SEMITONE), -1.0f, 1.0f);
        }
        return std::clamp((float)in.bend / (float)(12 * WTOSC_CV_SEMITONE), -1.0f, 1.0f);
    case modSrcAftertouch:
        if (e.hasPerVoicePressure) {
            return std::clamp(e.smoothedPressure / 65535.0f, 0.0f, 1.0f);
        }
        return (float)in.pressure / 65535.0f;
    case modSrcTimbreSlide:
        if (e.hasPerVoiceTimbre) {
            return std::clamp(e.smoothedTimbre / 65535.0f, 0.0f, 1.0f);
        }
        return (float)in.timbre / 65535.0f;
    case modSrcVelocity:
        return (float)e.noteOnVelocity / 65535.0f;
    case modSrcReleaseVelocity:
        return (float)e.noteOffVelocity / 65535.0f;
    case modSrcKeyTrack:
        if (e.noteNumber != ASSIGNER_NO_NOTE) {
            return std::clamp((float)((int)e.noteNumber - MIDDLE_C_NOTE) / 60.0f, -1.0f, 1.0f);
        }
        return 0.0f;
    case modSrcBreath:
        return (float)in.breath / 65535.0f;
    case modSrcExpression:
        return (float)in.expression11 / 65535.0f;
    case modSrcFilterEnv:
        return (float)in.voice.getFilEnv().getOutput() / 65535.0f;
    case modSrcAmpEnv:
        return (float)in.voice.getAmpEnv().getOutput() / 65535.0f;
    case modSrcWaveModEnv:
        return (float)in.voice.getWmodEnv().getOutput() / 65535.0f;
    case modSrcLFO1:
        return (float)in.lfo1.getOutput() / 32768.0f;
    case modSrcLFO1_Uni:
        return (float)in.lfo1.getLevelCV() / 65535.0f;
    case modSrcLFO2:
        return (float)in.lfo2.getOutput() / 32768.0f;
    case modSrcLFO2_Uni:
        return (float)in.lfo2.getLevelCV() / 65535.0f;
    case modSrcConstant:
        return 1.0f;
    default:
        return 0.0f;
    }
}

Targets evaluateMatrix(const ModulationInputs& in) {
    Targets targets{};
    for (const auto& slot : in.part.modMatrix) {
        if (!slot.enabled || slot.source == modSrcNone || slot.dest == modDestNone
            || slot.dest >= modDestCount || slot.depth == 0) continue;
        const float srcVal = source(in, slot.source);
        const float viaVal = slot.viaSource != modSrcNone ? source(in, slot.viaSource) : 1.0f;
        const float depthNorm = (float)slot.depth / 100.0f;
        targets[slot.dest] += srcVal * viaVal * depthNorm;
    }
    return targets;
}

namespace {

// Performance controllers after the part's pressure range; per-note MPE
// values replace the channel-wide ones.
struct Performance {
    int16_t bend;
    int32_t pressure;       // pressure after the range shift
    int32_t timbreBipolar;  // -32768..32767
};

Performance performance(const ModulationInputs& in) {
    static const int8_t pressureShift[] = { 5, 3, 1, 0 };
    const auto& e = in.expression;
    const int8_t pShift = pressureShift[std::clamp((int)in.part.steppedParams[spPressureRange], 0, 3)];
    const uint16_t rawPress = e.hasPerVoicePressure
        ? (uint16_t)std::clamp((int)std::round(e.smoothedPressure), 0, 65535) : in.pressure;
    const uint16_t rawTimbre = e.hasPerVoiceTimbre
        ? (uint16_t)std::clamp((int)std::round(e.smoothedTimbre), 0, 65535) : in.timbre;
    Performance result;
    result.bend = e.hasPerVoiceBend ? (int16_t)std::round(e.smoothedBend) : in.bend;
    result.pressure = (rawPress >> pShift);
    result.timbreBipolar = ((int32_t)rawTimbre - 32768);
    return result;
}

// Noise enters the mixer at 35 % of the oscillator scale.
constexpr float kNoiseMixGain = 0.35f;

// Resonance CV (LFOs + matrix) and the oscillator/noise mixer gains.
void resonanceAndMixer(const ModulationInputs& in, const Targets& t, VoiceControls& out) {
    const PresetData& p = in.part;
    int32_t resVal = p.continuousParams[cpResonance];
    resVal += scaleU16S16(p.continuousParams[cpLFOResAmt], in.lfo1.getOutput());
    resVal += scaleU16S16(p.continuousParams[cpLFO2ResAmt], in.lfo2.getOutput());
    resVal = __USAT(resVal, 16);

    float gainA = (float)p.continuousParams[cpAVol] / 65535.0f;
    float gainB = (float)p.continuousParams[cpBVol] / 65535.0f;
    float gainNoise = ((float)p.continuousParams[cpNoiseVol] / 65535.0f) * kNoiseMixGain;

    resVal = std::clamp((int32_t)(resVal + (int32_t)(t[modDestResonance] * 65535.0f)), 0, 65535);
    out.resonance = (uint16_t)resVal;
    out.gainA = std::clamp(gainA + t[modDestVolOscA], 0.0f, 2.0f);
    out.gainB = std::clamp(gainB + t[modDestVolOscB], 0.0f, 2.0f);
    out.gainNoise = std::clamp(gainNoise + t[modDestNoiseVol] * kNoiseMixGain, 0.0f, 1.0f);
}

void pitch(const ModulationInputs& in, const Targets& t, const Performance& perf, VoiceControls& out) {
    const PresetData& p = in.part;
    int32_t pitchAVal = 0, pitchBVal = 0;
    int32_t lfo1Pitch = scaleU16S16(p.continuousParams[cpLFOPitchAmt], in.lfo1.getOutput() >> 1);
    if (p.steppedParams[spLFOTargets] & otA) pitchAVal += lfo1Pitch;
    if (p.steppedParams[spLFOTargets] & otB) pitchBVal += lfo1Pitch;

    int32_t lfo2Pitch = scaleU16S16(p.continuousParams[cpLFO2PitchAmt], in.lfo2.getOutput() >> 1);
    if (p.steppedParams[spLFO2Targets] & otA) pitchAVal += lfo2Pitch;
    if (p.steppedParams[spLFO2Targets] & otB) pitchBVal += lfo2Pitch;

    pitchAVal += perf.bend;
    pitchBVal += perf.bend;

    pitchAVal += (int32_t)((t[modDestPitchAll] + t[modDestPitchOscA]) * (12.0f * (float)WTOSC_CV_SEMITONE));
    pitchBVal += (int32_t)((t[modDestPitchAll] + t[modDestPitchOscB]) * (12.0f * (float)WTOSC_CV_SEMITONE));
    pitchAVal -= (int32_t)(t[modDestDetune] * 256.0f);
    pitchBVal += (int32_t)(t[modDestDetune] * 256.0f);

    if (p.steppedParams[spModwheelTarget] == modPitch) {
        int32_t mwPitch = scaleU16S16(in.modwheel, in.lfo1.getOutput() >> 1);
        pitchAVal += mwPitch;
        pitchBVal += mwPitch;
    }
    if (p.steppedParams[spPressureTarget] == modPitch) {
        pitchAVal -= (perf.pressure >> 2);
        pitchBVal -= (perf.pressure >> 2);
    }
    if (p.steppedParams[spTimbreTarget] == modPitch) {
        int32_t tPitch = perf.timbreBipolar >> 5;
        pitchAVal += tPitch;
        pitchBVal += tPitch;
    }

    int32_t detuneRaw = p.continuousParams[cpDetune];
    int32_t detune = (detuneRaw >> 8) + INT8_MIN;
    pitchAVal -= (detune >> 1);
    pitchBVal += (detune >> 1);

    int32_t mTuneRaw = p.continuousParams[cpMasterTune];
    int32_t mTune = (mTuneRaw >> 7) + INT8_MIN * 2;
    pitchAVal += mTune;
    pitchBVal += mTune;

    int32_t vpa = pitchAVal + in.oscANote;
    int32_t vpb = pitchBVal + in.oscBNote;

    // Unison spread: voices 0/1 +-1, 2/3 +-2, 4/5 +-3 steps.
    const int v = in.voiceIndex;
    int16_t unisonDetuneRaw = p.continuousParams[cpUnisonDetune];
    int16_t uDetune = (int16_t)((1 + (v >> 1)) * (v & 1 ? -1 : 1) * (unisonDetuneRaw >> 9));
    vpa += uDetune;
    vpb += uDetune;

    // WaveMod type "Frequency" adds the wave modulation to the pitch.
    if (out.wmodTypeA == wmFrequency) vpa += (int32_t)out.wmodA - (int32_t)HALF_RANGE;
    if (out.wmodTypeB == wmFrequency) vpb += (int32_t)out.wmodB - (int32_t)HALF_RANGE;

    out.pitchA = (uint16_t)__USAT(vpa, 16);
    out.pitchB = (uint16_t)__USAT(vpb, 16);
}

void cutoff(const ModulationInputs& in, const Targets& t, const Performance& perf, VoiceControls& out) {
    const PresetData& p = in.part;
    int32_t filterMod = scaleU16S16(p.continuousParams[cpLFOFilAmt], in.lfo1.getOutput());
    filterMod += scaleU16S16(p.continuousParams[cpLFO2FilAmt], in.lfo2.getOutput());
    filterMod += (int32_t)(t[modDestCutoff] * 65535.0f);

    if (p.steppedParams[spModwheelTarget] == modFilter) {
        filterMod += (in.modwheel >> 2);
    }
    if (p.steppedParams[spPressureTarget] == modFilter) {
        filterMod += perf.pressure;
    }
    if (p.steppedParams[spTimbreTarget] == modFilter) {
        filterMod += (perf.timbreBipolar >> 1);
    }

    int32_t filEnvAmt = (int32_t)p.continuousParams[cpFilEnvAmt] + INT16_MIN;
    int32_t vf = filterMod;
    vf += scaleU16S16(in.voice.getFilEnv().getOutput(), filEnvAmt);
    vf += in.filterNote;
    out.cutoff = (uint16_t)__USAT(vf, 16);
}

void amp(const ModulationInputs& in, const Targets& t, const Performance& perf, VoiceControls& out) {
    const PresetData& p = in.part;
    int32_t ampVal = UINT16_MAX;
    ampVal -= scaleU16U16(p.continuousParams[cpLFOAmpAmt], in.lfo1.getLevelCV() >> 1);
    ampVal += scaleU16S16(p.continuousParams[cpLFOAmpAmt], in.lfo1.getOutput());
    ampVal -= scaleU16U16(p.continuousParams[cpLFO2AmpAmt], in.lfo2.getLevelCV() >> 1);
    ampVal += scaleU16S16(p.continuousParams[cpLFO2AmpAmt], in.lfo2.getOutput());

    if (p.steppedParams[spPressureTarget] == modVolume) {
        ampVal = std::clamp(ampVal + (perf.pressure >> 1), 0, 65535);
    }
    if (p.steppedParams[spTimbreTarget] == modVolume) {
        ampVal = std::clamp(ampVal + (perf.timbreBipolar >> 2), 0, 65535);
    }
    ampVal = std::clamp((int32_t)(ampVal + (int32_t)(t[modDestAmpLevel] * 65535.0f)), 0, 65535);
    ampVal = scaleU16U16((uint16_t)__USAT(ampVal, 16), p.continuousParams[cpAmpLevel]);

    out.amp = scaleU16U16(in.voice.getAmpEnv().getOutput(), (uint16_t)ampVal);
}

void waveMod(const ModulationInputs& in, const Targets& t, const Performance& perf, VoiceControls& out) {
    const PresetData& p = in.part;
    int32_t wmodAEnvAmt = (int32_t)p.continuousParams[cpWModAEnv] + INT16_MIN;
    int32_t wmodBEnvAmt = (int32_t)p.continuousParams[cpWModBEnv] + INT16_MIN;

    // Frequency modulation uses half the base amount (firmware: "half scale").
    auto baseWMod = [&](continuousParameter_t cp, steppedParameter_t type) {
        const int32_t base = p.continuousParams[cp];
        return p.steppedParams[type] == wmFrequency ? ((base - (int32_t)HALF_RANGE) >> 1) + (int32_t)HALF_RANGE : base;
    };
    int32_t wmodAVal = baseWMod(cpABaseWMod, spAWModType);
    if (p.steppedParams[spLFOTargets] & otA)
        wmodAVal += scaleU16S16(p.continuousParams[cpLFOWModAmt], in.lfo1.getOutput());
    if (p.steppedParams[spLFO2Targets] & otA)
        wmodAVal += scaleU16S16(p.continuousParams[cpLFO2WModAmt], in.lfo2.getOutput());

    int32_t wmodBVal = baseWMod(cpBBaseWMod, spBWModType);
    if (p.steppedParams[spLFOTargets] & otB)
        wmodBVal += scaleU16S16(p.continuousParams[cpLFOWModAmt], in.lfo1.getOutput());
    if (p.steppedParams[spLFO2Targets] & otB)
        wmodBVal += scaleU16S16(p.continuousParams[cpLFO2WModAmt], in.lfo2.getOutput());

    if (p.steppedParams[spModwheelTarget] == modWaveMod) {
        wmodAVal += (in.modwheel >> 2);
        wmodBVal += (in.modwheel >> 2);
    }
    if (p.steppedParams[spPressureTarget] == modWaveMod) {
        wmodAVal += perf.pressure;
        wmodBVal += perf.pressure;
    }
    if (p.steppedParams[spTimbreTarget] == modWaveMod) {
        wmodAVal += (perf.timbreBipolar >> 1);
        wmodBVal += (perf.timbreBipolar >> 1);
    }

    wmodAVal += (int32_t)((t[modDestWaveModAll] + t[modDestWaveModOscA]) * 65535.0f);
    wmodBVal += (int32_t)((t[modDestWaveModAll] + t[modDestWaveModOscB]) * 65535.0f);

    int32_t vma = wmodAVal + scaleU16S16(in.voice.getWmodEnv().getOutput(), wmodAEnvAmt);
    int32_t vmb = wmodBVal + scaleU16S16(in.voice.getWmodEnv().getOutput(), wmodBEnvAmt);
    out.wmodA = (uint16_t)__USAT(vma, 16);
    out.wmodB = (uint16_t)__USAT(vmb, 16);
    out.wmodTypeA = (oscWModTarget_t)p.steppedParams[spAWModType];
    out.wmodTypeB = (oscWModTarget_t)p.steppedParams[spBWModType];
}

ElementsControls elements(const ModulationInputs& in, const Targets& t, uint16_t pitchA) {
    const PresetData& p = in.part;
    auto knob = [&](continuousParameter_t cp, modDest_t dest) {
        return std::clamp(((float)p.continuousParams[cp] / 65535.0f) + t[dest], 0.0f, 1.0f);
    };
    ElementsControls c;
    c.geometry = knob(cpElementsGeometry, modDestElementsGeometry);
    c.brightness = knob(cpElementsBrightness, modDestElementsBrightness);
    c.damping = knob(cpElementsDamping, modDestElementsDamping);
    c.position = knob(cpElementsPosition, modDestElementsPosition);
    c.space = knob(cpElementsSpace, modDestElementsSpace);
    c.bow = knob(cpElementsBow, modDestElementsBow);
    c.blow = knob(cpElementsBlow, modDestElementsBlow);
    c.strike = knob(cpElementsStrike, modDestElementsStrike);
    c.mallet = std::clamp((float)p.continuousParams[cpElementsMallet] / 65535.0f, 0.0f, 1.0f);
    c.model = p.steppedParams[spElementsModel];
    c.pitchMidiNote = (float)pitchA / (float)WTOSC_CV_SEMITONE;
    return c;
}

} // namespace

VoiceControls computeVoiceControls(const ModulationInputs& in) {
    const Targets targets = evaluateMatrix(in);
    const Performance perf = performance(in);
    VoiceControls out;
    resonanceAndMixer(in, targets, out);
    waveMod(in, targets, perf, out);   // before pitch: "Frequency" feeds the pitch
    pitch(in, targets, perf, out);
    cutoff(in, targets, perf, out);
    amp(in, targets, perf, out);
    out.hardSync = in.part.steppedParams[spOscSync] != 0;
    out.oscEngine = in.part.steppedParams[spOscEngine];
    if (out.oscEngine != oeWavetable) out.elements = elements(in, targets, out.pitchA);
    return out;
}

void apply(Voice& voice, const VoiceControls& c) {
    voice.updateVoiceCVs(c.pitchA, c.pitchB, c.wmodTypeA, c.wmodA, c.wmodTypeB, c.wmodB,
                         c.cutoff, c.resonance, c.amp, c.gainA, c.gainB, c.gainNoise, c.hardSync);
    voice.setOscEngine(c.oscEngine);
    if (c.oscEngine != oeWavetable) {
        const auto& e = c.elements;
        voice.updateElementsParams(e.model, e.geometry, e.brightness, e.damping, e.position, e.space,
                                   e.bow, e.blow, e.strike, e.mallet, e.pitchMidiNote);
    }
}

} // namespace modulation
