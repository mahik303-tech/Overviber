#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../data/OutputScope.h"
#include "../theme/ModernTheme.h"
#include "SpectrumAnalyser.h"
#include <array>
#include <vector>

// The master output's spectrum as a calm 8-bit style background for the
// filter, envelope and LFO displays: frequency bands (20 Hz .. 20 kHz, one
// per about 14 px of the owner's width), each a column of square cells in the meters' palette (accent dark,
// accent, white, red towards the top), with a peak cap per band. Levels
// rise smoothly, fall 20 dB/s; caps hold a second, then sink 10 dB/s. The
// owner calls update() about 12 times a second while it is on screen and
// draw() in its paint(), behind its own curve.
//
// Randomness (0..1, settings): one random value per cell, redrawn about four
// times a second, drops cells out at its low end (up to half of them at
// full randomness) and doubles the opacity of accent cells at its high end
// (the same share). The peak caps are never touched.
class RetroSpectrum {
public:
    static constexpr int kMinBands = 16, kMaxBands = 64;
    static constexpr float kBandPixels = 14.0f;
    static constexpr float kTopDb = 0.0f, kBottomDb = -72.0f;
    static constexpr float kUpdateHz = 12.0f;
    static constexpr int kPatternUpdates = 3;   // new random pattern every 3 updates (4 Hz)

    explicit RetroSpectrum(OutputScope& source) : scope(source) { reset(); }

    void reset() {
        levels.assign((size_t)bands, kBottomDb);
        peaks.assign((size_t)bands, kBottomDb);
        peakHold.assign((size_t)bands, 0);
    }

    // The owner's drawing width decides the band count (resets on change).
    void setWidth(float width) {
        const int count = juce::jlimit(kMinBands, kMaxBands, (int)(width / kBandPixels));
        if (count != bands) { bands = count; reset(); }
    }

    // Returns true while something is lit (the owner repaints then).
    bool update() {
        scope.copyLatest(samples.data(), SpectrumAnalyser::kSize);
        analyser.analyse(samples.data(), scope.getSampleRate(), bands, bandLevels);
        constexpr float fall = 20.0f / kUpdateHz, peakFall = 10.0f / kUpdateHz;
        constexpr int holdUpdates = (int)kUpdateHz;   // one second
        if (++patternTick >= kPatternUpdates) { patternTick = 0; ++patternSeed; }
        bool lit = false;
        for (int b = 0; b < bands; ++b) {
            const float target = std::max(kBottomDb, bandLevels[(size_t)b]);
            float& level = levels[(size_t)b];
            level = target > level ? level + 0.5f * (target - level) : std::max(target, level - fall);
            if (level >= peaks[(size_t)b]) { peaks[(size_t)b] = level; peakHold[(size_t)b] = holdUpdates; }
            else if (peakHold[(size_t)b] > 0) --peakHold[(size_t)b];
            else peaks[(size_t)b] = std::max(kBottomDb, peaks[(size_t)b] - peakFall);
            lit |= peaks[(size_t)b] > kBottomDb + 1.0f;
        }
        return lit;
    }

    bool isLit() const {
        for (float p : peaks) if (p > kBottomDb + 1.0f) return true;
        return false;
    }

    void draw(juce::Graphics& g, juce::Rectangle<float> area, const ModernTheme& theme, float opacity,
              float randomness = 0.0f) const {
        if (area.getWidth() < (float)bands * 2.0f || area.getHeight() < 8.0f || !isLit()) return;
        const float columnW = area.getWidth() / (float)bands;
        const float gap = std::max(1.0f, std::floor(columnW * 0.18f));
        const float cell = std::max(2.0f, std::floor(columnW - gap));
        const int rows = std::max(1, (int)(area.getHeight() / (cell + gap)));
        const juce::Colour red(0xffe53935);
        auto isAccentRow = [&](int row) {
            const float h = (float)(row + 1) / (float)rows;
            return h > 0.55f && h <= 0.8f;
        };
        auto colourForRow = [&](int row) {
            const float h = (float)(row + 1) / (float)rows;
            return h > 0.92f ? red : h > 0.8f ? juce::Colours::white : h > 0.55f ? theme.accent : theme.accentDark;
        };
        const float share = 0.5f * juce::jlimit(0.0f, 1.0f, randomness);
        auto rowsFor = [&](float db) {
            return (int)std::round(juce::jlimit(0.0f, 1.0f, (db - kBottomDb) / (kTopDb - kBottomDb)) * (float)rows);
        };
        for (int b = 0; b < bands; ++b) {
            const float x = std::floor(area.getX() + (float)b * columnW + gap * 0.5f);
            const int litRows = rowsFor(levels[(size_t)b]);
            for (int r = 0; r < litRows; ++r) {
                float alpha = opacity;
                if (share > 0.0f) {
                    const float chance = cellRandom(b, r);
                    if (chance < share) continue;                             // dropped out
                    if (chance >= 1.0f - share && isAccentRow(r)) alpha = std::min(1.0f, opacity * 2.0f);
                }
                const float y = area.getBottom() - (float)(r + 1) * (cell + gap) + gap;
                g.setColour(colourForRow(r).withMultipliedAlpha(alpha));
                g.fillRect(x, y, cell, cell);
            }
            const int peakRow = rowsFor(peaks[(size_t)b]) - 1;
            if (peakRow >= litRows && peakRow >= 0) {
                const float y = area.getBottom() - (float)(peakRow + 1) * (cell + gap) + gap;
                g.setColour(colourForRow(peakRow).withMultipliedAlpha(opacity * 1.2f));
                g.fillRect(x, y, cell, cell);
            }
        }
    }

private:
    // 0..1, fixed per cell until the next pattern.
    float cellRandom(int band, int row) const {
        uint32_t h = (uint32_t)band * 0x9E3779B1u ^ (uint32_t)row * 0x85EBCA77u ^ patternSeed * 0xC2B2AE3Du;
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
        return (float)(h >> 8) / 16777216.0f;
    }

    OutputScope& scope;
    SpectrumAnalyser analyser;
    std::vector<float> samples = std::vector<float>(SpectrumAnalyser::kSize);
    std::vector<float> bandLevels;
    int bands = 24;
    std::vector<float> levels, peaks;
    std::vector<int> peakHold;
    uint32_t patternSeed = 1;
    int patternTick = 0;
};
