#include "dsp/SynthEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    const juce::File input(argc > 1 ? argv[1] : "test poly.mid");
    const int presetNumber = argc > 2 ? std::atoi(argv[2]) : 48;
    auto stream = input.createInputStream();
    juce::MidiFile file;
    if (!stream || !file.readFrom(*stream)) {
        std::cerr << "Cannot read MIDI file: " << input.getFullPathName() << '\n';
        return 1;
    }

    file.convertTimestampTicksToSeconds();
    juce::MidiMessageSequence events;
    for (int track = 0; track < file.getNumTracks(); ++track)
        events.addSequence(*file.getTrack(track), 0);

    constexpr int rate = 44100;
    auto engine = std::make_unique<SynthEngine>();
    engine->getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
    engine->getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
    engine->prepare(rate);
    int preset = -1;
    for (int index = 0; index < engine->getPresetManager().getPresetCount(); ++index)
        if (engine->getPresetManager().getPresetNumber(index) == presetNumber) preset = index;
    if (preset < 0) return 1;
    engine->loadPreset(preset);
    const int expectedVoices = engine->getCurrentPreset().steppedParams[spVoiceCount] + 1;

    int event = 0, failures = 0, lastNoteOnFrame = -1;
    float left[64]{}, right[64]{};
    const int frames = static_cast<int>((events.getEndTime() + 0.1) * rate);
    for (int frame = 0; frame < frames; frame += 64) {
        while (event < events.getNumEvents()
               && std::llround(events.getEventTime(event) * rate) <= frame) {
            const auto& message = events.getEventPointer(event++)->message;
            const auto velocity = static_cast<uint16_t>(message.getVelocity() * 65535.0f / 127.0f);
            if (message.isNoteOn()) {
                engine->noteOn(message.getNoteNumber(), velocity, message.getChannel());
                lastNoteOnFrame = frame;
            }
            else if (message.isNoteOff()) engine->noteOff(message.getNoteNumber(), velocity, message.getChannel());
        }
        engine->renderBlock(left, right, 64);

        // Wait for the 250 Hz envelope tick before sampling the active voices.
        if (lastNoteOnFrame >= 0 && frame - lastNoteOnFrame >= rate / 100) {
            int active = 0;
            std::cout << "t=" << static_cast<double>(frame) / rate << " voices:";
            for (int voice = 0; voice < SYNTH_VOICE_COUNT; ++voice) {
                const bool on = engine->getVoiceAmpLevel(voice) > 0;
                std::cout << ' ' << on;
                active += on;
            }
            double leftEnergy = 0.0, rightEnergy = 0.0;
            for (int sample = 0; sample < 64; ++sample) {
                leftEnergy += std::abs(left[sample]);
                rightEnergy += std::abs(right[sample]);
            }
            const double stereoRatio = rightEnergy > 0.0 ? leftEnergy / rightEnergy : 0.0;
            std::cout << " active=" << active << " L/R=" << stereoRatio << '\n';
            if (active != expectedVoices) ++failures;
            if (expectedVoices == 1 && std::abs(stereoRatio - 1.0) > 0.001) ++failures;
            lastNoteOnFrame = -1;
        }
    }
    return failures ? 1 : 0;
}
