#pragma once

#include "../dsp/OvercyclerTypes.h"
#include "../data/ParamLabels.h"
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>

namespace ClassicUI {

enum class PageId {
    Osc = 1,
    WMod = 2,
    Fil = 3,
    Amp = 4,
    Lfo1 = 5,
    Lfo2 = 6,
    Arp = 7,
    Seq = 8,
    Misc = 9,
    Presets = 0,
    Help = 10
};

enum class ParamKind {
    None,
    Continuous,
    Stepped,
    Custom
};

enum class CustomActionId {
    None = 0,
    // Bank & Waveform Potentiometers
    OscABank,
    OscAWave,
    OscBBank,
    OscBWave,
    // Envelope Type Buttons (FExp, SExp, FLin, SLin)
    WModEnvType,
    FilEnvType,
    AmpEnvType,
    // Crossover swap buttons
    AXoSwap,
    BXoSwap,
    // Performance & Transpose
    TransposeMode,
    TransposeValue,
    // System Actions
    LoadBasic,
    MidiPanic,
    ShowHelp,
    TuneFilters,
    // Misc Settings
    MidiChannel,
    SyncMode,
    LcdContrast,
    UsbMode,
    // Presets
    PresetLoad,
    PresetSave,
    PresetPrev,
    PresetNext,
    // Numeric Entry
    NumericValueEntry,
    NumericPresetEntry,
    // Arp / Seq Actions
    ArpMode,
    ArpHold,
    SeqPlayA,
    SeqPlayB,
    SeqRecord
};

struct ParamDef {
    ParamKind kind{ ParamKind::None };
    continuousParameter_t cp{ cpCount };
    steppedParameter_t sp{ spCount };
    CustomActionId customId{ CustomActionId::None };

    const char* shortName{ "    " };
    const char* longName{ "" };

    float minVal{ 0.0f };
    float maxVal{ 999.0f };
    float step{ 1.0f };
    bool zeroCentered{ false };

    std::vector<std::string> options;
};

struct PageDef {
    PageId id;
    const char* name;
    std::array<ParamDef, 10> pots;     // 0..4 top row, 5..9 bottom row
    std::array<ParamDef, 4>  buttons;  // A, B, C, D
};

class SchemaRegistry {
public:
    static const PageDef& getPage(PageId page) {
        static const auto pages = initPages();
        for (const auto& p : pages) {
            if (p.id == page) return p;
        }
        return pages[0]; // fallback
    }

private:
    static std::vector<PageDef> initPages() {
        std::vector<PageDef> list;

        // =====================================================================
        // Page 1: Oscillators (OSC)
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Osc;
            p.name = "OSCILLATORS";

            // Pots: Row 1
            p.pots[0] = { ParamKind::Custom, cpCount, spCount, CustomActionId::OscABank, "ABnk", "Osc A Bank", 0, 99, 1 };
            p.pots[1] = { ParamKind::Custom, cpCount, spCount, CustomActionId::OscAWave, "AWav", "Osc A Waveform", 0, 999, 1 };
            p.pots[2] = { ParamKind::Continuous, cpAFreq, spCount, CustomActionId::None, "AFrq", "Osc A Frequency", 0, 999, 1 };
            p.pots[3] = { ParamKind::Continuous, cpNoiseVol, spCount, CustomActionId::None, "NVol", "Noise Volume", 0, 999, 1 };
            p.pots[4] = { ParamKind::Continuous, cpAVol, spCount, CustomActionId::None, "AVol", "Osc A Volume", 0, 999, 1 };

            // Pots: Row 2
            p.pots[5] = { ParamKind::Custom, cpCount, spCount, CustomActionId::OscBBank, "BBnk", "Osc B Bank", 0, 99, 1 };
            p.pots[6] = { ParamKind::Custom, cpCount, spCount, CustomActionId::OscBWave, "BWav", "Osc B Waveform", 0, 999, 1 };
            p.pots[7] = { ParamKind::Continuous, cpBFreq, spCount, CustomActionId::None, "BFrq", "Osc B Frequency", 0, 999, 1 };
            p.pots[8] = { ParamKind::Continuous, cpDetune, spCount, CustomActionId::None, "Detn", "Osc A/B Detune", -499, 499, 1, true };
            p.pots[9] = { ParamKind::Continuous, cpBVol, spCount, CustomActionId::None, "BVol", "Osc B Volume", 0, 999, 1 };

            // Buttons: A, B, C, D
            p.buttons[0] = { ParamKind::Custom, cpCount, spCount, CustomActionId::AXoSwap, "AXoS", "Swap A Bank/Wave with Crossover" };
            p.buttons[1] = { ParamKind::Custom, cpCount, spCount, CustomActionId::BXoSwap, "BXoS", "Swap B Bank/Wave with Crossover" };
            p.buttons[2] = { ParamKind::Stepped, cpCount, spChromaticPitch, CustomActionId::None, "FrqM", "Frequency Pitch Mode", 0, 2, 1, false, { "Free", "Semi", "Oct " } };
            p.buttons[3] = { ParamKind::Stepped, cpCount, spOscSync, CustomActionId::None, "Sync", "Osc A to B Sync", 0, 1, 1, false, { "Off ", "On  " } };

            list.push_back(p);
        }

