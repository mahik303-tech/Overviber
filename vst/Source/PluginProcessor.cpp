#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "data/OverviberPaths.h"
#include "data/SessionState.h"

namespace {
juce::String addProcessorState(const juce::String& engineState, int midiInputChannel) {
    auto root = juce::JSON::parse(engineState);
    if (auto* object = root.getDynamicObject())
        object->setProperty("midiInputChannel", std::clamp(midiInputChannel, 0, 16));
    return juce::JSON::toString(root, true);
}

int readMidiInputChannel(const juce::String& state) {
    const auto root = juce::JSON::parse(state);
    const auto value = root["midiInputChannel"];
    return value.isVoid() ? 0 : std::clamp(static_cast<int>(value), 0, 16);
}
}

void OvercyclerAudioProcessor::setDesiredFromPreset(const PresetData& preset) {
    for (int i = 0; i < cpCount; ++i) desiredContinuous[i].store(preset.continuousParams[i]);
    for (int i = 0; i < spCount; ++i) desiredStepped[i].store(preset.steppedParams[i]);
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        const auto& m = preset.modMatrix[s];
        const int values[] = {m.source, m.dest, m.viaSource, m.depth, m.enabled ? 1 : 0};
        for (int f = 0; f < 5; ++f) desiredMatrix[s][f].store(values[f]);
    }
}
// Host automation reaches the editor model and the audio engine alike.
template <typename Target>
void OvercyclerAudioProcessor::applyDesiredParameters(Target& target) {
    auto& preset = target.getCurrentPreset();
    for (int i = 0; i < cpCount; ++i) {
        const auto value = static_cast<uint16_t>(desiredContinuous[i].load());
        if (preset.continuousParams[i] != value) target.setContinuousParam(static_cast<continuousParameter_t>(i), value);
    }
    for (int i = 0; i < spCount; ++i) {
        const auto value = static_cast<uint8_t>(desiredStepped[i].load());
        if (preset.steppedParams[i] != value) target.setSteppedParam(static_cast<steppedParameter_t>(i), value);
    }
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        auto& m = preset.modMatrix[s];
        m.source = static_cast<uint8_t>(desiredMatrix[s][0].load());
        m.dest = static_cast<uint8_t>(desiredMatrix[s][1].load());
        m.viaSource = static_cast<uint8_t>(desiredMatrix[s][2].load());
        m.depth = static_cast<int16_t>(desiredMatrix[s][3].load());
        m.enabled = desiredMatrix[s][4].load() != 0;
    }
}
void OvercyclerAudioProcessor::publishEditorState() {
    model.capturePreparedState(*editorSnapshot);
    const int currentMidiInputChannel = midiInputChannel.load();
    if (hasPublished && publishedPresetName == model.getCurrentPreset().presetName
        && publishedMidiInputChannel == currentMidiInputChannel
        && sameIgnoringWaveData(*editorSnapshot, *lastPublished)) return;
    if (!stateQueue->push(*editorSnapshot)) return; // Retry next control tick.
    *lastPublished = *editorSnapshot; hasPublished = true;
    publishedPresetName = model.getCurrentPreset().presetName;
    publishedMidiInputChannel = currentMidiInputChannel;
    // The saved session (JSON with embedded waves) is encoded once changes
    // settle, not for every control movement; see encodeSessionIfChanged().
    sessionDirty = true;
    lastSessionChangeMs = juce::Time::getMillisecondCounter();
}

void OvercyclerAudioProcessor::encodeSessionIfChanged(bool immediately) {
    if (!sessionDirty) return;
    if (!immediately && juce::Time::getMillisecondCounter() - lastSessionChangeMs < kSessionSettleMs) return;
    const auto text = addProcessorState(SessionState::encode(model), midiInputChannel.load());
    { const juce::ScopedLock lock(savedStateLock); savedState = text; }
    sessionDirty = false;
}

ArpVisualizationState OvercyclerAudioProcessor::getArpVisualizationState() const {
    ArpVisualizationState state;
    state.valid = true;
    state.tick = arpTick.load();
    state.currentStep = arpCurrentStep.load();
    state.gateActive = arpGateActive.load();
    state.activeCount = std::clamp(arpActiveCount.load(), 0, 16);
    for (int i = 0; i < 16; ++i) {
        state.activeNotes[i] = static_cast<uint8_t>(arpActiveNotes[i].load());
        state.patternNotes[i] = static_cast<uint8_t>(arpPatternNotes[i].load());
    }
    return state;
}
void OvercyclerAudioProcessor::timerCallback() {
    delete retiredState.exchange(nullptr);
    juce::String restore;
    { const juce::ScopedLock lock(savedStateLock); restore.swapWith(editorRestore); }
    // A restored session replaces the whole engine state (processBlock applies
    // it before queued editor states). Publish the editor state again after
    // it, even when it looks unchanged, so an editor state queued before the
    // restore cannot leave the engine with the session's older data.
    if (restore.isNotEmpty()) { SessionState::decode(restore, model); hasPublished = false; }
    const int program = requestedProgram.exchange(-1);
    if (program >= 0 && program < model.getPresetManager().getPresetCount()) {
        model.loadPreset(program); currentProgram.store(program);
        setDesiredFromPreset(model.getCurrentPreset()); updateAPVTSFromEngine();
    }
    applyDesiredParameters(model);
    SynthModel::MeterLevels levels;
    for (int i = 0; i < SynthModel::kMeterCount; ++i) levels[i] = meterLevels[i].exchange(0);
    model.addMeterLevels(levels);
    model.setArpVisualizationState(getArpVisualizationState());
    bool midiChange = false;
    for (auto& value : midiContinuous) midiChange |= value.exchange(-1) >= 0;
    for (auto& value : midiStepped) midiChange |= value.exchange(-1) >= 0;
    if (midiChange || restore.isNotEmpty()) updateAPVTSFromEngine();
    publishEditorState();
    encodeSessionIfChanged(false);
}
void OvercyclerAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    // The model belongs to the message thread; other threads use the last
    // encoded session (at most kSessionSettleMs older than the last edit).
    if (juce::MessageManager::existsAndIsCurrentThread()) encodeSessionIfChanged(true);
    juce::String text;
    { const juce::ScopedLock lock(savedStateLock); text = savedState; }
    auto temporary = std::make_unique<SynthModel>();
    if (SessionState::decode(text, *temporary)) {
        applyDesiredParameters(*temporary);
        text = addProcessorState(SessionState::encode(*temporary), midiInputChannel.load());
    }
    destination.replaceAll(text.toRawUTF8(), text.getNumBytesAsUTF8());
}
void OvercyclerAudioProcessor::setStateInformation(const void* data, int size) {
    if (!data || size <= 0 || size > 4 * 1024 * 1024) return;
    const auto text = juce::String::fromUTF8(static_cast<const char*>(data), size);
    auto temporary = std::make_unique<SynthModel>();
    temporary->getWaveManager().setBaseDirectory(OverviberPaths::getWaveDataDirectory().getFullPathName().toStdString());
    if (!SessionState::decode(text, *temporary)) return;
    midiInputChannel.store(readMidiInputChannel(text));
    auto prepared = std::make_unique<PreparedState>();
    temporary->capturePreparedState(*prepared);
    const auto canonical = addProcessorState(SessionState::encode(*temporary), midiInputChannel.load());
    setDesiredFromPreset(temporary->getCurrentPreset());
    { const juce::ScopedLock lock(savedStateLock); savedState = canonical; editorRestore = canonical; }
    delete restoredState.exchange(prepared.release());
}

