#include "MidiInput.h"

void MidiInput::reset() {
    for (auto& v : voices) v.reset();
    bend = 0;
    modwheel = 0;
    pressure = 0;
    timbre = 0;
}

void MidiInput::releaseAll() {
    for (auto& v : voices) v.reset();
    pressure = 0;
    bend = 0;
}

void MidiInput::setPitchBend(int16_t value) {
    bend = (int16_t)(std::clamp<int32_t>(value, -8192, 8191) * 4);
}

static inline int mpeBendSemitones(uint8_t mpeBendRange) {
    switch (mpeBendRange) {
    case 0: return 2;
    case 1: return 12;
    case 2: return 24;
    case 3: return 48;
    case 4: return 96;
    default: return 24;
    }
}

void MidiInput::setChannelPitchBend(uint8_t channel, int16_t value, uint8_t mpeBendRange) {
    const int16_t bendCv = (int16_t)(((int32_t)value * mpeBendSemitones(mpeBendRange) * WTOSC_CV_SEMITONE) / 8192);
    for (auto& v : voices) {
        if (v.midiChannel == channel) {
            v.pitchBendOffset = bendCv;
            v.hasPerVoiceBend = true;
        }
    }
}

void MidiInput::setChannelPressure(uint8_t channel, uint16_t value) {
    for (auto& v : voices) if (v.midiChannel == channel) {
        v.pressure = value;
        v.hasPerVoicePressure = true;
    }
}

void MidiInput::setChannelTimbre(uint8_t channel, uint16_t value) {
    for (auto& v : voices) if (v.midiChannel == channel) {
        v.timbre = value;
        v.hasPerVoiceTimbre = true;
    }
}

void MidiInput::setVoicePressure(int voice, uint16_t value) {
    voices[voice].pressure = value;
    voices[voice].hasPerVoicePressure = true;
}

void MidiInput::noteStarted(int voice, uint8_t note, uint8_t channel, uint16_t velocity) {
    voices[voice].noteNumber = note;
    voices[voice].midiChannel = channel;
    voices[voice].noteOnVelocity = velocity;
}

void MidiInput::smooth(int voice) {
    const float alpha = 0.12f;
    auto& v = voices[voice];
    v.smoothedBend += alpha * ((float)v.pitchBendOffset - v.smoothedBend);
    v.smoothedPressure += alpha * ((float)v.pressure - v.smoothedPressure);
    v.smoothedTimbre += alpha * ((float)v.timbre - v.smoothedTimbre);
}
