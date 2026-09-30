#pragma once

#include "../../data/PresetData.h"
#include <juce_core/juce_core.h>
#include <algorithm>

// ==============================================================================
// Modulation matrix helpers for the editor: which knob shows which matrix
// destination (by component ID), the depth the matrix puts on a destination
// and the matrix's free slots.
// ==============================================================================
namespace modtargets {

struct Target {
    const char* componentId;
    modDest_t dest;
};

// Master pitch and WaveMod A+B have no knob of their own.
inline constexpr Target kTargets[] = {
    { "oscAFreqKnob", modDestPitchOscA },
    { "oscBFreqKnob", modDestPitchOscB },
    { "oscBDetuneKnob", modDestDetune },
    { "oscAWModKnob", modDestWaveModOscA },
    { "oscBWModKnob", modDestWaveModOscB },
    { "oscAVolKnob", modDestVolOscA },
    { "oscBVolKnob", modDestVolOscB },
    { "noiseVolKnob", modDestNoiseVol },
    { "cutoffKnob", modDestCutoff },
    { "resoKnob", modDestResonance },
    { "ampLevelKnob", modDestAmpLevel },
    { "elementsGeometryKnob", modDestElementsGeometry },
    { "elementsBrightnessKnob", modDestElementsBrightness },
    { "elementsDampingKnob", modDestElementsDamping },
    { "elementsPositionKnob", modDestElementsPosition },
    { "elementsSpaceKnob", modDestElementsSpace },
    { "elementsBowKnob", modDestElementsBow },
    { "elementsBlowKnob", modDestElementsBlow },
    { "elementsStrikeKnob", modDestElementsStrike },
    { "elementsContourKnob", modDestElementsContour },
    { "elementsFlowKnob", modDestElementsFlow },
    { "elementsMalletKnob", modDestElementsMallet },
    { "elementsBowTimbreKnob", modDestElementsBowTimbre },
    { "elementsBlowTimbreKnob", modDestElementsBlowTimbre },
    { "elementsStrikeTimbreKnob", modDestElementsStrikeTimbre },
};

inline modDest_t destinationFor(const juce::String& componentId) {
    for (const auto& t : kTargets)
        if (componentId == t.componentId) return t.dest;
    return modDestNone;
}

// A slot is free while it has no source or no destination.
inline bool isFree(const ModMatrixSlot& slot) {
    return slot.source == modSrcNone || slot.dest == modDestNone;
}

inline int firstFreeSlot(const PresetData& preset) {
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s)
        if (isFree(preset.modMatrix[s])) return s;
    return -1;
}

// The summed depth (-1 .. 1) of the enabled slots on a destination.
inline float depthOn(const PresetData& preset, modDest_t dest) {
    float depth = 0.0f;
    for (const auto& slot : preset.modMatrix)
        if (slot.enabled && slot.dest == dest && slot.source != modSrcNone) depth += slot.depth / 100.0f;
    return std::clamp(depth, -1.0f, 1.0f);
}

} // namespace modtargets
