#pragma once

#include <shelves.hpp>   // third party: a system include (CMakeLists.txt)
#include <algorithm>
#include <cmath>
#include <cstring>

// ==============================================================================
// Audible Instruments Shelves Filter (Mutable Instruments Shelves Emulation)
// ==============================================================================
//
// Sources:
//   Original Hardware & DSP Design: Émilie Gillet (Mutable Instruments)
//   VCV Rack Port:                  Tyler Coy (Audible Instruments)
//   Repository:                     https://github.com/VCVRack/AudibleInstruments
//   Files:                          shelves.hpp, shelves_aafilter.hpp, sos.hpp
//   License:                        GNU General Public License v3.0 (GPLv3)
//
// Filter Characteristics:
//   Shelves is inspired by classic British console equalizers and modular analog
//   state-variable filter (SVF) designs. It provides both a flexible 4-band parametric
//   channel equalizer and independent direct-tap 2-pole (12 dB/oct) state-variable
//   filter outputs with continuous frequency and resonance control.
//
// Available Output Modes:
//   - Mode4BandEQ (0): Full 4-Band Parametric Equalizer:
//                      * Low Shelf (20 Hz - 2 kHz, +/- 15 dB)
//                      * Parametric Mid 1 (20 Hz - 20 kHz, +/- 15 dB, variable Q)
//                      * Parametric Mid 2 (20 Hz - 20 kHz, +/- 15 dB, variable Q)
//                      * High Shelf (2 kHz - 20 kHz, +/- 15 dB)
//   - ModeSVF_LP  (1): Direct State-Variable Lowpass (12 dB/oct) derived from Mid 1.
//   - ModeSVF_BP  (2): Direct State-Variable Bandpass (12 dB/oct) derived from Mid 1.
//   - ModeSVF_HP  (3): Direct State-Variable Highpass (12 dB/oct) derived from Mid 1.
//
// Signal & CV Scaling:
//   - Input Scaling:  Overcycler VST audio [-1.0 .. +1.0] is scaled by 5.0x to match
//                     the +/- 5V modular Eurorack standard expected by ShelvesEngine.
//   - Output Scaling: Filter output is scaled by 0.22x to preserve studio headroom.
//   - CV Control:     16-bit DAC register values [0 .. 65535] are linearly mapped to
//                     normalized [0.0 .. 1.0] control inputs.
// ==============================================================================
class ShelvesFilter {
public:
    enum OutputMode {
        Mode4BandEQ = 0, // 4-Band Parametric Equalizer
        ModeSVF_LP  = 1, // State-Variable Lowpass (12 dB/oct)
        ModeSVF_BP  = 2, // State-Variable Bandpass (12 dB/oct)
        ModeSVF_HP  = 3  // State-Variable Highpass (12 dB/oct)
    };

    ShelvesFilter() {
        mode = Mode4BandEQ;
        currentSr = 48000.0f;
        std::memset(&frame, 0, sizeof(frame));

        // Setup default Shelves parameter positions (neutral, unity response)
        frame.ls_freq_knob = 0.2f;
        frame.ls_gain_knob = 0.0f;
        frame.hs_freq_knob = 0.8f;
        frame.hs_gain_knob = 0.0f;
        frame.p2_freq_knob = 0.65f;
        frame.p2_gain_knob = 0.0f;
        frame.p2_q_knob = 0.3f;
        frame.p1_freq_knob = 0.5f;
        frame.p1_gain_knob = 0.0f;
        frame.p1_q_knob = 0.3f;
        frame.pre_gain = false;

        engine.setSampleRate(currentSr);
    }

    void setSampleRate(float sr) {
        currentSr = std::max(22050.0f, sr);
        engine.setSampleRate(currentSr);
    }

    void reset() {
        engine.setSampleRate(currentSr);
    }

    void setMode(uint8_t m) {
        mode = static_cast<OutputMode>(m % 4);
    }

    OutputMode getMode() const {
        return mode;
    }

    // Explicitly configure all 4 parametric EQ bands
    void setEQParams(float lsFreq, float lsGain,
                     float p1Freq, float p1Gain, float p1Q,
                     float p2Freq, float p2Gain, float p2Q,
                     float hsFreq, float hsGain) {
        frame.ls_freq_knob = std::clamp(lsFreq, 0.001f, 1.0f);
        frame.ls_gain_knob = std::clamp(lsGain, -1.0f, 1.0f);
        frame.p1_freq_knob = std::clamp(p1Freq, 0.001f, 1.0f);
        frame.p1_gain_knob = std::clamp(p1Gain, -1.0f, 1.0f);
        frame.p1_q_knob = std::clamp(p1Q, 0.0f, 1.0f);
        frame.p2_freq_knob = std::clamp(p2Freq, 0.001f, 1.0f);
        frame.p2_gain_knob = std::clamp(p2Gain, -1.0f, 1.0f);
        frame.p2_q_knob = std::clamp(p2Q, 0.0f, 1.0f);
        frame.hs_freq_knob = std::clamp(hsFreq, 0.001f, 1.0f);
        frame.hs_gain_knob = std::clamp(hsGain, -1.0f, 1.0f);
    }

    // Set cutoff and resonance from synth 16-bit CV registers [0 .. 65535]
    void setCV(uint16_t cvCutoff, uint16_t cvResonance) {
        float normCut = (float)cvCutoff / 65535.0f;
        normCut = std::clamp(normCut, 0.001f, 1.0f);

        float normRes = (float)cvResonance / 65535.0f;
        normRes = std::clamp(normRes, 0.0f, 1.0f);

        // Sweep Mid 1 frequency with Cutoff, Q with Resonance
        frame.p1_freq_knob = normCut;
        frame.p1_q_knob = normRes;
    }

    // Processes a single audio sample through the Shelves EQ / SVF model
    float processSample(float input) {
        frame.main_in = input * 5.0f;
        engine.process(frame);

        float out = 0.0f;
        switch (mode) {
            case ModeSVF_LP:
                out = frame.p1_lp_out;
                break;
            case ModeSVF_BP:
                out = frame.p1_bp_out;
                break;
            case ModeSVF_HP:
                out = frame.p1_hp_out;
                break;
            case Mode4BandEQ:
            default:
                out = frame.main_out;
                break;
        }

        // Scale back to VST floating-point audio level [-1.0 .. +1.0]
        return out * 0.22f;
    }

private:
    shelves::ShelvesEngine engine;
    shelves::ShelvesEngine::Frame frame;
    float currentSr = 48000.0f;
    OutputMode mode = Mode4BandEQ;
};
