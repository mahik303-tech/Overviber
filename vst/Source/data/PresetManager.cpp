#include "PresetManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

static const char* cpNames[cpCount] = {
    "cpAFreq","cpAVol","cpABaseWMod","cpBFreq","cpBVol","cpBBaseWMod","cpDetune",
    "cpCutoff","cpResonance","cpFilEnvAmt","cpFilKbdAmt","cpWModAEnv",
    "cpFilAtt","cpFilDec","cpFilSus","cpFilRel",
    "cpAmpAtt","cpAmpDec","cpAmpSus","cpAmpRel",
    "cpLFOFreq","cpLFOAmt","cpLFOPitchAmt","cpLFOWModAmt","cpLFOFilAmt","cpLFOAmpAmt",
    "cpLFO2Freq","cpLFO2Amt","cpModDelay","cpGlide",
    "cpAmpVelocity","cpFilVelocity","cpMasterTune","cpUnisonDetune",
    "cpMasterLeft_Legacy","cpMasterRight_Legacy","cpSeqArpClock_Legacy","cpNoiseVol",
    "cpLFO2PitchAmt","cpLFO2WModAmt","cpLFO2FilAmt","cpLFO2AmpAmt",
    "cpLFOResAmt","cpLFO2ResAmt",
    "cpWModAtt","cpWModDec","cpWModSus","cpWModRel",
    "cpWModBEnv","cpWModVelocity","cpAmpLevel",
    "cpShelvesLsFreq","cpShelvesLsGain","cpShelvesP1Gain",
    "cpShelvesP2Freq","cpShelvesP2Gain","cpShelvesP2Q",
    "cpShelvesHsFreq","cpShelvesHsGain",
    "cpMackityInTrim","cpMackityOutPad",
    "cpArpGate","cpArpSwing","cpArpBpm",
    "cpConsoleDiscontinuity",
    "cpElementsGeometry","cpElementsBrightness","cpElementsDamping",
    "cpElementsPosition","cpElementsSpace","cpElementsBow",
    "cpElementsBlow","cpElementsStrike","cpElementsMallet",
    "cpMackitySend","cpMackityDrive"
};

static const uint8_t cpZeroCentered[cpCount] = {
    0,0,1,0,0,1,1,
    0,0,1,0,1,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,0,0,
    0,0,0,0,
    0,0,1,0,
    0,0,0,0,
    0,0,0,0,
    0,0,
    0,0,0,0,
    1,0,0,
    0,1,1,
    0,1,0,
    0,1,
    0,0,
    0,0,0,
    0,
    0,0,0,0,0,0,0,0,0,
    0,0
};