        // =====================================================================
        // Page 2: WaveMod (WMOD)
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::WMod;
            p.name = "WAVEMOD";

            p.pots[0] = { ParamKind::Continuous, cpABaseWMod, spCount, CustomActionId::None, "AWmo", "Osc A WaveMod Amount", -499, 499, 1, true };
            p.pots[1] = { ParamKind::Continuous, cpBBaseWMod, spCount, CustomActionId::None, "BWmo", "Osc B WaveMod Amount", -499, 499, 1, true };
            p.pots[2] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[3] = { ParamKind::Continuous, cpWModAEnv, spCount, CustomActionId::None, "AWEA", "Osc A WaveMod Env Amount", -499, 499, 1, true };
            p.pots[4] = { ParamKind::Continuous, cpWModBEnv, spCount, CustomActionId::None, "BWEA", "Osc B WaveMod Env Amount", -499, 499, 1, true };

            p.pots[5] = { ParamKind::Continuous, cpWModAtt, spCount, CustomActionId::None, "WAtk", "WaveMod Envelope Attack", 0, 999, 1 };
            p.pots[6] = { ParamKind::Continuous, cpWModDec, spCount, CustomActionId::None, "WDec", "WaveMod Envelope Decay", 0, 999, 1 };
            p.pots[7] = { ParamKind::Continuous, cpWModSus, spCount, CustomActionId::None, "WSus", "WaveMod Envelope Sustain", 0, 999, 1 };
            p.pots[8] = { ParamKind::Continuous, cpWModRel, spCount, CustomActionId::None, "WRel", "WaveMod Envelope Release", 0, 999, 1 };
            p.pots[9] = { ParamKind::Continuous, cpWModVelocity, spCount, CustomActionId::None, "WVel", "WaveMod Velocity Amount", 0, 999, 1 };

            p.buttons[0] = { ParamKind::Stepped, cpCount, spAWModType, CustomActionId::None, "AWmT", "Osc A WaveMod Type", 0, 6, 1, false, { "None", "Grit", "Wdth", "Freq", "XOvr", "Fold", "BitC" } };
            p.buttons[1] = { ParamKind::Stepped, cpCount, spBWModType, CustomActionId::None, "BWmT", "Osc B WaveMod Type", 0, 6, 1, false, { "None", "Grit", "Wdth", "Freq", "XOvr", "Fold", "BitC" } };
            p.buttons[2] = { ParamKind::Custom, cpCount, spCount, CustomActionId::WModEnvType, "WEnT", "WaveMod Envelope Type", 0, 3, 1, false, { "FExp", "SExp", "FLin", "SLin" } };
            p.buttons[3] = { ParamKind::Stepped, cpCount, spWModEnvLoop, CustomActionId::None, "WEnL", "WaveMod Envelope Loop", 0, 1, 1, false, { "Norm", "Loop" } };

