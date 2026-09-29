#include "assigner.h"
#include <algorithm>
#include <cstring>

static const uint8_t polyPattern[SYNTH_VOICE_COUNT] = {0, ASSIGNER_NO_NOTE, ASSIGNER_NO_NOTE, ASSIGNER_NO_NOTE, ASSIGNER_NO_NOTE, ASSIGNER_NO_NOTE};

VoiceAssigner::VoiceAssigner() {
    init();
}

void VoiceAssigner::init() {
    std::memset(allocation, 0, sizeof(allocation));
    std::memset(noteTimestamps, 0xFF, sizeof(noteTimestamps));
    std::memset(noteVelocities, 0, sizeof(noteVelocities));
    voiceMask = (1 << SYNTH_VOICE_COUNT) - 1;
    std::memset(patternOffsets, ASSIGNER_NO_NOTE, SYNTH_VOICE_COUNT);
    patternOffsets[0] = 0;
    priority = apLast;
    mono = 0;
    hold = 0;
}

void VoiceAssigner::setNoteState(uint8_t note, int8_t gate, uint16_t velocity, uint32_t timestamp) {
    if (note >= ASSIGNER_NOTE_COUNT) return;
    noteVelocities[note] = velocity;
    noteTimestamps[note] = gate ? timestamp : UINT32_MAX;
}

int8_t VoiceAssigner::getNoteState(uint8_t note, uint16_t* velocity, uint32_t* timestamp) {
    if (note >= ASSIGNER_NOTE_COUNT) return 0;
    int8_t gate = (noteTimestamps[note] != UINT32_MAX);
    if (gate && timestamp) *timestamp = noteTimestamps[note];
    if (gate && velocity) *velocity = noteVelocities[note];
    return gate;
}

int8_t VoiceAssigner::isVoiceDisabled(int8_t voice) const {
    return !(voiceMask & (1 << voice));
}

int8_t VoiceAssigner::getNoteAllocation(uint8_t note) {
    for (int8_t vi = 0; vi < SYNTH_VOICE_COUNT; ++vi) {
        if (isVoiceDisabled(vi)) continue;
        if (allocation[vi].allocated && allocation[vi].rootNote == note) {
            return vi;
        }
    }
    return -1;
}

int8_t VoiceAssigner::getAvailableVoice(uint8_t note, uint32_t timestamp, uint8_t channel, uint8_t part) {
    int8_t oldestVoice = -1, sameNote = -1;
    uint32_t oldestTimestamp = UINT32_MAX;

    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (isVoiceDisabled(v)) continue;

        if (allocation[v].allocated) {
            if (allocation[v].timestamp < timestamp && allocation[v].note == note && allocation[v].channel == channel && allocation[v].part == part) {
                sameNote = v;
                break;
            }
        } else {
            if (allocation[v].timestamp < oldestTimestamp) {
                oldestTimestamp = allocation[v].timestamp;
                oldestVoice = v;
            }
        }
    }

    return (sameNote >= 0) ? sameNote : oldestVoice;
}

int8_t VoiceAssigner::getDispensableVoice(uint8_t note) {
    int8_t res = -1;
    uint32_t ts = UINT32_MAX;

    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (isVoiceDisabled(v)) continue;
        if (!allocation[v].keyPressed && allocation[v].timestamp < ts) {
            ts = allocation[v].timestamp;
            res = v;
        }
    }

    if (res >= 0) return res;

    ts = UINT32_MAX;
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (isVoiceDisabled(v)) continue;

        switch (priority) {
        case apLast:
            if (allocation[v].timestamp < ts) {
                res = v;
                ts = allocation[v].timestamp;
            }
            break;
        case apLow:
            if (allocation[v].note > note) {
                res = v;
                note = allocation[v].note;
            }
            break;
        case apHigh:
            if (allocation[v].note < note) {
                res = v;
                note = allocation[v].note;
            }
            break;
        }
    }

    return res;
}

void VoiceAssigner::voiceDone(int8_t voice) {
    if (voice < 0 || voice >= SYNTH_VOICE_COUNT) return;
    allocation[voice].allocated = 0;
    allocation[voice].keyPressed = 0;
    allocation[voice].note = ASSIGNER_NO_NOTE;
    allocation[voice].rootNote = ASSIGNER_NO_NOTE;
}

void VoiceAssigner::voicesDone() {
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (isVoiceDisabled(v)) continue;
        voiceDone(v);
        allocation[v].timestamp = 0;
    }
    std::memset(noteTimestamps, 0xFF, sizeof(noteTimestamps));
}

void VoiceAssigner::releaseAllGates() {
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (allocation[v].allocated && allocation[v].gated) {
            if (eventCallback) eventCallback(allocation[v].note, 0, v, allocation[v].velocity, 0);
            allocation[v].gated = 0;
            allocation[v].keyPressed = 0;
        }
    }
}

void VoiceAssigner::allKeysOff() {
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!isVoiceDisabled(v) && allocation[v].gated && allocation[v].fromKeyboard) {
            if (eventCallback) eventCallback(allocation[v].note, 0, v, allocation[v].velocity, 0);
            allocation[v].gated = 0;
            allocation[v].keyPressed = 0;
        }
    }
    std::memset(noteTimestamps, 0xFF, sizeof(noteTimestamps));
    hold = 0;
}

