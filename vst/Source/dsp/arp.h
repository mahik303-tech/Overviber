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

// The arp's step sequencer, a session state (not preset data): 16 step
// types (0 play, 1 accent, 2 tie, 3 mute), 16 chord degrees for Chord
// Degree and Strum, and the keyboard transpose. The editor's model edits
// it; the audio engine's arp plays it.
struct ArpSequence {
    uint8_t pattern[16] = {0};
    uint8_t degrees[16] = {0, 1, 2, 0,  1, 2, 3, 1,  2, 3, 0, 2,  3, 0, 1, 2};
    int8_t transpose = 0;

    uint8_t getStepPattern(int step) const { return (step >= 0 && step < 16) ? pattern[step] : 0; }
    void setStepPattern(int step, uint8_t type) { if (step >= 0 && step < 16) pattern[step] = type % 4; }
    void cycleStepPattern(int step) { if (step >= 0 && step < 16) pattern[step] = (pattern[step] + 1) % 4; }
    uint8_t getStepDegree(int step) const { return (step >= 0 && step < 16) ? degrees[step] : 0; }
    void setStepDegree(int step, uint8_t deg) { if (step >= 0 && step < 16) degrees[step] = deg % 12; }
    void cycleStepDegree(int step) { if (step >= 0 && step < 16) degrees[step] = (degrees[step] + 1) % 12; }
};

// Arp parameters (16-bit CV) to their values: gate 10 .. 100 % of a step,
// swing 50 .. 75 %, internal tempo 20 .. 300 BPM.
inline float arpGateFraction(uint16_t cv) {
    return std::clamp((float)scan_potFrom16bits(cv) / 999.0f, 0.10f, 1.0f);
}
inline float arpSwingFraction(uint16_t cv) {
    return std::clamp(0.50f + ((float)scan_potFrom16bits(cv) - 500.0f) * (0.25f / 250.0f), 0.50f, 0.75f);
}
inline float arpInternalBpm(uint16_t cv) {
    return 20.0f + ((float)scan_potFrom16bits(cv) / 999.0f) * 280.0f;
}

// Ticks (48 PPQ) per step of the arp rates 0..5: 1/4, 1/8, 1/8T, 1/16,
// 1/16T, 1/32.
uint32_t arpStepTicks(uint8_t rateIndex);

// One note a step plays: an index into the held notes (sorted, or in entry
// order for As Played) and an octave above it.
struct ArpPick {
    int note = 0;
    int octave = 0;
};

