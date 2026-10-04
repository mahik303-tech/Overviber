#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <functional>

// ==============================================================================
// Small symbols for option buttons (ModernChoiceButtons::GlyphPainter): the
// response of a filter or EQ band and the curve of an envelope, drawn as a
// line in the button's text colour, with a short text beside them.
// ==============================================================================
namespace modernglyphs {

enum class Response { lowpass, highpass, bandpass, notch, lowShelf, peak, highShelf };

// The response in r: pass band high, stop band low. steepness 0..1 is the
// low- or highpass slope (6 .. 24 dB per octave).
inline juce::Path response(juce::Rectangle<float> r, Response type, float steepness = 1.0f) {
    juce::Path p;
    const float x0 = r.getX(), x1 = r.getRight(), w = r.getWidth(), cx = r.getCentreX();
    const float top = r.getY() + 1.0f, bottom = r.getBottom(), low = r.getY() + r.getHeight() * 0.72f;
    switch (type) {
        case Response::lowpass:
        case Response::highpass: {
            const float knee = x0 + w * 0.42f, fall = w * (0.58f - 0.4f * steepness);
            p.startNewSubPath(x0, top);
            p.lineTo(knee, top);
            p.quadraticTo(knee + fall * 0.45f, top, knee + fall, bottom);
            if (type == Response::highpass) p.applyTransform(juce::AffineTransform::scale(-1.0f, 1.0f, cx, 0.0f));
            break;
        }
        case Response::bandpass:
            p.startNewSubPath(x0 + w * 0.08f, bottom);
            p.quadraticTo(cx - w * 0.14f, top, cx, top);
            p.quadraticTo(cx + w * 0.14f, top, x1 - w * 0.08f, bottom);
            break;
        case Response::notch:
            p.startNewSubPath(x0, top);
            p.lineTo(cx - w * 0.24f, top);
            p.quadraticTo(cx - w * 0.04f, top, cx, bottom);
            p.quadraticTo(cx + w * 0.04f, top, cx + w * 0.24f, top);
            p.lineTo(x1, top);
            break;
        case Response::lowShelf:
        case Response::highShelf:
            p.startNewSubPath(x0, top);
            p.lineTo(cx - w * 0.2f, top);
            p.cubicTo(cx, top, cx, low, cx + w * 0.2f, low);
            p.lineTo(x1, low);
            if (type == Response::highShelf) p.applyTransform(juce::AffineTransform::scale(-1.0f, 1.0f, cx, 0.0f));
            break;
        case Response::peak:
            p.startNewSubPath(x0, low);
            p.lineTo(cx - w * 0.3f, low);
            p.cubicTo(cx - w * 0.1f, low, cx - w * 0.1f, top, cx, top);
            p.cubicTo(cx + w * 0.1f, top, cx + w * 0.1f, low, cx + w * 0.3f, low);
            p.lineTo(x1, low);
            break;
    }
    return p;
}

// An envelope's rise and fall in r: straight lines, or the exponential
// curves of the analogue stages (fast start, long tail).
inline juce::Path envelope(juce::Rectangle<float> r, bool linear) {
    juce::Path p;
    const float x0 = r.getX(), w = r.getWidth(), top = r.getY() + 1.0f, bottom = r.getBottom();
    const float peakX = x0 + w * 0.3f, endX = r.getRight();
    p.startNewSubPath(x0, bottom);
    if (linear) {
        p.lineTo(peakX, top);
        p.lineTo(endX, bottom);
    } else {
        p.quadraticTo(x0 + w * 0.02f, top, peakX, top);
        p.quadraticTo(peakX + w * 0.06f, bottom, endX, bottom);
    }
    return p;
}

enum class WaveMod { grit, pwm, fm, morph, fold, crush };

// One cycle of what a WaveMod type does to a sine, in r.
inline juce::Path waveMod(juce::Rectangle<float> r, WaveMod type) {
    constexpr int points = 48;
    const float twoPi = juce::MathConstants<float>::twoPi;
    juce::Path p;
    for (int i = 0; i <= points; ++i) {
        const float t = (float)i / (float)points;
        float y = std::sin(twoPi * t);
        switch (type) {
            case WaveMod::grit: y = 0.75f * y + 0.25f * std::sin(twoPi * 13.0f * t) * std::sin(twoPi * 5.0f * t); break;
            case WaveMod::pwm: y = t < 0.3f ? 1.0f : -1.0f; break;
            case WaveMod::fm: y = std::sin(twoPi * t + 2.2f * std::sin(twoPi * 2.0f * t)); break;
            case WaveMod::morph: y = t < 0.5f ? std::sin(twoPi * t) : 2.0f * (t - 0.5f) * 2.0f - 1.0f; break;
            case WaveMod::fold: {
                float x = 1.8f * y;
                if (x > 1.0f) x = 2.0f - x;
                if (x < -1.0f) x = -2.0f - x;
                y = x;
                break;
            }
            case WaveMod::crush: y = std::round(y * 2.0f) / 2.0f; break;
        }
        const float px = r.getX() + r.getWidth() * t, py = r.getCentreY() - y * r.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath(px, py);
        else if (type == WaveMod::pwm || type == WaveMod::crush) {
            // steps: vertical edges
            p.lineTo(px, p.getCurrentPosition().y);
            p.lineTo(px, py);
        } else p.lineTo(px, py);
    }
    return p;
}

// The symbol (made for its rectangle) and the text beside it, centred as a
// pair in area; the text shrinks to fit.
template <typename SymbolMaker>
void drawWithText(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, const juce::Font& font,
                  juce::Colour colour, SymbolMaker&& makeSymbol) {
    const float symbolH = std::min(area.getHeight() - 2.0f, 12.0f);
    const float symbolW = symbolH * 2.0f, gap = text.isEmpty() ? 0.0f : 6.0f;
    const float textW = text.isEmpty() ? 0.0f
                                       : std::min(font.getStringWidthFloat(text) + 2.0f, area.getWidth() - symbolW - gap);
    const float x = area.getX() + (area.getWidth() - symbolW - gap - textW) * 0.5f;
    const juce::Rectangle<float> symbolArea(x, area.getCentreY() - symbolH * 0.5f, symbolW, symbolH);
    g.setColour(colour);
    g.strokePath(makeSymbol(symbolArea), juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (text.isEmpty()) return;
    g.setFont(font);
    g.drawFittedText(text, juce::Rectangle<float>(x + symbolW + gap, area.getY(), textW, area.getHeight()).toNearestInt(),
                     juce::Justification::centredLeft, 1, 0.7f);
}

// A text button that draws a symbol beside its text (none: a plain text
// button), in the colours of the look and feel.
class SymbolButton : public juce::TextButton {
public:
    using SymbolMaker = std::function<juce::Path(juce::Rectangle<float>)>;
    SymbolButton(const juce::String& label, SymbolMaker maker) : juce::TextButton(label), symbol(std::move(maker)) {}

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override {
        auto& lnf = getLookAndFeel();
        lnf.drawButtonBackground(g, *this, findColour(getToggleState() ? buttonOnColourId : buttonColourId), highlighted, down);
        if (!symbol) {
            lnf.drawButtonText(g, *this, highlighted, down);
            return;
        }
        const auto colour = findColour(getToggleState() ? textColourOnId : textColourOffId)
                                .withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f);
        drawWithText(g, getLocalBounds().toFloat().reduced(6.0f, 4.0f), getButtonText(),
                     lnf.getTextButtonFont(*this, getHeight()), colour, symbol);
    }

private:
    SymbolMaker symbol;
};

} // namespace modernglyphs
