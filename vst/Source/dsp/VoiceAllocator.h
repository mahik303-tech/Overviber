#pragma once

#include "OvercyclerTypes.h"
#include "PreparedState.h"
#include "assigner.h"
#include "../data/PresetData.h"
#include <array>
#include <algorithm>

// ==============================================================================
// Parts, routing and note CVs of the six voices.
//
// Routes note events to the voice assigner (with custom routing, a note-on
// reaches every part whose channel and note zone match), decides the part of
// a newly gated voice and keeps each voice's pitch/filter note CVs, including
// glide and the slew of retargeted filter CVs.
// ==============================================================================
class VoiceAllocator {
public:
    VoiceAllocator();

    // ---- Parts and routing
    PartRoute& route(int part) { return routes[std::clamp(part, 0, 15)]; }
    const PartRoute& route(int part) const { return routes[std::clamp(part, 0, 15)]; }
    bool usesCustomRouting() const { return customRouting; }
    void setCustomRouting(bool enabled) { customRouting = enabled; }

    // Sends a note event to the assigner. `mpeMember` routes MPE member
    // channels like channel 1.
    void assign(VoiceAssigner& assigner, uint8_t note, int8_t gate, uint16_t velocity,
                int8_t fromKeyboard, uint32_t tick, uint8_t channel, bool mpeMember);

    // Part of a voice gated by the assigner: the routed part while assign()
    // runs, otherwise by engine mode (AFX: per key, MPE: part 1, else channel).
    uint8_t partForNewVoice(uint8_t note, uint8_t channel, const PresetData& main,
                            const std::array<uint8_t, 128>& noteMap) const;
    int8_t part(int voice) const { return voicePart[voice]; }
    void setPart(int voice, uint8_t part) { voicePart[voice] = static_cast<int8_t>(part); }
    // A voice follows the main part (part 1, the edited preset) until it is
    // assigned to another part; main-part edits reach only these voices.
    bool followsMainPart(int voice) const { return voicePart[voice] <= 0; }

    // ---- Note CVs and glide
    // Pitch and filter-tracking CVs of a new note from the voice's part
    // preset; with glide the voice moves there from its previous note.
    void startNote(int voice, uint8_t note, const PresetData& preset);
    // Glide time of one voice (cpGlide of its part).
    void setGlide(int voice, uint16_t glideParam);
    bool isGliding(int voice) const { return gliding[voice] != 0; }
    void glideTick();                     // per clock tick, voices with glide
    void slewFilter(int voice);           // per CV tick, voices without glide
    void retargetFilter(int voice, int32_t delta);
    void clearNoteCVs();

    uint16_t oscANote(int v) const { return oscANoteCV[v]; }
    uint16_t oscBNote(int v) const { return oscBNoteCV[v]; }
    uint16_t filterNote(int v) const { return filterNoteCV[v]; }
    uint16_t oscATarget(int v) const { return oscATargetCV[v]; }

private:
    PartRoute routes[16];
    bool customRouting = false;
    int pendingPart = -1;                 // part being routed during assign()
    int8_t voicePart[SYNTH_VOICE_COUNT];

    int16_t glideAmount[SYNTH_VOICE_COUNT]{};
    int8_t gliding[SYNTH_VOICE_COUNT]{};
    uint16_t oscANoteCV[SYNTH_VOICE_COUNT];
    uint16_t oscBNoteCV[SYNTH_VOICE_COUNT];
    uint16_t filterNoteCV[SYNTH_VOICE_COUNT];
    uint16_t oscATargetCV[SYNTH_VOICE_COUNT];
    uint16_t oscBTargetCV[SYNTH_VOICE_COUNT];
    uint16_t filterTargetCV[SYNTH_VOICE_COUNT];
};
