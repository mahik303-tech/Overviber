#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <algorithm>
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define OVERVIBER_SIMD_SSE 1
    #include <immintrin.h>
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64) || defined(__arm64__)
    #define OVERVIBER_SIMD_NEON 1
    #include <arm_neon.h>
#else
    #define OVERVIBER_SIMD_SCALAR 1
#endif
#include <cstdint>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rack {

namespace math {

inline float clamp(float x, float a = 0.f, float b = 1.f) {
    return std::clamp(x, a, b);
}

inline float rescale(float x, float xMin, float xMax, float yMin, float yMax) {
    return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
}

} // namespace math

namespace random {

// Fast xorshift pseudo-random [0.0, 1.0) to bootstrap filter self-oscillation.
// The caller owns the state, so every filter instance is reproducible on its
// own, independent of other instances rendered on the same thread.
inline float uniform(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return (float)(s & 0x00ffffff) * (1.0f / 16777216.0f);
}

} // namespace random

namespace simd {

template <typename TYPE, int SIZE>
struct Vector;

template <>
struct Vector<float, 4> {
    using type = float;
    constexpr static int size = 4;

    union {
#if defined(OVERVIBER_SIMD_SSE)
        __m128 v;
#elif defined(OVERVIBER_SIMD_NEON)
        float32x4_t v;
#endif
        float s[4];
    };

    Vector() = default;

#if defined(OVERVIBER_SIMD_SSE)
    Vector(__m128 v) : v(v) {}
    Vector(float x) {
        v = _mm_set1_ps(x);
    }
    Vector(float x1, float x2, float x3, float x4) {
        v = _mm_setr_ps(x1, x2, x3, x4);
    }

    static Vector zero() {
        return Vector(_mm_setzero_ps());
    }

    static Vector mask() {
        return Vector(_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128())));
    }

    static Vector load(const float* x) {
        return Vector(_mm_loadu_ps(x));
    }

    void store(float* x) const {
        _mm_storeu_ps(x, v);
    }
#elif defined(OVERVIBER_SIMD_NEON)
    Vector(float32x4_t v) : v(v) {}
    Vector(float x) {
        v = vdupq_n_f32(x);
    }
    Vector(float x1, float x2, float x3, float x4) {
        s[0] = x1; s[1] = x2; s[2] = x3; s[3] = x4;
    }

    static Vector zero() {
        return Vector(vdupq_n_f32(0.0f));
    }

    static Vector mask() {
        return Vector(vreinterpretq_f32_u32(vdupq_n_u32(0xFFFFFFFF)));
    }

    static Vector load(const float* x) {
        return Vector(vld1q_f32(x));
    }

    void store(float* x) const {
        vst1q_f32(x, v);
    }
#else // Scalar Fallback
    Vector(float x) {
        s[0] = s[1] = s[2] = s[3] = x;
    }
    Vector(float x1, float x2, float x3, float x4) {
        s[0] = x1; s[1] = x2; s[2] = x3; s[3] = x4;
    }

    static Vector zero() {
        return Vector(0.0f, 0.0f, 0.0f, 0.0f);
    }

    static Vector mask() {
        Vector res;
        uint32_t allOnes = 0xFFFFFFFF;
        std::memcpy(&res.s[0], &allOnes, sizeof(float));
        std::memcpy(&res.s[1], &allOnes, sizeof(float));
        std::memcpy(&res.s[2], &allOnes, sizeof(float));
        std::memcpy(&res.s[3], &allOnes, sizeof(float));
        return res;
    }

    static Vector load(const float* x) {
        Vector res;
        res.s[0] = x[0]; res.s[1] = x[1]; res.s[2] = x[2]; res.s[3] = x[3];
        return res;
    }

    void store(float* x) const {
        x[0] = s[0]; x[1] = s[1]; x[2] = s[2]; x[3] = s[3];
    }
#endif

    float& operator[](int i) { return s[i]; }
    const float& operator[](int i) const { return s[i]; }
};

using float_4 = Vector<float, 4>;

