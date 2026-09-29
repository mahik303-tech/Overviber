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
    auto fromDegree = [&](int degree) { return ArpPick{ degree % noteCount, degree / noteCount }; };

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
    transpose = 0;
    hold = 0;
    gateState = 0;
    mode = amOff;
    octaves = 1;
    rateIndex = 3; // 1/16th
    gateFraction = 0.833f;
    swingFraction = 0.50f;
    std::memset(stepPattern, 0, sizeof(stepPattern));
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
    rateIndex = std::clamp((uint8_t)rIdx, (uint8_t)0, (uint8_t)5);
}

void Arpeggiator::setGateLength(float fraction) {
    gateFraction = std::clamp(fraction, 0.10f, 1.0f);
}

void Arpeggiator::setSwing(float swing) {
    swingFraction = std::clamp(swing, 0.50f, 0.75f);
}

void Arpeggiator::setTranspose(int8_t t) {
    transpose = t;
}

void Arpeggiator::resetCounter() {
    stepIndex = 0;
    stepCounter = 0;
}

uint32_t Arpeggiator::getStepDivisionTicks() const {
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
        const int n = arpPicks(mode, s, s, count, octaves, stepDegrees, randomValue, picks);
        outNotes[s] = n > 0 ? static_cast<uint8_t>(std::min(127, active[picks[0].note] + picks[0].octave * 12))
                            : ASSIGNER_NO_NOTE;
    }
    return count;
}

void Arpeggiator::emitNote(const ArpNote& source, int octaveOffset, uint16_t velocity) {
    ArpNote played = source;
    played.note = clampMidiNote(static_cast<int>(source.note) + octaveOffset + transpose);
    played.velocity = velocity;
    played.held = false;
    if (assignCallback) assignCallback(played.note, 1, velocity, played.channel);
    previousOutputNotes.push_back(played);
    previousNote = played.note;
}

void Arpeggiator::clockTick() {
    if (mode == amOff || isEmpty()) return;

    ArpNote active[ARP_NOTE_MEMORY];
    int count = collectActiveNotes(active, ARP_NOTE_MEMORY, mode == amAssign);
    if (count == 0) return;

    int curStepIn16 = stepCounter % 16;
    uint8_t patternType = stepPattern[curStepIn16];

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
    const int pickCount = arpPicks(mode, stepIndex, curStepIn16, count, octaves, stepDegrees, randomValue, picks);
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
                const uint8_t output = clampMidiNote(static_cast<int>(source.note) + octaveOffset + transpose);
                if (output == previousNote && source.channel == active[picks[0].note].channel) continue;
                emitNote(source, octaveOffset, static_cast<uint16_t>((static_cast<uint32_t>(firstVelocity) * 85U) / 100U));
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
