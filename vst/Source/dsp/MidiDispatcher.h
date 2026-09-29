#pragma once

#include "SynthEngine.h"
#include <cstdint>

// ==============================================================================
// The plugin's MIDI input: turns one short MIDI message into engine calls.
//
// OvercyclerAudioProcessor and the engine tests share it, so the tests play
// exactly what a host sends. Messages of more than three bytes (SysEx) and
// system messages are ignored. A note-on with velocity 0 is a note-off.
//
// Controllers: CC 1 mod wheel, 2 breath, 11 expression, 64 sustain, 120/123
// all notes off, 74 timbre on MPE member channels; every other mapped CC sets
// a preset parameter (standard CCs first, then the Overcycler's hardware
// CCs), see ccTarget().
// ==============================================================================
namespace mididispatch {

// 7-bit MIDI value to the engine's 16-bit range.
inline uint16_t to16(uint8_t value) { return static_cast<uint16_t>((static_cast<uint32_t>(value) * 65535U) / 127U); }

// The preset parameter a CC sets, if any. A continuous parameter takes the
// value as pot position (value * 999 / 127); a stepped one value * maximum / 127.
struct CcTarget {
    enum Kind : uint8_t { none, continuous, stepped };
    Kind kind = none;
    uint8_t param = 0;
    uint8_t maximum = 0;   // stepped parameters
};
CcTarget ccTarget(uint8_t cc);

// Effects outside the engine: the processor mirrors parameter changes into
// its host parameters and loads programs on its message thread.
struct Listener {
    virtual ~Listener() = default;
    virtual void continuousParam(continuousParameter_t, uint16_t) {}
    virtual void steppedParam(steppedParameter_t, uint8_t) {}
    virtual void programChange(int) {}
};

// `acceptedChannel` 1..16 accepts only that channel, 0 all channels.
void dispatch(SynthEngine& engine, const uint8_t* data, int size, int acceptedChannel = 0,
              Listener* listener = nullptr);

}  // namespace mididispatch
