#pragma once
#include "OvercyclerTypes.h"
#include <array>
#include <atomic>
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
    PartRoute route;
};
struct PreparedState {
    PreparedPart parts[16];
    float faders[SYNTH_VOICE_COUNT]{};
    float pans[SYNTH_VOICE_COUNT]{};
    uint8_t panCustomized[SYNTH_VOICE_COUNT]{};
    uint8_t noteMap[128]{};
    uint8_t arpPattern[16]{}, arpDegrees[16]{};
    int8_t transpose = 0;
    bool customRouting = false;
    uint32_t panicGeneration = 0;
};
static_assert(std::is_trivially_copyable<PreparedState>::value, "Audio states must not allocate");

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