static const char* cpDisplayNames[cpCount] = {
    "Osc A: Pitch",               // cpAFreq
    "Osc A: Level",               // cpAVol
    "Osc A: WaveMod",             // cpABaseWMod
    "Osc B: Pitch",               // cpBFreq
    "Osc B: Level",               // cpBVol
    "Osc B: WaveMod",             // cpBBaseWMod
    "Osc B: Detune",              // cpDetune
    "Filter: Cutoff",             // cpCutoff
    "Filter: Reso",               // cpResonance
    "Filter: EnvAmt",             // cpFilEnvAmt
    "Filter: KeyTrk",             // cpFilKbdAmt
    "Osc A: EnvAmt",              // cpWModAEnv
    "FilEnv: Attack",             // cpFilAtt
    "FilEnv: Decay",              // cpFilDec
    "FilEnv: Sustain",            // cpFilSus
    "FilEnv: Release",            // cpFilRel
    "AmpEnv: Attack",             // cpAmpAtt
    "AmpEnv: Decay",              // cpAmpDec
    "AmpEnv: Sustain",            // cpAmpSus
    "AmpEnv: Release",            // cpAmpRel
    "LFO 1: Speed",               // cpLFOFreq
    "LFO 1: Depth",               // cpLFOAmt
    "LFO 1: Pitch",               // cpLFOPitchAmt
    "LFO 1: WaveMod",             // cpLFOWModAmt
    "LFO 1: Filter",              // cpLFOFilAmt
    "LFO 1: Volume",              // cpLFOAmpAmt
    "LFO 2: Speed",               // cpLFO2Freq
    "LFO 2: Depth",               // cpLFO2Amt
    "LFO 1: Delay",               // cpModDelay
    "Perf: Glide",                // cpGlide
    "AmpEnv: Velo",               // cpAmpVelocity
    "FilEnv: Velo",               // cpFilVelocity
    "Master: Tune",               // cpMasterTune
    "Voice: Spread",              // cpUnisonDetune
    "Legacy: Left",               // cpMasterLeft_Legacy
    "Legacy: Right",              // cpMasterRight_Legacy
    "Legacy: Clock",              // cpSeqArpClock_Legacy
    "Master: Noise",              // cpNoiseVol
    "LFO 2: Pitch",               // cpLFO2PitchAmt
    "LFO 2: WaveMod",             // cpLFO2WModAmt
    "LFO 2: Filter",              // cpLFO2FilAmt
    "LFO 2: Volume",              // cpLFO2AmpAmt
    "LFO 1: Reso",                // cpLFOResAmt
    "LFO 2: Reso",                // cpLFO2ResAmt
    "WModEnv: Attack",            // cpWModAtt
    "WModEnv: Decay",             // cpWModDec
    "WModEnv: Sustain",           // cpWModSus
    "WModEnv: Release",           // cpWModRel
    "Osc B: EnvAmt",              // cpWModBEnv
    "WModEnv: Velo",              // cpWModVelocity
    "Master: Volume",             // cpAmpLevel
    "EQ: Low Freq",               // cpShelvesLsFreq
    "EQ: Low Gain",               // cpShelvesLsGain
    "EQ: Mid Low Gain",           // cpShelvesP1Gain
    "EQ: Mid High Freq",          // cpShelvesP2Freq
    "EQ: Mid High Gain",          // cpShelvesP2Gain
    "EQ: Mid High Q",             // cpShelvesP2Q
    "EQ: High Freq",              // cpShelvesHsFreq
    "EQ: High Gain",              // cpShelvesHsGain
    "Console: Drive",             // cpConsoleDrive
    "Console: Level",             // cpConsolePad
    "Arp: Gate",                  // cpArpGate
    "Arp: Swing",                 // cpArpSwing
    "Arp: BPM",                   // cpArpBpm
    "Console: Discontinuity",     // cpConsoleDiscontinuity
    "Elements: Geometry",         // cpElementsGeometry
    "Elements: Brightness",       // cpElementsBrightness
    "Elements: Damping",          // cpElementsDamping
    "Elements: Position",         // cpElementsPosition
    "Elements: Space",            // cpElementsSpace
    "Elements: Bow Level",        // cpElementsBow
    "Elements: Blow Level",       // cpElementsBlow
    "Elements: Strike Level",     // cpElementsStrike
    "Elements: Mallet Hardness",  // cpElementsMallet
    "Mackity: Send",              // cpMackitySend
    "Mackity: Drive"              // cpMackityDrive
};

static const char* spNames[spCount] = {
    "spABank_Unsaved","spAWave_Unsaved","spAWModType","spAWModEnvEn_Legacy",
    "spBBank_Unsaved","spBWave_Unsaved","spBWModType","spBWModEnvEn_Legacy",
    "spLFOShape","spLFOSpeed","spLFOTargets",
    "spFilEnvSlow","spAmpEnvSlow",
    "spBenderRange","spBenderTarget",
    "spModwheelRange","spModwheelTarget",
    "spUnison","spAssignerPriority","spChromaticPitch",
    "spSync",
    "spAXOvrBank_Unsaved","spAXOvrWave_Unsaved",
    "spFilEnvLin",
    "spLFO2Shape","spLFO2Speed","spLFO2Targets","spVoiceCount",
    "spPresetType","spPresetStyle",
    "spAmpEnvLin","spFilEnvLoop","spAmpEnvLoop",
    "spWModEnvSlow","spWModEnvLin","spWModEnvLoop",
    "spPressureRange","spPressureTarget",
    "spBXOvrBank_Unsaved","spBXOvrWave_Unsaved",
    "spLFOTrig","spLFO2Trig",
    "spFilterModel","spFilterMode",
    "spArpOctaves","spArpRate","spArpHold","spArpMode","spArpSync",
    "spTimbreTarget","spMPEMode","spMPEPitchBendRange","spReleaseVelocityAmt",
    "spEngineMode","spAFXSelectedSlot",
    "spOscEngine","spElementsModel",
    "spMackityReturnPad",
    "spSemModel"
};

