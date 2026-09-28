#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>

#define SYNTH_VOICE_COUNT 6
#define SYNTH_MASTER_CLOCK 120000000.0

#define OVERVIBER_VERSION_STRING "v0.8"
#define OVERVIBER_VERSION_NUMBER "0.8.0"
#define OVERCYCLER_VERSION_STRING OVERVIBER_VERSION_STRING
#define OVERCYCLER_VERSION_NUMBER OVERVIBER_VERSION_NUMBER

#define WTOSC_SAMPLE_COUNT 2400
#define WTOSC_CV_SEMITONE 256
#define WTOSC_HIGHEST_NOTE 108
#define WTOSC_SAMPLES_GUARD_BAND 4600

#define FULL_RANGE 65535U
#define HALF_RANGE 32768U
#define HALF_RANGE_L (65536UL * HALF_RANGE)

#define MIDDLE_C_NOTE 60
#define SCAN_POT_MAX_VALUE 999
#define SCAN_POT_DEAD_ZONE 512
#define MAX_FILENAME 256

#define ASSIGNER_NOTE_COUNT 128
#define ASSIGNER_NO_NOTE UINT8_MAX
#define ASSIGNER_EVENT_FLAG_LEGATO 1

#define DACSPI_BUFFER_COUNT 64
#define DACSPI_CV_COUNT 16
#define DACSPI_CHANNEL_COUNT 7
#define DACSPI_OSC_CHANNEL_WAIT_STATES 9
#define DACSPI_CV_CHANNEL_WAIT_STATES 3
#define DACSPI_TIMER_MATCH 24
#define DACSPI_TIME_CONSTANT ((DACSPI_CHANNEL_COUNT-1)*(1+1+DACSPI_OSC_CHANNEL_WAIT_STATES)+(1+1+4+DACSPI_CV_CHANNEL_WAIT_STATES))
#define DACSPI_TICK_RATE ((uint32_t)((DACSPI_TIMER_MATCH+1)*DACSPI_TIME_CONSTANT)) // 1875
#define DACSPI_UPDATE_HZ 4000
#define TICKER_HZ 500

typedef enum {
    cvAVol=0, cvBVol=1, cvCutoff=2, cvResonance=3, cvAPitch=4, cvBPitch=5, cvWaveMod=6, cvAmp=7, cvNoiseVol=8,
    cvCount
} cv_t;

typedef enum {
    modNone=0, modPitch=1, modFilter=2, modVolume=3, modWaveMod=4, modLFO1=5, modLFO2=6
} modulationTarget_t;

typedef enum {
    otNone=0, otA=1, otB=2, otBoth=3
} oscTarget_t;

typedef enum {
    abxNone=-1, abxAMain=0, abxBMain=1, abxACrossover=2, abxBCrossover=3,
    abxCount=4
} abx_t;

typedef enum {
    wmOff=0, wmAliasing=1, wmWidth=2, wmFrequency=3, wmCrossOver=4, wmFolder=5, wmBitCrush=6,
    wmCount=7
} oscWModTarget_t;

typedef enum {
    osmNone=0, osmMaster=1, osmSlave=2
} oscSyncMode_t;

typedef enum {
    sWait=0, sAttack=1, sDecay=2, sSustain=3, sRelease=4, sDone=5
} adsrStage_t;

typedef enum {
    lsPulse=0, lsTri=1, lsRand=2, lsSine=3, lsNoise=4, lsSaw=5, lsRevSaw=6
} lfoShape_t;

typedef enum {
    apLast=0, apLow=1, apHigh=2
} assignerPriority_t;

typedef enum {
    amOff=0, amUp=1, amDown=2, amUpDown=3, amRandom=4, amAssign=5, amChord=6, amConverge=7,
    amDegree=8, amStrum=9,
    amCount=10
} arpMode_t;

