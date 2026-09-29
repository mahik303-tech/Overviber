#pragma once

#include <array>
#include <atomic>
#include <cstdint>

// The master output for the editor's spectrum: the audio thread appends
// mono samples, the message thread copies the latest ones. Lock-free, one
// writer and one reader; a copy that overlaps a write only shows a mixed
// window, which the display tolerates.
class OutputScope {
public:
    static constexpr int kSize = 4096;   // power of two

    void setSampleRate(double rate) { sampleRate.store(rate, std::memory_order_relaxed); }
    double getSampleRate() const { return sampleRate.load(std::memory_order_relaxed); }

    // Audio thread: the block's left and right output.
    void push(const float* left, const float* right, int count) {
        uint32_t pos = writePos.load(std::memory_order_relaxed);
        for (int i = 0; i < count; ++i) data[(pos++) & (kSize - 1)] = 0.5f * (left[i] + right[i]);
        writePos.store(pos, std::memory_order_release);
    }

    // Message thread: the latest `count` samples (count <= kSize), oldest first.
    void copyLatest(float* dest, int count) const {
        const uint32_t end = writePos.load(std::memory_order_acquire);
        for (int i = 0; i < count; ++i) dest[i] = data[(end - (uint32_t)count + (uint32_t)i) & (kSize - 1)];
    }

private:
    std::array<float, kSize> data{};
    std::atomic<uint32_t> writePos{0};
    std::atomic<double> sampleRate{48000.0};
};
