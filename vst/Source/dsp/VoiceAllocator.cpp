#include "VoiceAllocator.h"
#include "ControlTimes.h"
#include <cstdlib>

VoiceAllocator::VoiceAllocator() {
    for (int part = 0; part < 16; ++part) routes[part].channel = static_cast<uint8_t>(part + 1);
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) voicePart[v] = -1;
    clearNoteCVs();
}

void VoiceAllocator::assign(VoiceAssigner& assigner, uint8_t note, int8_t gate, uint16_t velocity,
                            int8_t fromKeyboard, uint32_t tick, uint8_t channel, bool mpeMember) {
    if (!gate || !customRouting) {
        assigner.assignNote(note, gate, velocity, fromKeyboard, tick, channel);
        return;
    }
    const auto routingChannel = mpeMember ? 1 : channel;
    for (int part = 0; part < 16; ++part) {
        const auto& r = routes[part];
        if (r.enabled && (r.channel == 0 || r.channel == routingChannel)
            && note >= r.low && note <= r.high) {
            pendingPart = part;
            assigner.assignNote(note, gate, velocity, fromKeyboard, tick, channel, static_cast<uint8_t>(part));
        }
    }
    pendingPart = -1;
}

uint8_t VoiceAllocator::partForNewVoice(uint8_t note, uint8_t channel, const PresetData& main,
                                        const std::array<uint8_t, 128>& noteMap) const {
    const auto mode = static_cast<engineMode_t>(main.steppedParams[spEngineMode]);
    return pendingPart >= 0 ? static_cast<uint8_t>(pendingPart)
        : mode == emAFX ? noteMap[note & 0x7F]
        : main.steppedParams[spMPEMode] != 0 ? 0
        : static_cast<uint8_t>(std::clamp<int>(channel, 1, 16) - 1);
}

void VoiceAllocator::startNote(int voice, uint8_t note, const PresetData& preset) {
    const uint16_t baseCutoffRaw = preset.continuousParams[cpCutoff];
    const uint16_t trackRaw = preset.continuousParams[cpFilKbdAmt];

    // Oscillator base pitch (64 semitones): spChromaticPitch 1 drops the
    // fine part, 2 also rounds down to whole octaves (firmware refreshTunedCVs).
    const uint8_t chromatic = preset.steppedParams[spChromaticPitch];
    auto basePitch = [chromatic](uint16_t freq) {
        uint16_t pitch = freq >> 2;
        if (chromatic == 0) return pitch;
        uint16_t semitone = pitch >> 8;
        if (chromatic > 1) semitone -= semitone % 12;
        return (uint16_t)(semitone << 8);
    };
    const uint16_t cva = (note * WTOSC_CV_SEMITONE) + basePitch(preset.continuousParams[cpAFreq]);
    const uint16_t cvb = (note * WTOSC_CV_SEMITONE) + basePitch(preset.continuousParams[cpBFreq]);

    const int32_t trackOffset = (((int8_t)note - MIDDLE_C_NOTE) * (trackRaw >> 8)) >> 8;
    const uint16_t cvf = (uint16_t)__USAT((int32_t)baseCutoffRaw + (trackOffset * FILTER_CV_SEMITONE), 16);

    setGlide(voice, preset.continuousParams[cpGlide]);
    if (gliding[voice]) {
        if (oscANoteCV[voice] == 0) {
            oscANoteCV[voice] = cva;
            oscBNoteCV[voice] = cvb;
            filterNoteCV[voice] = cvf;
        }
        oscATargetCV[voice] = cva;
        oscBTargetCV[voice] = cvb;
        filterTargetCV[voice] = cvf;
    } else {
        oscANoteCV[voice] = cva;
        oscBNoteCV[voice] = cvb;
        filterNoteCV[voice] = cvf;
        filterTargetCV[voice] = cvf;
    }
}

void VoiceAllocator::setGlide(int voice, uint16_t glideParam) {
    glideAmount[voice] = controltimes::glideAmount(glideParam);
    gliding[voice] = (glideAmount[voice] < 2000);
}

static inline void computeGlide(uint16_t& out, uint16_t target, uint16_t amount) {
    if (out < target) {
        uint16_t diff = target - out;
        out += std::min(amount, diff);
    } else if (out > target) {
        uint16_t diff = out - target;
        out -= std::min(amount, diff);
    }
}

void VoiceAllocator::glideStep() {
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (!gliding[v]) continue;
        glideCarry[v] = static_cast<uint16_t>(glideCarry[v] + glideAmount[v]);
        const auto step = static_cast<uint16_t>(glideCarry[v] / controltimes::kCvUpdatesPerTick);
        glideCarry[v] = static_cast<uint16_t>(glideCarry[v] % controltimes::kCvUpdatesPerTick);
        computeGlide(oscANoteCV[v], oscATargetCV[v], step);
        computeGlide(oscBNoteCV[v], oscBTargetCV[v], step);
        computeGlide(filterNoteCV[v], filterTargetCV[v], step);
    }
}

// Moves a retargeted filter CV by 1/32 of the distance per CV tick.
void VoiceAllocator::slewFilter(int v) {
    if (filterNoteCV[v] == filterTargetCV[v]) return;
    const int32_t difference = static_cast<int32_t>(filterTargetCV[v]) - filterNoteCV[v];
    const int32_t magnitude = std::abs(difference);
    const int32_t step = std::max<int32_t>(1, magnitude / 32);
    filterNoteCV[v] = static_cast<uint16_t>(static_cast<int32_t>(filterNoteCV[v])
        + (difference > 0 ? std::min(step, difference) : std::max(-step, difference)));
}

void VoiceAllocator::retargetFilter(int v, int32_t delta) {
    filterTargetCV[v] = static_cast<uint16_t>(__USAT(static_cast<int32_t>(filterTargetCV[v]) + delta, 16));
}

void VoiceAllocator::clearNoteCVs() {
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        oscANoteCV[v] = oscBNoteCV[v] = filterNoteCV[v] = 0;
        oscATargetCV[v] = oscBTargetCV[v] = filterTargetCV[v] = 0;
    }
}
