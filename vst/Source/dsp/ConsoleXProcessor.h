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
// decoder and nonlinear bus deliberately colour the sum of the voices; a
// single voice passes unchanged below the bus knee. The encoder has a smooth
// knee above 0.75. It runs at the voices' 2x rate (MasterBus), so the output
// decimator removes the harmonics above the audio band; the former
// "ultrasonic" lowpass at the output rate (about -2 dB at 18 kHz at 44.1 kHz)
// is gone. See SignalQualityScenarioTest for measured figures.
class ConsoleXProcessor {
public:
    static constexpr double PHI = 1.6180339887498948482;
    static constexpr double INV_PHI = 0.6180339887498948482; // (PHI - 1.0) = 1.0 / PHI
    // Discontinuity knob (0..1): its default and the threshold there.
    static constexpr double kDiscontinuityDefault = 17.0 / 999.0;
    static constexpr double kDiscontinuityDefaultTop = 2.0 - 1.75 * 500.0 / 999.0;   // the former default (knob 500)

    ConsoleXProcessor() {
        reset();
    }

    void reset() {
        drive = 1.0f;
        outPad = 1.0f;
        discontinuity = 0.5f;
    }

    void setSampleRate(float sr) {
        sampleRate = sr > 8000.0f ? sr : 48000.0f;
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
    // Decodes the accumulated sum of all voices and applies Discontinuity.
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

    float sampleRate = 48000.0f;
    float drive = 1.0f;
    float outPad = 1.0f;
    float discontinuity = 0.5f;
};