static const char* spDisplayNames[spCount] = {
    "Osc A: Bank",                   // spABank_Unsaved
    "Osc A: Wave",                   // spAWave_Unsaved
    "Osc A: WMod Mode",              // spAWModType
    "Osc A: WMod Env",               // spAWModEnvEn_Legacy
    "Osc B: Bank",                   // spBBank_Unsaved
    "Osc B: Wave",                   // spBWave_Unsaved
    "Osc B: WMod Mode",              // spBWModType
    "Osc B: WMod Env",               // spBWModEnvEn_Legacy
    "LFO 1: Wave",                   // spLFOShape
    "LFO 1: Rate",                   // spLFOSpeed
    "LFO 1: Target",                 // spLFOTargets
    "FilEnv: Slow",                  // spFilEnvSlow
    "AmpEnv: Slow",                  // spAmpEnvSlow
    "Perf: Bend Range",              // spBenderRange
    "Perf: Bend Dest",               // spBenderTarget
    "Perf: Wheel Int",               // spModwheelRange
    "Perf: Wheel Dest",              // spModwheelTarget
    "Voice: Unison",                 // spUnison
    "Voice: Priority",               // spAssignerPriority
    "Voice: Quantize",               // spChromaticPitch
    "Osc: Hard Sync",                // spSync
    "Osc A: XOvr Bank",              // spAXOvrBank_Unsaved
    "Osc A: XOvr Wave",              // spAXOvrWave_Unsaved
    "FilEnv: Linear",                // spFilEnvLin
    "LFO 2: Wave",                   // spLFO2Shape
    "LFO 2: Rate",                   // spLFO2Speed
    "LFO 2: Target",                 // spLFO2Targets
    "Voice: Count",                  // spVoiceCount
    "Preset: Type",                  // spPresetType
    "Preset: Style",                 // spPresetStyle
    "AmpEnv: Linear",                // spAmpEnvLin
    "FilEnv: Loop",                  // spFilEnvLoop
    "AmpEnv: Loop",                  // spAmpEnvLoop
    "WModEnv: Slow",                 // spWModEnvSlow
    "WModEnv: Linear",               // spWModEnvLin
    "WModEnv: Loop",                 // spWModEnvLoop
    "Perf: AT Sens",                 // spPressureRange
    "Perf: AT Dest",                 // spPressureTarget
    "Osc B: XOvr Bank",              // spBXOvrBank_Unsaved
    "Osc B: XOvr Wave",              // spBXOvrWave_Unsaved
    "LFO 1: KeySync",                // spLFOTrig
    "LFO 2: KeySync",                // spLFO2Trig
    "Filter Model",                  // spFilterModel
    "Filter Mode",                   // spFilterMode
    "Arp: Octaves",                  // spArpOctaves
    "Arp: Rate",                     // spArpRate
    "Arp: Hold",                     // spArpHold
    "Arp: Mode",                     // spArpMode
    "Arp: Sync",                     // spArpSync
    "Perf: Timbre Dest",             // spTimbreTarget
    "MPE: Mode",                     // spMPEMode
    "MPE: Bend Range",               // spMPEPitchBendRange
    "Perf: Rel Velocity",            // spReleaseVelocityAmt
    "Engine: Mode",                  // spEngineMode
    "AFX: Selected Slot",            // spAFXSelectedSlot
    "Osc: Engine",                   // spOscEngine
    "Elements: Model",               // spElementsModel
    "Mackity: Pad -6 dB",            // spMackityReturnPad
    "Filter: SEM Model"              // spSemModel
};

const char* PresetManager::getContinuousParamName(continuousParameter_t cp) {
    if (cp >= 0 && cp < cpCount) return cpNames[cp];
    return "";
}

const char* PresetManager::getContinuousParamDisplayName(continuousParameter_t cp) {
    if (cp >= 0 && cp < cpCount) return cpDisplayNames[cp];
    return "";
}

bool PresetManager::isContinuousParamZeroCentered(continuousParameter_t cp) {
    if (cp >= 0 && cp < cpCount) return cpZeroCentered[cp] != 0;
    return false;
}

