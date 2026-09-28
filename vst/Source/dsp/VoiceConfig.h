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
};
inline constexpr EnvelopeParams kFilterEnvelope{
    cpFilAtt, cpFilDec, cpFilSus, cpFilRel, cpFilVelocity, spFilEnvLin, spFilEnvLoop, spFilEnvSlow};
inline constexpr EnvelopeParams kAmpEnvelope{
    cpAmpAtt, cpAmpDec, cpAmpSus, cpAmpRel, cpAmpVelocity, spAmpEnvLin, spAmpEnvLoop, spAmpEnvSlow};
inline constexpr EnvelopeParams kWaveModEnvelope{
    cpWModAtt, cpWModDec, cpWModSus, cpWModRel, cpWModVelocity, spWModEnvLin, spWModEnvLoop, spWModEnvSlow};

// ADSR times; the level is reset to full scale until the velocity is applied.
inline void applyEnvelopeTimes(AdsrEnv& env, const PresetData& p, const EnvelopeParams& e) {
    env.setCVs(p.continuousParams[e.attack], p.continuousParams[e.decay],
               p.continuousParams[e.sustain], p.continuousParams[e.release], UINT16_MAX, 0x1F);
}
inline void applyEnvelopeShape(AdsrEnv& env, const PresetData& p, const EnvelopeParams& e) {
    env.setShape(p.steppedParams[e.linear] ? 0 : 1, p.steppedParams[e.loop]);
}
inline void applyEnvelopeSpeed(AdsrEnv& env, const PresetData& p, const EnvelopeParams& e) {
    env.setSpeedShift(p.steppedParams[e.slow] ? 2 : 0);
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