typedef enum {
    cpAFreq=0, cpAVol=1, cpABaseWMod=2,
    cpBFreq=3, cpBVol=4, cpBBaseWMod=5, cpDetune=6,
    cpCutoff=7, cpResonance=8, cpFilEnvAmt=9, cpFilKbdAmt=10, cpWModAEnv=11,
    cpFilAtt=12, cpFilDec=13, cpFilSus=14, cpFilRel=15,
    cpAmpAtt=16, cpAmpDec=17, cpAmpSus=18, cpAmpRel=19,
    cpLFOFreq=20, cpLFOAmt=21,
    cpLFOPitchAmt=22, cpLFOWModAmt=23, cpLFOFilAmt=24, cpLFOAmpAmt=25,
    cpLFO2Freq=26, cpLFO2Amt=27,
    cpModDelay=28, cpGlide=29,
    cpAmpVelocity=30, cpFilVelocity=31,
    cpMasterTune=32, cpUnisonDetune=33,
    cpMasterLeft_Legacy=34, cpMasterRight_Legacy=35,
    cpSeqArpClock_Legacy=36, cpNoiseVol=37,
    cpLFO2PitchAmt=38, cpLFO2WModAmt=39, cpLFO2FilAmt=40, cpLFO2AmpAmt=41,
    cpLFOResAmt=42, cpLFO2ResAmt=43,
    // WaveMod Envelope B & Velocity
    cpWModAtt=44, cpWModDec=45, cpWModSus=46, cpWModRel=47,
    cpWModBEnv=48, cpWModVelocity=49,

    // Master Voice Amplitude
    cpAmpLevel=50,

    // Audible Shelves 4-Band Parametric EQ Parameters
    cpShelvesLsFreq=51,
    cpShelvesLsGain=52,
    cpShelvesP1Gain=53,
    cpShelvesP2Freq=54,
    cpShelvesP2Gain=55,
    cpShelvesP2Q=56,
    cpShelvesHsFreq=57,
    cpShelvesHsGain=58,

    // Airwindows ConsoleX Master Summing & Saturation Parameters
    // (preset keys keep their historic names cpMackityInTrim / cpMackityOutPad)
    cpConsoleDrive=59,   // Console Drive (0..999, mapped to 0.7..4.0x drive, default 100 = 1.0x / 0 dB)
    cpConsolePad=60,     // Console Output Pad / master fader (0..999, 0..100% output level, default 999 = 100%)

    // Arpeggiator Performance Parameters
    cpArpGate=61,        // Gate Length (0..999, default 833 = 83.3%, 999 = Legato)
    cpArpSwing=62,       // Swing / Groove (500..750, default 500 = 50% Straight)
    cpArpBpm=63,         // Internal Free BPM (0..999, mapped to 20..300 BPM, default 357 = 120 BPM)

    // Airwindows ConsoleX Air Non-Linearity Parameter
    cpConsoleDiscontinuity=64, // Discontinuity / Air Non-Linearity (0..999, mapped to 70..140 dB, default 500 = 105 dB)

    // Mutable Instruments Elements Modal Synthesizer Parameters
    cpElementsGeometry=65,     // Resonator Geometry / Structure (0..999, 0..100%)
    cpElementsBrightness=66,   // Resonator Brightness / High Frequency Modes (0..999, 0..100%)
    cpElementsDamping=67,      // Resonator Damping / Decay Time (0..999, 0..100%)
    cpElementsPosition=68,     // Resonator Strike Position / Harmonics (0..999, 0..100%)
    cpElementsSpace=69,        // Resonator Stereo Space / Diffuser (0..999, 0..100%)
    cpElementsBow=70,          // Bow Friction Exciter Level (0..999, 0..100%)
    cpElementsBlow=71,         // Blow Air/Noise Exciter Level (0..999, 0..100%)
    cpElementsStrike=72,       // Strike Mallet/Impact Level (0..999, 0..100%)
    cpElementsMallet=73,       // Mallet Hardness / Envelope Contour (0..999, 0..100%)

    // Airwindows Mackity parallel send on the master bus
    cpMackitySend=74,          // Send amount (0..999, default 0 = off)
    cpMackityDrive=75,         // Mackity input trim (0..999, 100 = 0 dB, default 300)
    cpCount=76
} continuousParameter_t;

