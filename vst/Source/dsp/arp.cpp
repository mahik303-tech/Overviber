// ==============================================================================
// Overviber / GliGli Overcycler - Arpeggiator & Pattern Sequencer Implementation
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

#include "arp.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cmath>

namespace {
uint8_t clampMidiNote(int note) {
    return static_cast<uint8_t>(std::clamp(note, 0, 127));
}

// Accent steps play 45 % louder.
uint16_t accentVelocity(uint16_t velocity) {
    return static_cast<uint16_t>(std::min(65535, static_cast<int>(velocity * 1.45f)));
}
}

int arpPicks(arpMode_t mode, int stepIndex, int stepInPattern, int noteCount, int octaves,
             const uint8_t* degrees, uint32_t randomValue, ArpPick* out) {
    if (noteCount <= 0 || !out) return 0;
    const int octCount = std::max(1, octaves);
    const int poolSize = noteCount * octCount;   // held notes, then again one octave up, ...
    auto fromPool = [&](int index) { return ArpPick{ index % noteCount, index / noteCount }; };
    // Degrees past the chord go an octave up, within the arp's octaves: with
    // one octave everything stays in the chord's register.
    auto fromDegree = [&](int degree) { return ArpPick{ degree % noteCount, (degree / noteCount) % octCount }; };

    switch (mode) {
    case amUp:
        out[0] = fromPool(stepIndex % poolSize);
        return 1;
    case amDown:
        out[0] = fromPool(poolSize - 1 - stepIndex % poolSize);
        return 1;
    case amUpDown: {
        int index = 0;
        if (poolSize == 2) {
            index = stepIndex % 2;
        } else if (poolSize > 2) {
            const int cycleLen = poolSize * 2 - 2;
            const int phase = stepIndex % cycleLen;
            index = phase < poolSize ? phase : cycleLen - phase;
        }
        out[0] = fromPool(index);
        return 1;
    }
    case amAssign:
        out[0] = { stepIndex % noteCount, (stepIndex / noteCount) % octCount };
        return 1;
    case amRandom:
        out[0] = fromPool(static_cast<int>(randomValue % static_cast<uint32_t>(poolSize)));
        return 1;
    case amChord:
        for (int i = 0; i < noteCount; ++i) out[i] = { i, stepIndex % octCount };
        return noteCount;
    case amConverge: {
        const int index = stepIndex % poolSize;
        out[0] = fromPool(index % 2 == 0 ? index / 2 : poolSize - 1 - index / 2);
        return 1;
    }
    case amDegree:
        out[0] = fromDegree(degrees[stepInPattern % 16]);
        return 1;
    case amStrum: {
        const int degree = degrees[stepInPattern % 16];
        out[0] = fromDegree(degree);
        if (noteCount == 1) return 1;
        out[1] = fromDegree(degree + 2);
        return 2;
    }
    default:
        return 0;
    }
}

Arpeggiator::Arpeggiator() {
    init();
}

void Arpeggiator::init() {
    notes.clear();
    stepIndex = 0;
    stepCounter = 0;
    previousNote = ASSIGNER_NO_NOTE;
    previousOutputNotes.clear();
    sequence.transpose = 0;
    hold = 0;
    gateState = 0;
    gateCloseAt = kNever;
    scheduled = false;
    restartPending = false;
    gridOrigin = 0.0;
    pendingStrum.valid = false;
    mode = amOff;
    octaves = 1;
    rateIndex = 3; // 1/16th
    gateFraction = 0.833f;
    swingFraction = 0.50f;
    std::memset(sequence.pattern, 0, sizeof(sequence.pattern));
}

int8_t Arpeggiator::isEmpty() const {
    return notes.empty() ? 1 : 0;
}

bool Arpeggiator::hasOnlyHeldNotes() const {
    if (notes.empty()) return false;
    for (const auto& note : notes) if (!note.held) return false;
    return true;
}

void Arpeggiator::finishPreviousNote() {
    if (gateState) {
        for (const auto& played : previousOutputNotes) {
            if (assignCallback) assignCallback(played.note, 0, 0, played.channel);
        }
    }
    previousOutputNotes.clear();
    pendingStrum.valid = false;
    gateState = 0;
}

void Arpeggiator::killAllNotes() {
    finishPreviousNote();
    stepIndex = 0;
    stepCounter = 0;
    previousNote = ASSIGNER_NO_NOTE;
    previousOutputNotes.clear();
    notes.clear();
}

