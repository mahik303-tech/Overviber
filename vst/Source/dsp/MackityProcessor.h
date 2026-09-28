#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==============================================================================
// Airwindows Mackity - Mackie 1202 Line/Preamp Overdrive & Slew-Clipper Model
// ==============================================================================
//
// Source:
//   Repository: https://github.com/airwindows/airwindows/tree/master/plugins/WinVST/Mackity
//   Files:      Mackity.cpp, MackityProc.cpp, Mackity.h
//   Original Author: Chris Johnson (Airwindows)
//   License:    MIT License (Open Source)
//
// Physical & Algorithmic Background:
//   Mackity models the iconic sonic behavior of the vintage Mackie CR1604 / MicroSeries
//   1202 compact mixer channel strip and preamps when driven into heavy saturation,
//   a legendary sound heavily utilized in 1990s French Touch, Techno, and Industrial music.
//
// Processing Stages:
//   1. DC Blocker A: First-order highpass filter removing sub-audible DC offsets
//      before entering non-linear stages.
//   2. Input Trim: Maps parameter A [0.0 .. 1.0] quadratically via (A * 10)^2.
//      - At A = 0.10: Gain is (0.1 * 10)^2 = 1.0 (Unity Gain, 0 dB).
//      - At A = 1.00: Gain is (1.0 * 10)^2 = 100.0 (+40 dB hot console drive).
//   3. Pre-Filter Biquad A: Direct Form 1 lowpass filter tuned to 19,160 Hz with
//      Q = 0.431685 (sub-critical Bessel-like damping) modeling input stage bandwidth.
//   4. Non-Linear Slew & Soft-Clipper: Hard limiting clamp [-1.0 .. +1.0] followed by
//      the polynomial expansion (x - 0.1768 * x^5) simulating analog bipolar transistor
//      differential pair saturation.
//   5. Post-Filter Biquad B: Direct Form 1 lowpass filter tuned to 19,160 Hz with
//      Q = 1.15823 (resonant peak) modeling op-amp inductive peaking / slew-recovery.
//   6. DC Blocker B: Second-order stage ensuring symmetrical zero-baseline output.
//   7. Output Pad: Attenuates output level from 0 dB down to silence via parameter B.
//   8. TPDF Dither: 32-bit floating-point dither powered by a Galois/Marsaglia XORShift32
//      PRNG to eliminate sub-LSB truncation distortion.
// ==============================================================================
class MackityProcessor {
public:
    // --------------------------------------------------------------------------
    // Strongly-typed Direct Form 1 Stereo Biquad Filter
    // --------------------------------------------------------------------------
    struct BiquadDF1 {
        // Filter Coefficients
        double a0 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double b1 = 0.0;
        double b2 = 0.0;

        // Stereo History / State Registers
        double x1L = 0.0, x2L = 0.0; // Input history Left (x[n-1], x[n-2])
        double y1L = 0.0, y2L = 0.0; // Output history Left (y[n-1], y[n-2])
        double x1R = 0.0, x2R = 0.0; // Input history Right (x[n-1], x[n-2])
        double y1R = 0.0, y2R = 0.0; // Output history Right (y[n-1], y[n-2])

        // Resets state registers while leaving filter coefficients intact
        void resetStates() {
            x1L = x2L = y1L = y2L = 0.0;
            x1R = x2R = y1R = y2R = 0.0;
        }

        // Calculates Direct Form 1 lowpass coefficients using the bilinear transform
        void setupLowpass(double freqNorm, double q) {
            double K = std::tan(M_PI * freqNorm);
            double norm = 1.0 / (1.0 + K / q + K * K);
            a0 = K * K * norm;
            a1 = 2.0 * a0;
            a2 = a0;
            b1 = 2.0 * (K * K - 1.0) * norm;
            b2 = (1.0 - K / q + K * K) * norm;
        }

        // Processes one stereo sample in-place:
        // y[n] = a0*x[n] + a1*x[n-1] + a2*x[n-2] - b1*y[n-1] - b2*y[n-2]
        inline void process(double& sampleL, double& sampleR) {
            double outL = a0 * sampleL + a1 * x1L + a2 * x2L - b1 * y1L - b2 * y2L;
            x2L = x1L;
            x1L = sampleL;
            y2L = y1L;
            y1L = outL;
            sampleL = outL;

            double outR = a0 * sampleR + a1 * x1R + a2 * x2R - b1 * y1R - b2 * y2R;
            x2R = x1R;
            x1R = sampleR;
            y2R = y1R;
            y1R = outR;
            sampleR = outR;
        }
    };

    MackityProcessor() {
        reset();
        setSampleRate(48000.0);
    }

    // Resets filter delay lines and re-initializes random dither states
    void reset() {
        iirSampleAL = 0.0;
        iirSampleAR = 0.0;
        iirSampleBL = 0.0;
        iirSampleBR = 0.0;

        biquadA.resetStates();
        biquadB.resetStates();

        fpdL = 16386;
        fpdR = 32768;
        updateCoefficients();
    }

    void setSampleRate(double sr) {
        if (sr <= 1000.0) sr = 48000.0;
        sampleRate = sr;
        updateCoefficients();
    }

    // Set parameters from synth pot values (0 .. 999)
    void setParameters(float inTrimPot, float outPadPot) {
        float a = std::clamp(inTrimPot / 999.0f, 0.0f, 1.0f);
        float b = std::clamp(outPadPot / 999.0f, 0.0f, 1.0f);
        setRawParameters(a, b);
    }

