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
