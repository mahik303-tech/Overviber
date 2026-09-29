// A session restore and a preset chosen right after it reach the audio engine
// in the same block, as when a host loads a project and selects a program.
// The engine must end up with the chosen preset, including its voice pattern
// (not a host parameter): preset 12 "Choir Voices" plays all six voices in
// unison, while the restored session's preset 0 plays one.
#include "PluginProcessor.h"
#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

std::unique_ptr<OvercyclerAudioProcessor> makeProcessor() {
    auto p = std::make_unique<OvercyclerAudioProcessor>(false);
    p->prepareToPlay(48000.0, 512);
    auto& model = p->getModel();
    model.getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
    model.getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
    return p;
}

// The processor's message-thread tick (its juce::Timer base is private).
void tick(OvercyclerAudioProcessor& p) { ((juce::Timer*)&p)->timerCallback(); }

int programIndex(OvercyclerAudioProcessor& p, int number) {
    auto& presets = p.getModel().getPresetManager();
    for (int i = 0; i < presets.getPresetCount(); ++i)
        if (presets.getPresetNumber(i) == number) return i;
    return -1;
}

// Plays middle C for half a second; returns how many voices reached the bus.
int soundingVoices(OvercyclerAudioProcessor& p) {
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer none, note;
    note.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    p.getModel().takeMeterLevels();
    p.processBlock(buffer, note);
    for (int b = 0; b < 48; ++b) p.processBlock(buffer, none);
    tick(p);
    const auto levels = p.getModel().takeMeterLevels();
    int voices = 0;
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) voices += levels[v] > 0;
    return voices;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI gui;

    // A saved session with preset 0 (one voice).
    juce::MemoryBlock session;
    {
        auto source = makeProcessor();
        source->setCurrentProgram(programIndex(*source, 0));
        tick(*source);
        const auto& pattern = source->getModel().getCurrentPreset().voicePattern;
        check(pattern[0] == 0 && pattern[1] == ASSIGNER_NO_NOTE, "preset 0 plays a single voice");
        source->getStateInformation(session);
    }

    // Restore only: the session's preset sounds.
    {
        auto p = makeProcessor();
        p->setStateInformation(session.getData(), (int)session.getSize());
        tick(*p);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer none;
        p->processBlock(buffer, none);
        tick(*p);
        const int voices = soundingVoices(*p);
        std::printf("restore only: %d voice(s)\n", voices);
        check(voices == 1, "the restored session plays its preset (one voice)");
    }

    // Restore and a program change before the first audio block.
    {
        auto p = makeProcessor();
        const int choir = programIndex(*p, 12);
        p->setStateInformation(session.getData(), (int)session.getSize());
        p->setCurrentProgram(choir);
        tick(*p);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer none;
        p->processBlock(buffer, none);
        tick(*p);
        const int voices = soundingVoices(*p);
        std::printf("restore, then preset 12: %d voice(s)\n", voices);
        check(voices == SYNTH_VOICE_COUNT, "the chosen unison preset plays all six voices after a restore");
    }

    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
