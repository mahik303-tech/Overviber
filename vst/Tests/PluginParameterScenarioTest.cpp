// Characterization of the plugin's host parameters: every group and
// parameter with ID, name, label, type, range, default and choices, in the
// host's order. Host automation and saved sessions address parameters by
// these IDs, so params.txt must match vst/Tests/fixtures/plugin-params/.
//
// Usage: PluginParameterScenarioTest [--out <dir>] [--update]
#include "PluginProcessor.h"
#include <iostream>
#include <sstream>

namespace {

void dumpGroup(const juce::AudioProcessorParameterGroup& group, std::ostringstream& out) {
    out << "## " << group.getID() << " | " << group.getName() << "\n";
    for (const auto* node : group) {
        if (const auto* sub = node->getGroup()) { dumpGroup(*sub, out); continue; }
        const auto* param = node->getParameter();
        const auto* withId = dynamic_cast<const juce::AudioProcessorParameterWithID*>(param);
        out << (withId ? withId->paramID : juce::String("?")) << " | " << param->getName(128)
            << " | label=" << param->getLabel() << " | ";
        if (const auto* i = dynamic_cast<const juce::AudioParameterInt*>(param)) {
            out << "int " << i->getRange().getStart() << ".." << i->getRange().getEnd();
        } else if (dynamic_cast<const juce::AudioParameterBool*>(param)) {
            out << "bool";
        } else if (const auto* c = dynamic_cast<const juce::AudioParameterChoice*>(param)) {
            out << "choice [" << c->choices.joinIntoString(", ") << "]";
        } else {
            out << "other steps=" << param->getNumSteps();
        }
        out << " | default=" << param->getText(param->getDefaultValue(), 128) << "\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File outDir = juce::File::getCurrentWorkingDirectory();
    bool update = false;
    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]);
        if (arg == "--update") update = true;
        else if (arg == "--out" && i + 1 < argc) outDir = juce::File::getCurrentWorkingDirectory().getChildFile(argv[++i]);
        else { std::cout << "Unknown argument " << arg << "\n"; return 2; }
    }

    auto processor = std::make_unique<OvercyclerAudioProcessor>(false);
    std::ostringstream out;
    dumpGroup(processor->getParameterTree(), out);
    out << "total " << processor->getParameters().size() << "\n";
    const std::string text = out.str();

    outDir.createDirectory();
    outDir.getChildFile("params.txt").replaceWithText(text, false, false, "\n");
    const juce::File fixture = juce::File(OVERVIBER_PLUGIN_PARAM_FIXTURES).getChildFile("params.txt");
    if (update) {
        fixture.getParentDirectory().createDirectory();
        fixture.replaceWithText(text, false, false, "\n");
        std::cout << "[UPDATE] " << fixture.getFullPathName() << "\nRESULT: PASS\n";
        return 0;
    }
    if (!fixture.existsAsFile()) {
        std::cout << "[FAIL] Missing fixture " << fixture.getFullPathName() << " (run with --update)\nRESULT: FAIL\n";
        return 1;
    }
    juce::StringArray expected, actual;
    expected.addLines(fixture.loadFileAsString());
    actual.addLines(juce::String(text));
    for (int i = 0; i < std::max(expected.size(), actual.size()); ++i)
        if (expected[i] != actual[i]) {
            std::cout << "[FAIL] params.txt differs from the fixture\n  line " << (i + 1)
                      << "\n    expected: " << expected[i] << "\n    actual:   " << actual[i] << "\nRESULT: FAIL\n";
            return 1;
        }
    std::cout << "[PASS] params.txt matches the fixture (" << actual.size() << " lines)\n";

    // Host automation of the modulation matrix reaches the audio engine:
    // slot 3 set to LFO 2 -> resonance, -30 %, then switched off.
    processor->prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto automate = [&](const char* id, float value) {
        auto* param = processor->getAPVTS().getParameter(id);
        param->setValueNotifyingHost(param->convertTo0to1(value));
        processor->processBlock(buffer, midi);
    };
    automate("matrixSlot2_src", (float)modSrcLFO2);
    automate("matrixSlot2_dest", (float)modDestResonance);
    automate("matrixSlot2_depth", -30.0f);
    const auto slot = [&] { return processor->getAudioEngine().getCurrentPreset().modMatrix[2]; };
    bool automated = slot().source == modSrcLFO2 && slot().dest == modDestResonance && slot().depth == -30 && slot().enabled;
    automate("matrixSlot2_en", 0.0f);
    automated = automated && !slot().enabled;
    processor->releaseResources();
    std::cout << (automated ? "[PASS]" : "[FAIL]") << " host automation of the modulation matrix reaches the engine\n";
    std::cout << (automated ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return automated ? 0 : 1;
}
