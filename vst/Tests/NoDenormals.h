#pragma once

// Flushes denormals to zero for the calling thread, as the plugin does with
// juce::ScopedNoDenormals in processBlock. Without it, decaying resonators
// in these JUCE-free tests run into denormals and get many times slower.
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
inline void flushDenormalsToZero() { _mm_setcsr(_mm_getcsr() | 0x8040); }   // FTZ | DAZ
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
#include <cstdint>
inline void flushDenormalsToZero() {
    uint64_t fpcr;
    asm volatile("mrs %0, fpcr" : "=r"(fpcr));
    asm volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));   // FZ
}
#else
inline void flushDenormalsToZero() {}
#endif
