#pragma once

#include "ripples.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

// ==============================================================================
// Audible Instruments Liquid Filter (Mutable Instruments Ripples Emulation)
// ==============================================================================
//
// Sources:
//   Original Hardware & DSP Design: Émilie Gillet (Mutable Instruments)
//   VCV Rack Port:                  Tyler Coy (Audible Instruments)
//   Repository:                     https://github.com/VCVRack/AudibleInstruments
//   Files:                          ripples.hpp, ripples_aafilter.hpp
//   License:                        GNU General Public License v3.0 (GPLv3)
//
// Filter Characteristics:
//   Liquid Filter models the renowned Mutable Instruments Ripples analog filter.
//   It features an operational transconductance amplifier (OTA) ladder topology
//   famous for its smooth, self-oscillating "liquid" sine resonance and transparent
//   overdrive without harsh clipping.
//
// Available Output Modes:
//   - ModeLP4 (0): 4-Pole 24 dB/oct Lowpass with classic creamy liquid resonance.
//   - ModeLP2 (1): 2-Pole 12 dB/oct Lowpass with open, bright top-end response.
//   - ModeBP2 (2): 2-Pole 12 dB/oct Bandpass for vocal formants and rhythmic filtering.
//
// Signal & CV Scaling:
//   - Input Scaling:  Overcycler VST audio [-1.0 .. +1.0] is scaled by 5.0x to match
//                     the +/- 5V modular Eurorack standard expected by RipplesEngine.
//   - Output Scaling: Filter output is attenuated by 0.22x to return to normalized
//                     digital floating-point studio headroom.
//   - CV Control:     16-bit DAC register values [0 .. 65535] are linearly mapped to
//                     normalized [0.0 .. 1.0] control inputs.
// ==============================================================================
class RipplesFilter {
public:
    enum OutputMode {
        ModeLP4 = 0, // 4-pole 24 dB/oct Lowpass (Liquid resonance)
        ModeLP2 = 1, // 2-pole 12 dB/oct Lowpass
        ModeBP2 = 2  // 2-pole 12 dB/oct Bandpass
    };

    RipplesFilter() {
        mode = ModeLP4;
        currentSr = 48000.0f;
        std::memset(&frame, 0, sizeof(frame));
        frame.res_knob = 0.0f;
        frame.freq_knob = 1.0f;
        frame.fm_knob = 0.0f;
        frame.gain_cv_present = false;
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
        mode = static_cast<OutputMode>(m % 3);
    }

    OutputMode getMode() const {
        return mode;
    }

    // Set cutoff and resonance from synth 16-bit CV registers [0 .. 65535]
    void setCV(uint16_t cvCutoff, uint16_t cvResonance) {
        // Frequency: 0 .. 65535 maps linearly to [0.001, 1.0] across 20 Hz .. 20 kHz
        float normCut = (float)cvCutoff / 65535.0f;
        frame.freq_knob = std::clamp(normCut, 0.001f, 1.0f);

        // Resonance: 0 .. 65535 maps to [0.0, 1.0]
        // Clean self-oscillation begins smoothly above ~0.80
        float normRes = (float)cvResonance / 65535.0f;
        frame.res_knob = std::clamp(normRes, 0.0f, 1.0f);
    }

    float getCutoffNorm() const { return frame.freq_knob; }
    float getResonanceNorm() const { return frame.res_knob; }

    // Processes a single audio sample through the analog OTA filter model
    float processSample(float input) {
        // Scale to Eurorack level (+/- 5V)
        frame.input = input * 5.0f;
        engine.process(frame);

        float out = 0.0f;
        switch (mode) {
            case ModeLP2:
                out = frame.lp2;
                break;
            case ModeBP2:
                out = frame.bp2;
                break;
            case ModeLP4:
            default:
                out = frame.lp4;
                break;
        }

        // Scale back to VST floating-point audio level [-1.0 .. +1.0]
        return out * 0.22f;
    }

private:
    ripples::RipplesEngine engine;
    ripples::RipplesEngine::Frame frame;
    float currentSr = 48000.0f;
    OutputMode mode = ModeLP4;
};
