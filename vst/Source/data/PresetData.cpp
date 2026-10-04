#include "PresetData.h"
#include "../dsp/adsr.h"
#include <cstring>

namespace {
// A pot position (0..999) as the 16-bit CV of a continuous parameter.
uint16_t potCV(int pot) { return static_cast<uint16_t>(scan_potTo16bits(pot)); }
}

PresetData::PresetData() {
    setDefaults();
}

void PresetData::setDefaults() {
    presetName = "Init Sound";
    presetNumber = 0;

    for (int i = 0; i < abxCount; ++i) {
        oscBank[i] = "_basic";
        if (i == abxAMain || i == abxBMain) oscWave[i] = "saw.wav";
        else oscWave[i] = "sin.wav";
    }

    std::memset(continuousParams, 0, sizeof(continuousParams));
    std::memset(steppedParams, 0, sizeof(steppedParams));

    continuousParams[cpAFreq] = 0;                              // concert pitch (0 of 64 semitones), as the firmware
    continuousParams[cpAVol] = potCV(999);                      // 100% volume
    continuousParams[cpABaseWMod] = HALF_RANGE;                 // Center
    continuousParams[cpBFreq] = 0;                              // concert pitch
    continuousParams[cpBVol] = 0;                               // Osc B muted by default in clean single-osc Init patch
    continuousParams[cpBBaseWMod] = HALF_RANGE;                 // Center
    continuousParams[cpDetune] = HALF_RANGE;                    // 0 cents
    continuousParams[cpUnisonDetune] = 512;
    continuousParams[cpMasterTune] = HALF_RANGE;                // 0 cents
    continuousParams[cpCutoff] = UINT16_MAX;                    // 100% full open cutoff
    continuousParams[cpResonance] = 0;                          // 0 resonance
    continuousParams[cpFilEnvAmt] = HALF_RANGE;                 // 0 envelope modulation
    continuousParams[cpFilKbdAmt] = 0;                          // 0 key tracking
    continuousParams[cpWModAEnv] = HALF_RANGE;                  // 0 env mod
    continuousParams[cpWModBEnv] = HALF_RANGE;                  // 0 env mod
    continuousParams[cpFilAtt] = 0;
    continuousParams[cpFilDec] = 0;
    continuousParams[cpFilSus] = UINT16_MAX;
    continuousParams[cpFilRel] = adsrCVForMilliseconds(200);         // Smooth ~200ms release
    continuousParams[cpAmpAtt] = 0;                             // Instant attack
    continuousParams[cpAmpDec] = 0;
    continuousParams[cpAmpSus] = UINT16_MAX;                    // Full sustain
    continuousParams[cpAmpRel] = adsrCVForMilliseconds(200);         // Smooth ~200ms release (eliminates note-off clicks)
    continuousParams[cpAmpLevel] = UINT16_MAX;                  // Full master volume
    continuousParams[cpLFOPitchAmt] = 0;
    continuousParams[cpLFOAmt] = 0;
    continuousParams[cpLFOFreq] = potCV(5 * 60);
    continuousParams[cpLFO2Freq] = potCV(5 * 60);
    continuousParams[cpShelvesLsFreq] = potCV(233);              // 100 Hz (Hz = 20 x 1000^(pot/999))
    continuousParams[cpShelvesLsGain] = HALF_RANGE;              // 0 dB
    continuousParams[cpShelvesP1Gain] = HALF_RANGE;              // 0 dB
    continuousParams[cpShelvesP2Freq] = potCV(698);              // 2.5 kHz
    continuousParams[cpShelvesP2Gain] = HALF_RANGE;              // 0 dB
    continuousParams[cpShelvesP2Q]    = potCV(158);              // Q 1.0 (0.5 x 80^(pot/999))
    continuousParams[cpShelvesHsFreq] = potCV(867);              // 8 kHz
    continuousParams[cpShelvesHsGain] = HALF_RANGE;              // 0 dB
    continuousParams[cpConsoleDrive]  = potCV(100);              // Unity drive
    continuousParams[cpConsolePad]    = potCV(999);              // 0 dB / full level
    continuousParams[cpArpGate]       = potCV(833);              // 83.3% standard gate length
    continuousParams[cpArpSwing]      = potCV(500);              // 50% straight swing
    continuousParams[cpArpBpm]        = potCV(357);              // 120 BPM default
    continuousParams[cpConsoleDiscontinuity] = potCV(17);             // the former default's threshold, see ConsoleXProcessor
    continuousParams[cpElementsGeometry]   = potCV(250);              // 25% (plate/string)
    continuousParams[cpElementsBrightness] = potCV(500);              // 50%
    continuousParams[cpElementsDamping]    = potCV(300);              // 30%
    continuousParams[cpElementsPosition]   = potCV(400);              // 40%
    continuousParams[cpElementsSpace]      = potCV(200);              // 20%
    continuousParams[cpElementsBow]        = 0;                       // 0%
    continuousParams[cpElementsBlow]       = 0;                       // 0%
    continuousParams[cpElementsStrike]     = potCV(800);              // 80%
    continuousParams[cpElementsContour]    = potCV(500);              // 50%
    continuousParams[cpElementsFlow]       = potCV(500);              // 50%
    continuousParams[cpElementsMallet]     = potCV(500);              // 50%
    continuousParams[cpElementsBowTimbre]  = potCV(500);              // 50%
    continuousParams[cpElementsBlowTimbre] = potCV(500);              // 50%
    continuousParams[cpElementsStrikeTimbre] = potCV(500);            // 50%
    continuousParams[cpMackitySend]        = 0;                       // Enrichment effect, off by default
    continuousParams[cpMackityDrive]       = potCV(300);              // Moderate warmth

    steppedParams[spFilterModel] = 0;                           // SSI2144
    steppedParams[spFilterMode] = 0;                            // 24dB LP
    steppedParams[spBenderTarget] = modPitch;
    steppedParams[spBenderRange] = 2;                           // 1 Octave (12 semitones)
    steppedParams[spModwheelRange] = 1;
    steppedParams[spChromaticPitch] = 2;
    steppedParams[spAssignerPriority] = apLast;
    steppedParams[spLFOShape] = lsTri;
    steppedParams[spLFOTargets] = otBoth;
    steppedParams[spLFO2Shape] = lsTri;
    steppedParams[spVoiceCount] = 5;                            // 6 voices (0-indexed: 5)
    steppedParams[spUnison] = 0;                                // Polyphonic mode
    steppedParams[spOscSync] = 0;                               // Sync Off
    steppedParams[spAWModType] = 0;                             // WaveMod Off
    steppedParams[spBWModType] = 0;
    steppedParams[spArpOctaves] = 0;                            // 1 Octave
    steppedParams[spArpRate] = 3;                               // 1/16th note division
    steppedParams[spArpHold] = 0;                               // Hold Off
    steppedParams[spArpMode] = 0;                               // Arp Off
    steppedParams[spArpSync] = 1;                               // Host Sync (DAW) by default
    steppedParams[spTimbreTarget] = modWaveMod;                 // Timbre / CC74 modulates WaveMod by default
    steppedParams[spMPEMode] = 0;                               // Standard MIDI by default
    steppedParams[spMPEPitchBendRange] = 2;                     // +/-24 semitones (MPE standard)
    steppedParams[spReleaseVelocityAmt] = 0;                    // Off by default
    steppedParams[spEngineMode] = emMultiChannel;               // 0 = Multi-Channel, 1 = AFX Mode
    steppedParams[spAFXSelectedSlot] = 0;                       // Slot 0 default
    steppedParams[spOscEngine] = 0;                             // 0 = Dual Wavetable, 1 = Elements Modal, 2 = Hybrid
    steppedParams[spElementsModel] = 0;                         // 0 = Modal Resonator
    steppedParams[spMackityReturnPad] = 0;                      // Pad off: Mackity return at -6 dB

    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        modMatrix[s].source = modSrcNone;
        modMatrix[s].dest = modDestNone;
        modMatrix[s].viaSource = modSrcNone;
        modMatrix[s].depth = 0;
        modMatrix[s].curve = 0;
        modMatrix[s].enabled = true;
    }
    // Default factory routing
    modMatrix[0].source = modSrcModWheel;
    modMatrix[0].dest = modDestCutoff;
    modMatrix[0].depth = 50; // +50%
    modMatrix[0].enabled = true;

    modMatrix[1].source = modSrcAftertouch;
    modMatrix[1].dest = modDestWaveModAll;
    modMatrix[1].depth = 50; // +50%
    modMatrix[1].enabled = true;

    voicePattern[0] = 0;
    for (int i = 1; i < SYNTH_VOICE_COUNT; ++i) {
        voicePattern[i] = 255;
    }
}
