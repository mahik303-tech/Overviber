// ==============================================================================
// Overviber / GliGli Overcycler - Voice Assigner & MIDI Event Dispatcher
//
// Origin:
//   GliGli Overcycler Hardware Synthesizer Firmware (firmware_17xx/synth/assigner.c)
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
#include <functional>

#define ASSIGNER_NOTE_COUNT 128
#define ASSIGNER_NO_NOTE UINT8_MAX
#define ASSIGNER_EVENT_FLAG_LEGATO 1

class VoiceAssigner {
public:
    using AssignerCallback = std::function<void(uint8_t note, int8_t gate, int8_t voice, uint16_t velocity, uint8_t flags)>;

    VoiceAssigner();
    void init();
    void setCallback(AssignerCallback cb) { eventCallback = cb; }

    void setPriority(assignerPriority_t prio);
    void setVoiceMask(uint8_t mask);
    int8_t getAssignment(int8_t voice, uint8_t* note);
    int8_t getAnyPressed();
    int8_t getAnyAssigned();
    int8_t getMono() const { return mono; }

    void assignNote(uint8_t note, int8_t gate, uint16_t velocity, int8_t fromKeyboard, uint32_t currentTick, uint8_t channel = 1, uint8_t part = 0);
    void voiceDone(int8_t voice);
    void allKeysOff();
    void allNotesOff();
    void panicOff();

    int8_t getVoiceByNote(uint8_t note) const;
    bool voiceMatches(int8_t voice, uint8_t rootNote, uint8_t channel) const;
    int8_t getVoiceByChannel(uint8_t channel) const;
    uint8_t getVoiceChannel(int8_t voice) const;

    void setPattern(const uint8_t* pattern, int8_t isMono);
    void getPattern(uint8_t* pattern, int8_t* isMono);
    void setPoly();
    void latchPattern();
    void holdEvent(int8_t hold);

private:
    struct Allocation {
        uint32_t timestamp;
        uint16_t velocity;
        uint8_t rootNote;
        uint8_t note;
        uint8_t channel;
        uint8_t part;
        int8_t allocated;
        int8_t gated;
        int8_t keyPressed;
        int8_t fromKeyboard;
    };

    void setNoteState(uint8_t note, int8_t gate, uint16_t velocity, uint32_t timestamp);
    int8_t getNoteState(uint8_t note, uint16_t* velocity, uint32_t* timestamp);
    int8_t isVoiceDisabled(int8_t voice) const;
    int8_t getNoteAllocation(uint8_t note);
    int8_t getAvailableVoice(uint8_t note, uint32_t timestamp, uint8_t channel, uint8_t part);
    int8_t getDispensableVoice(uint8_t note);
    void voicesDone();
    void releaseAllGates();
    void startNote(uint8_t note, uint16_t velocity, int8_t fromKeyboard, uint32_t timestamp, uint8_t channel, uint8_t part);
    void releasePolyNote(uint8_t note, uint16_t velocity, uint8_t channel);
    void releaseMonoNote(uint8_t note, uint16_t velocity, int8_t fromKeyboard, uint32_t timestamp, uint8_t channel, uint8_t part);
    uint8_t nextHeldNote(uint16_t* velocity);

    uint32_t noteTimestamps[ASSIGNER_NOTE_COUNT];
    uint16_t noteVelocities[ASSIGNER_NOTE_COUNT];
    Allocation allocation[SYNTH_VOICE_COUNT];
    uint8_t patternOffsets[SYNTH_VOICE_COUNT];
    assignerPriority_t priority;
    uint8_t voiceMask;
    int8_t mono;
    int8_t hold;

    AssignerCallback eventCallback;
};
