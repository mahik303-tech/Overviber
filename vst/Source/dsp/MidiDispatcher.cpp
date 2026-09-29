#include "MidiDispatcher.h"

namespace mididispatch {
namespace {

struct CcEntry {
    uint8_t cc;
    CcTarget target;
};

constexpr CcTarget cp(continuousParameter_t p) { return { CcTarget::continuous, static_cast<uint8_t>(p), 0 }; }
constexpr CcTarget sp(steppedParameter_t p, uint8_t maximum) { return { CcTarget::stepped, static_cast<uint8_t>(p), maximum }; }

// Standard DAW CCs first: they win over the hardware CCs (CC 10, pan, is
// static per voice in the Overcycler and deliberately unmapped).
constexpr CcEntry kCcTable[] = {
    { 7, cp(cpAmpLevel) }, { 5, cp(cpGlide) }, { 74, cp(cpCutoff) }, { 71, cp(cpResonance) },
    { 73, cp(cpAmpAtt) }, { 72, cp(cpAmpRel) }, { 10, {} },
    // Overcycler hardware CCs
    { 12, cp(cpAFreq) }, { 13, cp(cpAVol) }, { 14, cp(cpABaseWMod) },
    { 15, cp(cpBFreq) }, { 16, cp(cpBVol) }, { 17, cp(cpBBaseWMod) },
    { 18, cp(cpDetune) }, { 19, cp(cpCutoff) }, { 20, cp(cpResonance) },
    { 21, cp(cpFilEnvAmt) }, { 22, cp(cpFilKbdAmt) }, { 23, cp(cpWModAEnv) },
    { 24, cp(cpFilAtt) }, { 25, cp(cpFilDec) }, { 26, cp(cpFilSus) }, { 27, cp(cpFilRel) },
    { 28, cp(cpAmpAtt) }, { 29, cp(cpAmpDec) }, { 30, cp(cpAmpSus) }, { 31, cp(cpAmpRel) },
    { 35, cp(cpAmpLevel) },
    { 44, cp(cpLFOFreq) }, { 45, cp(cpLFOAmt) }, { 46, cp(cpLFOPitchAmt) },
    { 47, cp(cpLFOWModAmt) }, { 48, cp(cpLFOFilAmt) }, { 49, cp(cpLFOAmpAmt) },
    { 50, cp(cpLFO2Freq) }, { 51, cp(cpLFO2Amt) }, { 52, cp(cpModDelay) }, { 53, cp(cpGlide) },
    { 54, cp(cpAmpVelocity) }, { 55, cp(cpFilVelocity) }, { 56, cp(cpMasterTune) },
    { 57, cp(cpUnisonDetune) }, { 58, cp(cpNoiseVol) },
    { 59, cp(cpLFO2PitchAmt) }, { 60, cp(cpLFO2WModAmt) }, { 61, cp(cpLFO2FilAmt) },
    { 62, cp(cpLFO2AmpAmt) }, { 63, cp(cpLFOResAmt) }, { 70, cp(cpLFO2ResAmt) },
    { 75, cp(cpWModBEnv) }, { 76, cp(cpWModVelocity) },
    { 80, sp(spAWModType, 6) }, { 83, sp(spBWModType, 6) }, { 84, sp(spLFOShape, 6) },
    { 85, sp(spLFOTargets, 3) }, { 86, sp(spFilEnvSlow, 1) }, { 87, sp(spAmpEnvSlow, 1) },
    { 88, sp(spBenderRange, 2) }, { 89, sp(spBenderTarget, 4) }, { 90, sp(spModwheelRange, 3) },
    { 91, sp(spModwheelTarget, 1) }, { 92, sp(spUnison, 1) }, { 93, sp(spAssignerPriority, 2) },
    { 94, sp(spChromaticPitch, 2) }, { 95, sp(spOscSync, 1) }, { 107, sp(spVoiceCount, 5) },
};

void parameterCC(SynthEngine& engine, uint8_t cc, uint8_t value, Listener* listener) {
    const CcTarget target = ccTarget(cc);
    if (target.kind == CcTarget::continuous) {
        const auto p = static_cast<continuousParameter_t>(target.param);
        const auto u16 = static_cast<uint16_t>(scan_potTo16bits((value * 999) / 127));
        engine.setContinuousParam(p, u16);
        if (listener) listener->continuousParam(p, u16);
    } else if (target.kind == CcTarget::stepped) {
        const auto p = static_cast<steppedParameter_t>(target.param);
        const auto step = static_cast<uint8_t>(value * target.maximum / 127);
        engine.setSteppedParam(p, step);
        if (listener) listener->steppedParam(p, step);
    }
}

void controller(SynthEngine& engine, uint8_t cc, uint8_t value, uint8_t channel, Listener* listener) {
    switch (cc) {
    case 1: engine.modWheel(to16(value), channel); break;
    case 2: engine.breathController(to16(value), channel); break;
    case 11: engine.expressionController(to16(value), channel); break;
    case 64: engine.holdPedal(value >= 64); break;
    case 120:
    case 123: engine.allNotesOff(); break;
    case 74:   // MPE Y axis on member channels, brightness (cutoff) otherwise
        if (engine.isMpeMemberChannel(channel)) engine.timbreSlide(to16(value), channel);
        else parameterCC(engine, cc, value, listener);
        break;
    default: parameterCC(engine, cc, value, listener); break;
    }
}

}  // namespace

CcTarget ccTarget(uint8_t cc) {
    for (const auto& entry : kCcTable)
        if (entry.cc == cc) return entry.target;
    return {};
}

void dispatch(SynthEngine& engine, const uint8_t* data, int size, int acceptedChannel, Listener* listener) {
    if (data == nullptr || size < 1 || size > 3) return;
    const uint8_t status = data[0];
    if (status < 0x80 || status >= 0xF0) return;   // running status / system messages
    const auto channel = static_cast<uint8_t>((status & 0x0F) + 1);
    if (acceptedChannel > 0 && channel != acceptedChannel) return;
    const uint8_t d1 = size > 1 ? data[1] : 0;
    const uint8_t d2 = size > 2 ? data[2] : 0;

    switch (status & 0xF0) {
    case 0x90:
        if (d2 > 0) engine.noteOn(d1, to16(d2), channel);
        else engine.noteOff(d1, 0, channel);
        break;
    case 0x80: engine.noteOff(d1, to16(d2), channel); break;
    case 0xE0: engine.pitchBend(static_cast<int16_t>((d1 | (d2 << 7)) - 8192), channel); break;
    case 0xA0: engine.polyAftertouch(d1, to16(d2), channel); break;
    case 0xD0: engine.channelPressure(to16(d1), channel); break;
    case 0xB0: controller(engine, d1, d2, channel, listener); break;
    case 0xC0: if (listener) listener->programChange(d1); break;
    default: break;
    }
}

}  // namespace mididispatch