typedef enum {
    // Oscillator Waves & Shapers
    spABank_Unsaved=0, spAWave_Unsaved=1, spAWModType=2, spAWModEnvEn_Legacy=3,
    spBBank_Unsaved=4, spBWave_Unsaved=5, spBWModType=6, spBWModEnvEn_Legacy=7,

    // LFO 1
    spLFOShape=8, spLFOSpeed=9, spLFOTargets=10,

    // Envelope Speeds
    spFilEnvSlow=11, spAmpEnvSlow=12,

    // Performance Controllers
    spBenderRange=13, spBenderTarget=14,
    spModwheelRange=15, spModwheelTarget=16,

    // Voice Assigner & Tuning
    spUnison=17, spAssignerPriority=18, spChromaticPitch=19,
    spOscSync=20,
    spAXOvrBank_Unsaved=21, spAXOvrWave_Unsaved=22,

    // Envelopes Lin/Loop Modes
    spFilEnvLin=23,
    spLFO2Shape=24, spLFO2Speed=25, spLFO2Targets=26, spVoiceCount=27,
    spPresetType=28, spPresetStyle=29,
    spAmpEnvLin=30, spFilEnvLoop=31, spAmpEnvLoop=32,
    spWModEnvSlow=33, spWModEnvLin=34, spWModEnvLoop=35,
    spPressureRange=36, spPressureTarget=37,
    spBXOvrBank_Unsaved=38, spBXOvrWave_Unsaved=39,
    spLFOTrig=40, spLFO2Trig=41,

    // Filter Model & Mode Selection
    spFilterModel=42, // 0 = SSI2144 Ladder, 1 = SEM, 2 = Shelves EQ / SVF, 3 = SST
    spFilterMode=43,  // Sub-mode for active filter model

    // Arpeggiator Stepped Parameters
    spArpOctaves=44,  // Multi-Octave Range (0 = 1 Octave, 1 = 2 Octaves, 2 = 3 Octaves, 3 = 4 Octaves)
    spArpRate=45,     // Clock Division (0 = 1/4, 1 = 1/8, 2 = 1/8T, 3 = 1/16, 4 = 1/16T, 5 = 1/32)
    spArpHold=46,     // 0 = Off, 1 = Latch / Hold Keys
    spArpMode=47,     // 0 = Off, 1 = Up, 2 = Down, 3 = Up/Down, 4 = Random, 5 = As Played, 6 = Chord, 7 = Converge, 8 = Chord Degree, 9 = Poly Strum
    spArpSync=48,     // 0 = Free / Internal BPM, 1 = Host Sync (DAW)

    // Expressive MIDI & MPE Performance Parameters
    spTimbreTarget=49,       // Timbre / Slide (CC 74 / Y-Axis) Target Destination
    spMPEMode=50,            // 0 = Off (Standard MIDI), 1 = MPE Lower (Ch 2-7), 2 = MPE Full (Ch 2-15)
    spMPEPitchBendRange=51,  // 0 = +/-2 st, 1 = +/-12 st, 2 = +/-24 st (MPE standard), 3 = +/-48 st, 4 = +/-96 st
    spReleaseVelocityAmt=52, // Note-Off Velocity (Lift) Sensitivity: 0 = Off, 1 = Low, 2 = Mid, 3 = High

    // Multitimbral & AFX Mode (Aphex Twin Sound-per-Key)
    spEngineMode=53,         // 0 = Multi-Channel, 1 = AFX Mode (Sound per Key)
    spAFXSelectedSlot=54,    // 0 .. 15 (Active sound slot edited in UI)

    // Oscillator Engine & Mutable Instruments Elements Models
    spOscEngine=55,          // 0 = Dual Wavetable, 1 = Elements Modal, 2 = Hybrid
    spElementsModel=56,      // 0 = Modal Resonator, 1 = Non-linear String, 2 = Chords, 3 = Ominous Voice

    // Mackity send return pad: 0 = off (return at -6 dB), 1 = -6 dB pad (return at -12 dB)
    spMackityReturnPad=57,
    // SEM filter variant: 0 = OB-Xd 12 dB, 1 = Oberheim (Pirkle), 2 = Vult SVF,
    // 3 = Cytomic SVF, 4 = Liquid (Ripples)
    spSemModel=58,
    spCount=59
} steppedParameter_t;