void VoiceAssigner::allNotesOff() {
    releaseAllGates();
    std::memset(noteTimestamps, 0xFF, sizeof(noteTimestamps));
    hold = 0;
}

void VoiceAssigner::panicOff() {
    voicesDone();
    init();
}

void VoiceAssigner::setPriority(assignerPriority_t prio) {
    if (prio == priority) return;
    allNotesOff();
    voicesDone();
    if (prio > 2) prio = apLast;
    priority = prio;
}

void VoiceAssigner::setVoiceMask(uint8_t mask) {
    if (mask == voiceMask) return;
    allNotesOff();
    voicesDone();
    voiceMask = mask;
}

int8_t VoiceAssigner::getAssignment(int8_t voice, uint8_t* note) {
    if (voice < 0 || voice >= SYNTH_VOICE_COUNT) return 0;
    int8_t a = allocation[voice].allocated;
    if (a && note) *note = allocation[voice].note;
    return a;
}

int8_t VoiceAssigner::getAnyPressed() {
    for (uint8_t n = 0; n < ASSIGNER_NOTE_COUNT; ++n) {
        if (getNoteState(n, nullptr, nullptr)) return 1;
    }
    return 0;
}

int8_t VoiceAssigner::getAnyAssigned() {
    int8_t f = 0;
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!isVoiceDisabled(v)) f |= allocation[v].allocated;
    }
    return f != 0;
}

int8_t VoiceAssigner::getVoiceByNote(uint8_t note) const {
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!isVoiceDisabled(v) && allocation[v].allocated && (allocation[v].rootNote == note || allocation[v].note == note)) {
            return v;
        }
    }
    return -1;
}

bool VoiceAssigner::voiceMatches(int8_t voice, uint8_t rootNote, uint8_t channel) const {
    if (voice < 0 || voice >= SYNTH_VOICE_COUNT) return false;
    const auto& a = allocation[voice];
    return !isVoiceDisabled(voice) && a.allocated && a.rootNote == rootNote && a.channel == channel;
}

int8_t VoiceAssigner::getVoiceByChannel(uint8_t channel) const {
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!isVoiceDisabled(v) && allocation[v].allocated && allocation[v].channel == channel) {
            return v;
        }
    }
    return -1;
}

uint8_t VoiceAssigner::getVoiceChannel(int8_t voice) const {
    if (voice >= 0 && voice < SYNTH_VOICE_COUNT) {
        return allocation[voice].channel;
    }
    return 1;
}

void VoiceAssigner::assignNote(uint8_t note, int8_t gate, uint16_t velocity, int8_t fromKeyboard, uint32_t currentTick, uint8_t channel, uint8_t part) {
    if (note >= ASSIGNER_NOTE_COUNT) return;

    const uint32_t timestamp = currentTick;
    setNoteState(note, gate, velocity, timestamp);

    if (gate) startNote(note, velocity, fromKeyboard, timestamp, channel, part);
    else if (!mono) releasePolyNote(note, velocity, channel);
    else releaseMonoNote(note, velocity, fromKeyboard, timestamp, channel, part);
}

// Polyphonic releases belong to the original MIDI channel, including all
// layers triggered by that key. A release must not stop another channel.
void VoiceAssigner::releasePolyNote(uint8_t note, uint16_t velocity, uint8_t channel) {
    for (int8_t vi = 0; vi < SYNTH_VOICE_COUNT; ++vi) {
        auto& a = allocation[vi];
        if (a.allocated && a.rootNote == note && a.channel == channel) {
            a.keyPressed = 0;
            if (!hold) {
                a.gated = 0;
                if (eventCallback) eventCallback(a.note, 0, vi, velocity, 0);
            }
        }
    }
}

// Starts a note on a free voice (poly) or voice 0 (mono), with one voice per
// entry of the pattern (unison, chords). In mono with low or high priority a
// note loses against a held lower or higher note and plays legato when
// others are held.
void VoiceAssigner::startNote(uint8_t note, uint16_t velocity, int8_t fromKeyboard, uint32_t timestamp, uint8_t channel, uint8_t part) {
    uint8_t flags = 0;
    int8_t v = -1;

    if (mono) {
        v = 0;
        if (priority != apLast) {
            for (uint8_t n = 0; n < ASSIGNER_NOTE_COUNT; ++n) {
                if (n != note && getNoteState(n, nullptr, nullptr)) {
                    if (note > n && priority == apLow) return;
                    if (note < n && priority == apHigh) return;
                    flags = ASSIGNER_EVENT_FLAG_LEGATO;
                }
            }
        }
    } else {
        v = getAvailableVoice(note, timestamp, channel, part);
        if (v < 0) v = getDispensableVoice(note);
        if (v < 0) return;
    }

    for (int8_t vi = 0; vi < SYNTH_VOICE_COUNT; ++vi) {
        if (patternOffsets[vi] == ASSIGNER_NO_NOTE) break;

        const uint8_t n = static_cast<uint8_t>(std::clamp(
            static_cast<int>(note) + static_cast<int>(patternOffsets[vi]), 0, 127));
        allocation[v].allocated = 1;
        allocation[v].gated = 1;
        allocation[v].keyPressed = 1;
        allocation[v].velocity = velocity;
        allocation[v].rootNote = note;
        allocation[v].note = n;
        allocation[v].channel = channel;
        allocation[v].part = part;
        allocation[v].timestamp = timestamp;
        allocation[v].fromKeyboard = fromKeyboard;

        if (eventCallback) eventCallback(n, 1, v, velocity, flags);

        do {
            v = (v + 1) % SYNTH_VOICE_COUNT;
        } while (isVoiceDisabled(v));
    }
}

