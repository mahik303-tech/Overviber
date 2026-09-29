#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Overviber custom console, inspired by Airwindows (Chris Johnson, MIT).
// This is not an implementation of upstream Console X. Its local Phi encoder /
// decoder, nonlinear bus and low-pass stages deliberately colour the signal.
// A single voice is not guaranteed transparent, nor is this oversampled or
// alias-free. The encoder has a smooth knee above 0.75. See
// AUDIO_REFACTORING_REPORT.md for measured limits.
class ConsoleXProcessor {
public:
    static constexpr double PHI = 1.6180339887498948482;
    static constexpr double INV_PHI = 0.6180339887498948482; // (PHI - 1.0) = 1.0 / PHI
    // Discontinuity knob (0..1): its default and the threshold there.
    static constexpr double kDiscontinuityDefault = 17.0 / 999.0;
    static constexpr double kDiscontinuityDefaultTop = 2.0 - 1.75 * 500.0 / 999.0;   // the former default (knob 500)

    // --------------------------------------------------------------------------
    // Strongly-typed Direct Form 1 Stereo Biquad Filter for Ultrasonic Smoothing
    // --------------------------------------------------------------------------
    struct BiquadDF1 {
        double a0 = 1.0, a1 = 0.0, a2 = 0.0;
        double b1 = 0.0, b2 = 0.0;
        double x1L = 0.0, x2L = 0.0, y1L = 0.0, y2L = 0.0;
        double x1R = 0.0, x2R = 0.0, y1R = 0.0, y2R = 0.0;

        void reset() {
            x1L = x2L = y1L = y2L = 0.0;
            x1R = x2R = y1R = y2R = 0.0;
        }

        void setupLowpass(double freqNorm, double q) {
            double K = std::tan(M_PI * std::clamp(freqNorm, 0.01, 0.46));
            double norm = 1.0 / (1.0 + K / q + K * K);
            a0 = K * K * norm;
            a1 = 2.0 * a0;
            a2 = a0;
            b1 = 2.0 * (K * K - 1.0) * norm;
            b2 = (1.0 - K / q + K * K) * norm;
        }

        inline void process(double& sampleL, double& sampleR) {
            double outL = a0 * sampleL + a1 * x1L + a2 * x2L - b1 * y1L - b2 * y2L;
            x2L = x1L; x1L = sampleL;
            y2L = y1L; y1L = outL;
            sampleL = outL;

            double outR = a0 * sampleR + a1 * x1R + a2 * x2R - b1 * y1R - b2 * y2R;
            x2R = x1R; x1R = sampleR;
            y2R = y1R; y1R = outR;
            sampleR = outR;
        }
    };

    ConsoleXProcessor() {
        reset();
    }

    void reset() {
        ultraFilter.reset();
        drive = 1.0f;
        outPad = 1.0f;
        discontinuity = 0.5f;
    }

    void setSampleRate(float sr) {
        sampleRate = sr > 8000.0f ? sr : 48000.0f;
        sampleRateScale = (double)sampleRate / 44100.0;
        setupUltrasonicFilter();
    }

    void setParameters(float drive0to1, float pad0to1, float discontinuity0to1) {
        // Drive: 0.1 = 1.0x (0 dB), 1.0 = 4.0x (+12 dB console push)
        drive = std::clamp(drive0to1 * 3.0f + 0.7f, 0.5f, 4.0f);
        outPad = std::clamp(pad0to1, 0.0f, 1.0f);
        discontinuity = std::clamp(discontinuity0to1, 0.0f, 1.0f);
    }

