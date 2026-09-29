#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "data/OverviberPaths.h"
#include "data/SessionState.h"
#include "data/ParamLabels.h"

namespace {
template <std::size_t N>
juce::StringArray hostChoices(const paramlabels::Choice (&choices)[N]) {
    juce::StringArray names;
    for (const auto& choice : choices) names.add(choice.host);
    return names;
}

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
    model.setHostBpm(hostBpmForEditor.load());
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

namespace {
// One host parameter: a continuous parameter (range and unit from its kind,
// default in that unit), or a stepped parameter as a choice or a toggle.
struct HostParam {
    enum Kind { Continuous, Choice, Toggle } kind;
    int param;
    int defaultValue;
    juce::StringArray choices;
};
struct HostGroup {
    const char* id;
    const char* name;
    std::vector<HostParam> params;   // empty for the modulation matrix (built per slot)
};

HostParam cont(continuousParameter_t cp, int defaultValue) { return { HostParam::Continuous, cp, defaultValue, {} }; }
HostParam choice(steppedParameter_t sp, juce::StringArray choices, int defaultValue = 0) {
    return { HostParam::Choice, sp, defaultValue, std::move(choices) };
}
HostParam toggle(steppedParameter_t sp) { return { HostParam::Toggle, sp, 0, {} }; }

// The host's parameters in their order. Automation and sessions address them
// by ID (PluginParameterScenarioTest keeps them stable).
std::vector<HostGroup> hostParameterGroups() {
    const juce::StringArray wmodTypes{ "Off", "Aliasing", "Width", "Frequency", "CrossOver", "Folder", "BitCrush" };
    const juce::StringArray lfoSpeeds{ "Normal (x1)", "Fast (x2)", "High (x4)", "Ultra (x8)" };
    const juce::StringArray lfoTriggers{ "Free-Running", "Key-Sync" };
    const juce::StringArray lfoTargets{ "None", "Osc A", "Osc B", "Both" };
    const juce::StringArray expressionRanges{ "Minimum", "Low", "High", "Maximum" };
    const juce::StringArray expressionTargets{ "None", "Osc Pitch", "Filter Cutoff", "Master Volume", "WaveMod Depth", "LFO 1 Depth", "LFO 2 Depth" };
    juce::StringArray afxSlots;
    for (int slot = 1; slot <= 16; ++slot) afxSlots.add("Slot " + juce::String(slot));

    return {
        { "grp_oscA", "Oscillator A", {
            cont(cpAFreq, 0), cont(cpAVol, 100), cont(cpABaseWMod, 0), choice(spAWModType, wmodTypes), cont(cpWModAEnv, 0) } },
        { "grp_oscB", "Oscillator B", {
            cont(cpBFreq, 0), cont(cpBVol, 100), cont(cpDetune, 0), cont(cpBBaseWMod, 0), choice(spBWModType, wmodTypes),
            cont(cpWModBEnv, 0) } },
        { "grp_sync", "Oscillator Sync", { toggle(spOscSync) } },
        { "grp_filter", "Filter (VCF)", {
            choice(spFilterModel, { "SSI2144 (Ladder)", "SEM (2-Pole SVF)", "Shelves (EQ/SVF)", "SST Vintage (Moog)" }),
            choice(spFilterMode, { "Mode 1", "Mode 2", "Mode 3", "Mode 4" }),
            choice(spSemModel, { "OB-Xd 12 dB", "Oberheim (Pirkle)", "Vult SVF", "Cytomic SVF", "Liquid (Ripples)" }),
            cont(cpCutoff, 100), cont(cpResonance, 0), cont(cpFilEnvAmt, 0), cont(cpFilKbdAmt, 50), cont(cpFilVelocity, 0),
            cont(cpShelvesLsFreq, 20), cont(cpShelvesLsGain, 0), cont(cpShelvesP1Gain, 0), cont(cpShelvesP2Freq, 65),
            cont(cpShelvesP2Gain, 0), cont(cpShelvesP2Q, 30), cont(cpShelvesHsFreq, 80), cont(cpShelvesHsGain, 0) } },
        { "grp_filEnv", "Filter Envelope", {
            cont(cpFilAtt, 0), cont(cpFilDec, 50), cont(cpFilSus, 50), cont(cpFilRel, 50),
            toggle(spFilEnvLoop), toggle(spFilEnvLin), toggle(spFilEnvSlow) } },
        { "grp_ampEnv", "Amp Envelope (VCA)", {
            cont(cpAmpAtt, 0), cont(cpAmpDec, 50), cont(cpAmpSus, 100), cont(cpAmpRel, 50), cont(cpAmpVelocity, 0),
            toggle(spAmpEnvLoop), toggle(spAmpEnvLin), toggle(spAmpEnvSlow) } },
        { "grp_wmodEnv", "WaveMod Envelope", {
            cont(cpWModAtt, 0), cont(cpWModDec, 50), cont(cpWModSus, 50), cont(cpWModRel, 50), cont(cpWModVelocity, 0),
            toggle(spWModEnvLoop), toggle(spWModEnvLin), toggle(spWModEnvSlow) } },
        { "grp_lfo1", "LFO 1", {
            choice(spLFOShape, hostChoices(paramlabels::kLfoShapes), 1), cont(cpLFOFreq, 50), cont(cpLFOAmt, 0),
            cont(cpModDelay, 0), choice(spLFOSpeed, lfoSpeeds), choice(spLFOTrig, lfoTriggers),
            choice(spLFOTargets, lfoTargets), cont(cpLFOPitchAmt, 0), cont(cpLFOWModAmt, 0), cont(cpLFOFilAmt, 0),
            cont(cpLFOResAmt, 0), cont(cpLFOAmpAmt, 0) } },
        { "grp_lfo2", "LFO 2", {
            choice(spLFO2Shape, hostChoices(paramlabels::kLfoShapes), 1), cont(cpLFO2Freq, 50), cont(cpLFO2Amt, 0),
            choice(spLFO2Speed, lfoSpeeds), choice(spLFO2Trig, lfoTriggers), choice(spLFO2Targets, lfoTargets),
            cont(cpLFO2PitchAmt, 0), cont(cpLFO2WModAmt, 0), cont(cpLFO2FilAmt, 0), cont(cpLFO2ResAmt, 0),
            cont(cpLFO2AmpAmt, 0) } },
        { "grp_voice", "Voice & Polyphony", {
            choice(spVoiceCount, { "1 Voice", "2 Voices", "3 Voices", "4 Voices", "5 Voices", "6 Voices" }, 5),
            choice(spAssignerPriority, { "Last Note", "Lowest Note", "Highest Note" }),
            toggle(spUnison), cont(cpUnisonDetune, 0),
            choice(spChromaticPitch, { "Continuous", "Semitones", "Octaves" }),
            choice(spEngineMode, { "Multi-Channel", "AFX Mode (Sound per Key)" }),
            choice(spAFXSelectedSlot, afxSlots) } },
        { "grp_master", "Master & Output", {
            cont(cpAmpLevel, 100), cont(cpMasterTune, 0), cont(cpNoiseVol, 0), cont(cpConsoleDrive, 10),
            cont(cpConsoleDiscontinuity, 2), cont(cpMackitySend, 0), cont(cpMackityDrive, 30), toggle(spMackityReturnPad) } },
        { "grp_perf", "Performance & Modulation", {
            cont(cpGlide, 0),
            choice(spBenderRange, { "Major Third", "Fifth", "1 Octave" }),
            choice(spBenderTarget, { "None", "Osc Pitch", "Filter Cutoff", "Master Volume", "WaveMod Depth" }, 1),
            choice(spModwheelRange, expressionRanges, 1),
            choice(spModwheelTarget, { "LFO 1 Depth", "LFO 2 Depth" }),
            choice(spPressureRange, expressionRanges, 1),
            choice(spPressureTarget, expressionTargets),
            choice(spTimbreTarget, expressionTargets, 4),
            choice(spMPEMode, { "Off (Standard MIDI)", "MPE Lower (Ch 2-7)", "MPE Full (Ch 2-15)" }),
            choice(spMPEPitchBendRange, { "+/-2 Semitones", "+/-12 Semitones", "+/-24 Semitones (Default)", "+/-48 Semitones", "+/-96 Semitones" }, 2),
            choice(spReleaseVelocityAmt, { "Off / Fixed", "Low Sensitivity", "Medium Sensitivity", "High Sensitivity" }) } },
        { "grp_arp", "Arpeggiator", {
            choice(spArpMode, hostChoices(paramlabels::kArpModes)),
            choice(spArpOctaves, { "1 Octave", "2 Octaves", "3 Octaves", "4 Octaves" }),
            choice(spArpRate, { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 3),
            toggle(spArpHold),
            choice(spArpSync, { "Free (Internal)", "Host Sync (DAW)" }, 1),
            cont(cpArpGate, 83), cont(cpArpSwing, 50), cont(cpArpBpm, 120) } },
        { "grp_modmatrix", "Modulation Matrix", {} },
        { "grp_legacy", "Legacy & System", {
            cont(cpMasterLeft_Legacy, 0), cont(cpMasterRight_Legacy, 0), cont(cpSeqArpClock_Legacy, 0) } },
        { "grp_elements", "Elements Modal Resonator", {
            choice(spOscEngine, { "Dual Wavetable", "Elements Modal", "Hybrid" }),
            choice(spElementsModel, { "Modal Resonator (64 SVF)", "Non-linear String", "Chords Resonator", "Ominous Voice" }),
            cont(cpElementsGeometry, 25), cont(cpElementsBrightness, 50), cont(cpElementsDamping, 30),
            cont(cpElementsPosition, 40), cont(cpElementsSpace, 20), cont(cpElementsBow, 0), cont(cpElementsBlow, 0),
            cont(cpElementsStrike, 80), cont(cpElementsContour, 50), cont(cpElementsFlow, 50),
            cont(cpElementsMallet, 50), cont(cpElementsBowTimbre, 50), cont(cpElementsBlowTimbre, 50),
            cont(cpElementsStrikeTimbre, 50) } },
    };
}

// A continuous parameter in its host unit: semitones, cents, BPM or percent.
std::unique_ptr<juce::AudioParameterInt> makeContinuous(continuousParameter_t cp, int defaultValue) {
    const char* id = PresetManager::getContinuousParamName(cp);
    const char* name = PresetManager::getContinuousParamDisplayName(cp);
    auto make = [&](int low, int high, const char* unit) {
        return std::make_unique<juce::AudioParameterInt>(id, name, low, high, defaultValue,
                                                         juce::AudioParameterIntAttributes().withLabel(unit));
    };
    if (cp == cpAFreq || cp == cpBFreq) return make(0, 64, "st");       // base pitch 0 .. 64 semitones
    if (cp == cpMasterTune) return make(-100, 100, "ct");               // +-1 semitone
    if (cp == cpDetune) return make(-50, 50, "ct");
    if (cp == cpArpBpm) return make(20, 300, "BPM");
    if (PresetManager::isContinuousParamZeroCentered(cp)) return make(-100, 100, "%");
    return make(0, 100, "%");
}

// The eight slots: source, destination, via source, depth and enable.
void addModMatrixParameters(juce::AudioProcessorParameterGroup& group) {
    juce::StringArray srcNames, destNames;
    for (int i = 0; i < modSrcCount; ++i) srcNames.add(PresetManager::getModSourceDisplayName((modSource_t)i));
    for (int i = 0; i < modDestCount; ++i) destNames.add(PresetManager::getModDestDisplayName((modDest_t)i));
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        const juce::String slotPrefix = "matrixSlot" + juce::String(s);
        const juce::String slotTitle = "Slot " + juce::String(s + 1) + " ";
        group.addChild(std::make_unique<juce::AudioParameterChoice>(
            slotPrefix + "_src", slotTitle + "Source", srcNames, (s == 0 ? 1 : (s == 1 ? 3 : 0))));
        group.addChild(std::make_unique<juce::AudioParameterChoice>(
            slotPrefix + "_dest", slotTitle + "Dest", destNames, (s == 0 ? 11 : (s == 1 ? 5 : 0))));
        group.addChild(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "_via", slotTitle + "Via", srcNames, 0));
        group.addChild(std::make_unique<juce::AudioParameterInt>(
            slotPrefix + "_depth", slotTitle + "Depth", -100, 100, (s < 2 ? 50 : 0), juce::AudioParameterIntAttributes().withLabel("%")));
        group.addChild(std::make_unique<juce::AudioParameterBool>(slotPrefix + "_en", slotTitle + "Enable", true));
    }
}
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout OvercyclerAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& g : hostParameterGroups()) {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>(g.id, g.name, " : ");
        if (juce::String(g.id) == "grp_modmatrix") addModMatrixParameters(*group);
        for (const auto& p : g.params) {
            if (p.kind == HostParam::Continuous) {
                group->addChild(makeContinuous((continuousParameter_t)p.param, p.defaultValue));
                continue;
            }
            const auto sp = (steppedParameter_t)p.param;
            const char* id = PresetManager::getSteppedParamName(sp);
            const char* name = PresetManager::getSteppedParamDisplayName(sp);
            if (p.kind == HostParam::Toggle)
                group->addChild(std::make_unique<juce::AudioParameterBool>(id, name, p.defaultValue != 0));
            else
                group->addChild(std::make_unique<juce::AudioParameterChoice>(id, name, p.choices, p.defaultValue));
        }
        layout.add(std::move(group));
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

void OvercyclerAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    audioEngine.prepare((float)sampleRate);
    model.getOutputScope().setSampleRate(sampleRate);
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
                hostBpmForEditor.store((float)*posOpt->getBpm());
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
        // Raw bytes: no heap-backed MidiMessage copies; SysEx is ignored.
        mididispatch::dispatch(audioEngine, metadata.data, metadata.numBytes, midiInputChannel.load(), &midiListener);
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
    model.getOutputScope().push(buffer.getReadPointer(0), buffer.getReadPointer(1), numSamples);
    // Console meters: keep the largest value until the timer takes it.
    auto keepMax = [](std::atomic<int>& meter, int value) {
        int current = meter.load(std::memory_order_relaxed);
        while (value > current && !meter.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
    };
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) keepMax(meterLevels[v], audioEngine.getVoiceBusLoad(v));
    keepMax(meterLevels[SYNTH_VOICE_COUNT], audioEngine.getOutputPeak(0));
    keepMax(meterLevels[SYNTH_VOICE_COUNT + 1], audioEngine.getOutputPeak(1));
    publishArpTelemetry();
}

// The editor's arp matrix: held notes, pattern and position of the audio
// engine's arpeggiator after each block.
void OvercyclerAudioProcessor::publishArpTelemetry() {
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