// Mono release of a sounding note: the next held key (by priority) takes
// over, otherwise the note's voices are released.
void VoiceAssigner::releaseMonoNote(uint8_t note, uint16_t velocity, int8_t fromKeyboard, uint32_t timestamp, uint8_t channel, uint8_t part) {
    if (getNoteAllocation(note) < 0) return;

    uint16_t restoredVelocity = 0;
    const uint8_t restoredNote = nextHeldNote(&restoredVelocity);
    if (restoredNote != ASSIGNER_NO_NOTE) {
        startNote(restoredNote, restoredVelocity, fromKeyboard, timestamp, channel, part);
        return;
    }

    for (int8_t vox = 0; vox < SYNTH_VOICE_COUNT; ++vox) {
        if (isVoiceDisabled(vox)) continue;
        if (allocation[vox].allocated && allocation[vox].rootNote == note) {
            allocation[vox].keyPressed = 0;
            if (!hold) {
                allocation[vox].gated = 0;
                if (eventCallback) eventCallback(allocation[vox].note, 0, vox, velocity, 0);
            }
        }
    }
}

// The held key without a voice that plays next in mono: the latest (last
// note priority), else the lowest or highest.
uint8_t VoiceAssigner::nextHeldNote(uint16_t* velocity) {
    uint8_t restoredNote = ASSIGNER_NO_NOTE;
    if (priority == apLast) {
        uint32_t restoredTimestamp = 0;
        for (uint8_t n = 0; n < ASSIGNER_NOTE_COUNT; ++n) {
            uint16_t vel;
            uint32_t ts;
            if (getNoteState(n, &vel, &ts) && ts > restoredTimestamp && getNoteAllocation(n) < 0) {
                restoredNote = n;
                *velocity = vel;
                restoredTimestamp = ts;
            }
        }
    } else {
        for (uint8_t ni = 0; ni < ASSIGNER_NOTE_COUNT; ++ni) {
            uint8_t n = (priority == apHigh) ? (127 - ni) : ni;
            uint16_t vel;
            if (getNoteState(n, &vel, nullptr) && getNoteAllocation(n) < 0) {
                restoredNote = n;
                *velocity = vel;
                break;
            }
        }
    }
    return restoredNote;
}

void VoiceAssigner::setPattern(const uint8_t* pattern, int8_t isMono) {
    if (isMono == mono && !std::memcmp(pattern, patternOffsets, SYNTH_VOICE_COUNT)) {
        return;
    }
    allKeysOff();
    mono = isMono;
    std::memset(patternOffsets, ASSIGNER_NO_NOTE, SYNTH_VOICE_COUNT);
    int8_t count = 0;
    for (int8_t i = 0; i < SYNTH_VOICE_COUNT; ++i) {
        if (pattern[i] == ASSIGNER_NO_NOTE) break;
        patternOffsets[i] = pattern[i];
        ++count;
    }
    if (count > 0) {
        patternOffsets[0] = 0;
    } else {
        std::memset(patternOffsets, 0, SYNTH_VOICE_COUNT);
    }
}

void VoiceAssigner::getPattern(uint8_t* pattern, int8_t* isMono) {
    std::memcpy(pattern, patternOffsets, SYNTH_VOICE_COUNT);
    if (isMono) *isMono = mono;
}

void VoiceAssigner::setPoly() {
    setPattern(polyPattern, 0);
}

void VoiceAssigner::latchPattern() {
    uint8_t pattern[SYNTH_VOICE_COUNT];
    int8_t count = 0;
    std::memset(pattern, ASSIGNER_NO_NOTE, SYNTH_VOICE_COUNT);

    for (uint8_t n = 0; n < ASSIGNER_NOTE_COUNT; ++n) {
        if (getNoteState(n, nullptr, nullptr)) {
            pattern[count] = n;
            if (count > 0) pattern[count] -= pattern[0];
            ++count;
            if (count >= SYNTH_VOICE_COUNT) break;
        }
    }
    setPattern(pattern, 1);
}

void VoiceAssigner::holdEvent(int8_t h) {
    if (h) {
        hold = 1;
        return;
    }
    hold = 0;
    for (int8_t v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!isVoiceDisabled(v) && allocation[v].gated && !allocation[v].keyPressed) {
            if (eventCallback) eventCallback(allocation[v].note, 0, v, allocation[v].velocity, 0);
            allocation[v].gated = 0;
        }
    }
}