void OvercyclerAudioProcessor::setMidiInputChannel(int channel) {
    midiInputChannel.store(std::clamp(channel, 0, 16));
}
bool OvercyclerAudioProcessor::saveSetup(const juce::File& file) {
    publishEditorState();
    juce::MemoryBlock state; getStateInformation(state);
    juce::TemporaryFile temporary(file);
    return temporary.getFile().replaceWithData(state.getData(), state.getSize()) && temporary.overwriteTargetFileWithTemporary();
}
bool OvercyclerAudioProcessor::loadSetup(const juce::File& file) {
    const auto text = file.loadFileAsString();
    auto validated = std::make_unique<SynthModel>();
    validated->getWaveManager().setBaseDirectory(model.getWaveManager().getBaseDirectory());
    if (!SessionState::decode(text, *validated)) return false;
    const auto canonical = addProcessorState(SessionState::encode(*validated), readMidiInputChannel(text));
    setStateInformation(canonical.toRawUTF8(), static_cast<int>(canonical.getNumBytesAsUTF8()));
    timerCallback();
    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout OvercyclerAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto makeCP = [](continuousParameter_t cp, int defaultVal) -> std::unique_ptr<juce::AudioParameterInt> {
        const char* id = PresetManager::getContinuousParamName(cp);
        const char* name = PresetManager::getContinuousParamDisplayName(cp);
        if (cp == cpAFreq || cp == cpBFreq) {   // base pitch 0 .. 64 semitones
            return std::make_unique<juce::AudioParameterInt>(
                id, name, 0, 64, defaultVal, juce::AudioParameterIntAttributes().withLabel("st")
            );
        }
        if (cp == cpMasterTune) {               // +-1 semitone
            return std::make_unique<juce::AudioParameterInt>(
                id, name, -100, 100, defaultVal, juce::AudioParameterIntAttributes().withLabel("ct")
            );
        }
        if (cp == cpDetune) {
            return std::make_unique<juce::AudioParameterInt>(
                id, name, -50, 50, defaultVal, juce::AudioParameterIntAttributes().withLabel("ct")
            );
        }
        if (cp == cpArpBpm) {
            return std::make_unique<juce::AudioParameterInt>(
                id, name, 20, 300, defaultVal, juce::AudioParameterIntAttributes().withLabel("BPM")
            );
        }
        if (PresetManager::isContinuousParamZeroCentered(cp)) {
            return std::make_unique<juce::AudioParameterInt>(
                id, name, -100, 100, defaultVal, juce::AudioParameterIntAttributes().withLabel("%")
            );
        }
        return std::make_unique<juce::AudioParameterInt>(
            id, name, 0, 100, defaultVal, juce::AudioParameterIntAttributes().withLabel("%")
        );
    };

    // 1. Oscillator A
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_oscA", "Oscillator A", " : ");
        grp->addChild(makeCP(cpAFreq, 0));
        grp->addChild(makeCP(cpAVol, 100));
        grp->addChild(makeCP(cpABaseWMod, 0));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spAWModType", PresetManager::getSteppedParamDisplayName(spAWModType),
            juce::StringArray{"Off", "Aliasing", "Width", "Frequency", "CrossOver", "Folder", "BitCrush"}, 0
        ));
        grp->addChild(makeCP(cpWModAEnv, 0));
        layout.add(std::move(grp));
    }

    // 2. Oscillator B
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_oscB", "Oscillator B", " : ");
        grp->addChild(makeCP(cpBFreq, 0));
        grp->addChild(makeCP(cpBVol, 100));
        grp->addChild(makeCP(cpDetune, 0));
        grp->addChild(makeCP(cpBBaseWMod, 0));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spBWModType", PresetManager::getSteppedParamDisplayName(spBWModType),
            juce::StringArray{"Off", "Aliasing", "Width", "Frequency", "CrossOver", "Folder", "BitCrush"}, 0
        ));
        grp->addChild(makeCP(cpWModBEnv, 0));
        layout.add(std::move(grp));
    }

    // 3. Oscillator Sync
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_sync", "Oscillator Sync", " : ");
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spSync", PresetManager::getSteppedParamDisplayName(spOscSync), false
        ));
        layout.add(std::move(grp));
    }

    // 4. Filter (VCF)
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_filter", "Filter (VCF)", " : ");
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spFilterModel", PresetManager::getSteppedParamDisplayName(spFilterModel),
            juce::StringArray{"SSI2144 (Ladder)", "SEM (2-Pole SVF)", "Shelves (EQ/SVF)", "SST Vintage (Moog)"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spFilterMode", PresetManager::getSteppedParamDisplayName(spFilterMode),
            juce::StringArray{"Mode 1", "Mode 2", "Mode 3", "Mode 4"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spSemModel", PresetManager::getSteppedParamDisplayName(spSemModel),
            juce::StringArray{"OB-Xd 12 dB", "Oberheim (Pirkle)", "Vult SVF", "Cytomic SVF", "Liquid (Ripples)"}, 0
        ));
        grp->addChild(makeCP(cpCutoff, 100));
        grp->addChild(makeCP(cpResonance, 0));
        grp->addChild(makeCP(cpFilEnvAmt, 0));
        grp->addChild(makeCP(cpFilKbdAmt, 50));
        grp->addChild(makeCP(cpFilVelocity, 0));
        grp->addChild(makeCP(cpShelvesLsFreq, 20));
        grp->addChild(makeCP(cpShelvesLsGain, 0));
        grp->addChild(makeCP(cpShelvesP1Gain, 0));
        grp->addChild(makeCP(cpShelvesP2Freq, 65));
        grp->addChild(makeCP(cpShelvesP2Gain, 0));
        grp->addChild(makeCP(cpShelvesP2Q, 30));
        grp->addChild(makeCP(cpShelvesHsFreq, 80));
        grp->addChild(makeCP(cpShelvesHsGain, 0));
        layout.add(std::move(grp));
    }

    // 5. Filter Envelope
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_filEnv", "Filter Envelope", " : ");
        grp->addChild(makeCP(cpFilAtt, 0));
        grp->addChild(makeCP(cpFilDec, 50));
        grp->addChild(makeCP(cpFilSus, 50));
        grp->addChild(makeCP(cpFilRel, 50));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spFilEnvLoop", PresetManager::getSteppedParamDisplayName(spFilEnvLoop), false
        ));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spFilEnvLin", PresetManager::getSteppedParamDisplayName(spFilEnvLin), false
        ));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spFilEnvSlow", PresetManager::getSteppedParamDisplayName(spFilEnvSlow), false
        ));
        layout.add(std::move(grp));
    }

    // 6. Amp Envelope (VCA)
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_ampEnv", "Amp Envelope (VCA)", " : ");
        grp->addChild(makeCP(cpAmpAtt, 0));
        grp->addChild(makeCP(cpAmpDec, 50));
        grp->addChild(makeCP(cpAmpSus, 100));
        grp->addChild(makeCP(cpAmpRel, 50));
        grp->addChild(makeCP(cpAmpVelocity, 0));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spAmpEnvLoop", PresetManager::getSteppedParamDisplayName(spAmpEnvLoop), false
        ));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spAmpEnvLin", PresetManager::getSteppedParamDisplayName(spAmpEnvLin), false
        ));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spAmpEnvSlow", PresetManager::getSteppedParamDisplayName(spAmpEnvSlow), false
        ));
        layout.add(std::move(grp));
    }

    // 7. WaveMod Envelope
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_wmodEnv", "WaveMod Envelope", " : ");
        grp->addChild(makeCP(cpWModAtt, 0));
        grp->addChild(makeCP(cpWModDec, 50));
        grp->addChild(makeCP(cpWModSus, 50));
        grp->addChild(makeCP(cpWModRel, 50));
        grp->addChild(makeCP(cpWModVelocity, 0));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spWModEnvLoop", PresetManager::getSteppedParamDisplayName(spWModEnvLoop), false
        ));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spWModEnvLin", PresetManager::getSteppedParamDisplayName(spWModEnvLin), false
        ));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spWModEnvSlow", PresetManager::getSteppedParamDisplayName(spWModEnvSlow), false
        ));
        layout.add(std::move(grp));
    }

    // 8. LFO 1
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_lfo1", "LFO 1", " : ");
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spLFOShape", PresetManager::getSteppedParamDisplayName(spLFOShape),
            juce::StringArray{"Pulse", "Triangle", "Random", "Sine", "Noise", "Saw", "RevSaw"}, 1
        ));
        grp->addChild(makeCP(cpLFOFreq, 50));
        grp->addChild(makeCP(cpLFOAmt, 0));
        grp->addChild(makeCP(cpModDelay, 0));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spLFOSpeed", PresetManager::getSteppedParamDisplayName(spLFOSpeed),
            juce::StringArray{"Normal (x1)", "Fast (x2)", "High (x4)", "Ultra (x8)"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spLFOTrig", PresetManager::getSteppedParamDisplayName(spLFOTrig),
            juce::StringArray{"Free-Running", "Key-Sync"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spLFOTargets", PresetManager::getSteppedParamDisplayName(spLFOTargets),
            juce::StringArray{"None", "Osc A", "Osc B", "Both"}, 0
        ));
        grp->addChild(makeCP(cpLFOPitchAmt, 0));
        grp->addChild(makeCP(cpLFOWModAmt, 0));
        grp->addChild(makeCP(cpLFOFilAmt, 0));
        grp->addChild(makeCP(cpLFOResAmt, 0));
        grp->addChild(makeCP(cpLFOAmpAmt, 0));
        layout.add(std::move(grp));
    }

    // 9. LFO 2
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_lfo2", "LFO 2", " : ");
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spLFO2Shape", PresetManager::getSteppedParamDisplayName(spLFO2Shape),
            juce::StringArray{"Pulse", "Triangle", "Random", "Sine", "Noise", "Saw", "RevSaw"}, 1
        ));
        grp->addChild(makeCP(cpLFO2Freq, 50));
        grp->addChild(makeCP(cpLFO2Amt, 0));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spLFO2Speed", PresetManager::getSteppedParamDisplayName(spLFO2Speed),
            juce::StringArray{"Normal (x1)", "Fast (x2)", "High (x4)", "Ultra (x8)"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spLFO2Trig", PresetManager::getSteppedParamDisplayName(spLFO2Trig),
            juce::StringArray{"Free-Running", "Key-Sync"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spLFO2Targets", PresetManager::getSteppedParamDisplayName(spLFO2Targets),
            juce::StringArray{"None", "Osc A", "Osc B", "Both"}, 0
        ));
        grp->addChild(makeCP(cpLFO2PitchAmt, 0));
        grp->addChild(makeCP(cpLFO2WModAmt, 0));
        grp->addChild(makeCP(cpLFO2FilAmt, 0));
        grp->addChild(makeCP(cpLFO2ResAmt, 0));
        grp->addChild(makeCP(cpLFO2AmpAmt, 0));
        layout.add(std::move(grp));
    }

    // 10. Voice & Polyphony
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_voice", "Voice & Polyphony", " : ");
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spVoiceCount", PresetManager::getSteppedParamDisplayName(spVoiceCount),
            juce::StringArray{"1 Voice", "2 Voices", "3 Voices", "4 Voices", "5 Voices", "6 Voices"}, 5
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spAssignerPriority", PresetManager::getSteppedParamDisplayName(spAssignerPriority),
            juce::StringArray{"Last Note", "Lowest Note", "Highest Note"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spUnison", PresetManager::getSteppedParamDisplayName(spUnison), false
        ));
        grp->addChild(makeCP(cpUnisonDetune, 0));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spChromaticPitch", PresetManager::getSteppedParamDisplayName(spChromaticPitch),
            juce::StringArray{"Continuous", "Semitones", "Octaves"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spEngineMode", PresetManager::getSteppedParamDisplayName(spEngineMode),
            juce::StringArray{"Multi-Channel", "AFX Mode (Sound per Key)"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spAFXSelectedSlot", PresetManager::getSteppedParamDisplayName(spAFXSelectedSlot),
            juce::StringArray{
                "Slot 1", "Slot 2", "Slot 3", "Slot 4",
                "Slot 5", "Slot 6", "Slot 7", "Slot 8",
                "Slot 9", "Slot 10", "Slot 11", "Slot 12",
                "Slot 13", "Slot 14", "Slot 15", "Slot 16"
            }, 0
        ));
        layout.add(std::move(grp));
    }

    // 11. Master & Output
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_master", "Master & Output", " : ");
        grp->addChild(makeCP(cpAmpLevel, 100));
        grp->addChild(makeCP(cpMasterTune, 0));
        grp->addChild(makeCP(cpNoiseVol, 0));
        grp->addChild(makeCP(cpConsoleDrive, 10));
        grp->addChild(makeCP(cpConsoleDiscontinuity, 2));
        grp->addChild(makeCP(cpMackitySend, 0));
        grp->addChild(makeCP(cpMackityDrive, 30));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spMackityReturnPad", PresetManager::getSteppedParamDisplayName(spMackityReturnPad), false
        ));
        layout.add(std::move(grp));
    }

    // 12. Performance & Modulation
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_perf", "Performance & Modulation", " : ");
        grp->addChild(makeCP(cpGlide, 0));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spBenderRange", PresetManager::getSteppedParamDisplayName(spBenderRange),
            juce::StringArray{"Major Third", "Fifth", "1 Octave"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spBenderTarget", PresetManager::getSteppedParamDisplayName(spBenderTarget),
            juce::StringArray{"None", "Osc Pitch", "Filter Cutoff", "Master Volume", "WaveMod Depth"}, 1
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spModwheelRange", PresetManager::getSteppedParamDisplayName(spModwheelRange),
            juce::StringArray{"Minimum", "Low", "High", "Maximum"}, 1
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spModwheelTarget", PresetManager::getSteppedParamDisplayName(spModwheelTarget),
            juce::StringArray{"LFO 1 Depth", "LFO 2 Depth"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spPressureRange", PresetManager::getSteppedParamDisplayName(spPressureRange),
            juce::StringArray{"Minimum", "Low", "High", "Maximum"}, 1
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spPressureTarget", PresetManager::getSteppedParamDisplayName(spPressureTarget),
            juce::StringArray{"None", "Osc Pitch", "Filter Cutoff", "Master Volume", "WaveMod Depth", "LFO 1 Depth", "LFO 2 Depth"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spTimbreTarget", PresetManager::getSteppedParamDisplayName(spTimbreTarget),
            juce::StringArray{"None", "Osc Pitch", "Filter Cutoff", "Master Volume", "WaveMod Depth", "LFO 1 Depth", "LFO 2 Depth"}, 4
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spMPEMode", PresetManager::getSteppedParamDisplayName(spMPEMode),
            juce::StringArray{"Off (Standard MIDI)", "MPE Lower (Ch 2-7)", "MPE Full (Ch 2-15)"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spMPEPitchBendRange", PresetManager::getSteppedParamDisplayName(spMPEPitchBendRange),
            juce::StringArray{"+/-2 Semitones", "+/-12 Semitones", "+/-24 Semitones (Default)", "+/-48 Semitones", "+/-96 Semitones"}, 2
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spReleaseVelocityAmt", PresetManager::getSteppedParamDisplayName(spReleaseVelocityAmt),
            juce::StringArray{"Off / Fixed", "Low Sensitivity", "Medium Sensitivity", "High Sensitivity"}, 0
        ));
        layout.add(std::move(grp));
    }

    // 13. Arpeggiator & Rhythm Sequencer
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_arp", "Arpeggiator", " : ");
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spArpMode", PresetManager::getSteppedParamDisplayName(spArpMode),
            juce::StringArray{"Off", "Up", "Down", "Up/Down", "Random", "As Played", "Chord", "Converge", "Chord Degree", "Poly Strum"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spArpOctaves", PresetManager::getSteppedParamDisplayName(spArpOctaves),
            juce::StringArray{"1 Octave", "2 Octaves", "3 Octaves", "4 Octaves"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spArpRate", PresetManager::getSteppedParamDisplayName(spArpRate),
            juce::StringArray{"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"}, 3
        ));
        grp->addChild(std::make_unique<juce::AudioParameterBool>(
            "spArpHold", PresetManager::getSteppedParamDisplayName(spArpHold), false
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spArpSync", PresetManager::getSteppedParamDisplayName(spArpSync),
            juce::StringArray{"Free (Internal)", "Host Sync (DAW)"}, 1
        ));
        grp->addChild(makeCP(cpArpGate, 83));
        grp->addChild(makeCP(cpArpSwing, 50));
        grp->addChild(makeCP(cpArpBpm, 120));
        layout.add(std::move(grp));
    }

    // 14. Modulation Matrix (8 Slots)
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_modmatrix", "Modulation Matrix", " : ");
        juce::StringArray srcNames, destNames;
        for (int i = 0; i < modSrcCount; ++i) srcNames.add(PresetManager::getModSourceDisplayName((modSource_t)i));
        for (int i = 0; i < modDestCount; ++i) destNames.add(PresetManager::getModDestDisplayName((modDest_t)i));

        for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
            juce::String slotPrefix = "matrixSlot" + juce::String(s);
            juce::String slotTitle = "Slot " + juce::String(s + 1) + " ";
            grp->addChild(std::make_unique<juce::AudioParameterChoice>(
                slotPrefix + "_src", slotTitle + "Source", srcNames, (s == 0 ? 1 : (s == 1 ? 3 : 0))
            ));
            grp->addChild(std::make_unique<juce::AudioParameterChoice>(
                slotPrefix + "_dest", slotTitle + "Dest", destNames, (s == 0 ? 11 : (s == 1 ? 5 : 0))
            ));
            grp->addChild(std::make_unique<juce::AudioParameterChoice>(
                slotPrefix + "_via", slotTitle + "Via", srcNames, 0
            ));
            grp->addChild(std::make_unique<juce::AudioParameterInt>(
                slotPrefix + "_depth", slotTitle + "Depth", -100, 100, (s < 2 ? 50 : 0), juce::AudioParameterIntAttributes().withLabel("%")
            ));
            grp->addChild(std::make_unique<juce::AudioParameterBool>(
                slotPrefix + "_en", slotTitle + "Enable", true
            ));
        }
        layout.add(std::move(grp));
    }

    // 15. Legacy & System Parameters
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_legacy", "Legacy & System", " : ");
        grp->addChild(makeCP(cpMasterLeft_Legacy, 0));
        grp->addChild(makeCP(cpMasterRight_Legacy, 0));
        grp->addChild(makeCP(cpSeqArpClock_Legacy, 0));
        layout.add(std::move(grp));
    }

    // 16. Mutable Instruments Elements Modal Synthesizer
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup>("grp_elements", "Elements Modal Resonator", " : ");
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spOscEngine", PresetManager::getSteppedParamDisplayName(spOscEngine),
            juce::StringArray{"Dual Wavetable", "Elements Modal", "Hybrid"}, 0
        ));
        grp->addChild(std::make_unique<juce::AudioParameterChoice>(
            "spElementsModel", PresetManager::getSteppedParamDisplayName(spElementsModel),
            juce::StringArray{"Modal Resonator (64 SVF)", "Non-linear String", "Chords Resonator", "Ominous Voice"}, 0
        ));
        grp->addChild(makeCP(cpElementsGeometry, 25));
        grp->addChild(makeCP(cpElementsBrightness, 50));
        grp->addChild(makeCP(cpElementsDamping, 30));
        grp->addChild(makeCP(cpElementsPosition, 40));
        grp->addChild(makeCP(cpElementsSpace, 20));
        grp->addChild(makeCP(cpElementsBow, 0));
        grp->addChild(makeCP(cpElementsBlow, 0));
        grp->addChild(makeCP(cpElementsStrike, 80));
        grp->addChild(makeCP(cpElementsMallet, 50));
        layout.add(std::move(grp));
    }

    return layout;
}