// Basic vector operators
#if defined(OVERVIBER_SIMD_SSE)
inline float_4 operator+(const float_4& a, const float_4& b) { return float_4(_mm_add_ps(a.v, b.v)); }
inline float_4 operator-(const float_4& a, const float_4& b) { return float_4(_mm_sub_ps(a.v, b.v)); }
inline float_4 operator*(const float_4& a, const float_4& b) { return float_4(_mm_mul_ps(a.v, b.v)); }
inline float_4 operator/(const float_4& a, const float_4& b) { return float_4(_mm_div_ps(a.v, b.v)); }
inline float_4 operator-(const float_4& a) { return float_4(_mm_sub_ps(_mm_setzero_ps(), a.v)); }
inline float_4 operator&(const float_4& a, const float_4& b) { return float_4(_mm_and_ps(a.v, b.v)); }
inline float_4 operator|(const float_4& a, const float_4& b) { return float_4(_mm_or_ps(a.v, b.v)); }
inline float_4 operator^(const float_4& a, const float_4& b) { return float_4(_mm_xor_ps(a.v, b.v)); }
inline float_4 fmin(float_4 a, float_4 b) { return float_4(_mm_min_ps(a.v, b.v)); }
inline float_4 fmax(float_4 a, float_4 b) { return float_4(_mm_max_ps(a.v, b.v)); }
#elif defined(OVERVIBER_SIMD_NEON)
inline float_4 operator+(const float_4& a, const float_4& b) { return float_4(vaddq_f32(a.v, b.v)); }
inline float_4 operator-(const float_4& a, const float_4& b) { return float_4(vsubq_f32(a.v, b.v)); }
inline float_4 operator*(const float_4& a, const float_4& b) { return float_4(vmulq_f32(a.v, b.v)); }
inline float_4 operator/(const float_4& a, const float_4& b) { return float_4(vdivq_f32(a.v, b.v)); }
inline float_4 operator-(const float_4& a) { return float_4(vnegq_f32(a.v)); }
inline float_4 operator&(const float_4& a, const float_4& b) {
    return float_4(vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a.v), vreinterpretq_u32_f32(b.v))));
}
inline float_4 operator|(const float_4& a, const float_4& b) {
    return float_4(vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(a.v), vreinterpretq_u32_f32(b.v))));
}
inline float_4 operator^(const float_4& a, const float_4& b) {
    return float_4(vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(a.v), vreinterpretq_u32_f32(b.v))));
}
inline float_4 fmin(float_4 a, float_4 b) { return float_4(vminq_f32(a.v, b.v)); }
inline float_4 fmax(float_4 a, float_4 b) { return float_4(vmaxq_f32(a.v, b.v)); }
#else // Scalar Fallback
inline float_4 operator+(const float_4& a, const float_4& b) { return float_4(a[0]+b[0], a[1]+b[1], a[2]+b[2], a[3]+b[3]); }
inline float_4 operator-(const float_4& a, const float_4& b) { return float_4(a[0]-b[0], a[1]-b[1], a[2]-b[2], a[3]-b[3]); }
inline float_4 operator*(const float_4& a, const float_4& b) { return float_4(a[0]*b[0], a[1]*b[1], a[2]*b[2], a[3]*b[3]); }
inline float_4 operator/(const float_4& a, const float_4& b) { return float_4(a[0]/b[0], a[1]/b[1], a[2]/b[2], a[3]/b[3]); }
inline float_4 operator-(const float_4& a) { return float_4(-a[0], -a[1], -a[2], -a[3]); }
inline float_4 operator&(const float_4& a, const float_4& b) {
    float_4 r;
    uint32_t ua[4], ub[4], ur[4];
    std::memcpy(ua, a.s, sizeof(ua));
    std::memcpy(ub, b.s, sizeof(ub));
    for (int i = 0; i < 4; ++i) ur[i] = ua[i] & ub[i];
    std::memcpy(r.s, ur, sizeof(ur));
    return r;
}
inline float_4 operator|(const float_4& a, const float_4& b) {
    float_4 r;
    uint32_t ua[4], ub[4], ur[4];
    std::memcpy(ua, a.s, sizeof(ua));
    std::memcpy(ub, b.s, sizeof(ub));
    for (int i = 0; i < 4; ++i) ur[i] = ua[i] | ub[i];
    std::memcpy(r.s, ur, sizeof(ur));
    return r;
}
inline float_4 operator^(const float_4& a, const float_4& b) {
    float_4 r;
    uint32_t ua[4], ub[4], ur[4];
    std::memcpy(ua, a.s, sizeof(ua));
    std::memcpy(ub, b.s, sizeof(ub));
    for (int i = 0; i < 4; ++i) ur[i] = ua[i] ^ ub[i];
    std::memcpy(r.s, ur, sizeof(ur));
    return r;
}
inline float_4 fmin(float_4 a, float_4 b) {
    return float_4(std::min(a[0], b[0]), std::min(a[1], b[1]), std::min(a[2], b[2]), std::min(a[3], b[3]));
}
inline float_4 fmax(float_4 a, float_4 b) {
    return float_4(std::max(a[0], b[0]), std::max(a[1], b[1]), std::max(a[2], b[2]), std::max(a[3], b[3]));
}
#endif

