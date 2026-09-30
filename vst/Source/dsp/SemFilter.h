#pragma once

#include "OvercyclerTypes.h"
#include "audible/RipplesFilter.h"
#include <algorithm>
#include <cmath>

// ==============================================================================
// SEM-style 2-pole state-variable filters (Oberheim SEM, 12 dB/oct), with the
// former Liquid filter kept as one selectable variant.
//
// Variants and sources (ported to per-voice scalar code):
//   0 OB-Xd 12 dB     sst-filters OBXDFilter.h (Surge Synth Team), adapted from
//                     OB-Xd Filter.h (reales/OB-Xd). GPL-3.0.
//                     https://github.com/surge-synthesizer/sst-filters
//                     Zero-delay SVF with a diode-pair resistance model in the
//                     feedback path; multimode mix and 0.74 output gain as in OB-Xd.
//   1 Oberheim        Faust standard library ve.oberheim by Eric Tarr, after
//                     Will Pirkle, "Designing Software Synthesizer Plug-ins in
//                     C++", section 7.2. MIT-style STK-4.3 license.
//                     https://github.com/SpotlightKid/faustfilters (faust/oberheim.dsp)
//                     SVF with a cubic soft clipper (ef.cubicnl) inside the loop.
//   2 Vult SVF        Vult examples filters/svf.vult and effects/saturate_soft.vult
//                     by Leonardo Laguna Ruiz. MIT license.
//                     https://github.com/vult-dsp/vult
//                     Trapezoidal SVF with 16*tanh(x/16) output saturation. The
//                     notch is the SEM-style sum of lowpass and highpass. (The
//                     Stabile module itself is not open source.)
//   3 Cytomic SVF     sst-filters CytomicSVF.h, after Andrew Simper's
//                     trapezoidal SVF. GPL-3.0. Linear.
//   4 Liquid          Mutable Instruments Ripples (Audible Instruments port),
//                     the former Liquid filter, unchanged.
//
// Modes 0-3: lowpass, bandpass, highpass, notch (Liquid: LP4, LP2, BP2).
// All variants run at the voice's 2x rate (Voice, Halfband2x.h).
// ==============================================================================
class SemFilter {
public:
    enum Variant : uint8_t { ObXd = 0, Oberheim = 1, Vult = 2, Cytomic = 3, Liquid = 4, VariantCount = 5 };
    enum Mode : uint8_t { Lowpass = 0, Bandpass = 1, Highpass = 2, Notch = 3 };

    void setSampleRate(float sr) {
        sampleRate = std::max(22050.0f, sr);
        liquid.setSampleRate(sampleRate);
        updateCoefficients();
    }

    void reset() {
        s1 = s2 = 0.0f;
        liquid.reset();
    }

    void setVariant(uint8_t v) {
        v = std::min<uint8_t>(v, VariantCount - 1);
        if (v == variant) return;
        variant = static_cast<Variant>(v);
        reset();
        updateCoefficients();
    }
    Variant getVariant() const { return variant; }

    void setMode(uint8_t m) {
        mode = static_cast<Mode>(m % 4);
        liquid.setMode(m);
    }

    void setCV(uint16_t cvCutoff, uint16_t cvResonance) {
        // Same range as the SSI2144 (filterCutoffHz), with an open top: see
        // openTopHz().
        cutoffHz = openTopHz(filterCutoffHz(cvCutoff), cvResonance, sampleRate);
        resonance = (float)cvResonance / 65535.0f;
        liquid.setCV(cvCutoff, liquidResonance(cvResonance));
        updateCoefficients();
    }

    float processSample(float input) {
        switch (variant) {
        case ObXd: return obxd(input);
        case Oberheim: return oberheim(input);
        case Vult: return vult(input);
        case Cytomic: return cytomic(input);
        case Liquid:
        default: return liquid.processSample(input);
        }
    }

private:
    // Above 16 kHz the top of the range opens further, up to 0.45 x the
    // running rate (the voice's 2x rate), so the open filter passes the audio
    // band: at 26 kHz a 2-pole lowpass still takes 1.6 dB off at 15 kHz. The
    // opening shrinks with the resonance and is gone at kFilterResonanceOnset,
    // where a resonant peak stays at the 26 kHz top. These SVFs stay stable at
    // any cutoff (unlike the ladders, which keep the 26 kHz top).
    static float openTopHz(float hz, uint16_t resonanceCv, float rate) {
        constexpr float from = 16000.0f, top = 26000.0f;
        if (hz <= from) return hz;
        const float opening = std::clamp(1.0f - (float)resonanceCv / 65535.0f / kFilterResonanceOnset, 0.0f, 1.0f);
        const float limit = top * std::pow(std::max(1.0f, 0.45f * rate / top), opening);
        return from * std::pow(limit / from, (std::min(hz, top) - from) / (top - from));
    }

