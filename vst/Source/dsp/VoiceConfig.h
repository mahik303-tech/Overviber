#pragma once

#include "OvercyclerTypes.h"
#include "Voice.h"
#include "../data/PresetManager.h"

// ==============================================================================
// Translation of one part's preset into the settings of a voice.
//
// The engine configures voices on note-on (from the voice's part), on preset
// and control changes (all voices following the main part) and on single
// parameter changes. All of these paths use the functions below, so a part
// preset always maps to the same DSP settings, whichever path applies it.
// The call order within configureVoice() is significant: the filter model is
// committed before the Shelves bands, because committing a filter reapplies
// the cutoff/resonance CV that the Shelves band 1 settings then replace.
// ==============================================================================
namespace voiceconfig {

struct EnvelopeParams {
    continuousParameter_t attack, decay, sustain, release, velocity;
    steppedParameter_t linear, loop, slow;
    AdsrEnv& (Voice::*envelope)();   // the voice's envelope these parameters set
};
inline constexpr EnvelopeParams kFilterEnvelope{
    cpFilAtt, cpFilDec, cpFilSus, cpFilRel, cpFilVelocity, spFilEnvLin, spFilEnvLoop, spFilEnvSlow,
    static_cast<AdsrEnv& (Voice::*)()>(&Voice::getFilEnv)};
inline constexpr EnvelopeParams kAmpEnvelope{
    cpAmpAtt, cpAmpDec, cpAmpSus, cpAmpRel, cpAmpVelocity, spAmpEnvLin, spAmpEnvLoop, spAmpEnvSlow,
    static_cast<AdsrEnv& (Voice::*)()>(&Voice::getAmpEnv)};
inline constexpr EnvelopeParams kWaveModEnvelope{
    cpWModAtt, cpWModDec, cpWModSus, cpWModRel, cpWModVelocity, spWModEnvLin, spWModEnvLoop, spWModEnvSlow,
    static_cast<AdsrEnv& (Voice::*)()>(&Voice::getWmodEnv)};
inline constexpr const EnvelopeParams* kEnvelopes[] = { &kFilterEnvelope, &kAmpEnvelope, &kWaveModEnvelope };

// The envelope whose stage time (attack, decay, sustain, release), curve
// (linear, loop) or speed range (slow) a parameter sets; nullptr if none.
inline const EnvelopeParams* envelopeWithTime(continuousParameter_t cp) {
    for (const auto* e : kEnvelopes)
        if (cp == e->attack || cp == e->decay || cp == e->sustain || cp == e->release) return e;
    return nullptr;
}
inline const EnvelopeParams* envelopeWithShape(steppedParameter_t sp) {
    for (const auto* e : kEnvelopes)
        if (sp == e->linear || sp == e->loop) return e;
    return nullptr;
}
inline const EnvelopeParams* envelopeWithSpeed(steppedParameter_t sp) {
    for (const auto* e : kEnvelopes)
        if (sp == e->slow) return e;
    return nullptr;
}

// ADSR times; the level is reset to full scale until the velocity is applied.
inline void applyEnvelopeTimes(AdsrEnv& env, const PresetData& p, const EnvelopeParams& e) {
    env.setCVs(p.continuousParams[e.attack], p.continuousParams[e.decay],
               p.continuousParams[e.sustain], p.continuousParams[e.release], UINT16_MAX, 0x1F);
}
inline void applyEnvelopeShape(AdsrEnv& env, const PresetData& p, const EnvelopeParams& e) {
    env.setShape(p.steppedParams[e.linear] ? 0 : 1, p.steppedParams[e.loop]);
}
inline void applyEnvelopeSpeed(AdsrEnv& env, const PresetData& p, const EnvelopeParams& e) {
    env.setSpeedShift(adsrSpeedShift(p.steppedParams[e.slow] != 0));
}
inline void applyEnvelope(AdsrEnv& env, const PresetData& p, const EnvelopeParams& e) {
    applyEnvelopeTimes(env, p, e);
    applyEnvelopeShape(env, p, e);
    applyEnvelopeSpeed(env, p, e);
}
// Peak level from note-on velocity: amount 0 = full level, full amount = velocity.
inline void applyEnvelopeVelocity(AdsrEnv& env, const PresetData& p, const EnvelopeParams& e, uint16_t velocity) {
    const uint16_t amount = p.continuousParams[e.velocity];
    env.setCVs(0, 0, 0, 0, (UINT16_MAX - amount) + scaleU16U16(velocity, amount), 0x10);
}

// Release velocity (lift) at note-off: with spReleaseVelocityAmt 1..3 a
// faster key release shortens the amp release (from the preset's release
// time; the next note-on restores it).
inline void applyReleaseVelocity(Voice& voice, const PresetData& p, uint16_t liftVelocity) {
    const uint8_t amount = p.steppedParams[spReleaseVelocityAmt];
    if (amount == 0 || liftVelocity == 0) return;
    const uint16_t baseRelease = p.continuousParams[cpAmpRel];
    const uint32_t scaledRelease = (baseRelease * (65535U - (liftVelocity >> (4 - amount)))) >> 16;
    voice.getAmpEnv().setCVs(0, 0, 0, (uint16_t)scaledRelease, UINT16_MAX, 0x08);
}

inline void applyEnvelopes(Voice& voice, const PresetData& p) {
    applyEnvelope(voice.getFilEnv(), p, kFilterEnvelope);
    applyEnvelope(voice.getAmpEnv(), p, kAmpEnvelope);
    applyEnvelope(voice.getWmodEnv(), p, kWaveModEnvelope);
}
inline void applyVelocity(Voice& voice, const PresetData& p, uint16_t velocity) {
    applyEnvelopeVelocity(voice.getWmodEnv(), p, kWaveModEnvelope, velocity);
    applyEnvelopeVelocity(voice.getFilEnv(), p, kFilterEnvelope, velocity);
    applyEnvelopeVelocity(voice.getAmpEnv(), p, kAmpEnvelope, velocity);
}

// Shelves: band 1 shares cutoff (frequency) and resonance (Q) with the filter.
inline void applyShelves(Voice& voice, const PresetData& p) {
    auto unipolar = [&](continuousParameter_t cp) { return (float)p.continuousParams[cp] / 65535.0f; };
    auto bipolar = [&](continuousParameter_t cp) { return ((float)p.continuousParams[cp] - 32768.0f) / 32768.0f; };
    voice.setShelvesEQParams(unipolar(cpShelvesLsFreq), bipolar(cpShelvesLsGain),
                             unipolar(cpCutoff), bipolar(cpShelvesP1Gain), unipolar(cpResonance),
                             unipolar(cpShelvesP2Freq), bipolar(cpShelvesP2Gain), unipolar(cpShelvesP2Q),
                             unipolar(cpShelvesHsFreq), bipolar(cpShelvesHsGain));
}
inline void applyFilterModel(Voice& voice, const PresetData& p) {
    voice.setFilterModelAndMode(p.steppedParams[spFilterModel], p.steppedParams[spFilterMode],
                                p.steppedParams[spSemModel]);
}

// Complete part configuration of a voice at note-on (wave data excluded).
inline void configureVoice(Voice& voice, const PresetData& p, uint16_t velocity) {
    applyFilterModel(voice, p);
    applyEnvelopes(voice, p);
    applyVelocity(voice, p, velocity);
    applyShelves(voice, p);
}

} // namespace voiceconfig
