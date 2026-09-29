#pragma once

#include "Modulation.h"

// ==============================================================================
// Controller state of the MIDI input.
//
// Holds the channel-wide controllers and the per-note expression of each voice
// (MPE member channels, polyphonic aftertouch, note-on/off velocity). The
// engine decides which messages reach it and which voices a per-note message
// addresses; this class converts, stores and smooths the values.
// ==============================================================================
class MidiInput {
public:
    // Engine reset: per-note state and bend/modwheel/pressure/timbre.
    // Breath and expression keep their last value.
    void reset();
    // All notes off: per-note state, channel pressure and bend.
    void releaseAll();

    // Channel-wide controllers.
    void setPitchBend(int16_t bend);    // -8192..8191; range and target are per part (Modulation)
    void setPressure(uint16_t value) { pressure = value; }
    void setTimbre(uint16_t value) { timbre = value; }
    void setModWheel(uint16_t value) { modwheel = value; }
    void setBreath(uint16_t value) { breath = value; }
    void setExpression(uint16_t value) { expression = value; }

    // Per-note expression of MPE member channels, addressed by channel.
    void setChannelPitchBend(uint8_t channel, int16_t bend, uint8_t mpeBendRange); // spMPEPitchBendRange
    void setChannelPressure(uint8_t channel, uint16_t value);
    void setChannelTimbre(uint8_t channel, uint16_t value);

    // Per-note expression addressed by voice.
    void setVoicePressure(int voice, uint16_t value);
    void noteStarted(int voice, uint8_t note, uint8_t channel, uint16_t velocity);
    void setReleaseVelocity(int voice, uint16_t velocity) { voices[voice].noteOffVelocity = velocity; }

    // Control-rate smoothing of the per-note values of one voice.
    void smooth(int voice);

    const VoiceExpressionState& voice(int v) const { return voices[v]; }
    int16_t getPitchBend() const { return bend; }
    uint16_t getModWheel() const { return modwheel; }
    uint16_t getPressure() const { return pressure; }
    uint16_t getTimbre() const { return timbre; }
    uint16_t getBreath() const { return breath; }
    uint16_t getExpression() const { return expression; }

private:
    VoiceExpressionState voices[SYNTH_VOICE_COUNT];
    int16_t bend = 0;        // full scale -32768..32764, as in the firmware's wheel event
    uint16_t modwheel = 0;
    uint16_t pressure = 0;
    uint16_t timbre = 0;
    uint16_t breath = 0;
    uint16_t expression = 0;
};
