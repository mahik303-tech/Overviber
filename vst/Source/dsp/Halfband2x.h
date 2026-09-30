#pragma once

#include <array>
#include <cmath>

// ==============================================================================
// 2x up- and downsampling with a polyphase IIR half-band filter
// ==============================================================================
// Two parallel chains of first-order allpass sections in z^-2 form the
// half-band lowpass (Valenzuela and Constantinides; the structure of Laurent
// de Soras' HIIR). The coefficients come from the elliptic design for a given
// transition band: the passband reaches (0.25 - transition) x the 2x rate,
// the stopband starts at (0.25 + transition). With kTransition at a 2x rate
// of 88.2 kHz the passband ends at 20 kHz and everything folding into it
// is at least kAttenuationDb down (SignalQualityScenarioTest measures both).
// Phase is not linear near the band edge, as with an analog lowpass; the
// group delay at low frequencies is a few samples.
// ==============================================================================
namespace halfband {

constexpr int kCoefficients = 12;
constexpr double kTransition = 0.25 - 20000.0 / 88200.0;

// Elliptic half-band design (HIIR's PolyphaseIir2Designer, same formulas).
inline std::array<double, kCoefficients> designCoefficients(double transition = kTransition) {
    constexpr double pi = 3.14159265358979323846;
    double k = std::tan((1.0 - transition * 2.0) * pi / 4.0);
    k *= k;
    const double kksqrt = std::pow(1.0 - k * k, 0.25);
    const double e = 0.5 * (1.0 - kksqrt) / (1.0 + kksqrt);
    const double e4 = e * e * e * e;
    const double q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
    const int order = kCoefficients * 2 + 1;

    std::array<double, kCoefficients> coefficients{};
    for (int index = 0; index < kCoefficients; ++index) {
        const int c = index + 1;
        double num = 0.0, term = 0.0;
        int i = 0, sign = 1;
        do {
            term = std::pow(q, i * (i + 1)) * std::sin((i * 2 + 1) * c * pi / order) * sign;
            num += term;
            sign = -sign;
            ++i;
        } while (std::abs(term) > 1e-100);
        num *= std::pow(q, 0.25);
        double den = 0.0;
        i = 1;
        sign = -1;
        do {
            term = std::pow(q, i * i) * std::cos(i * 2 * c * pi / order) * sign;
            den += term;
            sign = -sign;
            ++i;
        } while (std::abs(term) > 1e-100);
        den += 0.5;
        const double ww = num / den;
        const double wwsq = ww * ww;
        const double x = std::sqrt((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
        coefficients[index] = (1.0 - x) / (1.0 + x);
    }
    return coefficients;
}

inline const std::array<float, kCoefficients>& coefficients() {
    static const auto table = [] {
        const auto designed = designCoefficients();
        std::array<float, kCoefficients> values{};
        for (int i = 0; i < kCoefficients; ++i) values[i] = static_cast<float>(designed[i]);
        return values;
    }();
    return table;
}

// One base-rate sample in, two 2x-rate samples out.
class Upsampler {
public:
    void reset() { x.fill(0.0f); y.fill(0.0f); }

    void process(float input, float& first, float& second) {
        const auto& c = coefficients();
        float even = input, odd = input;
        for (int i = 0; i < kCoefficients; i += 2) {
            const float t0 = (even - y[i]) * c[i] + x[i];
            x[i] = even; y[i] = t0; even = t0;
            const float t1 = (odd - y[i + 1]) * c[i + 1] + x[i + 1];
            x[i + 1] = odd; y[i + 1] = t1; odd = t1;
        }
        first = even;
        second = odd;
    }

private:
    std::array<float, kCoefficients> x{}, y{};
};

// Two 2x-rate samples in (the earlier first), one base-rate sample out.
class Downsampler {
public:
    void reset() { x.fill(0.0f); y.fill(0.0f); }

    float process(float first, float second) {
        const auto& c = coefficients();
        float a = second, b = first;
        for (int i = 0; i < kCoefficients; i += 2) {
            const float t0 = (a - y[i]) * c[i] + x[i];
            x[i] = a; y[i] = t0; a = t0;
            const float t1 = (b - y[i + 1]) * c[i + 1] + x[i + 1];
            x[i + 1] = b; y[i + 1] = t1; b = t1;
        }
        return 0.5f * (a + b);
    }

private:
    std::array<float, kCoefficients> x{}, y{};
};

} // namespace halfband
