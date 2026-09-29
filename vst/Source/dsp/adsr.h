// ==============================================================================
// Overviber / GliGli Overcycler - Analog ADSR Envelope Generator
//
// Origin:
//   GliGli Overcycler Hardware Synthesizer Firmware (firmware_17xx/synth/adsr.c)
//   Original Author & Copyright (C) 2018-2024 GliGli (http://gliglisynth.blogspot.com/)
//
// License:
//   This program is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#pragma once

#include "OvercyclerTypes.h"
#include "LookupTables.h"
#include <cmath>

class AdsrEnv {
public:
    AdsrEnv();
    void init();
    void reset();

    void setCVs(uint16_t atk, uint16_t dec, uint16_t sus, uint16_t rls, uint16_t lvl, uint8_t mask);
    void setGate(int8_t gate);
    void setShape(int8_t isExp, int8_t isLoop);
    void setSpeedShift(int8_t shift);

    adsrStage_t getStage() const { return stage; }
    uint16_t getOutput() const { return output; }

    void update();

private:
    uint32_t getPhaseInc(uint8_t v);
    void updateStageVars(adsrStage_t s);
    void updateIncrements();
    void handlePhaseOverflow();
    uint16_t computeOutput(uint32_t phase, const uint16_t lookup[], int8_t isExp);

    uint32_t stageIncrement;
    uint32_t phase;
    uint32_t attackIncrement, decayIncrement, releaseIncrement;

    uint16_t sustainCV, levelCV;
    uint16_t attackCV, decayCV, releaseCV;
    uint16_t stageLevel, stageAdd, stageMul;
    uint16_t output;

    int8_t expOutput, gate, loop;
    int8_t speedShift;

    adsrStage_t stage;
};

// ---- Stage durations
// Speed shift of the normal and the "slow" range, as the firmware sets it
// (synth.c refreshEnvSettings).
inline int8_t adsrSpeedShift(bool slow) { return slow ? 4 : 2; }

// The time CVs (attack, decay, release) follow the hardware's strongly
// exponential curve: in the normal range about 125 ms at half scale and
// 12.5 s at full scale; the "slow" range is four times longer. Envelopes
// update at DACSPI_UPDATE_HZ; a stage runs the phase from 0 to 2^24.
inline float adsrStageMilliseconds(uint16_t cv, bool slow = false) {
    const int speedShift = adsrSpeedShift(slow);
    const uint8_t v = static_cast<uint8_t>(cv >> 8);
    const uint32_t base = (uint32_t)phaseLookupLo[v] | ((uint32_t)phaseLookupMid[v] << 8)
                        | ((uint32_t)phaseLookupHi[v] << 16);
    const uint32_t increment = (base >> speedShift) << 4;
    if (increment == 0) return 0.0f;
    return 16777216.0f / (float)increment / (float)DACSPI_UPDATE_HZ * 1000.0f;
}

// Time CV whose stage duration is closest to `ms` (compared on a log scale).
inline uint16_t adsrCVForMilliseconds(float ms, bool slow = false) {
    if (ms <= 0.0f) return 0;
    int best = 0;
    float bestError = 1e30f;
    for (int v = 0; v < 256; ++v) {
        const float t = adsrStageMilliseconds(static_cast<uint16_t>(v << 8), slow);
        const float error = std::fabs(std::log((t + 0.01f) / (ms + 0.01f)));
        if (error < bestError) { bestError = error; best = v; }
    }
    return static_cast<uint16_t>((best << 8) | 0x80);
}
