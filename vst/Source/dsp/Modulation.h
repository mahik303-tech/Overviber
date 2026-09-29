#pragma once

#include "OvercyclerTypes.h"
#include "Voice.h"
#include "lfo.h"
#include "../data/PresetManager.h"
#include <array>

// ==============================================================================
// Control-rate modulation of one voice (~4 kHz).
//
// The engine collects everything a voice's modulation reads into
// ModulationInputs; computeVoiceControls() turns it into the CVs and gains
// the voice renders with. The functions read nothing else, so they can be
// tested and changed without the rest of the engine.
// ==============================================================================

struct VoiceExpressionState {
    int16_t pitchBendOffset = 0;       // -8192..8191 scaled to active bend range (in 1/256 semitone units)
    float smoothedBend = 0.0f;
    uint16_t pressure = 0;             // 0..65535 (from Poly-AT or per-channel pressure)
    float smoothedPressure = 0.0f;
    uint16_t timbre = 0;               // 0..65535 (from CC 74 Slide/Y-axis)
    float smoothedTimbre = 0.0f;
    uint16_t noteOnVelocity = 0;       // 0..65535 (16-bit high-resolution)
    uint16_t noteOffVelocity = 0;      // 0..65535 (16-bit high-resolution lift)
    uint8_t noteNumber = ASSIGNER_NO_NOTE;
    uint8_t midiChannel = 1;
    bool hasPerVoicePressure = false;
    bool hasPerVoiceTimbre = false;
    bool hasPerVoiceBend = false;

    void reset() { *this = VoiceExpressionState{}; }
};

struct ModulationInputs {
    const PresetData& part;            // preset of the voice's part
    const LfoModule& lfo1;             // LFOs of that part
    const LfoModule& lfo2;
    const Voice& voice;                // envelope outputs
    const VoiceExpressionState& expression;
    int voiceIndex;                    // unison detune spread
    // Channel-wide controllers, used where the voice has no per-note value.
    int16_t bend;
    uint16_t modwheel, pressure, timbre, breath, expression11;
    // Note CVs after glide and filter tracking.
    uint16_t oscANote, oscBNote, filterNote;
};

namespace modulation {

// Value of one matrix source: bipolar sources -1..1, unipolar 0..1.
float source(const ModulationInputs& in, uint8_t src);

// Summed matrix depth per destination, in full-scale units.
using Targets = std::array<float, modDestCount>;
Targets evaluateMatrix(const ModulationInputs& in);


struct VoiceControls {
    uint16_t pitchA = 0, pitchB = 0;
    oscWModTarget_t wmodTypeA = wmOff, wmodTypeB = wmOff;
    uint16_t wmodA = 0, wmodB = 0;
    uint16_t cutoff = 0, resonance = 0, amp = 0;
    float gainA = 0, gainB = 0, gainNoise = 0;
    bool hardSync = false;
    uint8_t oscEngine = oeWavetable;
    ElementsControls elements;         // used unless oscEngine == oeWavetable
};

VoiceControls computeVoiceControls(const ModulationInputs& in);

// Channel bend (full scale) in the units of the part's bender target, as the
// firmware's wheel event: pitch +-4/7/12 semitones (spBenderRange), filter
// four times that in semitones, volume and WaveMod bend/12 x range.
int32_t benderAmount(const PresetData& part, int16_t bend);

// Pressure after the part's range shift (firmware synth_pressureEvent).
uint16_t pressureAmount(const PresetData& part, uint16_t pressure);

// LFO 1 and LFO 2 amounts of a part: the knobs plus modwheel (spModwheelRange,
// on LFO 1 or LFO 2 by spModwheelTarget), pressure and timbre when they
// target an LFO; the start delay level scales the LFO the wheel does not
// control (firmware refreshLfoSettings).
std::array<uint16_t, 2> lfoAmounts(const PresetData& part, uint16_t modwheel, uint16_t pressure,
                                   uint16_t timbre, uint16_t delayLevel);

// Hands the computed controls to the voice.
void apply(Voice& voice, const VoiceControls& controls);

} // namespace modulation
