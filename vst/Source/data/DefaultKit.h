#pragma once

#include "PresetData.h"
#include "../dsp/adsr.h"

// The default AFX kit: 16 parts with distinct sounds and a note map of eight
// notes per part. Plain data, shared by the editor model and the engine.
namespace defaultkit {

inline constexpr int kPartCount = 16;

inline const char* partName(int slot) {
    static const char* names[kPartCount] = {
        "Sub Bass", "Analog Saw Bass", "Acid Reso 303", "FM Metallic Kick",
        "Analog Snare/Hat", "Noise Glitch", "FM Percussion", "Chirp Click",
        "SuperSaw Lead", "Vocal Pluck", "Brite Bell", "WaveFolder Lead",
        "Glass Texture", "Liquid Pad", "Dark Drone", "Modular S&H"
    };
    return names[slot % kPartCount];
}

inline uint8_t partForNote(int note) {
    return (uint8_t)((note / 8) % kPartCount);
}

inline void partPreset(int slot, PresetData& preset) {
    preset.setDefaults();
    // Give each slot a distinct timbre / filter / envelope identity
    switch (slot % 6) {
    case 0: // Bass / Sub
        preset.steppedParams[spFilterModel] = fmSSI2144; // 24dB Ladder
        preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(280);
        preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(150);
        preset.continuousParams[cpFilAtt] = adsrCVForMilliseconds(10);
        preset.continuousParams[cpFilDec] = adsrCVForMilliseconds(350);
        preset.continuousParams[cpFilSus] = (uint16_t)scan_potTo16bits(200);
        preset.continuousParams[cpFilRel] = adsrCVForMilliseconds(200);
        preset.continuousParams[cpAmpAtt] = adsrCVForMilliseconds(5);
        preset.continuousParams[cpAmpDec] = adsrCVForMilliseconds(400);
        preset.continuousParams[cpAmpSus] = (uint16_t)scan_potTo16bits(500);
        preset.continuousParams[cpAmpRel] = adsrCVForMilliseconds(250);
        break;

    case 1: // Percussion / Click / Glitch
        preset.steppedParams[spFilterModel] = fmSem;
        preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(600);
        preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(750);
        preset.steppedParams[spAWModType] = wmFolder; // WaveFolder
        preset.continuousParams[cpABaseWMod] = (uint16_t)scan_potTo16bits(600);
        preset.continuousParams[cpFilAtt] = adsrCVForMilliseconds(0);
        preset.continuousParams[cpFilDec] = adsrCVForMilliseconds(120);
        preset.continuousParams[cpFilSus] = 0;
        preset.continuousParams[cpFilRel] = adsrCVForMilliseconds(80);
        preset.continuousParams[cpAmpAtt] = adsrCVForMilliseconds(0);
        preset.continuousParams[cpAmpDec] = adsrCVForMilliseconds(140);
        preset.continuousParams[cpAmpSus] = 0;
        preset.continuousParams[cpAmpRel] = adsrCVForMilliseconds(100);
        break;

    case 2: // Acid / Resonance Lead
        preset.steppedParams[spFilterModel] = fmSSI2144;
        preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(350);
        preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(820);
        preset.continuousParams[cpFilEnvAmt] = (uint16_t)scan_potTo16bits(780);
        preset.steppedParams[spBWModType] = wmBitCrush;
        preset.continuousParams[cpBBaseWMod] = (uint16_t)scan_potTo16bits(300);
        break;

    case 3: // Pluck / Bell
        preset.steppedParams[spFilterModel] = fmEQ; // Shelves Parametric
        preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(700);
        preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(400);
        preset.continuousParams[cpFilAtt] = adsrCVForMilliseconds(5);
        preset.continuousParams[cpFilDec] = adsrCVForMilliseconds(300);
        preset.continuousParams[cpFilSus] = (uint16_t)scan_potTo16bits(100);
        preset.continuousParams[cpAmpAtt] = adsrCVForMilliseconds(5);
        preset.continuousParams[cpAmpDec] = adsrCVForMilliseconds(450);
        preset.continuousParams[cpAmpSus] = 0;
        break;

    case 4: // WaveMod Morph
        preset.steppedParams[spAWModType] = wmCrossOver;
        preset.continuousParams[cpABaseWMod] = (uint16_t)scan_potTo16bits(500);
        preset.continuousParams[cpDetune] = (uint16_t)scan_potTo16bits(550);
        break;

    case 5: // Soft Pad
        preset.steppedParams[spFilterModel] = fmSem;
        preset.continuousParams[cpCutoff] = (uint16_t)scan_potTo16bits(450);
        preset.continuousParams[cpResonance] = (uint16_t)scan_potTo16bits(200);
        preset.continuousParams[cpFilAtt] = adsrCVForMilliseconds(450);
        preset.continuousParams[cpFilDec] = adsrCVForMilliseconds(600);
        preset.continuousParams[cpFilSus] = (uint16_t)scan_potTo16bits(700);
        preset.continuousParams[cpFilRel] = adsrCVForMilliseconds(500);
        preset.continuousParams[cpAmpAtt] = adsrCVForMilliseconds(400);
        preset.continuousParams[cpAmpDec] = adsrCVForMilliseconds(500);
        preset.continuousParams[cpAmpSus] = (uint16_t)scan_potTo16bits(800);
        preset.continuousParams[cpAmpRel] = adsrCVForMilliseconds(600);
        break;
    }
}

} // namespace defaultkit