const char* PresetManager::getSteppedParamName(steppedParameter_t sp) {
    if (sp >= 0 && sp < spCount) return spNames[sp];
    return "";
}

const char* PresetManager::getSteppedParamDisplayName(steppedParameter_t sp) {
    if (sp >= 0 && sp < spCount) return spDisplayNames[sp];
    return "";
}

PresetManager::PresetManager() {
}

void PresetManager::setBaseDirectory(const std::string& path) {
    basePath = path;
    scanPresets();
}

void PresetManager::scanPresets() {
    presetFiles.clear();
    presetNames.clear();

    if (basePath.empty() || !fs::exists(basePath)) return;

    try {
        for (const auto& entry : fs::directory_iterator(basePath)) {
            if (entry.is_regular_file()) {
                std::string fname = entry.path().filename().string();
                if (fname.rfind("preset_", 0) == 0 && entry.path().extension() == ".conf") {
                    // Extract number
                    int num = 0;
                    try {
                        std::string numStr = fname.substr(7, 4);
                        num = std::stoi(numStr);
                    } catch (...) {}

                    presetFiles.push_back({num, entry.path().string()});
                }
            }
        }
        std::sort(presetFiles.begin(), presetFiles.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        for (const auto& p : presetFiles) {
            PresetData data;
            if (loadPresetFile(p.second, data)) {
                presetNames.push_back(data.presetName);
            } else {
                presetNames.push_back("Preset " + std::to_string(p.first));
            }
        }
    } catch (...) {}
}

std::string PresetManager::getPresetName(int index) const {
    if (index >= 0 && index < (int)presetNames.size()) {
        return presetNames[index];
    }
    return "Preset " + std::to_string(index);
}

bool PresetManager::loadPreset(int index, PresetData& outPreset) {
    if (index >= 0 && index < (int)presetFiles.size()) {
        return loadPresetFile(presetFiles[index].second, outPreset);
    }
    return false;
}

bool PresetManager::loadPresetFile(const std::string& filePath, PresetData& outPreset) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parsePresetString(buffer.str(), outPreset);
}

static const char* modSrcNames[modSrcCount] = {
    "None", "ModWheel", "PitchBend", "Aftertouch", "TimbreSlide", "Velocity", "ReleaseVelocity",
    "KeyTrack", "Breath", "Expression", "FilterEnv", "AmpEnv", "WaveModEnv",
    "LFO1", "LFO1_Uni", "LFO2", "LFO2_Uni", "Constant"
};

static const char* modSrcDisplayNames[modSrcCount] = {
    "Off / None",
    "Mod Wheel (CC 1)",
    "Pitch Bend Wheel",
    "Aftertouch (Poly & Ch)",
    "Timbre / Slide (CC 74)",
    "Note-On Velocity",
    "Note-Off Lift Velocity",
    "Key Tracking (Pitch)",
    "Breath Controller (CC 2)",
    "Expression (CC 11)",
    "Filter Envelope",
    "Amp Envelope",
    "WaveMod Envelope",
    "LFO 1 (Bipolar)",
    "LFO 1 (Unipolar +)",
    "LFO 2 (Bipolar)",
    "LFO 2 (Unipolar +)",
    "Constant (+1.0 Bias)"
};

static const char* modDestNames[modDestCount] = {
    "None", "PitchAll", "PitchOscA", "PitchOscB", "Detune",
    "WaveModAll", "WaveModOscA", "WaveModOscB", "VolOscA", "VolOscB", "NoiseVol",
    "Cutoff", "Resonance", "AmpLevel",
    "ElementsGeometry", "ElementsBrightness", "ElementsDamping", "ElementsPosition",
    "ElementsSpace", "ElementsBow", "ElementsBlow", "ElementsStrike"
};

static const char* modDestDisplayNames[modDestCount] = {
    "Off / None",
    "Master Pitch (Osc A+B)",
    "Osc A Pitch",
    "Osc B Pitch",
    "Osc Detune",
    "WaveMod Depth (Osc A+B)",
    "Osc A WaveMod Depth",
    "Osc B WaveMod Depth",
    "Osc A Level",
    "Osc B Level",
    "Noise Generator Level",
    "Filter Cutoff",
    "Filter Resonance",
    "Master Amp (VCA)",
    "Elements: Geometry",
    "Elements: Brightness",
    "Elements: Damping",
    "Elements: Strike Position",
    "Elements: Stereo Space",
    "Elements: Bow Level",
    "Elements: Blow Level",
    "Elements: Strike Level"
};