OvercyclerAudioProcessor::OvercyclerAudioProcessor(bool initializeUserStorage)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      currentProgram(0) {

    // Initialize storage & auto-seed factory presets and wavetables on first run
    if (initializeUserStorage) OverviberPaths::initializeStorage();

    // Set base directories for presets and waveforms from Documents/Overviber
    auto presetsDir = OverviberPaths::getPresetsDirectory();
    auto waveDir = OverviberPaths::getWaveDataDirectory();

    if (initializeUserStorage && presetsDir.exists()) {
        model.getPresetManager().setBaseDirectory(presetsDir.getFullPathName().toStdString());
    }
    if (initializeUserStorage && waveDir.exists()) {
        model.getWaveManager().setBaseDirectory(waveDir.getFullPathName().toStdString());
    }

    // Fallback: If preset count is 0, probe factory disk directly
    if (initializeUserStorage && model.getPresetManager().getPresetCount() == 0) {
        auto factory = OverviberPaths::findFactoryDiskDirectory();
        if (factory.exists()) {
            auto fp = factory.getChildFile("PRESETS");
            auto fw = factory.getChildFile("WAVEDATA");
            if (fp.exists()) model.getPresetManager().setBaseDirectory(fp.getFullPathName().toStdString());
            if (fw.exists()) model.getWaveManager().setBaseDirectory(fw.getFullPathName().toStdString());
        }
    }

    // Load factory preset 0 if available
    if (model.getPresetManager().getPresetCount() > 0) {
        model.loadPreset(0);
    }

    // Register APVTS parameter listeners for host automation & MIDI control
    for (int i = 0; i < cpCount; ++i) {
        const char* name = PresetManager::getContinuousParamName((continuousParameter_t)i);
        if (name && std::strlen(name) > 0 && apvts.getParameter(name) != nullptr) {
            apvts.addParameterListener(name, this);
        }
    }
    for (int i = 0; i < spCount; ++i) {
        const char* name = PresetManager::getSteppedParamName((steppedParameter_t)i);
        if (name && std::strlen(name) > 0 && apvts.getParameter(name) != nullptr) {
            apvts.addParameterListener(name, this);
        }
    }

    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        juce::String slotPrefix = "matrixSlot" + juce::String(s);
        apvts.addParameterListener(slotPrefix + "_src", this);
        apvts.addParameterListener(slotPrefix + "_dest", this);
        apvts.addParameterListener(slotPrefix + "_via", this);
        apvts.addParameterListener(slotPrefix + "_depth", this);
        apvts.addParameterListener(slotPrefix + "_en", this);
    }

    for (auto& value : midiContinuous) value.store(-1);
    for (auto& value : midiStepped) value.store(-1);
    for (auto& value : arpActiveNotes) value.store(ASSIGNER_NO_NOTE);
    for (auto& value : arpPatternNotes) value.store(ASSIGNER_NO_NOTE);
    const char* suffixes[] = {"src", "dest", "via", "depth", "en"};
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s)
        for (int f = 0; f < 5; ++f) matrixIds[s][f] = "matrixSlot" + juce::String(s) + "_" + suffixes[f];
    setDesiredFromPreset(model.getCurrentPreset());
    updateAPVTSFromEngine();
    publishEditorState();
    startTimerHz(30);
}