typedef enum {
    oeWavetable = 0, // Dual Wavetable (classic Overcycler)
    oeElements  = 1, // Mutable Instruments Elements Modal Synthesis
    oeHybrid    = 2, // Hybrid Wavetable + Elements Modal
    oeCount     = 3
} oscEngine_t;

typedef enum {
    emModal = 0,         // 64-band SVF Modal Resonator
    emString = 1,        // Non-linear String (Karplus-Strong with dispersion)
    emChords = 2,        // Modal Chords Resonator
    emOminousVoice = 3,  // Granular vocal formant choir
    emElementsCount = 4
} elementsModel_t;

typedef enum {
    emMultiChannel = 0,   // MIDI channel N plays part N (channel 1 = the edited preset)
    emAFX = 1,            // AFX Sound Kit Mode (Sound per Key)
    emCount = 2
} engineMode_t;

#define MOD_MATRIX_SLOT_COUNT 8

typedef enum {
    modSrcNone = 0,
    modSrcModWheel = 1,          // MIDI CC 1 (0..65535)
    modSrcPitchBend = 2,         // Pitch Wheel (-32768..32767 bipolar)
    modSrcAftertouch = 3,        // Channel Pressure & Polyphonic Aftertouch (0..65535, per-voice)
    modSrcTimbreSlide = 4,       // CC 74 / MPE Slide / Y-Axis (0..65535, per-voice)
    modSrcVelocity = 5,          // Note-On Velocity (0..65535, per-voice)
    modSrcReleaseVelocity = 6,   // Note-Off Velocity / Lift (0..65535, per-voice)
    modSrcKeyTrack = 7,          // Key Pitch / Note Number (-32768..32767 relative to Middle C)
    modSrcBreath = 8,            // MIDI CC 2 Breath Controller (0..65535)
    modSrcExpression = 9,        // MIDI CC 11 Expression (0..65535)
    modSrcFilterEnv = 10,        // Filter Envelope Output (0..65535, per-voice)
    modSrcAmpEnv = 11,           // Amp Envelope Output (0..65535, per-voice)
    modSrcWaveModEnv = 12,       // WaveMod Envelope Output (0..65535, per-voice)
    modSrcLFO1 = 13,             // LFO 1 Bipolar (-32768..32767)
    modSrcLFO1_Uni = 14,         // LFO 1 Unipolar (0..65535)
    modSrcLFO2 = 15,             // LFO 2 Bipolar (-32768..32767)
    modSrcLFO2_Uni = 16,         // LFO 2 Unipolar (0..65535)
    modSrcConstant = 17,         // Fixed +1.0 full scale for constant manual bias
    modSrcCount = 18
} modSource_t;

