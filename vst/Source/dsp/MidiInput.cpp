#include "MidiInput.h"
#include <cmath>
#include <initializer_list>

void MidiInput::reset() {
    for (auto& v : voices) v.reset();
    bend.snap(0);
    modwheel.snap(0);
    pressure.snap(0);
    timbre.snap(0);
}

void MidiInput::releaseAll() {
    for (auto& v : voices) v.reset();
    pressure.snap(0);
    bend.snap(0);
}

void MidiInput::setPitchBend(int16_t value) {
    bend.set(std::clamp<int32_t>(value, -8192, 8191) * 4);
}

void MidiInput::Controller::smooth(float alpha) {
    if (output == target) return;
    value += alpha * (static_cast<float>(target) - value);
    output = static_cast<int32_t>(std::lround(value));
    // The last step lands on the value itself.
    if (std::abs(static_cast<float>(target) - value) < 0.5f) snap(target);
}

void MidiInput::smoothControllers() {
    static const float alpha = 1.0f - std::exp(-1.0f / (kControllerSmoothingSeconds * (float)DACSPI_UPDATE_HZ));
    for (auto* c : { &bend, &modwheel, &pressure, &timbre, &breath, &expression }) c->smooth(alpha);
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