OvercyclerAudioProcessor::~OvercyclerAudioProcessor() {
    stopTimer();
    delete restoredState.exchange(nullptr);
    delete retiredState.exchange(nullptr);
    for (int i = 0; i < cpCount; ++i) {
        const char* name = PresetManager::getContinuousParamName((continuousParameter_t)i);
        if (name && std::strlen(name) > 0 && apvts.getParameter(name) != nullptr) {
            apvts.removeParameterListener(name, this);
        }
    }
    for (int i = 0; i < spCount; ++i) {
        const char* name = PresetManager::getSteppedParamName((steppedParameter_t)i);
        if (name && std::strlen(name) > 0 && apvts.getParameter(name) != nullptr) {
            apvts.removeParameterListener(name, this);
        }
    }
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        juce::String slotPrefix = "matrixSlot" + juce::String(s);
        apvts.removeParameterListener(slotPrefix + "_src", this);
        apvts.removeParameterListener(slotPrefix + "_dest", this);
        apvts.removeParameterListener(slotPrefix + "_via", this);
        apvts.removeParameterListener(slotPrefix + "_depth", this);
        apvts.removeParameterListener(slotPrefix + "_en", this);
    }
}

float OvercyclerAudioProcessor::potToParamVal(continuousParameter_t cp, float potVal) {
    if (cp == cpAFreq || cp == cpBFreq) {
        return (float)scan_potTo16bits((int)std::round(potVal)) / 1024.0f;
    }
    if (cp == cpMasterTune) {
        return (potVal - 500.0f) * 100.0f / 499.0f;
    }
    if (cp == cpDetune) {
        return (potVal - 500.0f) * 50.0f / 499.0f;
    }
    if (cp == cpArpBpm) {
        return 20.0f + (potVal / 999.0f) * 280.0f;
    }
    if (PresetManager::isContinuousParamZeroCentered(cp)) {
        return (potVal - 500.0f) * 100.0f / 499.0f;
    }
    return potVal * 100.0f / 999.0f;
}