// The note choice of every arp mode, shared by playback (clockTick) and the
// matrix preview (getPattern). `stepIndex` counts the mode's steps,
// `stepInPattern` (0..15) selects the degree for Chord Degree and Strum.
// Random takes `randomValue` (playback: the arp's generator; preview: a
// fixed hash). Chord picks every held note, Strum two (the second two
// degrees higher), all other modes one. `out` holds at least
// max(noteCount, 2) picks; returns their number (0 for Off).
int arpPicks(arpMode_t mode, int stepIndex, int stepInPattern, int noteCount, int octaves,
             const uint8_t* degrees, uint32_t randomValue, ArpPick* out);

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
    int8_t getTranspose() const { return sequence.transpose; }

    arpMode_t getMode() const { return mode; }
    int8_t getHold() const { return hold; }
    uint8_t getOctaves() const { return octaves; }
    uint8_t getRate() const { return rateIndex; }
    float getGateLength() const { return gateFraction; }
    float getSwing() const { return swingFraction; }

    int8_t assignNote(uint8_t note, int8_t on, uint16_t velocity = HALF_RANGE, uint8_t channel = 1);

    // ---- Clock. Positions are 48 PPQ ticks, fractional: the engine calls
    // advance() with the position of each sample it starts a segment on and
    // splits its segments at nextEvent(), so every arp event lands on its
    // sample. Steps lie on a grid of step length (setRate) from the grid
    // origin, every second one late by the swing (0.5 .. 0.75 of a pair of
    // steps); a step's notes end after gate x step length; Poly Strum's
    // second note follows a quarter step later.
    // Plays every event up to `window` after `position`, in time order; the
    // grid (after a relocation) starts `window` before it. The engine passes
    // half a sample, so each event plays on its nearest sample: an event a
    // hair off a sample boundary (swing, gate) lands on the same sample on
    // every platform, whatever the last bit of its position. An event up to
    // kTimeTolerance beyond the window counts as in it.
    void advance(double position, double window = 0.0);
    static constexpr double kTimeTolerance = 1e-7;
    // Position of the next event, infinity while the arp is off.
    double nextEvent() const;
    // The host transport jumped (loop, locate, start): the grid continues
    // from `position` (a step exactly there plays); a sounding note keeps
    // its remaining length.
    void relocate(double position);
    // Free running (internal tempo, or a stopped or absent host transport):
    // the first key of an empty arp starts the grid at once, as the
    // firmware's beat reset. Synced, the grid starts at song position 0.
    void setFreeRunning(bool value);
    bool isFreeRunning() const { return freeRunning; }
    // advance() on a whole tick (tests).
    void clock(uint32_t tick) { advance(static_cast<double>(tick)); }
    // Transport stop: the sounding step ends now.
    void stopClock();
    void clockTick(); // plays one step now (advance() calls it on the grid)
    void finishPreviousNote();
    void allNotesOff();
    void resetCounter();

    bool hasActiveNotes() const { return !isEmpty(); }
    uint8_t getLastPlayedNote() const { return previousNote != ASSIGNER_NO_NOTE ? previousNote : 0; }
    int16_t getCurrentNoteIndex() const { return stepIndex; }
    int32_t getStepCount() const { return stepCounter; }

    // The step sequencer (see ArpSequence).
    uint8_t getStepPattern(int step) const { return sequence.getStepPattern(step); }
    void setStepPattern(int step, uint8_t type) { sequence.setStepPattern(step, type); }
    uint8_t getStepDegree(int step) const { return sequence.getStepDegree(step); }
    void setStepDegree(int step, uint8_t deg) { sequence.setStepDegree(step, deg); }

    // Returns unique active notes (sorted for Up/Down/UpDown/Random/Degree, entry order for Assign)
    int getActiveNotes(uint8_t* outNotes, int maxNotes) const;

    // Computes the note sequence for maxSteps (e.g. 16 steps) for display in visualizer
    int getPattern(uint8_t* outNotes, int maxSteps) const;

    bool isGateActive() const { return gateState != 0; }
    bool isNextStepTie() const { return gateState != 0 && sequence.pattern[stepCounter % 16] == 2; }

    // Ticks (48 PPQ) per step of the current rate.
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
    int8_t hold;
    int8_t gateState;
    arpMode_t mode;

    uint8_t octaves = 1;
    uint8_t rateIndex = 3; // 1/16th note division
    float gateFraction = 0.833f;
    float swingFraction = 0.50f;
    // Clock state (positions in ticks, see advance()).
    static constexpr double kNever = 1e300;
    bool freeRunning = false;
    bool restartPending = false;     // free running: the next advance() starts the grid
    bool scheduled = false;          // nextStepAt is valid
    double gridOrigin = 0.0;
    double nextStepAt = 0.0;
    double lastPosition = 0.0;       // of the latest advance()
    double gateCloseAt = kNever;     // the sounding step ends here
    double stepAtOrAfter(double position, bool inclusive) const;
    void reschedule();
    void playStep(double at);
    // Strum: the second note of a step, played strumDelay after the step.
    struct PendingNote {
        bool valid = false;
        ArpNote source;
        int octaveOffset = 0;
        uint16_t velocity = 0;
        double dueAt = 0.0;
    } pendingStrum;
    double clockNow = 0.0;
    double strumDelay = 0.0;         // 0: clockTick() called directly, strum at once
    ArpSequence sequence;
    FixedBuffer<ArpNote, ARP_NOTE_MEMORY * 4> previousOutputNotes;

    NoteAssignFn assignCallback;
};