            list.push_back(p);
        }

        // =====================================================================
        // Page 3: Filter (FIL)
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Fil;
            p.name = "FILTER";

            p.pots[0] = { ParamKind::Continuous, cpCutoff, spCount, CustomActionId::None, "FCut", "Filter Cutoff Frequency", 0, 999, 1 };
            p.pots[1] = { ParamKind::Continuous, cpResonance, spCount, CustomActionId::None, "FRes", "Filter Resonance", 0, 999, 1 };
            p.pots[2] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[3] = { ParamKind::Continuous, cpFilKbdAmt, spCount, CustomActionId::None, "FKbd", "Filter Keyboard Tracking", 0, 999, 1 };
            p.pots[4] = { ParamKind::Continuous, cpFilEnvAmt, spCount, CustomActionId::None, "FEnv", "Filter Envelope Amount", -499, 499, 1, true };

            p.pots[5] = { ParamKind::Continuous, cpFilAtt, spCount, CustomActionId::None, "FAtk", "Filter Envelope Attack", 0, 999, 1 };
            p.pots[6] = { ParamKind::Continuous, cpFilDec, spCount, CustomActionId::None, "FDec", "Filter Envelope Decay", 0, 999, 1 };
            p.pots[7] = { ParamKind::Continuous, cpFilSus, spCount, CustomActionId::None, "FSus", "Filter Envelope Sustain", 0, 999, 1 };
            p.pots[8] = { ParamKind::Continuous, cpFilRel, spCount, CustomActionId::None, "FRel", "Filter Envelope Release", 0, 999, 1 };
            p.pots[9] = { ParamKind::Continuous, cpFilVelocity, spCount, CustomActionId::None, "FVel", "Filter Velocity Amount", 0, 999, 1 };

            p.buttons[0] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.buttons[1] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.buttons[2] = { ParamKind::Custom, cpCount, spCount, CustomActionId::FilEnvType, "FEnT", "Filter Envelope Type", 0, 3, 1, false, { "FExp", "SExp", "FLin", "SLin" } };
            p.buttons[3] = { ParamKind::Stepped, cpCount, spFilEnvLoop, CustomActionId::None, "FEnL", "Filter Envelope Loop", 0, 1, 1, false, { "Norm", "Loop" } };

