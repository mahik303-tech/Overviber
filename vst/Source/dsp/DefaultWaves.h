#pragma once

#include "OvercyclerTypes.h"
#include <cmath>

// Built-in single-cycle waves inside the guard band, used when no wave file
// is loaded: oscillators A/B start with the saw, the crossover waves with
// the sine. Pure computation, usable by the editor model and the engine.
namespace defaultwaves {

enum class Shape { Saw, Sine, Square, Triangle };

inline void generate(Shape shape, uint16_t* out) {
    const float guard = (float)WTOSC_SAMPLES_GUARD_BAND;
    const float range = (float)(UINT16_MAX - 2 * WTOSC_SAMPLES_GUARD_BAND);
    for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
        const float phase = (float)i / (float)WTOSC_SAMPLE_COUNT;
        float value = 0.0f;
        switch (shape) {
        case Shape::Saw: value = 1.0f - 2.0f * phase; break;
        case Shape::Sine: value = std::sin(2.0f * 3.14159265358979323846f * phase); break;
        case Shape::Square: value = (phase < 0.5f) ? 1.0f : -1.0f; break;
        case Shape::Triangle:
            value = (phase < 0.25f) ? (4.0f * phase) :
                    (phase < 0.75f) ? (2.0f - 4.0f * phase) :
                    (-4.0f + 4.0f * phase);
            break;
        }
        out[i] = (uint16_t)(guard + (value * 0.5f + 0.5f) * range);
    }
}

// Default wave of an oscillator slot (A/B main: saw, crossover: sine).
inline Shape forSlot(int abx) {
    return abx == abxAMain || abx == abxBMain ? Shape::Saw : Shape::Sine;
}

} // namespace defaultwaves