void Arpeggiator::allNotesOff() {
    killAllNotes();
}

void Arpeggiator::killHeldNotes() {
    FixedBuffer<ArpNote, ARP_NOTE_MEMORY> remaining;
    for (const auto& note : notes) if (!note.held) remaining.push_back(note);
    notes = remaining;
    if (isEmpty()) finishPreviousNote();
}

void Arpeggiator::setMode(arpMode_t newMode, int8_t newHold) {
    if (newMode != mode) {
        const bool preserveNoteMemory = (newMode == amRandom && mode == amAssign)
            || (mode == amRandom && newMode == amAssign);
        if (preserveNoteMemory) {
            // These two modes share their note memory, but the note currently
            // sounding still has to be released immediately on a mode change.
            finishPreviousNote();
            resetCounter();
        } else {
            killAllNotes();
            if (newMode != amOff) resetCounter();
        }
    }

    if (!newHold && hold) {
        killHeldNotes();
    }

    mode = newMode;
    hold = newHold;
}

void Arpeggiator::setOctaves(uint8_t octs) {
    octaves = std::clamp((uint8_t)octs, (uint8_t)1, (uint8_t)4);
}

void Arpeggiator::setRate(uint8_t rIdx) {
    const auto rate = std::clamp((uint8_t)rIdx, (uint8_t)0, (uint8_t)5);
    if (rate == rateIndex) return;
    rateIndex = rate;
    reschedule();
}

void Arpeggiator::setGateLength(float fraction) {
    gateFraction = std::clamp(fraction, 0.10f, 1.0f);
}

void Arpeggiator::setSwing(float swing) {
    swing = std::clamp(swing, 0.50f, 0.75f);
    if (std::abs(swing - swingFraction) < 1e-6f) return;   // the same value, set again
    swingFraction = swing;
    reschedule();
}

void Arpeggiator::setTranspose(int8_t t) {
    sequence.transpose = t;
}

void Arpeggiator::resetCounter() {
    stepIndex = 0;
    stepCounter = 0;
    if (freeRunning) restartPending = true;
}

uint32_t Arpeggiator::getStepDivisionTicks() const {
    return arpStepTicks(rateIndex);
}

uint32_t arpStepTicks(uint8_t rateIndex) {
    switch (rateIndex) {
    case 0: return 48; // 1/4
    case 1: return 24; // 1/8
    case 2: return 16; // 1/8T
    case 3: return 12; // 1/16
    case 4: return 8;  // 1/16T
    case 5: return 6;  // 1/32
    default: return 12;
    }
}

int8_t Arpeggiator::assignNote(uint8_t note, int8_t on, uint16_t velocity, uint8_t channel) {
    if (mode == amOff || note >= ARP_NOTE_MEMORY) return 0;
    channel = std::clamp<uint8_t>(channel, 1, 16);

    int8_t handled = 0;

    if (on) {
        if (hold && hasOnlyHeldNotes()) {
            killHeldNotes();
        }

        if (isEmpty()) resetCounter();

        bool alreadyAssigned = false;
        for (auto& active : notes) {
            if (active.note == note && active.channel == channel) {
                active.velocity = velocity;
                active.held = false;
                alreadyAssigned = true;
                break;
            }
        }
        handled = alreadyAssigned || notes.push_back({note, channel, velocity, false});
    } else {
        if (hold) {
            for (auto& active : notes) {
                if (active.note == note && active.channel == channel) {
                    active.held = true;
                    handled = 1;
                }
            }
        } else {
            FixedBuffer<ArpNote, ARP_NOTE_MEMORY> remaining;
            for (const auto& active : notes) {
                if (active.note == note && active.channel == channel) handled = 1;
                else remaining.push_back(active);
            }
            notes = remaining;

            if (isEmpty()) finishPreviousNote();
        }
    }

    return handled;
}

int Arpeggiator::collectActiveNotes(ArpNote* outNotes, int maxNotes, bool preserveOrder) const {
    if (!outNotes || maxNotes <= 0) return 0;
    const int count = std::min<int>(static_cast<int>(notes.size()), maxNotes);
    std::copy_n(notes.begin(), count, outNotes);
    if (!preserveOrder) {
        std::sort(outNotes, outNotes + count, [](const ArpNote& a, const ArpNote& b) {
            return a.note != b.note ? a.note < b.note : a.channel < b.channel;
        });
    }
    return count;
}

