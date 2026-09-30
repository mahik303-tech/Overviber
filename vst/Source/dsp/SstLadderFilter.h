#pragma once

#include "OvercyclerTypes.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==============================================================================
// Surge Synthesizer Team (SST) Vintage Moog 4-Pole Transistor Ladder Filter
// ==============================================================================
//
// Source & Inspiration:
//   Surge Synthesizer Team (SST) - sst-filters & Surge XT
//   https://github.com/surge-synthesizer/sst-filters
//   Original Authors: Paul Walker, Baconpaul, and the Surge Synth Team
//   Underlying Research: Antti Huovilainen (DAFx-04) & Vadim Zavalishin (TPT/ZDF)
//   License: GNU General Public License v3.0 (GPL-3.0)
//
// Algorithmic Foundations:
//   1. 4-Pole Non-Linear Transistor Cascade:
//      Models the differential transistor pairs with a tanh on the input pair
//      and on each stage's step: stage_out = Vs tanh((stage_in - state) G / Vs)
//      + state. The scales are set against the voice's filter input (about
//      +-0.25 at full mix): kInputScale saturates the input pair softly, as
//      the SSI2144 does; kStageScale only touches large, fast steps. (With
//      the transistors' 26 mV the input clipped hard from about 0.01 and every
//      stage limited its step per sample: -6 dB at full mix, -27 dB aliasing.)
//
//   2. Pole Taps & Selectable Slopes:
//      - Mode 0: 24 dB/oct Lowpass (4-Pole Vintage Moog Ladder)
//      - Mode 1: 18 dB/oct Lowpass (3-Pole Ladder)
//      - Mode 2: 12 dB/oct Lowpass (2-Pole Ladder)
//      - Mode 3: 6 dB/oct Lowpass (1-Pole Gentle Slope)
//
//   3. Runs at the voice's 2x rate (Voice, Halfband2x.h): the saturation's
//      harmonics stay above the audio band until the output decimator.
// ==============================================================================

class SstLadderFilter {
public:
    SstLadderFilter() {
        sampleRate = 48000.0f;
        reset();
        updateCoefficients();
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(22050.0f, sr);
        updateCoefficients();
    }

    void reset() {
        s[0] = s[1] = s[2] = s[3] = 0.0f;
        zi = 0.0f;
    }

    void setMode(uint8_t m) {
        mode = m & 3;
    }

    // Maps 16-bit synth CV values [0 .. 65535] to physical filter parameters
    void setCV(uint16_t cvCutoff, uint16_t cvResonance) {
        // Same range as the SSI2144 (filterCutoffHz): in the UI both share the
        // LADDER entry, so switching 24 -> 18 dB keeps the cutoff.
        cutoffHz = std::clamp(filterCutoffHz(cvCutoff), 10.0f, sampleRate * 0.45f);

        // Self-oscillation starts near a feedback of 4 (3.8 .. 4.25 with the
        // cutoff, measured by ResonanceCalibrationTest), reached at two thirds
        // of the knob (kFilterResonanceOnset); above it up to 5.
        resonance = ladderResonanceFeedback(cvResonance, 4.0f, 5.0f);

        updateCoefficients();
    }

    float getCutoffHz() const { return cutoffHz; }
    float getResonance() const { return resonance; }

    inline float processSample(float input) {
        {
            // Delayed feedback from 4th stage
            float feedback = resonance * s[3];

            // Input differential pair soft saturation
            float u = kInputScale * fastTanh((input - feedback) / kInputScale);

            // Stage 1
            float delta0 = (u - s[0]) * G;
            float y0 = kStageScale * fastTanh(delta0 / kStageScale) + s[0];
            s[0] = std::clamp(y0 + delta0, -4.0f, 4.0f);

            // Stage 2
            float delta1 = (y0 - s[1]) * G;
            float y1 = kStageScale * fastTanh(delta1 / kStageScale) + s[1];
            s[1] = std::clamp(y1 + delta1, -4.0f, 4.0f);

            // Stage 3
            float delta2 = (y1 - s[2]) * G;
            float y2 = kStageScale * fastTanh(delta2 / kStageScale) + s[2];
            s[2] = std::clamp(y2 + delta2, -4.0f, 4.0f);

            // Stage 4
            float delta3 = (y2 - s[3]) * G;
            float y3 = kStageScale * fastTanh(delta3 / kStageScale) + s[3];
            s[3] = std::clamp(y3 + delta3, -4.0f, 4.0f);

            // Selectable pole tap based on mode
            switch (mode) {
                case 1: return y2; // 18 dB / 3-Pole
                case 2: return y1; // 12 dB / 2-Pole
                case 3: return y0; // 6 dB / 1-Pole
                case 0:
                default: return y3; // 24 dB / 4-Pole
            }
        }
    }

private:
    static constexpr float kInputScale = 0.8f;
    static constexpr float kStageScale = 0.5f;

    void updateCoefficients() {
        float omega = 2.0f * (float)M_PI * cutoffHz / sampleRate;
        // Bilinear transform integrator coefficient with frequency warping compensation
        G = std::tan(omega * 0.5f);
        G = std::clamp(G, 0.0001f, 0.999f);
    }

    // High-performance rational Padé approximation of tanh(x)
    static inline float fastTanh(float x) {
        if (x < -3.0f) return -1.0f;
        if (x > 3.0f) return 1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float resonance = 0.0f;
    float G = 0.1f;
    uint8_t mode = 0; // 0=24dB, 1=18dB, 2=12dB, 3=6dB

    float s[4] = { 0.0f };
    float zi = 0.0f;
};
