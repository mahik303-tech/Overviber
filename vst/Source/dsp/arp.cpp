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
    if (mode == amOff) {
        for (int s = 0; s < maxSteps; ++s) outNotes[s] = ASSIGNER_NO_NOTE;
        return 0;
    }

    uint8_t active[ARP_NOTE_MEMORY];
    int count = getActiveNotes(active, ARP_NOTE_MEMORY);
    if (count == 0) {
        for (int s = 0; s < maxSteps; ++s) outNotes[s] = ASSIGNER_NO_NOTE;
        return 0;
    }

    int octCount = std::max(1, (int)octaves);
    int totalPoolSize = count * octCount;
    FixedBuffer<uint8_t, ARP_NOTE_MEMORY * 4> pool;
    pool.reserve(totalPoolSize);

    for (int o = 0; o < octCount; ++o) {
        for (int i = 0; i < count; ++i) {
            int n = (int)active[i] + o * 12;
            pool.push_back((uint8_t)std::min(127, n));
        }
    }

    for (int s = 0; s < maxSteps; ++s) {
        switch (mode) {
        case amUp:
            outNotes[s] = pool[s % totalPoolSize];
            break;
        case amDown:
            outNotes[s] = pool[totalPoolSize - 1 - (s % totalPoolSize)];
            break;
        case amUpDown: {
            if (totalPoolSize == 1) {
                outNotes[s] = pool[0];
            } else if (totalPoolSize == 2) {
                outNotes[s] = pool[s % 2];
            } else {
                int cycleLen = totalPoolSize * 2 - 2;
                int phase = s % cycleLen;
                int idx = (phase < totalPoolSize) ? phase : (cycleLen - phase);
                outNotes[s] = pool[idx];
            }
            break;
        }
        case amAssign: {
            int m = s % count;
            int o = (s / count) % octCount;
            int n = (int)active[m] + o * 12;
            outNotes[s] = (uint8_t)std::min(127, n);
            break;
        }
        case amRandom:
            outNotes[s] = pool[((s * 5 + 3) ^ 0x5a) % totalPoolSize];
            break;
        case amChord: {
            int o = s % octCount;
            outNotes[s] = (uint8_t)std::min(127, (int)active[0] + o * 12);
            break;
        }
        case amConverge: {
            int idx = s % totalPoolSize;
            int mapped = (idx % 2 == 0) ? (idx / 2) : (totalPoolSize - 1 - idx / 2);
            outNotes[s] = pool[mapped];
            break;
        }
        case amDegree:
        case amStrum: {
            int deg = stepDegrees[s % 16];
            int octShift = deg / count;
            int noteIdx = deg % count;
            int n = (int)active[noteIdx] + octShift * 12;
            outNotes[s] = (uint8_t)std::min(127, n);
            break;
        }
        default:
            outNotes[s] = pool[0];
            break;
        }
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

    int octCount = std::max(1, (int)octaves);

    if (mode == amChord) {
        int oct = (stepIndex++) % octCount;
        int octOffset = oct * 12;
        previousOutputNotes.clear();

        for (int i = 0; i < count; ++i) {
            const uint16_t velocity = patternType == 1
                ? static_cast<uint16_t>(std::min(65535, static_cast<int>(active[i].velocity * 1.45f)))
                : active[i].velocity;
            emitNote(active[i], octOffset, velocity);
        }
        gateState = 1;
        stepCounter++;
        return;
    }

    if (mode == amStrum) {
        previousOutputNotes.clear();
        int deg = stepDegrees[curStepIn16];
        int octShift = deg / count;
        int noteIdx = deg % count;
        const auto first = active[noteIdx];
        const uint16_t firstVelocity = patternType == 1
            ? static_cast<uint16_t>(std::min(65535, static_cast<int>(first.velocity * 1.45f)))
            : first.velocity;
        emitNote(first, octShift * 12, firstVelocity);

        if (count > 1) {
            int deg2 = deg + 2;
            int octShift2 = deg2 / count;
            int noteIdx2 = deg2 % count;
            const auto second = active[noteIdx2];
            const uint8_t secondOutput = clampMidiNote(static_cast<int>(second.note) + octShift2 * 12 + transpose);
            if (secondOutput != previousNote || second.channel != first.channel) {
                emitNote(second, octShift2 * 12,
                    static_cast<uint16_t>((static_cast<uint32_t>(firstVelocity) * 85U) / 100U));
            }
        }
        gateState = 1;
        stepIndex++;
        stepCounter++;
        return;
    }

    int totalPoolSize = count * octCount;
    FixedBuffer<ArpNote, ARP_NOTE_MEMORY * 4> pool;
    pool.reserve(totalPoolSize);

    for (int o = 0; o < octCount; ++o) {
        for (int i = 0; i < count; ++i) {
            auto expanded = active[i];
            expanded.note = clampMidiNote(static_cast<int>(expanded.note) + o * 12);
            pool.push_back(expanded);
        }
    }

    ArpNote noteToPlay = pool[0];

    switch (mode) {
    case amUp: {
        int idx = (stepIndex++) % totalPoolSize;
        noteToPlay = pool[idx];
        break;
    }
    case amDown: {
        int idx = (stepIndex++) % totalPoolSize;
        noteToPlay = pool[totalPoolSize - 1 - idx];
        break;
    }
    case amUpDown: {
        if (totalPoolSize == 1) {
            noteToPlay = pool[0];
            stepIndex++;
        } else if (totalPoolSize == 2) {
            int idx = (stepIndex++) % 2;
            noteToPlay = pool[idx];
        } else {
            int cycleLen = totalPoolSize * 2 - 2;
            int phase = (stepIndex++) % cycleLen;
            int idx = (phase < totalPoolSize) ? phase : (cycleLen - phase);
            noteToPlay = pool[idx];
        }
        break;
    }
    case amAssign: {
        int m = stepIndex % count;
        int o = (stepIndex / count) % octCount;
        noteToPlay = active[m];
        noteToPlay.note = clampMidiNote(static_cast<int>(noteToPlay.note) + o * 12);
        stepIndex++;
        break;
    }
    case amRandom: {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        int idx = static_cast<int>(randomState % totalPoolSize);
        noteToPlay = pool[idx];
        stepIndex++;
        break;
    }
    case amConverge: {
        int idx = (stepIndex++) % totalPoolSize;
        int mapped = (idx % 2 == 0) ? (idx / 2) : (totalPoolSize - 1 - idx / 2);
        noteToPlay = pool[mapped];
        break;
    }
    case amDegree: {
        int deg = stepDegrees[curStepIn16];
        int octShift = deg / count;
        int noteIdx = deg % count;
        noteToPlay = active[noteIdx];
        noteToPlay.note = clampMidiNote(static_cast<int>(noteToPlay.note) + octShift * 12);
        stepIndex++;
        break;
    }
    default:
        return;
    }

    const uint16_t velocity = patternType == 1
        ? static_cast<uint16_t>(std::min(65535, static_cast<int>(noteToPlay.velocity * 1.45f)))
        : noteToPlay.velocity;
    emitNote(noteToPlay, 0, velocity);
    gateState = 1;
    stepCounter++;
}
