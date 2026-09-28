#pragma once

#include "../dsp/OvercyclerTypes.h"
#include <string>

// Parameters of one sound (one part). Plain data, shared by the editor model
// and the audio engine; file handling lives in PresetManager.
struct PresetData {
    std::string presetName;
    std::string oscBank[abxCount];
    std::string oscWave[abxCount];
    uint16_t continuousParams[cpCount];
    uint8_t steppedParams[spCount];
    ModMatrixSlot modMatrix[MOD_MATRIX_SLOT_COUNT];
    uint8_t voicePattern[SYNTH_VOICE_COUNT];
    int16_t presetNumber;

    PresetData();
    void setDefaults();
};
