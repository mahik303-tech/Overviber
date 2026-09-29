#pragma once
#include "OvercyclerTypes.h"
#include <array>
#include <atomic>
#include <cstring>
#include <type_traits>

struct PartRoute {
    uint8_t enabled = 1, channel = 0, low = 0, high = 127;
};
struct PreparedPart {
    uint16_t continuous[cpCount]{};
    uint8_t stepped[spCount]{};
    ModMatrixSlot matrix[MOD_MATRIX_SLOT_COUNT]{};
    uint8_t pattern[SYNTH_VOICE_COUNT]{};
    uint16_t waves[abxCount][WTOSC_SAMPLE_COUNT]{};
    uint32_t waveRevision = 0;   // WaveManager revision of `waves`; 0 = unknown
    PartRoute route;
};
struct PreparedState {
    PreparedPart parts[16];
    float faders[SYNTH_VOICE_COUNT]{};
    bool masterMute = false;
    float pans[SYNTH_VOICE_COUNT]{};
    uint8_t panCustomized[SYNTH_VOICE_COUNT]{};
    uint8_t noteMap[128]{};
    uint8_t arpPattern[16]{}, arpDegrees[16]{};
    int8_t transpose = 0;
    bool customRouting = false;
    uint32_t panicGeneration = 0;
};
static_assert(std::is_trivially_copyable<PreparedState>::value, "Audio states must not allocate");

// Equality of two states without reading the wave data: waves compare by
// revision (about 6 KB compared instead of 310 KB).
inline bool sameIgnoringWaveData(const PreparedState& a, const PreparedState& b) {
    for (int part = 0; part < 16; ++part) {
        const auto& x = a.parts[part];
        const auto& y = b.parts[part];
        if (x.waveRevision != y.waveRevision
            || std::memcmp(x.continuous, y.continuous, sizeof(x.continuous)) != 0
            || std::memcmp(x.stepped, y.stepped, sizeof(x.stepped)) != 0
            || std::memcmp(x.matrix, y.matrix, sizeof(x.matrix)) != 0
            || std::memcmp(x.pattern, y.pattern, sizeof(x.pattern)) != 0
            || std::memcmp(&x.route, &y.route, sizeof(x.route)) != 0) return false;
    }
    return std::memcmp(a.faders, b.faders, sizeof(a.faders)) == 0 && a.masterMute == b.masterMute
        && std::memcmp(a.pans, b.pans, sizeof(a.pans)) == 0
        && std::memcmp(a.panCustomized, b.panCustomized, sizeof(a.panCustomized)) == 0
        && std::memcmp(a.noteMap, b.noteMap, sizeof(a.noteMap)) == 0
        && std::memcmp(a.arpPattern, b.arpPattern, sizeof(a.arpPattern)) == 0
        && std::memcmp(a.arpDegrees, b.arpDegrees, sizeof(a.arpDegrees)) == 0
        && a.transpose == b.transpose && a.customRouting == b.customRouting
        && a.panicGeneration == b.panicGeneration;
}

// Single message-thread producer, single audio-thread consumer. A writer never
// touches an unread slot. The consumer releases a slot only after applying it.
class PreparedStateQueue {
public:
    bool push(const PreparedState& state) {
        const auto w = write.load(std::memory_order_relaxed);
        const auto next = (w + 1) % states.size();
        if (next == read.load(std::memory_order_acquire)) return false;
        states[w] = state; write.store(next, std::memory_order_release); return true;
    }
    const PreparedState* front() const {
        const auto r = read.load(std::memory_order_relaxed);
        return r == write.load(std::memory_order_acquire) ? nullptr : &states[r];
    }
    void pop() { read.store((read.load(std::memory_order_relaxed) + 1) % states.size(), std::memory_order_release); }
private:
    std::array<PreparedState, 3> states{};
    std::atomic<size_t> read{0}, write{0};
};
