#pragma once

#include "OvercyclerTypes.h"
#include <cmath>

// ==============================================================================
// National Semiconductor / TI LM13700 Operational Transconductance Amplifier (OTA) VCA
// ==============================================================================
//
// Hardware Background:
//   The LM13700 is the industry standard dual operational transconductance amplifier
//   (OTA) employed as voltage-controlled amplifiers (VCAs) and filters in classic
//   polyphonic synthesizers (Roland Juno-106, Jupiter-8, Prophet-5, Oberheim OB-8).
//
// DSP Modeling Architecture:
//   1. Control Voltage Smoothing:
//      Models the physical analog RC lowpass filter placed across the I_abc pin
//      (time constant tau ~ 0.6 ms) in hardware circuits. This eliminates digital
//      zipper noise, control stepping, and transient pop artifacts.
//   2. Differential Pair Transconductance Soft Saturation:
//      The transfer function of an uncompensated bipolar differential pair is
//      governed by the hyperbolic tangent equation:
//          I_out = I_abc * tanh(V_in / (2 * V_t))
//      where V_t is the thermal voltage (~26 mV at room temperature).
//      This soft compression imparts natural warm analog saturation to loud audio peaks.
// ==============================================================================
class Lm13700Vca {
public:
    Lm13700Vca() : currentGain(0.0f), targetGain(0.0f), smoothCoeff(0.04f) {}

    // Configures smoothing filter based on sampling rate
    void setSampleRate(float sr) {
        // Hardware RC filter on I_abc control pin (~0.6 ms time constant)
        const float tau = 0.0006f; // 0.6 ms
        smoothCoeff = 1.0f - std::exp(-1.0f / (sr * tau));
    }

    void reset() {
        currentGain = 0.0f;
        targetGain = 0.0f;
    }

    // Set target amplitude from 16-bit CV register [0 .. 65535]
    void setCV(uint16_t cvAmp) {
        targetGain = (float)cvAmp / 65535.0f;
    }

    // Processes a single audio sample through the OTA saturation and gain stage
    float processSample(float input) {
        // One-pole IIR lowpass smoothing on control gain
        currentGain += (targetGain - currentGain) * smoothCoeff;

        // Bipolar differential pair OTA analog curve:
        // Purely linear and transparent for crisp, brilliant transients on normal levels (|x| < 0.75);
        // smooth soft tanh saturation above 0.75 for musical warmth on hot peaks, up to 1.1.
        // The tanh starts with slope 1 and no curvature, so the knee has no
        // kink (a kink spreads harmonics far above the audio band).
        float sat;
        float ax = std::abs(input);
        if (ax < kKnee) {
            sat = input;
        } else {
            float excess = ax - kKnee;
            float sign = (input > 0.0f) ? 1.0f : -1.0f;
            sat = sign * (kKnee + kRange * fastTanh(excess / kRange));
        }

        // Modulate with smoothed VCA gain
        return sat * currentGain;
    }

    // Fast Padé [3/2] rational approximant of tanh(x) for efficient soft saturation
    static inline float fastTanh(float x) {
        if (x < -3.0f) return -1.0f;
        if (x > 3.0f) return 1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    static constexpr float kKnee = 0.75f;
    static constexpr float kRange = 0.35f;

    float getCurrentGain() const { return currentGain; }
    float getTargetGain() const { return targetGain; }

private:
    float currentGain;
    float targetGain;
    float smoothCoeff;
};
