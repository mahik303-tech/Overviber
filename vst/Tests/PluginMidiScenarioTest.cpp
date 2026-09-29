// Characterization of the plugin's MIDI input path: everything that
// OvercyclerAudioProcessor::processBlock does with incoming MIDI (CC mapping,
// channel controllers, per-note expression, MPE, sustain, all notes off,
// program change, channel filter) and the arpeggiator's MIDI output.
//
// midi.txt must match vst/Tests/fixtures/plugin-midi/midi.txt. It holds only
// integers (parameter values, controller states, MIDI events), so it is the
// same on every platform. The rendered audio is compared separately against a
// local hash file, created on the first run (hashes depend on the compiler).
//
// Usage: PluginMidiScenarioTest [--out <dir>] [--update] [--audio-baseline <file>]
//   --update   rewrite the fixture after an intentional MIDI change
#include "PluginProcessor.h"
#include "data/PresetManager.h"
#include <cstdio>
#include <iostream>
#include <sstream>

namespace {

constexpr int kBlock = 256;

struct Harness {
    std::unique_ptr<OvercyclerAudioProcessor> p = std::make_unique<OvercyclerAudioProcessor>(false);
    juce::AudioBuffer<float> buffer{ 2, kBlock };
    std::ostringstream& out;
    uint64_t audioHash = 1469598103934665603ULL;   // FNV-1a over all rendered samples
    std::vector<std::string> midiOut;               // generated events of the last block

    explicit Harness(std::ostringstream& o) : out(o) {
        auto& model = p->getModel();
        model.getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
        model.getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
        p->prepareToPlay(48000.0, kBlock);
        p->setCurrentProgram(0);
        tick();
        run({});
        tick();
    }

    // The processor's editor timer (program changes, state publishing).
    void tick() { ((juce::Timer*)p.get())->timerCallback(); }

    void run(std::vector<juce::MidiMessage> messages) {
        juce::MidiBuffer midi;
        for (auto& m : messages) midi.addEvent(m, 0);
        buffer.clear();
        p->processBlock(buffer, midi);
        midiOut.clear();
        for (const auto metadata : midi) {
            const auto m = metadata.getMessage();
            char line[64];
            std::snprintf(line, sizeof line, "%s ch%d n%d v%d @%d", m.isNoteOn() ? "on" : m.isNoteOff() ? "off" : "other",
                          m.getChannel(), m.getNoteNumber(), m.getVelocity(), metadata.samplePosition);
            midiOut.emplace_back(line);
        }
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i) {
                uint32_t bits;
                const float s = buffer.getSample(ch, i);
                std::memcpy(&bits, &s, sizeof bits);
                for (int b = 0; b < 4; ++b) { audioHash ^= (bits >> (8 * b)) & 0xff; audioHash *= 1099511628211ULL; }
            }
    }

    struct Snapshot {
        std::array<int, cpCount> cp{};
        std::array<int, spCount> sp{};
        int bend = 0, mod = 0, pressure = 0, timbre = 0, breath = 0, expression = 0;
    };
    Snapshot snapshot() const {
        Snapshot s;
        for (int i = 0; i < cpCount; ++i) s.cp[i] = p->getDesiredContinuousParam((continuousParameter_t)i);
        for (int i = 0; i < spCount; ++i) s.sp[i] = p->getDesiredSteppedParam((steppedParameter_t)i);
        const auto& e = p->getAudioEngine();
        s.bend = e.getGlobalPitchBend(); s.mod = e.getGlobalModWheel(); s.pressure = e.getGlobalPressure();
        s.timbre = e.getGlobalTimbre(); s.breath = e.getGlobalBreath(); s.expression = e.getGlobalExpression();
        return s;
    }