inline float_4& operator+=(float_4& a, const float_4& b) { return a = a + b; }
inline float_4& operator-=(float_4& a, const float_4& b) { return a = a - b; }
inline float_4& operator*=(float_4& a, const float_4& b) { return a = a * b; }
inline float_4& operator/=(float_4& a, const float_4& b) { return a = a / b; }

inline float_4 operator+(const float_4& a, float b) { return a + float_4(b); }
inline float_4 operator-(const float_4& a, float b) { return a - float_4(b); }
inline float_4 operator*(const float_4& a, float b) { return a * float_4(b); }
inline float_4 operator/(const float_4& a, float b) { return a / float_4(b); }

inline float_4 operator+(float a, const float_4& b) { return float_4(a) + b; }
inline float_4 operator-(float a, const float_4& b) { return float_4(a) - b; }
inline float_4 operator*(float a, const float_4& b) { return float_4(a) * b; }
inline float_4 operator/(float a, const float_4& b) { return float_4(a) / b; }

inline float_4 fmin(float_4 a, float b) { return fmin(a, float_4(b)); }
inline float_4 fmax(float_4 a, float b) { return fmax(a, float_4(b)); }

inline float_4 clamp(float_4 x, float_4 a = 0.f, float_4 b = 1.f) {
    return fmin(fmax(x, a), b);
}
inline float_4 clamp(float_4 x, float a, float b) {
    return clamp(x, float_4(a), float_4(b));
}

inline float_4 rescale(float_4 x, float_4 xMin, float_4 xMax, float_4 yMin, float_4 yMax) {
    return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
}
inline float_4 rescale(float_4 x, float xMin, float xMax, float yMin, float yMax) {
    return rescale(x, float_4(xMin), float_4(xMax), float_4(yMin), float_4(yMax));
}

inline float_4 exp(float_4 x) {
    return float_4(
        std::exp(x[0]),
        std::exp(x[1]),
        std::exp(x[2]),
        std::exp(x[3])
    );
}

inline float_4 exp2(float_4 x) {
    return float_4(
        std::exp2(x[0]),
        std::exp2(x[1]),
        std::exp2(x[2]),
        std::exp2(x[3])
    );
}

inline float_4 pow(float_4 a, float_4 b) {
    return float_4(
        std::pow(a[0], b[0]),
        std::pow(a[1], b[1]),
        std::pow(a[2], b[2]),
        std::pow(a[3], b[3])
    );
}

inline float_4 pow(float a, float_4 b) {
    return float_4(
        std::pow(a, b[0]),
        std::pow(a, b[1]),
        std::pow(a, b[2]),
        std::pow(a, b[3])
    );
}

} // namespace simd

namespace dsp {

template <typename T = float>
struct TRCFilter {
    T c = 0.f;
    T xstate[1];
    T ystate[1];

    TRCFilter() {
        reset();
    }

    void reset() {
        xstate[0] = 0.f;
        ystate[0] = 0.f;
    }

    void setCutoff(T r) {
        c = 2.f / r;
    }

    void setCutoffFreq(T f) {
        setCutoff(2.f * (float)M_PI * f);
    }

    void process(T x) {
        T y = (x + xstate[0] - ystate[0] * (1.f - c)) / (1.f + c);
        xstate[0] = x;
        ystate[0] = y;
    }

    T lowpass() {
        return ystate[0];
    }

    T highpass() {
        return xstate[0] - ystate[0];
    }
};

template <typename T = float>
struct TSlewLimiter {
    T out = 0.f;
    T rise = 0.f;
    T fall = 0.f;

    void reset() {
        out = 0.f;
    }

    void setRiseFall(T r, T f) {
        this->rise = r;
        this->fall = f;
    }

    T process(float deltaTime, T in) {
        T delta = in - out;
        if (delta > 0.f) {
            out += std::min(delta, rise * deltaTime);
        } else {
            out += std::max(delta, -fall * deltaTime);
        }
        return out;
    }
};

using SlewLimiter = TSlewLimiter<float>;

} // namespace dsp

} // namespace rack