    // Set raw Airwindows parameters:
    //   a: In Trim [0.0 .. 1.0] (0.1 = unity 0 dB, 1.0 = +40 dB drive)
    //   b: Out Pad [0.0 .. 1.0] (1.0 = 0 dB full, 0.0 = -inf silence)
    void setRawParameters(float a, float b) {
        paramA = std::clamp(a, 0.0f, 1.0f);
        paramB = std::clamp(b, 0.0f, 1.0f);

        // Quadratic drive curve: (A * 10)^2
        double trim = (double)paramA * 10.0;
        inTrim = trim * trim;
        outPad = (double)paramB;
    }

    float getParamA() const { return paramA; }
    float getParamB() const { return paramB; }
    double getInTrim() const { return inTrim; }
    double getOutPad() const { return outPad; }

    // Processes one stereo frame in-place through the Mackity console pipeline
    inline void processSample(float& sampleL, float& sampleR) {
        double inputSampleL = (double)sampleL;
        double inputSampleR = (double)sampleR;

        // Anti-denormal noise injection below audible range (-300 dBFS)
        if (std::abs(inputSampleL) < 1.18e-23) inputSampleL = (double)fpdL * 1.18e-17;
        if (std::abs(inputSampleR) < 1.18e-23) inputSampleR = (double)fpdR * 1.18e-17;

        // 1. Highpass Filter / DC Blocker Stage A
        if (std::abs(iirSampleAL) < 1.18e-37) iirSampleAL = 0.0;
        iirSampleAL = (iirSampleAL * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
        inputSampleL -= iirSampleAL;

        if (std::abs(iirSampleAR) < 1.18e-37) iirSampleAR = 0.0;
        iirSampleAR = (iirSampleAR * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
        inputSampleR -= iirSampleAR;

        // 2. Input Preamp Gain / Drive
        if (inTrim != 1.0) {
            inputSampleL *= inTrim;
            inputSampleR *= inTrim;
        }

        // 3. Pre-Clipping Filter (Direct Form 1 Lowpass, fc = 19.16 kHz, Q = 0.431685)
        biquadA.process(inputSampleL, inputSampleR);

        // 4. Non-Linear Slew & Soft-Clipping Saturation Curve (x - 0.1768 * x^5)
        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        double l2 = inputSampleL * inputSampleL;
        inputSampleL -= (l2 * l2 * inputSampleL) * 0.1768;

        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        double r2 = inputSampleR * inputSampleR;
        inputSampleR -= (r2 * r2 * inputSampleR) * 0.1768;

        // 5. Post-Clipping Filter (Direct Form 1 Lowpass, fc = 19.16 kHz, Q = 1.15823)
        biquadB.process(inputSampleL, inputSampleR);

        // 6. Highpass Filter / DC Blocker Stage B
        if (std::abs(iirSampleBL) < 1.18e-37) iirSampleBL = 0.0;
        iirSampleBL = (iirSampleBL * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
        inputSampleL -= iirSampleBL;

        if (std::abs(iirSampleBR) < 1.18e-37) iirSampleBR = 0.0;
        iirSampleBR = (iirSampleBR * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
        inputSampleR -= iirSampleBR;

        // 7. Output Level Pad
        if (outPad != 1.0) {
            inputSampleL *= outPad;
            inputSampleR *= outPad;
        }

        // 8. Airwindows High-Frequency 32-bit Floating Point TPDF Dither
        int expon;
        frexpf((float)inputSampleL, &expon);
        fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
        inputSampleL += ((double(fpdL) - (double)0x7fffffff) * 5.5e-36 * std::pow(2.0, expon + 62));

        frexpf((float)inputSampleR, &expon);
        fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
        inputSampleR += ((double(fpdR) - (double)0x7fffffff) * 5.5e-36 * std::pow(2.0, expon + 62));

        sampleL = (float)inputSampleL;
        sampleR = (float)inputSampleR;
    }

private:
    void updateCoefficients() {
        double overallscale = sampleRate / 44100.0;
        if (overallscale < 0.1) overallscale = 0.1;

        // Scale DC blocker pole time-constants with sample rate
        iirAmountA = 0.001860867 / overallscale;
        iirAmountB = 0.000287496 / overallscale;

        // Normalized cutoff frequency clamped safely below Nyquist for low sample rates
        double freqNorm = std::clamp(19160.0 / sampleRate, 0.01, 0.49);

        // Biquad A: Q = 0.431684981684982 (Bessel-like smooth rolloff)
        biquadA.setupLowpass(freqNorm, 0.431684981684982);

        // Biquad B: Q = 1.1582298 (Resonant overshoot modeling slew recovery)
        biquadB.setupLowpass(freqNorm, 1.1582298);
    }

    double sampleRate = 48000.0;
    float paramA = 0.1f;
    float paramB = 1.0f;
    double inTrim = 1.0;
    double outPad = 1.0;

    double iirAmountA = 0.001860867;
    double iirAmountB = 0.000287496;

    double iirSampleAL = 0.0;
    double iirSampleAR = 0.0;
    double iirSampleBL = 0.0;
    double iirSampleBR = 0.0;

    BiquadDF1 biquadA;
    BiquadDF1 biquadB;

    uint32_t fpdL = 16386;
    uint32_t fpdR = 32768;
};
