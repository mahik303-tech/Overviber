#include "PresetData.h"
#include "../dsp/adsr.h"
#include <cstring>

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
    continuousParams[cpAVol] = scan_potTo16bits(999);           // 100% volume
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
    continuousParams[cpLFOFreq] = scan_potTo16bits(5 * 60);
    continuousParams[cpLFO2Freq] = scan_potTo16bits(5 * 60);
    continuousParams[cpShelvesLsFreq] = scan_potTo16bits(233);   // 100 Hz (Hz = 20 x 1000^(pot/999))
    continuousParams[cpShelvesLsGain] = HALF_RANGE;              // 0 dB
    continuousParams[cpShelvesP1Gain] = HALF_RANGE;              // 0 dB
    continuousParams[cpShelvesP2Freq] = scan_potTo16bits(698);   // 2.5 kHz
    continuousParams[cpShelvesP2Gain] = HALF_RANGE;              // 0 dB
    continuousParams[cpShelvesP2Q]    = scan_potTo16bits(158);   // Q 1.0 (0.5 x 80^(pot/999))
    continuousParams[cpShelvesHsFreq] = scan_potTo16bits(867);   // 8 kHz
    continuousParams[cpShelvesHsGain] = HALF_RANGE;              // 0 dB
    continuousParams[cpConsoleDrive]  = scan_potTo16bits(100);   // Unity drive
    continuousParams[cpConsolePad]    = scan_potTo16bits(999);   // 0 dB / full level
    continuousParams[cpArpGate]       = scan_potTo16bits(833);   // 83.3% standard gate length
    continuousParams[cpArpSwing]      = scan_potTo16bits(500);   // 50% straight swing
    continuousParams[cpArpBpm]        = scan_potTo16bits(357);   // 120 BPM default
    continuousParams[cpConsoleDiscontinuity] = scan_potTo16bits(17);  // the former default's threshold, see ConsoleXProcessor
    continuousParams[cpElementsGeometry]   = scan_potTo16bits(250);   // 25% (plate/string)
    continuousParams[cpElementsBrightness] = scan_potTo16bits(500);   // 50%
    continuousParams[cpElementsDamping]    = scan_potTo16bits(300);   // 30%
    continuousParams[cpElementsPosition]   = scan_potTo16bits(400);   // 40%
    continuousParams[cpElementsSpace]      = scan_potTo16bits(200);   // 20%
    continuousParams[cpElementsBow]        = 0;                       // 0%
    continuousParams[cpElementsBlow]       = 0;                       // 0%
    continuousParams[cpElementsStrike]     = scan_potTo16bits(800);   // 80%
    continuousParams[cpElementsContour]    = scan_potTo16bits(500);   // 50%
    continuousParams[cpElementsFlow]       = scan_potTo16bits(500);   // 50%
    continuousParams[cpElementsMallet]     = scan_potTo16bits(500);   // 50%
    continuousParams[cpElementsBowTimbre]  = scan_potTo16bits(500);   // 50%
    continuousParams[cpElementsBlowTimbre] = scan_potTo16bits(500);   // 50%
    continuousParams[cpElementsStrikeTimbre] = scan_potTo16bits(500); // 50%
    continuousParams[cpMackitySend]        = 0;                       // Enrichment effect, off by default
    continuousParams[cpMackityDrive]       = scan_potTo16bits(300);   // Moderate warmth

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