    // Liquid (Ripples) starts to self-oscillate at 78 % of its resonance CV,
    // independent of the cutoff; the knob puts that at kFilterResonanceOnset.
    static uint16_t liquidResonance(uint16_t cv) {
        constexpr float onset = 0.78f;
        const float x = (float)cv / 65535.0f;
        const float y = x <= kFilterResonanceOnset
            ? x / kFilterResonanceOnset * onset
            : onset + (x - kFilterResonanceOnset) / (1.0f - kFilterResonanceOnset) * (1.0f - onset);
        return (uint16_t)std::clamp(y * 65535.0f + 0.5f, 0.0f, 65535.0f);
    }

    void updateCoefficients() {
        const float hz = std::clamp(cutoffHz, 10.0f, sampleRate * 0.45f);
        g = std::tan((float)M_PI * hz / sampleRate);
        // One resonance curve for all variants: Q 0.5 .. 20, cubic so the
        // lower knob range stays subtle. In all four models the small-signal
        // damping is 1/Q (OB-Xd adds its diode-pair term on top). Like the
        // SEM itself, none of them self-oscillates.
        const float r = std::clamp(resonance, 0.0f, 1.0f);
        const float q = 0.5f + 19.5f * r * r * r;
        svfR = 1.0f / (2.0f * q);
        alpha0 = 1.0f / (1.0f + 2.0f * svfR * g + g * g);
        const float k = 2.0f * svfR;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        cytomicK = k;
    }

    // ---- OB-Xd 12 dB (sst-filters OBXDFilter::process_2_pole, one lane)
    static float diodePairResistance(float x) {
        return (((((0.0103592f * x) + 0.00920833f) * x + 0.185f) * x + 0.05f) * x + 1.0f);
    }
    float obxd(float sample) {
        const float tCfb = diodePairResistance(s1 * 0.0876f) - 1.0f;
        const float v = (sample - 2.0f * (s1 * (svfR + tCfb)) - g * s1 - s2)
                      / (1.0f + g * (2.0f * (svfR + tCfb) + g));
        const float y1 = v * g + s1;
        s1 = v * g + y1;
        const float y2 = y1 * g + s2;
        s2 = y1 * g + y2;
        float out;
        switch (mode) {
        case Lowpass: out = y2; break;                          // multimode 0
        case Highpass: out = v; break;                          // multimode 1
        case Notch: out = 0.5f * y2 + 0.5f * v; break;          // multimode 0.5
        case Bandpass: default: out = 0.5f * y1; break;         // bandpass, multimode 0.5
        }
        return out * 0.74f;
    }

    // ---- Oberheim after Pirkle/Tarr (Faust ve.oberheim)
    static float cubicnl(float x) {
        x = std::clamp(x, -1.0f, 1.0f);
        return x - x * x * x / 3.0f;
    }
    float oberheim(float x) {
        const float hp = (x - s2 - s1 * (2.0f * svfR + g)) * alpha0;
        const float bp = cubicnl(hp * g + s1);
        const float lp = bp * g + s2;
        s1 = hp * g + bp;
        s2 = bp * g * 2.0f + s2;
        switch (mode) {
        case Lowpass: return lp;
        case Highpass: return hp;
        case Notch: return lp + hp;
        case Bandpass: default: return bp;
        }
    }

    // ---- Vult svf.vult with saturate_soft
    float vult(float x) {
        const float high = (x - (2.0f * svfR + g) * s1 - s2) * alpha0;
        const float band = g * high + s1;
        const float low = g * band + s2;
        s1 = g * high + band;
        s2 = g * band + low;
        float out;
        switch (mode) {
        case Lowpass: out = low; break;
        case Highpass: out = high; break;
        case Notch: out = low + high; break;
        case Bandpass: default: out = band; break;
        }
        return 16.0f * std::tanh(out / 16.0f);
    }

    // ---- Cytomic SVF (sst-filters CytomicSVF::stepSSE, one lane)
    float cytomic(float v0) {
        const float v3 = v0 - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        switch (mode) {
        case Lowpass: return v2;
        case Highpass: return v0 - cytomicK * v1 - v2;
        case Notch: return v0 - cytomicK * v1;
        case Bandpass: default: return v1;
        }
    }

    Variant variant = ObXd;
    Mode mode = Lowpass;
    float sampleRate = 48000.0f;
    float cutoffHz = 26000.0f;
    float resonance = 0.0f;
    float g = 0.5f;
    float s1 = 0.0f, s2 = 0.0f;           // integrator states of the SVF variants
    float svfR = 1.0f, alpha0 = 1.0f;     // damping R = 1/(2Q); OB-Xd: R12
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, cytomicK = 2.0f;
    RipplesFilter liquid;
};
