#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../data/OutputScope.h"
#include "../theme/ModernTheme.h"
#include "SpectrumAnalyser.h"
#include <algorithm>
#include <deque>
#include <vector>

// The master output's spectrum as a line waterfall (one level per 8 px of
// the owner's width, coarse on purpose): the newest spectrum as a filled
// area in the meters' colours, older ones falling 3 px per update and
// paler. Levels rise at once and fall 45 dB/s. The owner calls update() on
// its spectrum timer while it is on screen and draw() in its paint().
class SpectrumWaterfall {
public:
    static constexpr int kHistory = 8;
    static constexpr float kColumnPixels = 8.0f;

    SpectrumWaterfall(OutputScope& source, float updateHz) : scope(source), fallDb(45.0f / updateHz) {}

    void reset() { history.clear(); held.clear(); }
    bool isActive() const { return !history.empty(); }

    // The owner's drawing width decides the column count.
    void setWidth(float width) { columns = std::max(1, (int)(width / kColumnPixels)); }

    void update() {
        if (columns <= 0) return;
        scope.copyLatest(samples.data(), SpectrumAnalyser::kSize);
        std::vector<float> levels;
        analyser.analyse(samples.data(), scope.getSampleRate(), columns, levels);
        if (held.size() != levels.size()) held.assign(levels.size(), SpectrumAnalyser::kFloorDb);
        bool silent = true;
        for (size_t c = 0; c < levels.size(); ++c) {
            levels[c] = held[c] = std::max(levels[c], held[c] - fallDb);
            silent &= levels[c] <= SpectrumAnalyser::kFloorDb + 0.5f;
        }
        if (silent && history.empty()) return;
        history.push_front(silent ? std::vector<float>() : std::move(levels));
        while ((int)history.size() > kHistory) history.pop_back();
        if (std::all_of(history.begin(), history.end(), [](const auto& s) { return s.empty(); })) history.clear();
    }

    // Levels from -90 dBFS (bottom) to 0 dBFS (top).
    void draw(juce::Graphics& g, juce::Rectangle<float> area, const ModernTheme& theme, float opacity) const {
        if (history.empty()) return;
        opacity = juce::jlimit(0.0f, 1.0f, opacity);
        const juce::Colour over(0xffe53935);
        auto yFor = [&](float db, float drop) {
            const float t = juce::jlimit(0.0f, 1.0f, (db - SpectrumAnalyser::kFloorDb) / -SpectrumAnalyser::kFloorDb);
            return std::min(area.getBottom(), area.getBottom() - t * area.getHeight() + drop);
        };
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(area.toNearestInt());
        for (int k = (int)history.size() - 1; k >= 0; --k) {
            const auto& levels = history[(size_t)k];
            if (levels.empty()) continue;
            const float drop = (float)k * 3.0f;
            const float fade = std::pow(1.0f - (float)k / (float)kHistory, 2.0f);
            juce::Path line;
            const float step = area.getWidth() / (float)levels.size();
            for (size_t c = 0; c < levels.size(); ++c) {
                const float x = area.getX() + ((float)c + 0.5f) * step;
                const float y = yFor(levels[c], drop);
                if (c == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
            }
            if (k == 0) {
                juce::Path fill(line);
                fill.lineTo(area.getRight(), area.getBottom());
                fill.lineTo(area.getX(), area.getBottom());
                fill.closeSubPath();
                juce::ColourGradient grad(over.withAlpha(0.30f * opacity), 0.0f, area.getY(),
                                          theme.accentDark.withAlpha(0.05f * opacity), 0.0f, area.getBottom(), false);
                grad.addColour(0.15, juce::Colours::white.withAlpha(0.22f * opacity));
                grad.addColour(0.35, theme.accent.withAlpha(0.18f * opacity));
                g.setGradientFill(grad);
                g.fillPath(fill);
            }
            g.setColour(theme.accent.interpolatedWith(juce::Colours::white, k == 0 ? 0.25f : 0.0f)
                            .withAlpha(0.65f * fade * opacity));
            g.strokePath(line, juce::PathStrokeType(k == 0 ? 1.2f : 1.0f));
        }
    }

private:
    OutputScope& scope;
    float fallDb;
    int columns = 0;
    SpectrumAnalyser analyser;
    std::vector<float> samples = std::vector<float>(SpectrumAnalyser::kSize);
    std::vector<float> held;
    std::deque<std::vector<float>> history;   // newest first
};
