#pragma once

#include "Modulation.h"

// ==============================================================================
// Controller state of the MIDI input.
//
// Holds the channel-wide controllers and the per-note expression of each voice
// (MPE member channels, polyphonic aftertouch, note-on/off velocity). The
// engine decides which messages reach it and which voices a per-note message
// addresses; this class converts, stores and smooths the values.
//
// The channel controllers glide to each new value at control rate
// (smoothControllers, time constant kControllerSmoothingSeconds): a 7-bit
// CC moves in steps of 1/127 every few milliseconds, which on cutoff,
// volume or pitch is audible as zipper noise. The getters return the
// smoothed values.
// ==============================================================================
class MidiInput {
public:
    // Engine reset: per-note state and bend/modwheel/pressure/timbre.
    // Breath and expression keep their last value.
    void reset();
    // Per-note state only (a preset change); the channel controllers stay.
    void resetNotes() { for (auto& v : voices) v.reset(); }
    // All notes off: per-note state, channel pressure and bend.
    void releaseAll();

    // Channel-wide controllers.
    void setPitchBend(int16_t bend);    // -8192..8191; range and target are per part (Modulation)
    void setPressure(uint16_t value) { pressure.set(value); }
    void setTimbre(uint16_t value) { timbre.set(value); }
    void setModWheel(uint16_t value) { modwheel.set(value); }
    void setBreath(uint16_t value) { breath.set(value); }
    void setExpression(uint16_t value) { expression.set(value); }

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
    // Control-rate smoothing of the channel controllers (DACSPI_UPDATE_HZ).
    void smoothControllers();
    static constexpr float kControllerSmoothingSeconds = 0.005f;

    const VoiceExpressionState& voice(int v) const { return voices[v]; }
    // Smoothed, as the modulation reads them.
    int16_t getPitchBend() const { return static_cast<int16_t>(bend.output); }
    uint16_t getModWheel() const { return static_cast<uint16_t>(modwheel.output); }
    uint16_t getPressure() const { return static_cast<uint16_t>(pressure.output); }
    uint16_t getTimbre() const { return static_cast<uint16_t>(timbre.output); }
    uint16_t getBreath() const { return static_cast<uint16_t>(breath.output); }
    uint16_t getExpression() const { return static_cast<uint16_t>(expression.output); }
    // As received (the smoothing's targets).
    int16_t receivedPitchBend() const { return static_cast<int16_t>(bend.target); }
    uint16_t receivedModWheel() const { return static_cast<uint16_t>(modwheel.target); }
    uint16_t receivedPressure() const { return static_cast<uint16_t>(pressure.target); }
    uint16_t receivedTimbre() const { return static_cast<uint16_t>(timbre.target); }
    uint16_t receivedBreath() const { return static_cast<uint16_t>(breath.target); }
    uint16_t receivedExpression() const { return static_cast<uint16_t>(expression.target); }

private:
    // A controller's last value and its smoothed value.
    struct Controller {
        int32_t target = 0;
        float value = 0.0f;
        int32_t output = 0;    // value rounded
        void set(int32_t v) { target = v; }
        void snap(int32_t v) { target = output = v; value = static_cast<float>(v); }
        void smooth(float alpha);
    };
    VoiceExpressionState voices[SYNTH_VOICE_COUNT];
    Controller bend;         // full scale -32768..32764, as in the firmware's wheel event
    Controller modwheel, pressure, timbre, breath, expression;
};
