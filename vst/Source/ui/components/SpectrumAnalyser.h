#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

// Spectrum of the master output for the filter curve: a 2048-point FFT with
// a Hann window, reduced to one level (dBFS) per display column on a
// logarithmic frequency axis (20 Hz .. 20 kHz).
class SpectrumAnalyser {
public:
    static constexpr int kOrder = 11;
    static constexpr int kSize = 1 << kOrder;   // 2048 samples
    static constexpr float kFloorDb = -90.0f;

    SpectrumAnalyser() {
        for (int i = 0; i < kSize; ++i)
            window[(size_t)i] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265358979f * (float)i / (float)(kSize - 1));
    }

    // Levels per column for `samples` (kSize values) at `sampleRate`.
    void analyse(const float* samples, double sampleRate, int columns, std::vector<float>& levels) {
        for (int i = 0; i < kSize; ++i) bins[(size_t)i] = { samples[i] * window[(size_t)i], 0.0f };
        fft();
        levels.assign((size_t)std::max(0, columns), kFloorDb);
        const float binHz = (float)sampleRate / (float)kSize;
        // A full-scale sine reads 0 dB: Hann-windowed peak is kSize / 4.
        const float norm = 4.0f / (float)kSize;
        std::vector<float> raw((size_t)std::max(0, columns), kFloorDb);
        for (int c = 0; c < columns; ++c) {
            const float f0 = 20.0f * std::pow(1000.0f, (float)c / (float)columns);
            const float f1 = 20.0f * std::pow(1000.0f, (float)(c + 1) / (float)columns);
            float magnitude = 0.0f;
            if (f1 - f0 < binHz) {
                // Narrower than a bin (the bass): interpolate at the centre.
                const float pos = std::clamp(0.5f * (f0 + f1) / binHz, 1.0f, (float)(kSize / 2 - 2));
                const int b = (int)pos;
                const float frac = pos - (float)b;
                magnitude = std::abs(bins[(size_t)b]) * (1.0f - frac) + std::abs(bins[(size_t)b + 1]) * frac;
            } else {
                const int b0 = std::max(1, (int)std::floor(f0 / binHz));
                const int b1 = std::min(kSize / 2 - 1, std::max(b0, (int)std::ceil(f1 / binHz)));
                for (int b = b0; b <= b1; ++b) magnitude = std::max(magnitude, std::abs(bins[(size_t)b]));
            }
            raw[(size_t)c] = std::max(kFloorDb, 20.0f * std::log10(std::max(magnitude * norm, 1.0e-9f)));
        }
        // Light smoothing over neighbouring columns: calmer than single
        // harmonics, the shape stays.
        for (int c = 0; c < columns; ++c) {
            float sum = 0.0f;
            int count = 0;
            for (int d = -1; d <= 1; ++d) {
                const int i = c + d;
                if (i >= 0 && i < columns) { sum += raw[(size_t)i]; ++count; }
            }
            levels[(size_t)c] = sum / (float)count;
        }
    }

private:
    // In-place radix-2 FFT.
    void fft() {
        for (int i = 1, j = 0; i < kSize; ++i) {
            int bit = kSize >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(bins[(size_t)i], bins[(size_t)j]);
        }
        for (int len = 2; len <= kSize; len <<= 1) {
            const float angle = -2.0f * 3.14159265358979f / (float)len;
            const std::complex<float> step(std::cos(angle), std::sin(angle));
            for (int i = 0; i < kSize; i += len) {
                std::complex<float> w(1.0f, 0.0f);
                for (int k = 0; k < len / 2; ++k) {
                    const auto u = bins[(size_t)(i + k)];
                    const auto v = bins[(size_t)(i + k + len / 2)] * w;
                    bins[(size_t)(i + k)] = u + v;
                    bins[(size_t)(i + k + len / 2)] = u - v;
                    w *= step;
                }
            }
        }
    }

    std::array<float, kSize> window{};
    std::array<std::complex<float>, kSize> bins{};
};