float OvercyclerAudioProcessor::paramToPotVal(continuousParameter_t cp, float paramVal) {
    if (cp == cpAFreq || cp == cpBFreq) {
        return (float)scan_potFrom16bits((int)std::clamp(paramVal * 1024.0f, 0.0f, 65535.0f));
    }
    if (cp == cpMasterTune) {
        return std::clamp(500.0f + (paramVal / 100.0f) * 499.0f, 0.0f, 999.0f);
    }
    if (cp == cpDetune) {
        return std::clamp(500.0f + (paramVal / 50.0f) * 499.0f, 0.0f, 999.0f);
    }
    if (cp == cpArpBpm) {
        return std::clamp(((paramVal - 20.0f) / 280.0f) * 999.0f, 0.0f, 999.0f);
    }
    if (PresetManager::isContinuousParamZeroCentered(cp)) {
        return std::clamp(500.0f + (paramVal / 100.0f) * 499.0f, 0.0f, 999.0f);
    }
    return std::clamp((paramVal / 100.0f) * 999.0f, 0.0f, 999.0f);
}

void OvercyclerAudioProcessor::setContinuousParamFromUI(continuousParameter_t cp, float potVal) {
    potVal = std::clamp(potVal, 0.0f, 999.0f);
    uint16_t u16 = (uint16_t)scan_potTo16bits((int)std::round(potVal));
    model.setContinuousParam(cp, u16);

    const char* name = PresetManager::getContinuousParamName(cp);
    if (name && std::strlen(name) > 0) {
        if (auto* param = apvts.getParameter(name)) {
            float paramVal = potToParamVal(cp, potVal);
            isUpdatingAPVTS = true;
            param->setValueNotifyingHost(param->convertTo0to1(paramVal));
            isUpdatingAPVTS = false;
        }
    }
}

