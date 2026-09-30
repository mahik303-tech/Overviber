// ==============================================================================
// Overviber / GliGli Overcycler - Anti-Aliased Wavetable Oscillator
//
// Origin:
//   GliGli Overcycler Hardware Synthesizer Firmware (firmware_17xx/synth/wtosc.c)
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

class WtOsc {
public:
    WtOsc();
    void init(int8_t channel);
    void setSampleData(const uint16_t* mainData, const uint16_t* xovrData);
    void setParameters(uint16_t pitch, oscWModTarget_t wmType, uint16_t wmAmount);

    // Process a single sample
    // tickStep: time step corresponding to the sample rate (e.g. 120000000.0 / sampleRate)
    uint16_t processSample(uint32_t tickStep, oscSyncMode_t syncMode, int16_t* syncPosition);

    void setSampleRate(float sr);

    int8_t getChannel() const { return channel; }
    uint16_t getPitch() const { return pitch; }

private:
    float maxVirtualRate;   // the wave's highest playback rate (setSampleRate)
    static const int WIDTH_MOD_BITS = 14;
    static const int FRAC_SHIFT = 12;

    uint32_t cvToFrequency(uint32_t cv);
    void updatePeriodIncrement(int8_t type);
    void handlePhaseUnderflow(oscSyncMode_t syncMode, int16_t* syncPosition);
    void handleSlaveSync(int16_t* syncPosition);

    int32_t handleCounterUnderflow_wmOff(oscSyncMode_t syncMode, int16_t* syncPosition);
    int32_t handleCounterUnderflow_wmAliasing(oscSyncMode_t syncMode, int16_t* syncPosition);
    int32_t handleCounterUnderflow_wmWidth(oscSyncMode_t syncMode, int16_t* syncPosition);
    int32_t handleCounterUnderflow_wmCrossOver(oscSyncMode_t syncMode, int16_t* syncPosition);
    int32_t handleCounterUnderflow_wmFolder(oscSyncMode_t syncMode, int16_t* syncPosition);
    int32_t handleCounterUnderflow_wmBitCrush(oscSyncMode_t syncMode, int16_t* syncPosition);

    const uint16_t* mainData;
    const uint16_t* crossoverData;

    int32_t period[2], pendingPeriod[2];
    // (1 << 2 x FRAC_SHIFT) / period (a period <= 0 counting as 1), kept
    // with period[]: the interpolation needs it every sample, the period
    // changes rarely, and a division costs 10 to 25 cycles.
    int32_t periodDiv[2];
    void updatePeriodDivs();
    int32_t increment[2], pendingIncrement[2];

    int32_t counter;
    int32_t phase;

    int32_t curSample, prevSample, prevSample2, prevSample3;

    int32_t aliasing;
    int32_t folder;
    int32_t bitcrush;
    uint16_t pitch;
    uint16_t width;
    uint16_t crossover;

    oscWModTarget_t wmType;
    int8_t channel;
    int8_t pendingUpdate;
};