int Arpeggiator::getActiveNotes(uint8_t* outNotes, int maxNotes) const {
    if (!outNotes || maxNotes <= 0) return 0;
    ArpNote active[ARP_NOTE_MEMORY];
    const int activeCount = collectActiveNotes(active, ARP_NOTE_MEMORY, mode == amAssign);
    int count = 0;
    for (int i = 0; i < activeCount && count < maxNotes; ++i) {
        bool seen = false;
        for (int existing = 0; existing < count; ++existing)
            seen |= outNotes[existing] == active[i].note;
        if (!seen) outNotes[count++] = active[i].note;
    }
    return count;
}

int Arpeggiator::getPattern(uint8_t* outNotes, int maxSteps) const {
    if (!outNotes || maxSteps <= 0) return 0;
    uint8_t active[ARP_NOTE_MEMORY];
    const int count = mode == amOff ? 0 : getActiveNotes(active, ARP_NOTE_MEMORY);
    ArpPick picks[ARP_NOTE_MEMORY];
    for (int s = 0; s < maxSteps; ++s) {
        // The preview shows the first note of a step (Chord: the lowest,
        // Strum: the first degree) without transpose; Random uses a fixed
        // hash instead of the playing generator.
        const uint32_t randomValue = static_cast<uint32_t>((s * 5 + 3) ^ 0x5a);
        const int n = arpPicks(mode, s, s, count, octaves, sequence.degrees, randomValue, picks);
        outNotes[s] = n > 0 ? static_cast<uint8_t>(std::min(127, active[picks[0].note] + picks[0].octave * 12))
                            : ASSIGNER_NO_NOTE;
    }
    return count;
}

void Arpeggiator::emitNote(const ArpNote& source, int octaveOffset, uint16_t velocity) {
    ArpNote played = source;
    played.note = clampMidiNote(static_cast<int>(source.note) + octaveOffset + sequence.transpose);
    played.velocity = velocity;
    played.held = false;
    if (assignCallback) assignCallback(played.note, 1, velocity, played.channel);
    previousOutputNotes.push_back(played);
    previousNote = played.note;
}

// The first grid step at (inclusive) or after `position`. A pair of steps
// spans two step lengths; its second step comes at swing x pair.
double Arpeggiator::stepAtOrAfter(double position, bool inclusive) const {
    const double base = static_cast<double>(getStepDivisionTicks());
    const double pair = 2.0 * base;
    const double offBeat = pair * static_cast<double>(swingFraction);
    double pairStart = gridOrigin + std::floor((position - gridOrigin) / pair) * pair;
    for (int i = 0; i < 3; ++i, pairStart += pair) {
        for (const double at : { pairStart, pairStart + offBeat })
            if (inclusive ? at >= position : at > position) return at;
    }
    return pairStart;
}

// Rate or swing changed: the next step on the new grid after what played.
void Arpeggiator::reschedule() {
    if (scheduled) nextStepAt = stepAtOrAfter(lastPosition, false);
}

void Arpeggiator::setFreeRunning(bool value) {
    if (value == freeRunning) return;
    freeRunning = value;
    if (!freeRunning) {
        gridOrigin = 0.0;
        restartPending = false;
        reschedule();
    }
}

void Arpeggiator::playStep(double at) {
    const double base = static_cast<double>(getStepDivisionTicks());
    double gateLength = base * static_cast<double>(gateFraction);
    // A latched 100% gate otherwise closes and retriggers on the same
    // sample. Dense MIDI then repeatedly steals a voice before its release
    // has advanced, producing a metallic release rattle. Keep one 48-PPQ
    // tick for a real release transition in Hold.
    const bool heldFullGate = hold != 0 && gateFraction >= 0.98f;
    if (heldFullGate) gateLength = std::min(gateLength, base - 1.0);

    // Strum: the second note a quarter step later, inside the gate.
    clockNow = at;
    strumDelay = std::min(base * 0.25, gateLength * 0.5);
    clockTick();
    strumDelay = 0.0;
    gateCloseAt = (gateFraction < 0.98f || heldFullGate) && isGateActive() && !isNextStepTie()
        ? at + gateLength : kNever;
}