void OvercyclerAudioProcessor::setSteppedParamFromUI(steppedParameter_t sp, uint8_t stepVal) {
    model.setSteppedParam(sp, stepVal);

    const char* name = PresetManager::getSteppedParamName(sp);
    if (name && std::strlen(name) > 0) {
        if (auto* param = apvts.getParameter(name)) {
            isUpdatingAPVTS = true;
            param->setValueNotifyingHost(param->convertTo0to1((float)stepVal));
            isUpdatingAPVTS = false;
        }
    }
}

void OvercyclerAudioProcessor::parameterChanged(const juce::String& id, float value) {
    // The host can call this on its audio thread. Only bounded atomic writes.
    for (int i = 0; i < cpCount; ++i) {
        const char* name = PresetManager::getContinuousParamName(static_cast<continuousParameter_t>(i));
        if (name && id == name) {
            desiredContinuous[i].store(scan_potTo16bits(static_cast<int>(std::round(paramToPotVal(static_cast<continuousParameter_t>(i), value)))));
            hostParamsChanged.store(true); return;
        }
    }
    for (int i = 0; i < spCount; ++i) {
        const char* name = PresetManager::getSteppedParamName(static_cast<steppedParameter_t>(i));
        if (name && id == name) { desiredStepped[i].store(static_cast<int>(std::round(value))); hostParamsChanged.store(true); return; }
    }
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) for (int f = 0; f < 5; ++f)
        if (id == matrixIds[s][f]) { desiredMatrix[s][f].store(static_cast<int>(std::round(value))); hostParamsChanged.store(true); return; }
}
void OvercyclerAudioProcessor::updateAPVTSFromEngine() {
    const auto& preset = model.getCurrentPreset();
    isUpdatingAPVTS = true;

    for (int i = 0; i < cpCount; ++i) {
        auto cp = (continuousParameter_t)i;
        const char* name = PresetManager::getContinuousParamName(cp);
        if (name && std::strlen(name) > 0) {
            if (auto* param = apvts.getParameter(name)) {
                float potVal = (float)scan_potFrom16bits(preset.continuousParams[cp]);
                float paramVal = potToParamVal(cp, potVal);
                param->setValueNotifyingHost(param->convertTo0to1(paramVal));
            }
        }
    }

    for (int i = 0; i < spCount; ++i) {
        auto sp = (steppedParameter_t)i;
        const char* name = PresetManager::getSteppedParamName(sp);
        if (name && std::strlen(name) > 0) {
            if (auto* param = apvts.getParameter(name)) {
                float val = (float)preset.steppedParams[sp];
                param->setValueNotifyingHost(param->convertTo0to1(val));
            }
        }
    }

    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        juce::String slotPrefix = "matrixSlot" + juce::String(s);
        if (auto* p = apvts.getParameter(slotPrefix + "_src"))
            p->setValueNotifyingHost(p->convertTo0to1((float)preset.modMatrix[s].source));
        if (auto* p = apvts.getParameter(slotPrefix + "_dest"))
            p->setValueNotifyingHost(p->convertTo0to1((float)preset.modMatrix[s].dest));
        if (auto* p = apvts.getParameter(slotPrefix + "_via"))
            p->setValueNotifyingHost(p->convertTo0to1((float)preset.modMatrix[s].viaSource));
        if (auto* p = apvts.getParameter(slotPrefix + "_depth"))
            p->setValueNotifyingHost(p->convertTo0to1((float)preset.modMatrix[s].depth));
        if (auto* p = apvts.getParameter(slotPrefix + "_en"))
            p->setValueNotifyingHost(p->convertTo0to1(preset.modMatrix[s].enabled ? 1.0f : 0.0f));
    }

    isUpdatingAPVTS = false;
}