const char* PresetManager::getModSourceName(modSource_t src) {
    if (src >= 0 && src < modSrcCount) return modSrcNames[src];
    return "None";
}

const char* PresetManager::getModSourceDisplayName(modSource_t src) {
    if (src >= 0 && src < modSrcCount) return modSrcDisplayNames[src];
    return "Off / None";
}

const char* PresetManager::getModDestName(modDest_t dest) {
    if (dest >= 0 && dest < modDestCount) return modDestNames[dest];
    return "None";
}

const char* PresetManager::getModDestDisplayName(modDest_t dest) {
    if (dest >= 0 && dest < modDestCount) return modDestDisplayNames[dest];
    return "Off / None";
}

static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool PresetManager::parsePresetString(const std::string& content, PresetData& outPreset) {
    outPreset.setDefaults();

    std::istringstream stream(content);
    std::string line;
    bool foundMatrixSlot = false;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eqPos));
        std::string valStr = trim(line.substr(eqPos + 1));

        if (key == "presetName") {
            outPreset.presetName = valStr;
        } else if (key == "bank0") outPreset.oscBank[abxAMain] = valStr;
        else if (key == "wave0") outPreset.oscWave[abxAMain] = valStr;
        else if (key == "bank1") outPreset.oscBank[abxBMain] = valStr;
        else if (key == "wave1") outPreset.oscWave[abxBMain] = valStr;
        else if (key == "bank2") outPreset.oscBank[abxACrossover] = valStr;
        else if (key == "wave2") outPreset.oscWave[abxACrossover] = valStr;
        else if (key == "bank3") outPreset.oscBank[abxBCrossover] = valStr;
        else if (key == "wave3") outPreset.oscWave[abxBCrossover] = valStr;
        else if (key.rfind("voicePattern", 0) == 0) {
            int vIdx = key.back() - '0';
            if (vIdx >= 0 && vIdx < SYNTH_VOICE_COUNT) {
                outPreset.voicePattern[vIdx] = (uint8_t)std::stoi(valStr);
            }
        } else if (key.size() >= 13 && key.rfind("matrixSlot", 0) == 0) {
            foundMatrixSlot = true;
            // Parse matrixSlot{0..7}_{src,dest,via,depth,curve,en}
            int slot = key[10] - '0';
            if (slot >= 0 && slot < MOD_MATRIX_SLOT_COUNT) {
                std::string sub = key.substr(12);
                if (sub == "src") {
                    // Match by number or name
                    try {
                        outPreset.modMatrix[slot].source = (uint8_t)std::stoi(valStr);
                    } catch (...) {
                        for (int s = 0; s < modSrcCount; ++s) {
                            if (valStr == modSrcNames[s]) { outPreset.modMatrix[slot].source = (uint8_t)s; break; }
                        }
                    }
                } else if (sub == "dest") {
                    try {
                        outPreset.modMatrix[slot].dest = (uint8_t)std::stoi(valStr);
                    } catch (...) {
                        for (int d = 0; d < modDestCount; ++d) {
                            if (valStr == modDestNames[d]) { outPreset.modMatrix[slot].dest = (uint8_t)d; break; }
                        }
                    }
                } else if (sub == "via") {
                    try {
                        outPreset.modMatrix[slot].viaSource = (uint8_t)std::stoi(valStr);
                    } catch (...) {
                        for (int s = 0; s < modSrcCount; ++s) {
                            if (valStr == modSrcNames[s]) { outPreset.modMatrix[slot].viaSource = (uint8_t)s; break; }
                        }
                    }
                } else if (sub == "depth") {
                    outPreset.modMatrix[slot].depth = (int16_t)std::clamp(std::stoi(valStr), -100, 100);
                } else if (sub == "curve") {
                    outPreset.modMatrix[slot].curve = (uint8_t)std::stoi(valStr);
                } else if (sub == "en") {
                    outPreset.modMatrix[slot].enabled = (std::stoi(valStr) != 0);
                }
            }
        } else {
            // Check continuous parameters
            bool foundCP = false;
            for (int i = 0; i < cpCount; ++i) {
                if (key == cpNames[i]) {
                    int v = std::stoi(valStr);
                    if (cpZeroCentered[i]) {
                        v += (SCAN_POT_MAX_VALUE + 1) / 2;
                    }
                    v = scan_potTo16bits(v);
                    outPreset.continuousParams[i] = (uint16_t)std::clamp(v, 0, (int)UINT16_MAX);
                    foundCP = true;
                    break;
                }
            }

            if (!foundCP) {
                // Check stepped parameters
                for (int i = 0; i < spCount; ++i) {
                    if (key == spNames[i] ||
                        (i == spAWModType && key == "spABaseWMod") ||
                        (i == spBWModType && key == "spBBaseWMod") ||
                        (i == spOscSync && key == "spSync")) {
                        int v = std::stoi(valStr);
                        outPreset.steppedParams[i] = (uint8_t)v;
                        break;
                    }
                }
            }
        }
    }

    // The former Single mode (2) is now plain Multi-Channel.
    if (outPreset.steppedParams[spEngineMode] >= emCount)
        outPreset.steppedParams[spEngineMode] = emMultiChannel;

    // Backward compatibility: If no mod matrix was explicitly in the preset file,
    // populate matrix slots from legacy performance parameters (spModwheelTarget, spPressureTarget, spTimbreTarget).
    if (!foundMatrixSlot) {
        for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
            outPreset.modMatrix[s] = ModMatrixSlot{};
        }
        int slotIdx = 0;
        if (outPreset.steppedParams[spModwheelTarget] == modPitch) {
            outPreset.modMatrix[slotIdx++] = { modSrcModWheel, modDestPitchAll, modSrcNone, 50, 0, true };
        } else if (outPreset.steppedParams[spModwheelTarget] == modFilter) {
            outPreset.modMatrix[slotIdx++] = { modSrcModWheel, modDestCutoff, modSrcNone, 50, 0, true };
        } else if (outPreset.steppedParams[spModwheelTarget] == modWaveMod) {
            outPreset.modMatrix[slotIdx++] = { modSrcModWheel, modDestWaveModAll, modSrcNone, 50, 0, true };
        }

        if (outPreset.steppedParams[spPressureTarget] == modPitch) {
            outPreset.modMatrix[slotIdx++] = { modSrcAftertouch, modDestPitchAll, modSrcNone, -50, 0, true };
        } else if (outPreset.steppedParams[spPressureTarget] == modFilter) {
            outPreset.modMatrix[slotIdx++] = { modSrcAftertouch, modDestCutoff, modSrcNone, 50, 0, true };
        } else if (outPreset.steppedParams[spPressureTarget] == modWaveMod) {
            outPreset.modMatrix[slotIdx++] = { modSrcAftertouch, modDestWaveModAll, modSrcNone, 50, 0, true };
        } else if (outPreset.steppedParams[spPressureTarget] == modVolume) {
            outPreset.modMatrix[slotIdx++] = { modSrcAftertouch, modDestAmpLevel, modSrcNone, 50, 0, true };
        }

        if (outPreset.steppedParams[spTimbreTarget] == modPitch) {
            outPreset.modMatrix[slotIdx++] = { modSrcTimbreSlide, modDestPitchAll, modSrcNone, 50, 0, true };
        } else if (outPreset.steppedParams[spTimbreTarget] == modFilter) {
            outPreset.modMatrix[slotIdx++] = { modSrcTimbreSlide, modDestCutoff, modSrcNone, 50, 0, true };
        } else if (outPreset.steppedParams[spTimbreTarget] == modWaveMod) {
            outPreset.modMatrix[slotIdx++] = { modSrcTimbreSlide, modDestWaveModAll, modSrcNone, 50, 0, true };
        } else if (outPreset.steppedParams[spTimbreTarget] == modVolume) {
            outPreset.modMatrix[slotIdx++] = { modSrcTimbreSlide, modDestAmpLevel, modSrcNone, 50, 0, true };
        }
    }

    return true;
}