typedef enum {
    modDestNone = 0,
    modDestPitchAll = 1,         // Master Pitch (Osc A + Osc B)
    modDestPitchOscA = 2,        // Osc A Pitch
    modDestPitchOscB = 3,        // Osc B Pitch
    modDestDetune = 4,           // Osc A/B Detune Spread
    modDestWaveModAll = 5,       // WaveMod Depth (Osc A + B)
    modDestWaveModOscA = 6,      // Osc A WaveMod Depth
    modDestWaveModOscB = 7,      // Osc B WaveMod Depth
    modDestVolOscA = 8,          // Osc A Level
    modDestVolOscB = 9,          // Osc B Level
    modDestNoiseVol = 10,        // Noise Generator Level
    modDestCutoff = 11,          // VCF Cutoff Frequency
    modDestResonance = 12,       // VCF Resonance
    modDestAmpLevel = 13,         // Voice VCA Level / Amplitude
    modDestElementsGeometry = 14,   // Elements Resonator Geometry
    modDestElementsBrightness = 15, // Elements Resonator Brightness
    modDestElementsDamping = 16,    // Elements Resonator Damping
    modDestElementsPosition = 17,   // Elements Resonator Strike Position
    modDestElementsSpace = 18,      // Elements Stereo Space
    modDestElementsBow = 19,        // Elements Bow Exciter Level
    modDestElementsBlow = 20,       // Elements Blow Exciter Level
    modDestElementsStrike = 21,     // Elements Strike Impact Level
    modDestCount = 22
} modDest_t;


struct ModMatrixSlot {
    uint8_t source = modSrcNone;     // modSource_t
    uint8_t dest = modDestNone;       // modDest_t
    uint8_t viaSource = modSrcNone;  // modSource_t (Secondary Scaler / Modulator)
    int16_t depth = 0;               // -100 .. +100 (% bipolar depth)
    uint8_t curve = 0;               // 0 = Linear, 1 = Exponential, 2 = S-Curve
    bool enabled = true;             // Slot active / bypassed
};

typedef enum {
    fmSSI2144 = 0, // Sound Semiconductor SSI2144 24dB 4-Pole Ladder
    fmSem     = 1, // SEM-style 2-pole SVF, variant by spSemModel (see SemFilter.h)
    fmEQ      = 2, // Mutable Instruments Shelves 4-Band Parametric EQ & SVF
    fmSST     = 3, // Surge Synthesizer Team (SST) Vintage Moog Ladder (24dB, 18dB, 12dB, 6dB)
    fmCount   = 4
} filterModel_t;

// Portability helpers for saturation
static inline uint32_t clamp_usat(int32_t val, uint32_t bits) {
    int32_t max_val = (1 << bits) - 1;
    if (val > max_val) return (uint32_t)max_val;
    if (val < 0) return 0;
    return (uint32_t)val;
}

static inline int32_t clamp_ssat(int32_t val, uint32_t bits) {
    int32_t max_val = (1 << (bits - 1)) - 1;
    int32_t min_val = -(1 << (bits - 1));
    if (val > max_val) return max_val;
    if (val < min_val) return min_val;
    return val;
}

#define __USAT(val, bits) clamp_usat(val, bits)
#define __SSAT(val, bits) clamp_ssat(val, bits)

static inline uint16_t satAddU16U16(uint16_t a, uint16_t b) {
    return (b > UINT16_MAX - a) ? UINT16_MAX : (uint16_t)(b + a);
}

static inline uint16_t satAddU16S32(uint16_t a, int32_t b) {
    int32_t r = a + b;
    return (uint16_t)__USAT(r, 16);
}

static inline uint16_t satAddU16S16(uint16_t a, int16_t b) {
    int32_t r = a + b;
    return (uint16_t)__USAT(r, 16);
}

static inline uint16_t lerp(uint16_t a, uint16_t b, uint8_t x) {
    return (uint16_t)(a + (((uint32_t)x * (b - a)) >> 8));
}

static inline uint16_t lerp16(uint16_t a, uint16_t b, uint16_t x) {
    return (uint16_t)(a + (((uint32_t)x * (b - a)) >> 16));
}

