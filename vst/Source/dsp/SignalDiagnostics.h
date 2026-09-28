#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Offline-only probes. The product targets do not define OVERVIBER_DIAGNOSTICS.
struct SignalStats {
    double peak = 0, squares = 0;
    uint64_t samples = 0, nonFinite = 0, overUnity = 0;
    void add(float x) {
        ++samples;
        if (!std::isfinite(x)) { ++nonFinite; return; }
        peak = std::max(peak, std::abs(static_cast<double>(x)));
        squares += static_cast<double>(x) * x;
        if (std::abs(x) > 1) ++overUnity;
    }
    double rms() const { return samples ? std::sqrt(squares / samples) : 0; }
};
struct VoiceDiagnostics {
    SignalStats oscillator, mixed, filtered, vca;
};
struct RenderDiagnostics {
    VoiceDiagnostics voices[6];
    SignalStats busLeft, busRight, consoleLeft, consoleRight, outputLeft, outputRight;
};
