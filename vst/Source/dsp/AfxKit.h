#pragma once

#include "OvercyclerTypes.h"
#include "../data/PresetManager.h"
#include "../data/DefaultKit.h"
#include "../data/WaveManager.h"
#include <array>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#define AFX_SLOT_COUNT 16

struct AfxSoundSlot {
    std::string name = "Slot";
    PresetData preset;
    WaveManager waveManager;
    bool isCustomized = false;

    AfxSoundSlot() {
        preset.setDefaults();
    }
};

class AfxKit {
public:
    AfxKit() {
        initDefaultKit();
    }

    void initDefaultKit() {
        for (int note = 0; note < 128; ++note) noteToSlot[note] = defaultkit::partForNote(note);
        for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
            slots[i].name = defaultkit::partName(i);
            defaultkit::partPreset(i, slots[i].preset);
        }
    }

    void setBaseDirectory(const std::string& path) {
        for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
            slots[i].waveManager.setBaseDirectory(path);
        }
    }

    AfxSoundSlot& getSlot(int index) {
        return slots[std::clamp(index, 0, AFX_SLOT_COUNT - 1)];
    }

    const AfxSoundSlot& getSlot(int index) const {
        return slots[std::clamp(index, 0, AFX_SLOT_COUNT - 1)];
    }

    uint8_t getSlotForNote(uint8_t note) const {
        return noteToSlot[note & 0x7F];
    }

    void setNoteMapping(uint8_t note, uint8_t slot) {
        if (note < 128) {
            noteToSlot[note] = (uint8_t)(slot % AFX_SLOT_COUNT);
        }
    }

    void mapOctaveZones() {
        // 12 notes per octave -> maps octaves to slots 0..9
        for (int note = 0; note < 128; ++note) {
            noteToSlot[note] = (uint8_t)((note / 12) % AFX_SLOT_COUNT);
        }
    }

    void mapChromatic16() {
        // Cycles across slots on every consecutive semitone
        for (int note = 0; note < 128; ++note) {
            noteToSlot[note] = (uint8_t)(note % AFX_SLOT_COUNT);
        }
    }

    // The key map of the default kit.
    void mapDefault() {
        for (int note = 0; note < 128; ++note) noteToSlot[note] = defaultkit::partForNote(note);
    }

    void mapAllToSlot(uint8_t slot) {
        slot %= AFX_SLOT_COUNT;
        for (int note = 0; note < 128; ++note) {
            noteToSlot[note] = slot;
        }
    }

private:
    std::array<AfxSoundSlot, AFX_SLOT_COUNT> slots;
    std::array<uint8_t, 128> noteToSlot;
};