    // Everything that differs between two snapshots, one line.
    static std::string diff(const Snapshot& a, const Snapshot& b) {
        std::ostringstream d;
        for (int i = 0; i < cpCount; ++i)
            if (a.cp[i] != b.cp[i]) d << " " << PresetManager::getContinuousParamName((continuousParameter_t)i) << "=" << b.cp[i];
        for (int i = 0; i < spCount; ++i)
            if (a.sp[i] != b.sp[i]) d << " " << PresetManager::getSteppedParamName((steppedParameter_t)i) << "=" << b.sp[i];
        auto field = [&](const char* name, int x, int y) { if (x != y) d << " " << name << "=" << y; };
        field("bend", a.bend, b.bend); field("mod", a.mod, b.mod); field("pressure", a.pressure, b.pressure);
        field("timbre", a.timbre, b.timbre); field("breath", a.breath, b.breath); field("expression", a.expression, b.expression);
        return d.str().empty() ? " -" : d.str();
    }

    // Voices with their note, channel, gate and per-note expression.
    std::string voices() const {
        std::ostringstream v;
        const auto& e = p->getAudioEngine();
        for (int i = 0; i < SYNTH_VOICE_COUNT; ++i) {
            if (!e.isVoiceActive(i)) continue;
            const auto* x = e.getVoiceExpressionState(i);
            v << " [" << i << " n" << (int)x->noteNumber << " ch" << (int)x->midiChannel
              << (e.getVoice(i).isGated() ? " gate" : " rel") << " vel" << x->noteOnVelocity;
            if (x->hasPerVoiceBend) v << " bend" << x->pitchBendOffset;
            if (x->hasPerVoicePressure) v << " press" << x->pressure;
            if (x->hasPerVoiceTimbre) v << " timbre" << x->timbre;
            v << "]";
        }
        return v.str().empty() ? " none" : v.str();
    }

    void step(const std::string& label, std::vector<juce::MidiMessage> messages) {
        const auto before = snapshot();
        run(std::move(messages));
        out << label << " |" << diff(before, snapshot()) << " | voices" << voices() << "\n";
    }
};

void ccTable(std::ostringstream& out) {
    out << "## CC table (channel 1, value 127 then 0)\n";
    Harness h(out);
    for (int cc = 0; cc < 128; ++cc) {
        if (cc == 120 || cc == 123 || cc == 64) continue;   // note handling, see the scenario
        for (int value : { 127, 0 }) {
            const auto before = h.snapshot();
            h.run({ juce::MidiMessage::controllerEvent(1, cc, value) });
            const auto change = Harness::diff(before, h.snapshot());
            if (change != " -" || value == 127) out << "cc" << cc << "=" << value << " |" << change << "\n";
        }
    }
}

void scenario(std::ostringstream& out) {
    out << "\n## Notes and channel controllers (channel 1)\n";
    Harness h(out);
    using M = juce::MidiMessage;
    h.step("note on 60 v100", { M::noteOn(1, 60, (juce::uint8)100) });
    h.step("note on 64 v1", { M::noteOn(1, 64, (juce::uint8)1) });
    h.step("note on 67 v127", { M::noteOn(1, 67, (juce::uint8)127) });
    h.step("bend max", { M::pitchWheel(1, 16383) });
    h.step("bend min", { M::pitchWheel(1, 0) });
    h.step("bend centre", { M::pitchWheel(1, 8192) });
    h.step("channel pressure 90", { M::channelPressureChange(1, 90) });
    h.step("poly aftertouch 64=70", { M::aftertouchChange(1, 64, 70) });
    h.step("cc1 100", { M::controllerEvent(1, 1, 100) });
    h.step("cc2 50", { M::controllerEvent(1, 2, 50) });
    h.step("cc11 30", { M::controllerEvent(1, 11, 30) });
    h.step("cc74 110", { M::controllerEvent(1, 74, 110) });
    h.step("sustain on", { M::controllerEvent(1, 64, 127) });
    h.step("note off 60", { M::noteOff(1, 60, (juce::uint8)40) });
    h.step("note off 64", { M::noteOff(1, 64, (juce::uint8)0) });
    for (int i = 0; i < 8; ++i) h.run({});
    h.step("sustain off", { M::controllerEvent(1, 64, 0) });
    h.step("note on 72 v80", { M::noteOn(1, 72, (juce::uint8)80) });
    h.step("cc123 all notes off", { M::controllerEvent(1, 123, 0) });
    h.step("note on 48 v90", { M::noteOn(1, 48, (juce::uint8)90) });
    h.step("cc120 all sound off", { M::controllerEvent(1, 120, 0) });
    h.step("note on 55 ch9", { M::noteOn(9, 55, (juce::uint8)90) });
    h.step("note off 55 ch9", { M::noteOff(9, 55, (juce::uint8)0) });
    h.step("program change 3", { M::programChange(1, 3) });
    h.tick();
    out << "program after timer | " << h.p->getCurrentProgram() << "\n";

    out << "\n## Input channel filter (channel 2 only)\n";
    h.p->setMidiInputChannel(2);
    h.step("note on 60 ch1", { M::noteOn(1, 60, (juce::uint8)100) });
    h.step("note on 62 ch2", { M::noteOn(2, 62, (juce::uint8)100) });
    h.step("cc1 127 ch1", { M::controllerEvent(1, 1, 127) });
    h.step("cc1 64 ch2", { M::controllerEvent(2, 1, 64) });
    h.step("cc123 ch2", { M::controllerEvent(2, 123, 0) });
    h.p->setMidiInputChannel(0);
}

