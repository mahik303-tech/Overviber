// A session restore and a preset chosen right after it reach the audio engine
// in the same block, as when a host loads a project and selects a program.
// The engine must end up with the chosen preset, including its voice pattern
// (not a host parameter): preset 12 "Choir Voices" plays all six voices in
// unison, while the restored session's preset 0 plays one.
#include "PluginProcessor.h"
#include "ui/tabs/SettingsTab.h"
#include <cstring>
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

    // Mixer state in the session: master mute and a voice fader above +6 dB
    // (the faders reach +12 dB); a muted master outputs silence.
    {
        juce::MemoryBlock mixerSession;
        {
            auto source = makeProcessor();
            source->getModel().setMasterMute(true);
            source->getModel().setVoiceFader(2, 3.0f);
            source->getStateInformation(mixerSession);
        }
        auto p = makeProcessor();
        p->setStateInformation(mixerSession.getData(), (int)mixerSession.getSize());
        tick(*p);
        check(p->getModel().isMasterMuted(), "the session restores the master mute");
        check(std::abs(p->getModel().getVoiceFader(2) - 3.0f) < 1.0e-4f, "the session restores a voice fader at +9.5 dB");

        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer none, note;
        note.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
        p->processBlock(buffer, none);
        p->processBlock(buffer, note);
        float peak = 0.0f;
        for (int b = 0; b < 20; ++b) {
            p->processBlock(buffer, none);
            if (b >= 2) peak = std::max(peak, buffer.getMagnitude(0, 512));
        }
        std::printf("muted output peak: %g\n", peak);
        check(peak == 0.0f, "a muted master outputs silence");
    }

    // The settings page's state copy: the preset part reads back into the
    // same values at the preset format's pot resolution, the comments carry
    // the exact 16-bit values, and the mixer is listed.
    {
        auto p = makeProcessor();
        p->setCurrentProgram(programIndex(*p, 14));
        tick(*p);
        auto& model = p->getModel();
        model.setMasterMute(true);
        const auto text = SettingsTab::describeState(model);
        PresetData parsed;
        const bool ok = model.getPresetManager().parsePresetString(text.toStdString(), parsed);
        const auto& current = model.getCurrentPreset();
        bool potsMatch = ok;
        for (int cp = 0; cp < cpCount; ++cp)
            potsMatch &= parsed.continuousParams[cp]
                == (uint16_t)scan_potTo16bits(scan_potFrom16bits(current.continuousParams[cp]));
        check(potsMatch && std::memcmp(parsed.steppedParams, current.steppedParams, sizeof(parsed.steppedParams)) == 0,
              "the state copy reads back into the same parameter values (pot resolution)");
        const juce::String rawAmpRel = juce::String("# raw ") + PresetManager::getContinuousParamName(cpAmpRel)
            + " = " + juce::String((int)current.continuousParams[cpAmpRel]) + "\n";
        check(text.contains(rawAmpRel), "the state copy carries the exact 16-bit values");
        check(text.contains("masterMute = 1") && text.contains("voiceFader5 = "), "the state copy lists the mixer");
    }

    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