void OvercyclerAudioProcessor::handleMidiCC(int cc, int val) {
    auto applyCP = [this](continuousParameter_t cp, int value) {
        const int u16 = scan_potTo16bits((value * 999) / 127);
        audioEngine.setContinuousParam(cp, static_cast<uint16_t>(u16));
        desiredContinuous[cp].store(u16); midiContinuous[cp].store(u16);
    };
    auto applySP = [this](steppedParameter_t sp, int value, int maximum) {
        const int step = value * maximum / 127;
        audioEngine.setSteppedParam(sp, static_cast<uint8_t>(step));
        desiredStepped[sp].store(step); midiStepped[sp].store(step);
    };
    // Standard DAW MIDI CCs
    switch (cc) {
    case 7:  applyCP(cpAmpLevel, val); return; // Standard Volume
    case 5:  applyCP(cpGlide, val); return;    // Standard Portamento / Glide
    case 74: applyCP(cpCutoff, val); return;   // Standard Brightness / Cutoff
    case 71: applyCP(cpResonance, val); return;// Standard Resonance
    case 73: applyCP(cpAmpAtt, val); return;   // Standard Attack
    case 72: applyCP(cpAmpRel, val); return;   // Standard Release
    case 10: return; // Pan (static per voice in Overcycler)
    default: break;
    }

    // Hardware Overcycler MIDI CCs
    switch (cc) {
    case 12: applyCP(cpAFreq, val); break;
    case 13: applyCP(cpAVol, val); break;
    case 14: applyCP(cpABaseWMod, val); break;
    case 15: applyCP(cpBFreq, val); break;
    case 16: applyCP(cpBVol, val); break;
    case 17: applyCP(cpBBaseWMod, val); break;
    case 18: applyCP(cpDetune, val); break;
    case 19: applyCP(cpCutoff, val); break;
    case 20: applyCP(cpResonance, val); break;
    case 21: applyCP(cpFilEnvAmt, val); break;
    case 22: applyCP(cpFilKbdAmt, val); break;
    case 23: applyCP(cpWModAEnv, val); break;
    case 24: applyCP(cpFilAtt, val); break;
    case 25: applyCP(cpFilDec, val); break;
    case 26: applyCP(cpFilSus, val); break;
    case 27: applyCP(cpFilRel, val); break;
    case 28: applyCP(cpAmpAtt, val); break;
    case 29: applyCP(cpAmpDec, val); break;
    case 30: applyCP(cpAmpSus, val); break;
    case 31: applyCP(cpAmpRel, val); break;
    case 35: applyCP(cpAmpLevel, val); break;
    case 44: applyCP(cpLFOFreq, val); break;
    case 45: applyCP(cpLFOAmt, val); break;
    case 46: applyCP(cpLFOPitchAmt, val); break;
    case 47: applyCP(cpLFOWModAmt, val); break;
    case 48: applyCP(cpLFOFilAmt, val); break;
    case 49: applyCP(cpLFOAmpAmt, val); break;
    case 50: applyCP(cpLFO2Freq, val); break;
    case 51: applyCP(cpLFO2Amt, val); break;
    case 52: applyCP(cpModDelay, val); break;
    case 53: applyCP(cpGlide, val); break;
    case 54: applyCP(cpAmpVelocity, val); break;
    case 55: applyCP(cpFilVelocity, val); break;
    case 56: applyCP(cpMasterTune, val); break;
    case 57: applyCP(cpUnisonDetune, val); break;
    case 58: applyCP(cpNoiseVol, val); break;
    case 59: applyCP(cpLFO2PitchAmt, val); break;
    case 60: applyCP(cpLFO2WModAmt, val); break;
    case 61: applyCP(cpLFO2FilAmt, val); break;
    case 62: applyCP(cpLFO2AmpAmt, val); break;
    case 63: applyCP(cpLFOResAmt, val); break;
    case 70: applyCP(cpLFO2ResAmt, val); break;
    case 75: applyCP(cpWModBEnv, val); break;
    case 76: applyCP(cpWModVelocity, val); break;
    case 80: applySP(spAWModType, val, 6); break;
    case 83: applySP(spBWModType, val, 6); break;
    case 84: applySP(spLFOShape, val, 6); break;
    case 85: applySP(spLFOTargets, val, 3); break;
    case 86: applySP(spFilEnvSlow, val, 1); break;
    case 87: applySP(spAmpEnvSlow, val, 1); break;
    case 88: applySP(spBenderRange, val, 2); break;
    case 89: applySP(spBenderTarget, val, 4); break;
    case 90: applySP(spModwheelRange, val, 3); break;
    case 91: applySP(spModwheelTarget, val, 1); break;
    case 92: applySP(spUnison, val, 1); break;
    case 93: applySP(spAssignerPriority, val, 2); break;
    case 94: applySP(spChromaticPitch, val, 2); break;
    case 95: applySP(spOscSync, val, 1); break;
    case 107: applySP(spVoiceCount, val, 5); break;
    default: break;
    }
}

void OvercyclerAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    audioEngine.prepare((float)sampleRate);
    outputMidi.ensureSize(65536);
}

void OvercyclerAudioProcessor::releaseResources() {
    audioEngine.reset();
}