void Arpeggiator::advance(double position, double window) {
    if (mode == amOff) {
        scheduled = false;
        lastPosition = position;
        return;
    }
    if (restartPending) {
        restartPending = false;
        gridOrigin = position;
        nextStepAt = position;
        scheduled = true;
    } else if (!scheduled) {
        nextStepAt = stepAtOrAfter(position - window - kTimeTolerance, true);
        scheduled = true;
    }
    // Strum before the gate end before the next step when they coincide.
    for (int guard = 0; guard < 256; ++guard) {
        double at = kNever;
        int event = -1;
        if (pendingStrum.valid && pendingStrum.dueAt < at) { at = pendingStrum.dueAt; event = 0; }
        if (gateCloseAt < at) { at = gateCloseAt; event = 1; }
        if (nextStepAt < at) { at = nextStepAt; event = 2; }
        if (event < 0 || at > position + window + kTimeTolerance) break;
        if (event == 0) {
            pendingStrum.valid = false;
            emitNote(pendingStrum.source, pendingStrum.octaveOffset, pendingStrum.velocity);
        } else if (event == 1) {
            finishPreviousNote();
            gateCloseAt = kNever;
        } else {
            nextStepAt = stepAtOrAfter(at, false);
            playStep(at);
        }
    }
    lastPosition = std::max(lastPosition, position + window);
}

double Arpeggiator::nextEvent() const {
    if (mode == amOff) return kNever;
    if (restartPending || !scheduled) return lastPosition;
    double at = std::min(nextStepAt, gateCloseAt);
    if (pendingStrum.valid) at = std::min(at, pendingStrum.dueAt);
    return at;
}

void Arpeggiator::relocate(double position) {
    const double shift = position - lastPosition;
    if (gateCloseAt < kNever) gateCloseAt += shift;
    if (pendingStrum.valid) pendingStrum.dueAt += shift;
    lastPosition = position;
    scheduled = false;
}

void Arpeggiator::stopClock() {
    finishPreviousNote();
    gateCloseAt = kNever;
}

void Arpeggiator::clockTick() {
    if (mode == amOff || isEmpty()) return;

    ArpNote active[ARP_NOTE_MEMORY];
    int count = collectActiveNotes(active, ARP_NOTE_MEMORY, mode == amAssign);
    if (count == 0) return;

    int curStepIn16 = stepCounter % 16;
    uint8_t patternType = sequence.pattern[curStepIn16];

    // Tie step
    if (patternType == 2 && gateState) {
        stepIndex++;
        stepCounter++;
        return;
    }

    finishPreviousNote();

    // Mute step
    if (patternType == 3) {
        stepIndex++;
        stepCounter++;
        return;
    }

    uint32_t randomValue = 0;
    if (mode == amRandom) {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        randomValue = randomState;
    }
    ArpPick picks[ARP_NOTE_MEMORY];
    const int pickCount = arpPicks(mode, stepIndex, curStepIn16, count, octaves, sequence.degrees, randomValue, picks);
    if (pickCount == 0) return;
    stepIndex++;
    previousOutputNotes.clear();

    if (mode == amChord || mode == amStrum) {
        // Chord: every held note, each with its own accent. Strum: the second
        // note 15 % softer than the first and skipped when it is the same note.
        uint16_t firstVelocity = 0;
        for (int i = 0; i < pickCount; ++i) {
            const auto& source = active[picks[i].note];
            const int octaveOffset = picks[i].octave * 12;
            if (mode == amStrum && i == 1) {
                const uint8_t output = clampMidiNote(static_cast<int>(source.note) + octaveOffset + sequence.transpose);
                if (output == previousNote && source.channel == active[picks[0].note].channel) continue;
                const auto velocity = static_cast<uint16_t>((static_cast<uint32_t>(firstVelocity) * 85U) / 100U);
                // From the clock the second note follows later (a strum);
                // a direct call plays it at once.
                if (strumDelay > 0.0)
                    pendingStrum = { true, source, octaveOffset, velocity, clockNow + strumDelay };
                else
                    emitNote(source, octaveOffset, velocity);
                continue;
            }
            const uint16_t velocity = patternType == 1 ? accentVelocity(source.velocity) : source.velocity;
            if (i == 0) firstVelocity = velocity;
            emitNote(source, octaveOffset, velocity);
        }
    } else {
        // Single notes: the octave is clamped before the transpose.
        ArpNote noteToPlay = active[picks[0].note];
        noteToPlay.note = clampMidiNote(static_cast<int>(noteToPlay.note) + picks[0].octave * 12);
        emitNote(noteToPlay, 0, patternType == 1 ? accentVelocity(noteToPlay.velocity) : noteToPlay.velocity);
    }
    gateState = 1;
    stepCounter++;
}
