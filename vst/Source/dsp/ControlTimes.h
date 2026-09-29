#pragma once

#include "OvercyclerTypes.h"

// ==============================================================================
// Glide and modulation delay, as in the Overcycler firmware (synth.c). Both run
// on the firmware's 500 Hz control tick, independent of tempo and transport.
// ==============================================================================
namespace controltimes {

inline constexpr int kTickHz = 500;                               // firmware TICKER_HZ
inline constexpr int kCvUpdatesPerTick = DACSPI_UPDATE_HZ / kTickHz;

// Glide moves the note CVs by a fixed amount per tick; it is off above 2000.
inline uint16_t glideAmount(uint16_t cv) { return exponentialCourse(cv, 11000.0f, 2100.0f); }
inline bool glideEnabled(uint16_t cv) { return glideAmount(cv) < 2000; }

// Duration of a one-octave glide, 0 when glide is off.
inline float glideOctaveMilliseconds(uint16_t cv) {
    const uint16_t amount = glideAmount(cv);
    if (amount >= 2000 || amount == 0) return 0.0f;
    return 12.0f * WTOSC_CV_SEMITONE / amount / kTickHz * 1000.0f;
}

// LFO 1 waits this many ticks after the first key press, then fades in over
// the same number of ticks. Off below the pot dead zone.
inline bool modDelayEnabled(uint16_t cv) { return cv >= SCAN_POT_DEAD_ZONE; }
inline uint16_t modDelayTicks(uint16_t cv) { return exponentialCourse(UINT16_MAX - cv, 12000.0f, 2500.0f); }
inline float modDelayMilliseconds(uint16_t cv) {
    return modDelayEnabled(cv) ? modDelayTicks(cv) * 1000.0f / kTickHz : 0.0f;
}

} // namespace controltimes