            list.push_back(p);
        }

        // =====================================================================
        // Page 4: Amplifier (AMP)
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Amp;
            p.name = "AMPLIFIER";

            p.pots[0] = { ParamKind::Continuous, cpAmpLevel, spCount, CustomActionId::None, "ALvl", "Amplifier Master Level", 0, 999, 1 };
            p.pots[1] = { ParamKind::Continuous, cpGlide, spCount, CustomActionId::None, "Glid", "Glide / Portamento Slew", 0, 999, 1 };
            p.pots[2] = { ParamKind::Continuous, cpUnisonDetune, spCount, CustomActionId::None, "MDet", "Master Unison Detune", 0, 999, 1 };
            p.pots[3] = { ParamKind::Continuous, cpMasterTune, spCount, CustomActionId::None, "MTun", "Master Synthesizer Tune", -499, 499, 1, true };
            p.pots[4] = { ParamKind::Stepped, cpCount, spVoiceCount, CustomActionId::None, "VCnt", "Voice Count (1-6)", 1, 6, 1, false, { "   1", "   2", "   3", "   4", "   5", "   6" } };

            p.pots[5] = { ParamKind::Continuous, cpAmpAtt, spCount, CustomActionId::None, "AAtk", "Amplifier Envelope Attack", 0, 999, 1 };
            p.pots[6] = { ParamKind::Continuous, cpAmpDec, spCount, CustomActionId::None, "ADec", "Amplifier Envelope Decay", 0, 999, 1 };
            p.pots[7] = { ParamKind::Continuous, cpAmpSus, spCount, CustomActionId::None, "ASus", "Amplifier Envelope Sustain", 0, 999, 1 };
            p.pots[8] = { ParamKind::Continuous, cpAmpRel, spCount, CustomActionId::None, "ARel", "Amplifier Envelope Release", 0, 999, 1 };
            p.pots[9] = { ParamKind::Continuous, cpAmpVelocity, spCount, CustomActionId::None, "AVel", "Amplifier Velocity Amount", 0, 999, 1 };

            p.buttons[0] = { ParamKind::Stepped, cpCount, spUnison, CustomActionId::None, "Unis", "Unison Poly Mode", 0, 1, 1, false, { "Off ", "On  " } };
            p.buttons[1] = { ParamKind::Stepped, cpCount, spAssignerPriority, CustomActionId::None, "Prio", "Voice Assigner Priority", 0, 2, 1, false, { "Last", "Low ", "High" } };
            p.buttons[2] = { ParamKind::Custom, cpCount, spCount, CustomActionId::AmpEnvType, "AEnT", "Amplifier Envelope Type", 0, 3, 1, false, { "FExp", "SExp", "FLin", "SLin" } };
            p.buttons[3] = { ParamKind::Stepped, cpCount, spAmpEnvLoop, CustomActionId::None, "AEnL", "Amplifier Envelope Loop", 0, 1, 1, false, { "Norm", "Loop" } };

            list.push_back(p);
        }

        // =====================================================================
        // Page 5: LFO 1
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Lfo1;
            p.name = "LFO 1";

            p.pots[0] = { ParamKind::Continuous, cpLFOFreq, spCount, CustomActionId::None, "1Spd", "LFO 1 Speed (BPM)", 0, 999, 1 };
            p.pots[1] = { ParamKind::Continuous, cpLFOAmt, spCount, CustomActionId::None, "1Amt", "LFO 1 Base Amount", 0, 999, 1 };
            p.pots[2] = { ParamKind::Stepped, cpCount, spLFOShape, CustomActionId::None, "1Wav", "LFO 1 Waveform Shape", 0, 6, 1, false, paramlabels::lcdOptions(paramlabels::kLfoShapes) };
            p.pots[3] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[4] = { ParamKind::Continuous, cpModDelay, spCount, CustomActionId::None, "MDly", "Modulation Delay Time", 0, 999, 1 };

            p.pots[5] = { ParamKind::Continuous, cpLFOPitchAmt, spCount, CustomActionId::None, "1Pit", "LFO 1 Pitch Mod Amount", 0, 999, 1 };
            p.pots[6] = { ParamKind::Continuous, cpLFOWModAmt, spCount, CustomActionId::None, "1Wmo", "LFO 1 WaveMod Mod Amount", 0, 999, 1 };
            p.pots[7] = { ParamKind::Continuous, cpLFOFilAmt, spCount, CustomActionId::None, "1Fil", "LFO 1 Filter Mod Amount", 0, 999, 1 };
            p.pots[8] = { ParamKind::Continuous, cpLFOResAmt, spCount, CustomActionId::None, "1Res", "LFO 1 Resonance Mod Amount", 0, 999, 1 };
            p.pots[9] = { ParamKind::Continuous, cpLFOAmpAmt, spCount, CustomActionId::None, "1Amp", "LFO 1 Amp Mod Amount", 0, 999, 1 };

            p.buttons[0] = { ParamKind::Stepped, cpCount, spLFOSpeed, CustomActionId::None, "1Spd", "LFO 1 Speed Multiplier", 0, 3, 1, false, { "  x1", "  x2", "  x4", "  x8" } };
            p.buttons[1] = { ParamKind::Stepped, cpCount, spLFOTargets, CustomActionId::None, "1Tgt", "LFO 1 Oscillator Target", 0, 3, 1, false, { "None", "OscA", "OscB", "Both" } };
            p.buttons[2] = { ParamKind::Stepped, cpCount, spLFOTrig, CustomActionId::None, "1Trg", "LFO 1 Keyboard Trigger", 0, 6, 1, false, { "Free", "Trig", "HPer", "1Per", "2Per", "4Per", "8Per" } };
            p.buttons[3] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };

            list.push_back(p);
        }

        // =====================================================================
        // Page 6: LFO 2
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Lfo2;
            p.name = "LFO 2";

            p.pots[0] = { ParamKind::Continuous, cpLFO2Freq, spCount, CustomActionId::None, "2Spd", "LFO 2 Speed (BPM)", 0, 999, 1 };
            p.pots[1] = { ParamKind::Continuous, cpLFO2Amt, spCount, CustomActionId::None, "2Amt", "LFO 2 Base Amount", 0, 999, 1 };
            p.pots[2] = { ParamKind::Stepped, cpCount, spLFO2Shape, CustomActionId::None, "2Wav", "LFO 2 Waveform Shape", 0, 6, 1, false, paramlabels::lcdOptions(paramlabels::kLfoShapes) };
            p.pots[3] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[4] = { ParamKind::Continuous, cpModDelay, spCount, CustomActionId::None, "MDly", "Modulation Delay Time", 0, 999, 1 };

            p.pots[5] = { ParamKind::Continuous, cpLFO2PitchAmt, spCount, CustomActionId::None, "2Pit", "LFO 2 Pitch Mod Amount", 0, 999, 1 };
            p.pots[6] = { ParamKind::Continuous, cpLFO2WModAmt, spCount, CustomActionId::None, "2Wmo", "LFO 2 WaveMod Mod Amount", 0, 999, 1 };
            p.pots[7] = { ParamKind::Continuous, cpLFO2FilAmt, spCount, CustomActionId::None, "2Fil", "LFO 2 Filter Mod Amount", 0, 999, 1 };
            p.pots[8] = { ParamKind::Continuous, cpLFO2ResAmt, spCount, CustomActionId::None, "2Res", "LFO 2 Resonance Mod Amount", 0, 999, 1 };
            p.pots[9] = { ParamKind::Continuous, cpLFO2AmpAmt, spCount, CustomActionId::None, "2Amp", "LFO 2 Amp Mod Amount", 0, 999, 1 };

            p.buttons[0] = { ParamKind::Stepped, cpCount, spLFO2Speed, CustomActionId::None, "2Spd", "LFO 2 Speed Multiplier", 0, 3, 1, false, { "  x1", "  x2", "  x4", "  x8" } };
            p.buttons[1] = { ParamKind::Stepped, cpCount, spLFO2Targets, CustomActionId::None, "2Tgt", "LFO 2 Oscillator Target", 0, 3, 1, false, { "None", "OscA", "OscB", "Both" } };
            p.buttons[2] = { ParamKind::Stepped, cpCount, spLFO2Trig, CustomActionId::None, "2Trg", "LFO 2 Keyboard Trigger", 0, 6, 1, false, { "Free", "Trig", "HPer", "1Per", "2Per", "4Per", "8Per" } };
            p.buttons[3] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };

            list.push_back(p);
        }

        // =====================================================================
        // Page 7: Arpeggiator (ARP)
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Arp;
            p.name = "ARPEGGIATOR";

            p.pots[0] = { ParamKind::Continuous, cpArpBpm, spCount, CustomActionId::None, "Clk ", "Arp Internal Clock (BPM)", 20, 300, 1 };
            p.pots[1] = { ParamKind::Continuous, cpArpGate, spCount, CustomActionId::None, "Gate", "Arp Note Gate Length", 0, 999, 1 };
            p.pots[2] = { ParamKind::Stepped, cpCount, spArpRate, CustomActionId::None, "Div ", "Arp Clock Division", 0, 5, 1, false, { " 1/4", " 1/8", "1/8T", "1/16", "16T ", "1/32" } };
            p.pots[3] = { ParamKind::Stepped, cpCount, spArpOctaves, CustomActionId::None, "Oct ", "Arp Octave Range", 1, 4, 1, false, { "   1", "   2", "   3", "   4" } };
            p.pots[4] = { ParamKind::Continuous, cpArpSwing, spCount, CustomActionId::None, "Swng", "Arp Groove / Swing", 500, 750, 1 };

            p.pots[5] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[6] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[7] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[8] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[9] = { ParamKind::Custom, cpCount, spCount, CustomActionId::TransposeValue, "Trsp", "Keyboard Transpose Offset", -24, 24, 1 };

            p.buttons[0] = { ParamKind::Custom, cpCount, spCount, CustomActionId::ArpMode, "AMod", "Arpeggiator Pattern Mode", 0, 9, 1, false, paramlabels::lcdOptions(paramlabels::kArpModes) };
            p.buttons[1] = { ParamKind::Custom, cpCount, spCount, CustomActionId::ArpHold, "AHld", "Arpeggiator Pattern Hold", 0, 1, 1, false, { "Off ", "On  " } };
            p.buttons[2] = { ParamKind::Stepped, cpCount, spArpSync, CustomActionId::None, "Sync", "Arp Clock Sync Source", 0, 1, 1, false, { "Int ", "DAW " } };
            p.buttons[3] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };

            list.push_back(p);
        }

        // =====================================================================
        // Page 8: Sequencer (SEQ)
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Seq;
            p.name = "SEQUENCER";

            p.pots[0] = { ParamKind::Continuous, cpArpBpm, spCount, CustomActionId::None, "Clk ", "Sequencer Clock (BPM)", 20, 300, 1 };
            p.pots[1] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[2] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[3] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[4] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };

            p.pots[5] = { ParamKind::Custom, cpCount, spCount, CustomActionId::None, "SBnk", "Sequencer Memory Bank", 1, 20, 1 };
            p.pots[6] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[7] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[8] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.pots[9] = { ParamKind::Custom, cpCount, spCount, CustomActionId::TransposeValue, "Trsp", "Sequencer Transpose", -24, 24, 1 };

            p.buttons[0] = { ParamKind::Custom, cpCount, spCount, CustomActionId::SeqPlayA, "APly", "Sequence A Play/Stop", 0, 3, 1, false, { "Stop", "Wait", "Play", "Rec " } };
            p.buttons[1] = { ParamKind::Custom, cpCount, spCount, CustomActionId::SeqPlayB, "BPly", "Sequence B Play/Stop", 0, 3, 1, false, { "Stop", "Wait", "Play", "Rec " } };
            p.buttons[2] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "----", "" };
            p.buttons[3] = { ParamKind::Custom, cpCount, spCount, CustomActionId::SeqRecord, "SRec", "Sequence Record Track", 0, 2, 1, false, { "Off ", "SeqA", "SeqB" } };

            list.push_back(p);
        }

        // =====================================================================
        // Page 9: Miscellaneous (MISC)
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Misc;
            p.name = "MISCELLANEOUS";

            p.pots[0] = { ParamKind::Stepped, cpCount, spModwheelRange, CustomActionId::None, "MRng", "Mod-Wheel Sensitivity Range", 0, 3, 1, false, { "Min ", "Low ", "High", "Full" } };
            p.pots[1] = { ParamKind::Stepped, cpCount, spBenderRange, CustomActionId::None, "BRng", "Pitch Bender Range", 0, 2, 1, false, { "3rd ", "5th ", "Oct " } };
            p.pots[2] = { ParamKind::Stepped, cpCount, spPressureRange, CustomActionId::None, "PRng", "Channel Pressure Range", 0, 3, 1, false, { "Min ", "Low ", "High", "Full" } };
            p.pots[3] = { ParamKind::Custom, cpCount, spCount, CustomActionId::MidiChannel, "MidC", "MIDI In Channel", 0, 16, 1, false, { "Omni", "Ch 1", "Ch 2", "Ch 3", "Ch 4", "Ch 5", "Ch 6", "Ch 7", "Ch 8", "Ch 9", "Ch10", "Ch11", "Ch12", "Ch13", "Ch14", "Ch15", "Ch16" } };
            p.pots[4] = { ParamKind::Custom, cpCount, spCount, CustomActionId::SyncMode, "Sync", "Master Clock Sync", 0, 2, 1, false, { "Int ", "MIDI", "USB " } };

            p.pots[5] = { ParamKind::Stepped, cpCount, spModwheelTarget, CustomActionId::None, "MTgt", "Mod-Wheel Modulation Target", 0, 1, 1, false, { "LFO1", "LFO2" } };
            p.pots[6] = { ParamKind::Stepped, cpCount, spBenderTarget, CustomActionId::None, "BTgt", "Pitch Bender Target", 0, 4, 1, false, { "None", "Pit ", "Fil ", "Vol ", "XOvr" } };
            p.pots[7] = { ParamKind::Stepped, cpCount, spPressureTarget, CustomActionId::None, "PTgt", "Aftertouch / Pressure Target", 0, 6, 1, false, { "None", "Pit ", "Fil ", "Vol ", "XOvr", "LFO1", "LFO2" } };
            p.pots[8] = { ParamKind::Custom, cpCount, spCount, CustomActionId::LcdContrast, "Ctst", "LCD Screen Contrast", 0, 10, 1 };
            p.pots[9] = { ParamKind::Custom, cpCount, spCount, CustomActionId::UsbMode, "UsbM", "USB Operational Mode", 0, 2, 1, false, { "None", "Disk", "MIDI" } };

            p.buttons[0] = { ParamKind::Custom, cpCount, spCount, CustomActionId::LoadBasic, "LBas", "Load Basic Init Preset" };
            p.buttons[1] = { ParamKind::Custom, cpCount, spCount, CustomActionId::MidiPanic, "Panc", "MIDI Panic All Notes Off" };
            p.buttons[2] = { ParamKind::Custom, cpCount, spCount, CustomActionId::ShowHelp, "Help", "Return to Startup Help Page" };
            p.buttons[3] = { ParamKind::Custom, cpCount, spCount, CustomActionId::TuneFilters, "Tune", "Analog Filter Auto-Calibration" };

            list.push_back(p);
        }

        // =====================================================================
        // Page 0: Presets (PRST)
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Presets;
            p.name = "PRESETS";

            // In PagePresets, the first 8 pots are unused or custom, Pot 8=Type, Pot 9=Styl
            for (int i = 0; i < 8; ++i) {
                p.pots[i] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "    ", "" };
            }
            p.pots[8] = { ParamKind::Stepped, cpCount, spPresetType, CustomActionId::None, "Type", "Instrument Sound Category", 0, 9, 1, false, { "Othr", "Bass", "Pad ", "Strn", "Bras", "Keys", "Lead", "Arpg", "Perc", "FX  " } };
            p.pots[9] = { ParamKind::Stepped, cpCount, spPresetStyle, CustomActionId::None, "Styl", "Instrument Sound Timbre", 0, 7, 1, false, { "Othr", "Neut", "Clen", "Real", "Slky", "Raw ", "Hevy", "Krch" } };

            p.buttons[0] = { ParamKind::Custom, cpCount, spCount, CustomActionId::PresetLoad, "Load", "Load Preset from Number" };
            p.buttons[1] = { ParamKind::Custom, cpCount, spCount, CustomActionId::PresetSave, "Save", "Save Current Preset" };
            p.buttons[2] = { ParamKind::Custom, cpCount, spCount, CustomActionId::PresetPrev, "Prev", "Load Previous Preset" };
            p.buttons[3] = { ParamKind::Custom, cpCount, spCount, CustomActionId::PresetNext, "Next", "Load Next Preset" };

            list.push_back(p);
        }

        // =====================================================================
        // Startup / Help Page
        // =====================================================================
        {
            PageDef p;
            p.id = PageId::Help;
            p.name = "HELP / STARTUP";

            for (int i = 0; i < 10; ++i) p.pots[i] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "    ", "" };
            p.buttons[0] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "    ", "" };
            p.buttons[1] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "    ", "" };
            p.buttons[2] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "    ", "" };
            p.buttons[3] = { ParamKind::None, cpCount, spCount, CustomActionId::None, "    ", "" };

            list.push_back(p);
        }

        return list;
    }
};

} // namespace ClassicUI
