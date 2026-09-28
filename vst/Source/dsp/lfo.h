// ==============================================================================
// Overviber / GliGli Overcycler - Low Frequency Oscillator (LFO) Engine
//
// Origin:
//   GliGli Overcycler Hardware Synthesizer Firmware (firmware_17xx/synth/lfo.c)
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

class LfoModule {
public:
    LfoModule();
    void init();
    void reset();

    void setCVs(uint16_t bpm, uint16_t lvl);
    void setShape(lfoShape_t shape, uint8_t halfPeriods = 0);
    void setSpeedShift(int8_t shift);

    int16_t getOutput() const { return output; }
    uint16_t getLevelCV() const { return levelCV; }

    void update();

private:
    void updateIncrement();
    void updateSpeed();
    void handlePhaseOverflow();

    uint32_t noise;
    uint32_t halfPeriodCounter;
    uint32_t phase;
    int32_t speed;
    int32_t increment;

    uint16_t levelCV, bpmCV;
    int16_t output;

    int8_t speedShift;
    lfoShape_t shape;
    uint8_t halfPeriodLimit;
};