    // --------------------------------------------------------------------------
    // 1. Channel Encoding (Per-Voice Pre-Summing Stage)
    // --------------------------------------------------------------------------
    // Transforms each voice before addition onto the master summing bus.
    inline void encodeVoice(float inL, float inR, float& outL, float& outR) const {
        outL = static_cast<float>(encodeSample(static_cast<double>(inL) * drive * INV_PHI));
        outR = static_cast<float>(encodeSample(static_cast<double>(inR) * drive * INV_PHI));
    }
    // --------------------------------------------------------------------------
    // 2. Master Buss Decoding & Dynamics (Post-Summing Stage)
    // --------------------------------------------------------------------------
    // Decodes the accumulated sum of all voices and applies Discontinuity & Ultrasonic filtering.
    inline void decodeMaster(float inL, float inR, float& outL, float& outR) {
        double sL = (double)inL;
        double sR = (double)inR;

        // Apply smooth soft-knee saturation on the summing bus for signals approaching/exceeding unity
        sL = saturateBus(sL);
        sR = saturateBus(sR);

        // Golden Ratio Buss Inverse Decoding
        if (sL > 0.0) sL = -std::expm1(std::log1p(-sL) * INV_PHI);
        else if (sL < 0.0) sL = std::expm1(std::log1p(sL) * INV_PHI);
        sL *= PHI;

        if (sR > 0.0) sR = -std::expm1(std::log1p(-sR) * INV_PHI);
        else if (sR < 0.0) sR = std::expm1(std::log1p(sR) * INV_PHI);
        sR *= PHI;

        // Discontinuity: Air acoustic wave steepening modeling. The threshold
        // spans the decoded bus's whole range: the bus saturation limits it to
        // about 1.14, so the former 2.0 - 1.75 x knob left the knob's lower
        // half without effect. kDiscontinuityDefault gives the former
        // default's threshold (1.124); the knob's end reaches 0.25.
        if (discontinuity > 0.01f) {
            const double top = kDiscontinuityDefaultTop
                - ((double)discontinuity - kDiscontinuityDefault) * (kDiscontinuityDefaultTop - 0.25)
                    / (1.0 - kDiscontinuityDefault);
            double absL = std::abs(sL);
            if (absL > top) {
                double excess = absL - top;
                double comp = top + std::log1p(excess * 0.7);
                sL = (sL > 0.0 ? 1.0 : -1.0) * comp;
            }
            double absR = std::abs(sR);
            if (absR > top) {
                double excess = absR - top;
                double comp = top + std::log1p(excess * 0.7);
                sR = (sR > 0.0 ? 1.0 : -1.0) * comp;
            }
        }

        // Ultrasonic Anti-Aliasing Filter
        ultraFilter.process(sL, sR);

        // Apply Master Pad and drive normalization
        float padScale = (outPad / std::max(0.5f, drive));
        outL = (float)(sL * padScale);
        outR = (float)(sR * padScale);
    }

private:
    // Constructed outside rendering. Linear interpolation replaces twelve
    // log/exp pairs per six-voice stereo sample, with a tested error bound.
    static const std::array<double, 4097>& encoderTable() {
        static const auto table = [] {
            std::array<double, 4097> values{};
            for (int i = 1; i < 4096; ++i)
                values[i] = -std::expm1(std::log1p(-static_cast<double>(i) / 4096.0) * PHI);
            values[4096] = 1.0;
            return values;
        }();
        return table;
    }
    const std::array<double, 4097>& encoderLut = encoderTable();
    double encodeSample(double input) const {
        double magnitude = std::abs(input);
        if (magnitude > 0.75)
            magnitude = 0.75 + 0.249999 * std::tanh((magnitude - 0.75) / 0.249999);
        const double position = std::min(magnitude, 1.0) * 4096.0;
        const int index = std::min(static_cast<int>(position), 4095);
        const double fraction = position - index;
        return std::copysign(encoderLut[index] + fraction * (encoderLut[index + 1] - encoderLut[index]), input);
    }
    // C1 smooth soft-knee bus ceiling keeping decoded bus within mathematical domain
    // and delivering authentic analog bus compression when multiple voices sum.
    static inline double saturateBus(double s) {
        constexpr double kneeThreshold = 0.65;
        constexpr double maxCeiling = 0.86;
        constexpr double range = maxCeiling - kneeThreshold; // 0.21

        double absS = std::abs(s);
        if (absS <= kneeThreshold) {
            return s;
        }
        double saturated = kneeThreshold + range * std::tanh((absS - kneeThreshold) / range);
        return (s > 0.0) ? saturated : -saturated;
    }

    void setupUltrasonicFilter() {
        double cutoff = std::min(20000.0, (double)sampleRate * 0.42);
        ultraFilter.setupLowpass(cutoff / (double)sampleRate, 0.70710678);
    }

    float sampleRate = 48000.0f;
    double sampleRateScale = 1.0884;
    float drive = 1.0f;
    float outPad = 1.0f;
    float discontinuity = 0.5f;

    BiquadDF1 ultraFilter;
};
