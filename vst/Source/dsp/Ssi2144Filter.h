#pragma once

#include "OvercyclerTypes.h"
#include <cmath>
#include <algorithm>

// ==============================================================================
// Sound Semiconductor SSI2144 / SSM2044 4-Pole Voltage-Controlled Ladder Filter
// ==============================================================================
//
// Hardware Background:
//   The SSI2144 is the modern, improved successor to Dave Rossum's classic SSM2044
//   4-pole lowpass filter IC, celebrated for its warm, musical character in legendary
//   synthesizers such as the Korg Polysix, Korg Mono/Poly, and PPG Wave 2.
//
// DSP Modeling Architecture:
//   - Zero-Delay Feedback (ZDF): Implemented using the bilinear transform and the
//     algebraic delay-free loop resolution method pioneered by Vadim Zavalishin
//     ("The Art of VA Filter Design", Native Instruments).
//   - 2x Internal Oversampling: Compensates for bilinear frequency warping near
//     Nyquist and reduces non-linear aliasing harmonics.
//   - Analog Differential Saturation: Models the bipolar transconductance differential
//     pair soft-clipping characteristic via an optimized Padé approximant of tanh(x).
//   - Self-Oscillation: Produces a clean, stable sine wave at high resonance values
//     with internal integrator state saturation damping.
// ==============================================================================
class Ssi2144Filter {
public:
    Ssi2144Filter() {
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
    }

    // Maps 16-bit synth CV values [0 .. 65535] to physical filter parameters
    void setCV(uint16_t cvCutoff, uint16_t cvResonance) {
        // Musical logarithmic mapping up to 26 kHz for open brilliance and zero high-end dampening
        float normCut = (float)cvCutoff / 65535.0f;
        float hz = 20.0f * std::pow(1300.0f, normCut); // at normCut=1.0 -> 26,000 Hz
        hz = std::clamp(hz, 10.0f, sampleRate * 0.495f);
        cutoffHz = hz;

        // The ZDF ladder oscillates at k = 4, reached at two thirds of the knob
        // (kFilterResonanceOnset); above it up to k = 5.
        k = ladderResonanceFeedback(cvResonance, 4.0f, 5.0f);

        updateCoefficients();
    }

    float getCutoffHz() const { return cutoffHz; }
    float getFeedbackGain() const { return k; }

    // Processes a single audio sample through the 2x oversampled ZDF ladder
    float processSample(float input) {
        float out = 0.0f;

        // 2x Oversampling loop
        for (int os = 0; os < 2; ++os) {
            // S: Total feedback contribution of the 4 internal integrator states
            float S = G3 * s[0] + G2 * s[1] + G * s[2] + s[3];

            // Closed-form algebraic solution for delay-free loop:
            // u = (x - k * S) / (1 + k * G^4) -> multiplied by precomputed reciprocal
            float u = (input - k * S) * inv1kG4;

            // Bipolar differential pair soft saturation
            u = fastTanh(u);

            // Cascade of 4 one-pole integrator stages (trapezoidal integration):
            float v0 = (u - s[0]) * G;
            float y0 = v0 + s[0];
            s[0] = std::clamp(y0 + v0, -4.0f, 4.0f);

            float v1 = (y0 - s[1]) * G;
            float y1 = v1 + s[1];
            s[1] = std::clamp(y1 + v1, -4.0f, 4.0f);

            float v2 = (y1 - s[2]) * G;
            float y2 = v2 + s[2];
            s[2] = std::clamp(y2 + v2, -4.0f, 4.0f);

            float v3 = (y2 - s[3]) * G;
            float y3 = v3 + s[3];
            s[3] = std::clamp(y3 + v3, -4.0f, 4.0f);

            out += y3;
        }

        // Decimate 2x oversampling accumulator back to sample rate
        return out * 0.5f;
    }

private:
    // Fast Padé [3/2] rational approximant of tanh(x) for efficient soft clipping
    static inline float fastTanh(float x) {
        if (x < -3.0f) return -1.0f;
        if (x > 3.0f) return 1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    void updateCoefficients() {
        float osRate = sampleRate * 2.0f;
        float w = 2.0f * 3.14159265358979323846f * cutoffHz;
        float g = std::tan(w / (2.0f * osRate));
        g = std::clamp(g, 0.0001f, 0.95f);

        G = g / (1.0f + g);
        G2 = G * G;
        G3 = G2 * G;
        float G4 = G3 * G;
        inv1kG4 = 1.0f / (1.0f + k * G4);
    }

    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float k = 0.0f;

    // Pre-computed feedback ladder multipliers & reciprocal
    float G = 0.0f;
    float G2 = 0.0f;
    float G3 = 0.0f;
    float inv1kG4 = 1.0f;

    // Internal integrator state registers (ZDF memory)
    float s[4] = {0.0f, 0.0f, 0.0f, 0.0f};

};
