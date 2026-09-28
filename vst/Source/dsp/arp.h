// ==============================================================================
// Overviber / GliGli Overcycler - Arpeggiator & Pattern Sequencer Engine
//
// Core Firmware Origin:
//   GliGli Overcycler Hardware Synthesizer Firmware (firmware_17xx/synth/arp.c)
//   Original Author & Copyright (C) 2018-2024 GliGli (http://gliglisynth.blogspot.com/)
//   Licensed under the GNU General Public License v3.0 (GPL-3.0).
//
// Extended Arpeggiator & Chord Concepts:
//   Chord Degree alignment mode (amDegree) and Polyphonic Strum mode (amStrum)
//   adapted from the Arpligner concept by Yves Parès:
//   https://github.com/YPares/arpligner
//   Copyright (C) 2023 Yves Parès (Mozilla Public License 2.0 with Commons Clause).
//
// Modern C++ Implementation:
//   Up/Down modes, Poly Chord, Converge, 16-step rhythm pattern sequencer,
//   dynamic degree wraparound, tempo-sync & interactive GUI visualizer matrix.
//   Copyright (C) 2026 Overviber Contributors
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
#include <functional>
#include <cstdint>
#include <algorithm>
#include "FixedBuffer.h"

#define ARP_NOTE_MEMORY 128
#define ARP_LAST_NOTE (ARP_NOTE_MEMORY - 1)

class Arpeggiator {
public:
    using NoteAssignFn = std::function<void(uint8_t note, int8_t gate, uint16_t velocity, uint8_t channel)>;
    using LegacyNoteAssignFn = std::function<void(uint8_t note, int8_t gate, uint16_t velocity)>;

    Arpeggiator();
    void init();
    void setNoteAssignCallback(NoteAssignFn cb) { assignCallback = cb; }
    void setNoteAssignCallback(LegacyNoteAssignFn cb) {
        assignCallback = [callback = std::move(cb)](uint8_t note, int8_t gate, uint16_t velocity, uint8_t) {
            callback(note, gate, velocity);
        };
    }

    void setMode(arpMode_t mode, int8_t hold);
    void setOctaves(uint8_t octaves);       // 1..4 (default 1)
    void setRate(uint8_t rateIndex);        // 0..5 (1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32)
    void setGateLength(float fraction);     // 0.10f .. 1.0f (default 0.833f)
    void setSwing(float swingFraction);     // 0.50f .. 0.75f (default 0.50f)
    void setTranspose(int8_t transpose);
    int8_t getTranspose() const { return transpose; }

    arpMode_t getMode() const { return mode; }
    int8_t getHold() const { return hold; }
    uint8_t getOctaves() const { return octaves; }
    uint8_t getRate() const { return rateIndex; }
    float getGateLength() const { return gateFraction; }
    float getSwing() const { return swingFraction; }

    int8_t assignNote(uint8_t note, int8_t on, uint16_t velocity = HALF_RANGE, uint8_t channel = 1);
    void clockTick(); // Called on beat/clock ticks to step the arpeggio
    void finishPreviousNote();
    void allNotesOff();
    void resetCounter();

    bool hasActiveNotes() const { return !isEmpty(); }
    uint8_t getLastPlayedNote() const { return previousNote != ASSIGNER_NO_NOTE ? previousNote : 0; }
    int16_t getCurrentNoteIndex() const { return stepIndex; }
    int32_t getStepCount() const { return stepCounter; }

    // 16-Step Rhythm Pattern Sequencer (0=Play, 1=Accent, 2=Tie, 3=Mute)
    uint8_t getStepPattern(int step) const { return (step >= 0 && step < 16) ? stepPattern[step] : 0; }
    void setStepPattern(int step, uint8_t type) { if (step >= 0 && step < 16) stepPattern[step] = type % 4; }
    void cycleStepPattern(int step) { if (step >= 0 && step < 16) stepPattern[step] = (stepPattern[step] + 1) % 4; }

    // 16-Step Chord Degree Sequencer (Arpligner Mode: 0=Root, 1=2nd tone, 2=3rd tone, etc. with smart wraparound)
    uint8_t getStepDegree(int step) const { return (step >= 0 && step < 16) ? stepDegrees[step] : 0; }
    void setStepDegree(int step, uint8_t deg) { if (step >= 0 && step < 16) stepDegrees[step] = deg % 12; }
    void cycleStepDegree(int step) { if (step >= 0 && step < 16) stepDegrees[step] = (stepDegrees[step] + 1) % 12; }

    // Returns unique active notes (sorted for Up/Down/UpDown/Random/Degree, entry order for Assign)
    int getActiveNotes(uint8_t* outNotes, int maxNotes) const;

    // Computes the note sequence for maxSteps (e.g. 16 steps) for display in visualizer
    int getPattern(uint8_t* outNotes, int maxSteps) const;

    bool isGateActive() const { return gateState != 0; }
    bool isNextStepTie() const { return gateState != 0 && stepPattern[stepCounter % 16] == 2; }

    // Timing helper: ticks per step for current rate (at ~250 Hz ticker)
    uint32_t getStepDivisionTicks() const;

private:
    struct ArpNote {
        uint8_t note = ASSIGNER_NO_NOTE;
        uint8_t channel = 1;
        uint16_t velocity = HALF_RANGE;
        bool held = false;
    };

    uint32_t randomState = 33;
    int8_t isEmpty() const;
    void killAllNotes();
    void killHeldNotes();
    bool hasOnlyHeldNotes() const;
    int collectActiveNotes(ArpNote* outNotes, int maxNotes, bool preserveOrder) const;
    void emitNote(const ArpNote& source, int octaveOffset, uint16_t velocity);

    FixedBuffer<ArpNote, ARP_NOTE_MEMORY> notes;

    int16_t stepIndex;
    int32_t stepCounter;
    uint8_t previousNote;
    int8_t transpose;
    int8_t hold;
    int8_t gateState;
    arpMode_t mode;

    uint8_t octaves = 1;
    uint8_t rateIndex = 3; // 1/16th note division
    float gateFraction = 0.833f;
    float swingFraction = 0.50f;
    uint8_t stepPattern[16] = {0};
    uint8_t stepDegrees[16] = {0, 1, 2, 0,  1, 2, 3, 1,  2, 3, 0, 2,  3, 0, 1, 2};
    FixedBuffer<ArpNote, ARP_NOTE_MEMORY * 4> previousOutputNotes;

    NoteAssignFn assignCallback;
};
