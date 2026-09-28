#pragma once

#include "OvercyclerTypes.h"
#include "../data/PresetManager.h"
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
        // Default note mapping: Octave-based distribution across the 16 sound slots
        for (int note = 0; note < 128; ++note) {
            // Map 128 notes across 16 slots (approx 8 notes per slot or octave based)
            noteToSlot[note] = (uint8_t)((note / 8) % AFX_SLOT_COUNT);
        }

        // Initialize slots with distinct musical sound characters
        static const char* slotNames[AFX_SLOT_COUNT] = {
            "Sub Bass", "Analog Saw Bass", "Acid Reso 303", "FM Metallic Kick",
            "Analog Snare/Hat", "Noise Glitch", "FM Percussion", "Chirp Click",
            "SuperSaw Lead", "Vocal Pluck", "Brite Bell", "WaveFolder Lead",
            "Glass Texture", "Liquid Pad", "Dark Drone", "Modular S&H"
        };

        for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
            slots[i].name = slotNames[i];
            slots[i].preset.setDefaults();

            // Give each slot a distinct timbre / filter / envelope identity
            switch (i % 6) {
            case 0: // Bass / Sub
                slots[i].preset.steppedParams[spFilterModel] = fmSSI2144; // 24dB Ladder
                slots[i].preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(280);
                slots[i].preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(150);
                slots[i].preset.continuousParams[cpFilAtt] = (uint16_t)scan_potTo16bits(10);
                slots[i].preset.continuousParams[cpFilDec] = (uint16_t)scan_potTo16bits(350);
                slots[i].preset.continuousParams[cpFilSus] = (uint16_t)scan_potTo16bits(200);
                slots[i].preset.continuousParams[cpFilRel] = (uint16_t)scan_potTo16bits(200);
                slots[i].preset.continuousParams[cpAmpAtt] = (uint16_t)scan_potTo16bits(5);
                slots[i].preset.continuousParams[cpAmpDec] = (uint16_t)scan_potTo16bits(400);
                slots[i].preset.continuousParams[cpAmpSus] = (uint16_t)scan_potTo16bits(500);
                slots[i].preset.continuousParams[cpAmpRel] = (uint16_t)scan_potTo16bits(250);
                break;

            case 1: // Percussion / Click / Glitch
                slots[i].preset.steppedParams[spFilterModel] = fmLiquid; // Liquid Ripples
                slots[i].preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(600);
                slots[i].preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(750);
                slots[i].preset.steppedParams[spAWModType] = wmFolder; // WaveFolder
                slots[i].preset.continuousParams[cpABaseWMod] = (uint16_t)scan_potTo16bits(600);
                slots[i].preset.continuousParams[cpFilAtt] = (uint16_t)scan_potTo16bits(0);
                slots[i].preset.continuousParams[cpFilDec] = (uint16_t)scan_potTo16bits(120);
                slots[i].preset.continuousParams[cpFilSus] = 0;
                slots[i].preset.continuousParams[cpFilRel] = (uint16_t)scan_potTo16bits(80);
                slots[i].preset.continuousParams[cpAmpAtt] = (uint16_t)scan_potTo16bits(0);
                slots[i].preset.continuousParams[cpAmpDec] = (uint16_t)scan_potTo16bits(140);
                slots[i].preset.continuousParams[cpAmpSus] = 0;
                slots[i].preset.continuousParams[cpAmpRel] = (uint16_t)scan_potTo16bits(100);
                break;

            case 2: // Acid / Resonance Lead
                slots[i].preset.steppedParams[spFilterModel] = fmSSI2144;
                slots[i].preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(350);
                slots[i].preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(820);
                slots[i].preset.continuousParams[cpFilEnvAmt] = (uint16_t)scan_potTo16bits(780);
                slots[i].preset.steppedParams[spBWModType] = wmBitCrush;
                slots[i].preset.continuousParams[cpBBaseWMod] = (uint16_t)scan_potTo16bits(300);
                break;

            case 3: // Pluck / Bell
                slots[i].preset.steppedParams[spFilterModel] = fmEQ; // Shelves Parametric
                slots[i].preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(700);
                slots[i].preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(400);
                slots[i].preset.continuousParams[cpFilAtt] = (uint16_t)scan_potTo16bits(5);
                slots[i].preset.continuousParams[cpFilDec] = (uint16_t)scan_potTo16bits(300);
                slots[i].preset.continuousParams[cpFilSus] = (uint16_t)scan_potTo16bits(100);
                slots[i].preset.continuousParams[cpAmpAtt] = (uint16_t)scan_potTo16bits(5);
                slots[i].preset.continuousParams[cpAmpDec] = (uint16_t)scan_potTo16bits(450);
                slots[i].preset.continuousParams[cpAmpSus] = 0;
                break;

            case 4: // WaveMod Morph
                slots[i].preset.steppedParams[spAWModType] = wmCrossOver;
                slots[i].preset.continuousParams[cpABaseWMod] = (uint16_t)scan_potTo16bits(500);
                slots[i].preset.continuousParams[cpDetune] = (uint16_t)scan_potTo16bits(550);
                break;

            case 5: // Soft Pad
                slots[i].preset.steppedParams[spFilterModel] = fmLiquid;
                slots[i].preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(450);
                slots[i].preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(200);
                slots[i].preset.continuousParams[cpFilAtt] = (uint16_t)scan_potTo16bits(450);
                slots[i].preset.continuousParams[cpFilDec] = (uint16_t)scan_potTo16bits(600);
                slots[i].preset.continuousParams[cpFilSus] = (uint16_t)scan_potTo16bits(700);
                slots[i].preset.continuousParams[cpFilRel] = (uint16_t)scan_potTo16bits(500);
                slots[i].preset.continuousParams[cpAmpAtt] = (uint16_t)scan_potTo16bits(400);
                slots[i].preset.continuousParams[cpAmpDec] = (uint16_t)scan_potTo16bits(500);
                slots[i].preset.continuousParams[cpAmpSus] = (uint16_t)scan_potTo16bits(800);
                slots[i].preset.continuousParams[cpAmpRel] = (uint16_t)scan_potTo16bits(600);
                break;
            }
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