bool OvercyclerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void OvercyclerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0) { midiMessages.clear(); return; }

    // Start a fresh host block before applying queued state/parameter changes.
    // Arp mode changes can emit mandatory note-offs; clearing after those
    // changes used to discard them and leave downstream instruments hanging.
    audioEngine.setEventOffset(0);
    audioEngine.clearPendingMidiOut();
    // A restored session first, then the editor states: an editor state is
    // at least as new as the session (the timer republishes after decoding
    // it). The other order let the session's older voice patterns, matrix
    // and waves overwrite a preset chosen right after the restore.
    if (retiredState.load(std::memory_order_acquire) == nullptr) {
        if (auto* state = restoredState.exchange(nullptr)) {
            audioEngine.applyPreparedState(*state);
            retiredState.store(state, std::memory_order_release);
        }
    }
    for (int update = 0; update < 2; ++update) {
        const auto* state = stateQueue->front();
        if (!state) break;
        audioEngine.applyPreparedState(*state, true);
        stateQueue->pop();
    }
    applyDesiredParameters(audioEngine);
    audioEngine.beginVoiceMeterBlock();
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    int cursor = 0;

    // Query DAW host tempo & transport info for exact arpeggiator / MIDI clock sync
    if (auto* ph = getPlayHead()) {
        if (auto posOpt = ph->getPosition()) {
            if (posOpt->getBpm().hasValue()) {
                audioEngine.setHostBpm((float)*posOpt->getBpm());
            }
            if (posOpt->getPpqPosition().hasValue()) {
                audioEngine.setHostTransport(*posOpt->getPpqPosition(), posOpt->getIsPlaying());
            }
        }
    }

    // Parse MIDI messages
    for (const auto metadata : midiMessages) {
        const int position = std::clamp(metadata.samplePosition, cursor, numSamples);
        if (position > cursor) audioEngine.renderBlock(leftChannel + cursor, rightChannel + cursor, position - cursor, cursor);
        cursor = position;
        audioEngine.setEventOffset(position);
        if (metadata.numBytes > 3) continue; // No SysEx support; avoid heap-backed MidiMessage copies.
        const auto msg = metadata.getMessage();
        const int channel = msg.getChannel();
        const int acceptedChannel = midiInputChannel.load();
        if (acceptedChannel > 0 && channel != acceptedChannel) continue;

        if (msg.isNoteOn()) {
            uint16_t vel16 = (uint16_t)(((uint32_t)msg.getVelocity() * 65535U) / 127U);
            audioEngine.noteOn((uint8_t)msg.getNoteNumber(), vel16, (uint8_t)channel);
        } else if (msg.isNoteOff()) {
            uint16_t vel16 = (uint16_t)(((uint32_t)msg.getVelocity() * 65535U) / 127U);
            audioEngine.noteOff((uint8_t)msg.getNoteNumber(), vel16, (uint8_t)channel);
        } else if (msg.isPitchWheel()) {
            audioEngine.pitchBend((int16_t)(msg.getPitchWheelValue() - 8192), (uint8_t)channel);
        } else if (msg.isAftertouch()) { // Polyphonic Aftertouch / Key Pressure (0xA0)
            uint16_t press16 = (uint16_t)(((uint32_t)msg.getAfterTouchValue() * 65535U) / 127U);
            audioEngine.polyAftertouch((uint8_t)msg.getNoteNumber(), press16, (uint8_t)channel);
        } else if (msg.isChannelPressure()) { // Channel Pressure (0xD0)
            uint16_t press16 = (uint16_t)(((uint32_t)msg.getChannelPressureValue() * 65535U) / 127U);
            audioEngine.channelPressure(press16, (uint8_t)channel);
        } else if (msg.isController()) {
            int cc = msg.getControllerNumber();
            int val = msg.getControllerValue();
            if (cc == 1) { // Mod wheel
                uint16_t mod16 = (uint16_t)(((uint32_t)val * 65535U) / 127U);
                audioEngine.modWheel(mod16, (uint8_t)channel);
            } else if (cc == 2) { // Breath controller
                uint16_t b16 = (uint16_t)(((uint32_t)val * 65535U) / 127U);
                audioEngine.breathController(b16, (uint8_t)channel);
            } else if (cc == 11) { // Expression controller
                uint16_t exp16 = (uint16_t)(((uint32_t)val * 65535U) / 127U);
                audioEngine.expressionController(exp16, (uint8_t)channel);
            } else if (cc == 74) { // MPE Y-Axis on member channels, brightness otherwise
                if (audioEngine.isMpeMemberChannel((uint8_t)channel)) {
                    uint16_t timbre16 = (uint16_t)(((uint32_t)val * 65535U) / 127U);
                    audioEngine.timbreSlide(timbre16, (uint8_t)channel);
                } else {
                    handleMidiCC(cc, val);
                }
            } else if (cc == 64) { // Sustain pedal
                audioEngine.holdPedal(val >= 64);
            } else if (cc == 120 || cc == 123) { // All sound / notes off
                audioEngine.allNotesOff();
            } else {
                handleMidiCC(cc, val);
            }
        } else if (msg.isProgramChange()) {
            requestedProgram.store(msg.getProgramChangeNumber());
        }
    }

    if (cursor < numSamples) audioEngine.renderBlock(leftChannel + cursor, rightChannel + cursor, numSamples - cursor, cursor);
    // Host-owned MidiBuffer may grow when returning generated events. All engine
    // event storage is fixed-capacity; overflow sends All Notes Off explicitly.
    outputMidi.clear();
    for (const auto& ev : audioEngine.getPendingMidiOut()) {
        const int position = std::clamp(ev.sampleOffset, 0, numSamples - 1);
        const auto message = ev.isNoteOn
            ? juce::MidiMessage::noteOn(ev.channel, static_cast<int>(ev.note), static_cast<juce::uint8>(ev.velocity))
            : juce::MidiMessage::noteOff(ev.channel, static_cast<int>(ev.note), static_cast<juce::uint8>(ev.velocity));
        outputMidi.addEvent(message, position);
    }
    if (audioEngine.hasMidiOverflow()) {
        for (int channel = 1; channel <= 16; ++channel)
            outputMidi.addEvent(juce::MidiMessage::allNotesOff(channel), numSamples - 1);
    }
    midiMessages.swapWith(outputMidi);
    // Console meters: keep the largest value until the timer takes it.
    auto keepMax = [](std::atomic<int>& meter, int value) {
        int current = meter.load(std::memory_order_relaxed);
        while (value > current && !meter.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
    };
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) keepMax(meterLevels[v], audioEngine.getVoiceBusLoad(v));
    keepMax(meterLevels[SYNTH_VOICE_COUNT], audioEngine.getOutputPeak(0));
    keepMax(meterLevels[SYNTH_VOICE_COUNT + 1], audioEngine.getOutputPeak(1));
    uint8_t activeNotes[16]{};
    uint8_t patternNotes[16]{};
    auto& audioArp = audioEngine.getArpeggiator();
    const int activeCount = audioArp.getActiveNotes(activeNotes, 16);
    audioArp.getPattern(patternNotes, 16);
    for (int i = 0; i < 16; ++i) {
        arpActiveNotes[i].store(i < activeCount ? activeNotes[i] : ASSIGNER_NO_NOTE);
        arpPatternNotes[i].store(patternNotes[i]);
    }
    arpActiveCount.store(activeCount);
    arpCurrentStep.store(std::max(0, audioArp.getStepCount() - 1) % 16);
    arpTick.store(audioEngine.getCurrentTick());
    arpGateActive.store(audioArp.isGateActive());
}
int OvercyclerAudioProcessor::getNumPrograms() {
    return std::max(1, model.getPresetManager().getPresetCount());
}

int OvercyclerAudioProcessor::getCurrentProgram() {
    return currentProgram;
}

void OvercyclerAudioProcessor::setCurrentProgram(int index) { requestedProgram.store(index); }

const juce::String OvercyclerAudioProcessor::getProgramName(int index) {
    return juce::String(model.getPresetManager().getPresetName(index));
}

void OvercyclerAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/) {
}

juce::AudioProcessorEditor* OvercyclerAudioProcessor::createEditor() {
    return new OvercyclerAudioProcessorEditor(*this);
}

// JUCE Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OvercyclerAudioProcessor();
}
