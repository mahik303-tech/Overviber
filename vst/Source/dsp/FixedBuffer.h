#pragma once
#include <array>
#include <cstddef>

// Bounded storage for audio-thread sequences. push_back reports overflow;
// callers choose a musically safe fallback instead of allocating.
template<class T, size_t Capacity> class FixedBuffer {
public:
    bool push_back(const T& value) {
        if (count == Capacity) return false;
        values[count++] = value; return true;
    }
    void clear() { count = 0; }
    void reserve(size_t) {} // Capacity is fixed at construction.
    size_t size() const { return count; }
    bool empty() const { return count == 0; }
    T* begin() { return values.data(); }
    T* end() { return values.data() + count; }
    const T* begin() const { return values.data(); }
    const T* end() const { return values.data() + count; }
    T& operator[](size_t i) { return values[i]; }
    const T& operator[](size_t i) const { return values[i]; }
private:
    std::array<T, Capacity> values{};
    size_t count = 0;
};