// 4-point, 3rd-order Hermite / Catmull-Rom spline interpolation
static inline uint16_t herp(int32_t alpha, int32_t cur, int32_t prev, int32_t prev2, int32_t prev3, int8_t frac_shift) {
    int32_t v = prev2 - prev;
    int32_t p0 = ((prev3 - cur - v) >> 1) - v;
    int32_t p1 = cur + v * 2 - ((prev3 + prev) >> 1);
    int32_t p2 = (prev2 - cur) >> 1;

    int32_t total = ((p0 * alpha) >> frac_shift) + p1;
    total = ((total * alpha) >> frac_shift) + p2;
    total = ((total * alpha) >> frac_shift) + prev;

    return (uint16_t)__USAT(total, 16);
}

static inline uint16_t computeShape(uint32_t phase, const uint16_t lookup[], int8_t interpolate) {
    if (interpolate == 2) {
        uint8_t p1i = (uint8_t)(phase >> 16);
        uint8_t ci = (p1i < UINT8_MAX) ? (p1i + 1) : p1i;
        uint8_t p2i = (p1i > 0) ? (p1i - 1) : p1i;
        uint8_t p3i = (p2i > 0) ? (p2i - 1) : p2i;

        int32_t x = 4095 - ((phase >> 4) & 4095);
        int32_t c = lookup[ci];
        int32_t p1 = lookup[p1i];
        int32_t p2 = lookup[p2i];
        int32_t p3 = lookup[p3i];

        return herp(x, c, p1, p2, p3, 12);
    } else if (interpolate == 1) {
        uint8_t x = (uint8_t)(phase >> 8);
        uint8_t ai = (uint8_t)(phase >> 16);
        uint8_t bi = (ai < UINT8_MAX) ? (ai + 1) : ai;

        uint16_t a = lookup[ai];
        uint16_t b = lookup[bi];

        return lerp(a, b, x);
    } else {
        return lookup[(phase >> 16) & 0xFF];
    }
}

static inline uint16_t scaleU16U16(uint16_t a, uint16_t b) {
    return (uint16_t)(((uint32_t)a * b) >> 16);
}

static inline int16_t scaleU16S16(uint16_t a, int16_t b) {
    return (int16_t)(((int32_t)a * b) >> 16);
}

static inline uint32_t lfsr(uint32_t v, uint8_t taps) {
    while (taps--) {
        uint8_t b24 = (uint8_t)(v >> 24);
        v <<= 1;
        v |= ((b24 >> 7) ^ (b24 >> 5) ^ (b24 >> 1) ^ b24) & 1;
    }
    return v;
}

static inline uint16_t exponentialCourse(uint16_t v, float ratio, float range) {
    return (uint16_t)(std::exp(-(float)v / ratio) * range);
}

static inline int scan_potTo16bits(int x) {
    return (int)std::round((((float)x) * UINT16_MAX) / SCAN_POT_MAX_VALUE);
}

static inline int scan_potFrom16bits(int x) {
    return (int)std::round((((float)x) * SCAN_POT_MAX_VALUE) / UINT16_MAX);
}

static inline void resampleWave(const uint16_t* src, uint16_t* dst, uint16_t src_samples, uint16_t dst_samples) {
    if (src_samples == dst_samples) {
        std::memcpy(dst, src, src_samples * sizeof(uint16_t));
    } else if (src_samples > 0 && dst_samples > 0) {
        for (uint16_t ds = 0; ds < dst_samples; ++ds) {
            float pos = (float)ds * ((float)src_samples / (float)dst_samples);
            int i = (int)std::floor(pos);
            float frac = pos - (float)i;

            int i0 = (i - 1 + src_samples) % src_samples;
            int i1 = i % src_samples;
            int i2 = (i + 1) % src_samples;
            int i3 = (i + 2) % src_samples;

            float p0 = (float)src[i0];
            float p1 = (float)src[i1];
            float p2 = (float)src[i2];
            float p3 = (float)src[i3];

            float c0 = p1;
            float c1 = 0.5f * (p2 - p0);
            float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);

            float val = ((c3 * frac + c2) * frac + c1) * frac + c0;
            dst[ds] = (uint16_t)std::clamp((int)std::round(val), 0, (int)UINT16_MAX);
        }
    }
}