void mpe(std::ostringstream& out, int mode) {
    out << "\n## MPE mode " << mode << "\n";
    Harness h(out);
    using M = juce::MidiMessage;
    h.p->setSteppedParamFromUI(spMPEMode, (uint8_t)mode);
    h.p->setSteppedParamFromUI(spMPEPitchBendRange, 2);
    h.run({});
    h.step("note on 60 ch2", { M::noteOn(2, 60, (juce::uint8)100) });
    h.step("note on 64 ch3", { M::noteOn(3, 64, (juce::uint8)90) });
    h.step("note on 67 ch9", { M::noteOn(9, 67, (juce::uint8)80) });
    h.step("bend ch2 +4096", { M::pitchWheel(2, 8192 + 4096) });
    h.step("bend ch1 +2048", { M::pitchWheel(1, 8192 + 2048) });
    h.step("pressure ch3 100", { M::channelPressureChange(3, 100) });
    h.step("pressure ch1 20", { M::channelPressureChange(1, 20) });
    h.step("cc74 ch2 90", { M::controllerEvent(2, 74, 90) });
    h.step("cc74 ch1 30", { M::controllerEvent(1, 74, 30) });
    h.step("cc74 ch9 60", { M::controllerEvent(9, 74, 60) });
    h.step("note off 60 ch2", { M::noteOff(2, 60, (juce::uint8)64) });
}

void arpOutput(std::ostringstream& out) {
    out << "\n## Arp MIDI output (Up, 2 octaves, 1/16, gate 50 %, swing 66 %, internal clock)\n";
    Harness h(out);
    using M = juce::MidiMessage;
    h.p->setSteppedParamFromUI(spArpMode, amUp);
    h.p->setSteppedParamFromUI(spArpOctaves, 1);
    h.p->setSteppedParamFromUI(spArpRate, 3);
    h.p->setContinuousParamFromUI(cpArpGate, 500.0f);
    h.p->setContinuousParamFromUI(cpArpSwing, 660.0f);
    h.run({ M::noteOn(1, 60, (juce::uint8)100), M::noteOn(1, 64, (juce::uint8)70) });
    for (int block = 0; block < 400; ++block) {
        if (block == 300) h.run({ M::noteOff(1, 60, (juce::uint8)0), M::noteOff(1, 64, (juce::uint8)0) });
        else h.run({});
        for (const auto& e : h.midiOut) out << "block " << block << " " << e << "\n";
    }
}

// The editor's step sequence reaches the audio engine's arp: step 2 mute,
// step 3 accent, set in the model and published by the processor's timer.
void arpSequence(std::ostringstream& out) {
    out << "\n## Arp step sequence from the editor (Up, step 2 mute, step 3 accent)\n";
    Harness h(out);
    using M = juce::MidiMessage;
    h.p->setSteppedParamFromUI(spArpMode, amUp);
    auto& sequence = h.p->getModel().getArpSequence();
    sequence.setStepPattern(1, 3);
    sequence.setStepPattern(2, 1);
    h.tick();
    h.run({ M::noteOn(1, 60, (juce::uint8)80), M::noteOn(1, 64, (juce::uint8)80) });
    for (int block = 0; block < 200; ++block) {
        h.run({});
        for (const auto& e : h.midiOut) out << "block " << block << " " << e << "\n";
    }
}