std::string PresetManager::serializePresetToString(const PresetData& preset) {
    std::ostringstream ss;
    ss << "# Overviber VST3 Preset\n";
    ss << "presetName = " << preset.presetName << "\n";
    ss << "bank0 = " << preset.oscBank[abxAMain] << "\n";
    ss << "wave0 = " << preset.oscWave[abxAMain] << "\n";
    ss << "bank1 = " << preset.oscBank[abxBMain] << "\n";
    ss << "wave1 = " << preset.oscWave[abxBMain] << "\n";
    ss << "bank2 = " << preset.oscBank[abxACrossover] << "\n";
    ss << "wave2 = " << preset.oscWave[abxACrossover] << "\n";
    ss << "bank3 = " << preset.oscBank[abxBCrossover] << "\n";
    ss << "wave3 = " << preset.oscWave[abxBCrossover] << "\n";

    for (int i = 0; i < cpCount; ++i) {
        if (cpNames[i] != nullptr) {
            int v = scan_potFrom16bits(preset.continuousParams[i]);
            if (cpZeroCentered[i]) {
                v -= (SCAN_POT_MAX_VALUE + 1) / 2;
            }
            ss << cpNames[i] << " = " << v << "\n";
        }
    }

    for (int i = 0; i < spCount; ++i) {
        if (spNames[i] != nullptr) {
            ss << spNames[i] << " = " << (int)preset.steppedParams[i] << "\n";
        }
    }

    for (int i = 0; i < SYNTH_VOICE_COUNT; ++i) {
        ss << "voicePattern" << i << " = " << (int)preset.voicePattern[i] << "\n";
    }

    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        ss << "matrixSlot" << s << "_src = " << (int)preset.modMatrix[s].source << "\n";
        // Destinations are written by name so the list can change without
        // shifting stored routings (the parser accepts names and numbers).
        const uint8_t dest = preset.modMatrix[s].dest;
        ss << "matrixSlot" << s << "_dest = " << (dest < modDestCount ? modDestNames[dest] : "None") << "\n";
        ss << "matrixSlot" << s << "_via = " << (int)preset.modMatrix[s].viaSource << "\n";
        ss << "matrixSlot" << s << "_depth = " << (int)preset.modMatrix[s].depth << "\n";
        ss << "matrixSlot" << s << "_curve = " << (int)preset.modMatrix[s].curve << "\n";
        ss << "matrixSlot" << s << "_en = " << (preset.modMatrix[s].enabled ? 1 : 0) << "\n";
    }

    return ss.str();
}

