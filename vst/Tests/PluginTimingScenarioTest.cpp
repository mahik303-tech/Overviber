#include "PluginProcessor.h"
#include "ui/ModernEditorView.h"
#include "ui/ModernPresetManager.h"
#include <chrono>
#include <iostream>

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    juce::Slider debugSlider;
    debugSlider.setComponentID("cutoffKnob");
    debugSlider.setRange(0.0, 999.0, 1.0);
    debugSlider.textFromValueFunction = [](double) { return juce::String("1.25 kHz"); };
    debugSlider.setValue(600.0, juce::dontSendNotification);
    juce::ToggleButton debugToggle;
    debugToggle.setComponentID("filterModelToggle[1]");
    debugToggle.setToggleState(true, juce::dontSendNotification);
    const bool debugHoverPass = ModernEditorView::getDebugHoverTextFor(debugSlider)
            == "cutoffKnob  |  1.25 kHz"
        && ModernEditorView::getDebugHoverTextFor(debugToggle)
            == "filterModelToggle[1]  |  On";
    std::cout << "Debug hover current values: " << (debugHoverPass ? "passed" : "FAILED") << '\n';
    if (!debugHoverPass) return 1;

    auto whole = std::make_unique<OvercyclerAudioProcessor>(false);
    auto split = std::make_unique<OvercyclerAudioProcessor>(false);
    whole->prepareToPlay(48000, 256); split->prepareToPlay(48000, 256);
    juce::AudioBuffer<float> output(2, 256), first(2, 128), second(2, 128);
    juce::MidiBuffer events, empty, note;
    events.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(110)), 128);
    note.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(110)), 0);
    whole->processBlock(output, events);
    split->processBlock(first, empty); split->processBlock(second, note);
    float error = 0, before = 0, after = 0;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) {
        before = std::max(before, std::abs(output.getSample(ch, i)));
        after = std::max(after, std::abs(output.getSample(ch, i + 128)));
        error = std::max(error, std::abs(output.getSample(ch, i + 128) - second.getSample(ch, i)));
    }
    std::cout << "Before note: " << before << "; after: " << after << "; split error: " << error << '\n';
    if (!(before == 0 && after > 0.00001f && error < 0.000001f)) return 1;

    // A note that started in direct-play mode must be released when the arp
    // is enabled. Its later physical note-off is routed to the arp and cannot
    // release the original direct assignment.
    auto transition = std::make_unique<SynthEngine>();
    transition->prepare(48000);
    transition->noteOn(64, 50000, 1);
    const bool directNoteStarted = transition->hasDirectKeysPressed();
    transition->setSteppedParam(spArpMode, amUp);
    const bool directNoteReleased = !transition->hasDirectKeysPressed();
    std::cout << "Direct note release on arp enable: "
              << (directNoteStarted && directNoteReleased ? "passed" : "FAILED") << '\n';
    if (!directNoteStarted || !directNoteReleased) return 1;

    whole->setSteppedParamFromUI(spArpMode, amUp);
    whole->setSteppedParamFromUI(spArpOctaves, 3);
    juce::MidiBuffer arpMidi;
    arpMidi.addEvent(juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    arpMidi.addEvent(juce::MidiMessage::noteOn(1, 52, static_cast<juce::uint8>(100)), 0);
    whole->processBlock(output, arpMidi);
    const auto arpVisual = whole->getArpVisualizationState();
    const std::array<uint8_t, 8> expectedPattern{48, 52, 60, 64, 72, 76, 84, 88};
    bool arpTelemetryPass = arpVisual.valid && arpVisual.activeCount == 2;
    for (int i = 0; i < 8 && arpTelemetryPass; ++i)
        arpTelemetryPass = arpVisual.patternNotes[i] == expectedPattern[i];
    std::cout << "Live arp telemetry / four-octave pattern: " << (arpTelemetryPass ? "passed" : "FAILED") << '\n';
    if (!arpTelemetryPass) return 1;

    bool generatedNoteOn = false;
    juce::MidiBuffer arpClockMidi;
    for (int block = 0; block < 32 && !generatedNoteOn; ++block) {
        arpClockMidi.clear();
        whole->processBlock(output, arpClockMidi);
        for (const auto metadata : arpClockMidi)
            generatedNoteOn |= metadata.getMessage().isNoteOn();
    }
    whole->setSteppedParamFromUI(spArpMode, amOff);
    arpClockMidi.clear();
    whole->processBlock(output, arpClockMidi);
    bool modeSwitchNoteOff = false;
    bool modeSwitchNoteOffAtStart = false;
    for (const auto metadata : arpClockMidi) {
        if (metadata.getMessage().isNoteOff()) {
            modeSwitchNoteOff = true;
            modeSwitchNoteOffAtStart |= metadata.samplePosition == 0;
        }
    }
    std::cout << "Arp mode switch note-off delivery: "
              << (generatedNoteOn && modeSwitchNoteOffAtStart ? "passed" : "FAILED") << '\n';
    if (!generatedNoteOn || !modeSwitchNoteOff || !modeSwitchNoteOffAtStart) return 1;

    auto channelFilter = std::make_unique<OvercyclerAudioProcessor>(false);
    channelFilter->prepareToPlay(48000, 256);
    channelFilter->setSteppedParamFromUI(spArpMode, amUp);
    channelFilter->setMidiInputChannel(2);
    juce::MidiBuffer channelMidi;
    channelMidi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
    channelFilter->processBlock(output, channelMidi);
    const bool rejectedWrongChannel = channelFilter->getArpVisualizationState().activeCount == 0;
    channelMidi.clear();
    channelMidi.addEvent(juce::MidiMessage::noteOn(2, 64, static_cast<juce::uint8>(100)), 0);
    channelFilter->processBlock(output, channelMidi);
    const bool acceptedSelectedChannel = channelFilter->getArpVisualizationState().activeCount == 1;
    std::cout << "MIDI input channel filter: "
              << (rejectedWrongChannel && acceptedSelectedChannel ? "passed" : "FAILED") << '\n';
    if (!rejectedWrongChannel || !acceptedSelectedChannel) return 1;

    channelFilter->setMidiInputChannel(7);
    juce::MemoryBlock channelState;
    channelFilter->getStateInformation(channelState);
    auto restoredChannelFilter = std::make_unique<OvercyclerAudioProcessor>(false);
    restoredChannelFilter->setStateInformation(channelState.getData(), static_cast<int>(channelState.getSize()));
    const bool channelStatePass = restoredChannelFilter->getMidiInputChannel() == 7;
    std::cout << "MIDI input channel state persistence: "
              << (channelStatePass ? "passed" : "FAILED") << '\n';
    if (!channelStatePass) return 1;

    auto brightness = std::make_unique<OvercyclerAudioProcessor>(false);
    brightness->prepareToPlay(48000, 256);
    brightness->setContinuousParamFromUI(cpCutoff, 100.0f);
    const auto cutoffBefore = brightness->getDesiredContinuousParam(cpCutoff);
    juce::MidiBuffer brightnessMidi;
    brightnessMidi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 0);
    brightness->processBlock(output, brightnessMidi);
    const auto cutoffAfter = brightness->getDesiredContinuousParam(cpCutoff);
    std::cout << "Standard MIDI CC74 brightness routing: "
              << (cutoffAfter > cutoffBefore ? "passed" : "FAILED")
              << " (" << cutoffBefore << " -> " << cutoffAfter << ")\n";
    if (cutoffAfter <= cutoffBefore) return 1;
    auto& model = whole->getModel();
    model.getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
    model.getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
    ModernPresetBar bar(model, whole.get());
    bar.selectPreset(2);
    for (int attempt = 0; attempt < 20 && whole->getCurrentProgram() != 2; ++attempt) {
        juce::Thread::sleep(10); juce::Timer::callPendingTimersSynchronously();
    }
    bar.updateDisplay();
    if (whole->getCurrentProgram() != 2 || bar.getDisplayedPresetName() != juce::String(model.getPresetManager().getPresetName(2))
) return 1;
    bar.initPatch();
    if (bar.getDisplayedPresetName() != juce::String(model.getCurrentPreset().presetName)) return 1;
    std::cout << "Preset selection / Init display: passed\n";
    whole->prepareToPlay(44100,512);
    juce::AudioBuffer<float> live(2,512); juce::MidiBuffer midi;
    double worst = 0, total = 0;
    for (int b = 0; b < 400; ++b) {
        midi.clear();
        if (b == 0) for (int n : {48,55,60,64,67,72}) midi.addEvent(juce::MidiMessage::noteOn(1,n,static_cast<juce::uint8>(100)),0);
        if (b % 25 == 0) whole->setSteppedParamFromUI(spFilterModel,static_cast<uint8_t>((b/25)%4));
        const auto start = std::chrono::steady_clock::now(); whole->processBlock(live,midi);
        const double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        worst = std::max(worst, ms); total += ms;
        for (int ch=0;ch<2;++ch) for(int i=0;i<512;++i) if (!std::isfinite(live.getSample(ch,i))) return 1;
        if (b%3==0) juce::Timer::callPendingTimersSynchronously();
    }
    std::cout << "44100/512 processor, filter changes: mean " << total/400 << " ms, worst " << worst << " ms; budget 11.61 ms\n";
    return 0;
}