bool parse(int argc, char* argv[], juce::File& outDir, bool& update, juce::File& audioBaseline) {
    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]);
        auto next = [&]() { return i + 1 < argc ? juce::String(argv[++i]) : juce::String(); };
        if (arg == "--update") update = true;
        else if (arg == "--out") outDir = juce::File::getCurrentWorkingDirectory().getChildFile(next());
        else if (arg == "--audio-baseline") audioBaseline = juce::File::getCurrentWorkingDirectory().getChildFile(next());
        else { std::cout << "Unknown argument " << arg << "\n"; return false; }
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File outDir = juce::File::getCurrentWorkingDirectory(), audioBaseline;
    bool update = false;
    if (!parse(argc, argv, outDir, update, audioBaseline)) return 2;

    std::ostringstream out;
    uint64_t audioHash = 0;
    ccTable(out);
    scenario(out);
    for (int mode : { 1, 2 }) mpe(out, mode);
    arpOutput(out);
    arpSequence(out);
    {
        // Audio of a short phrase with controllers through the plugin path.
        std::ostringstream ignored;
        Harness h(ignored);
        using M = juce::MidiMessage;
        h.run({ M::noteOn(1, 48, (juce::uint8)100), M::noteOn(1, 55, (juce::uint8)80) });
        for (int b = 0; b < 200; ++b) {
            std::vector<M> m;
            if (b % 10 == 0) m.push_back(M::pitchWheel(1, 8192 + (b * 37) % 4000));
            if (b % 7 == 0) m.push_back(M::controllerEvent(1, 1, (b * 3) % 128));
            if (b % 13 == 0) m.push_back(M::channelPressureChange(1, (b * 5) % 128));
            if (b == 120) { m.push_back(M::noteOff(1, 48, (juce::uint8)0)); m.push_back(M::noteOn(1, 60, (juce::uint8)110)); }
            h.run(m);
        }
        audioHash = h.audioHash;
    }

    const std::string text = out.str();
    outDir.createDirectory();
    outDir.getChildFile("midi.txt").replaceWithText(text, false, false, "\n");
    const juce::File fixture = juce::File(OVERVIBER_PLUGIN_MIDI_FIXTURES).getChildFile("midi.txt");
    bool pass = true;
    if (update) {
        fixture.getParentDirectory().createDirectory();
        fixture.replaceWithText(text, false, false, "\n");
        std::cout << "[UPDATE] " << fixture.getFullPathName() << "\n";
    } else if (!fixture.existsAsFile()) {
        std::cout << "[FAIL] Missing fixture " << fixture.getFullPathName() << " (run with --update)\n";
        pass = false;
    } else {
        juce::StringArray expected, actual;
        expected.addLines(fixture.loadFileAsString());
        actual.addLines(juce::String(text));
        for (int i = 0; i < std::max(expected.size(), actual.size()); ++i)
            if (expected[i] != actual[i]) {
                std::cout << "[FAIL] midi.txt differs from the fixture\n  line " << (i + 1)
                          << "\n    expected: " << expected[i] << "\n    actual:   " << actual[i] << "\n";
                pass = false;
                break;
            }
        if (pass) std::cout << "[PASS] midi.txt matches the fixture (" << actual.size() << " lines)\n";
    }

    const juce::String hashText = juce::String::toHexString((juce::int64)audioHash);
    if (audioBaseline != juce::File()) {
        if (!audioBaseline.existsAsFile() || update) {
            audioBaseline.getParentDirectory().createDirectory();
            audioBaseline.replaceWithText(hashText);
            std::cout << "[BASELINE] audio hash " << hashText << " written\n";
        } else if (audioBaseline.loadFileAsString().trim() != hashText) {
            std::cout << "[FAIL] audio hash " << hashText << " differs from baseline "
                      << audioBaseline.loadFileAsString().trim() << "\n";
            pass = false;
        } else {
            std::cout << "[PASS] audio hash " << hashText << "\n";
        }
    }
    std::cout << "RESULT: " << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