bool PresetManager::savePresetFile(const std::string& filePath, const PresetData& preset) {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    file << serializePresetToString(preset);
    return true;
}

bool PresetManager::savePreset(int index, const PresetData& preset) {
    if (index >= 0 && index < (int)presetFiles.size()) {
        if (savePresetFile(presetFiles[index].second, preset)) {
            presetNames[index] = preset.presetName;
            return true;
        }
    }
    return false;
}

bool PresetManager::saveNewPreset(const std::string& name, PresetData preset) {
    if (basePath.empty()) return false;
    int maxNum = -1;
    for (const auto& p : presetFiles) {
        if (p.first > maxNum) maxNum = p.first;
    }
    int newNum = maxNum + 1;
    char fname[64];
    std::snprintf(fname, sizeof(fname), "preset_%04d.conf", newNum);
    std::string fullPath = (fs::path(basePath) / fname).string();
    preset.presetName = name;
    preset.presetNumber = (int16_t)newNum;
    if (savePresetFile(fullPath, preset)) {
        scanPresets();
        return true;
    }
    return false;
}

bool PresetManager::deletePreset(int index) {
    if (index >= 0 && index < (int)presetFiles.size()) {
        try {
            fs::remove(presetFiles[index].second);
            scanPresets();
            return true;
        } catch (...) {}
    }
    return false;
}

int PresetManager::getPresetNumber(int index) const {
    if (index >= 0 && index < (int)presetFiles.size()) {
        return presetFiles[index].first;
    }
    return index;
}

const std::string& PresetManager::getPresetFilePath(int index) const {
    static const std::string empty;
    if (index >= 0 && index < (int)presetFiles.size()) {
        return presetFiles[index].second;
    }
    return empty;
}
