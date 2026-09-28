#include "ModernTelemetryComponents.h"
#include <cmath>
#include <algorithm>

// ==============================================================================
// ModernVoiceMeterPanel Implementation (Mixing Desk with Vertical Meters & ConsoleX)
// ==============================================================================
void ModernVoiceMeterPanel::ConsoleFaderLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    const juce::Slider::SliderStyle style, juce::Slider& slider) {
    juce::ignoreUnused(minSliderPos, maxSliderPos, style);
    if (width <= 0 || height <= 0) return;

    auto theme = getTheme();
    bool isHovered = slider.isMouseOverOrDragging();
    bool hasFocus = slider.hasKeyboardFocus(true);

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    float midX = bounds.getCentreX();

    // 1. Recessed Fader Track Slot
    float trackW = 4.0f;
    auto trackRect = juce::Rectangle<float>(midX - trackW * 0.5f, bounds.getY() + 4.0f, trackW, bounds.getHeight() - 8.0f);
    g.setColour(theme.windowBg);
    g.fillRect(trackRect);
    g.setColour(theme.cardBorder);
    g.drawRect(trackRect, 1.0f);

    // Track active fill from bottom to slider thumb
    float fillTop = sliderPos;
    float fillBottom = bounds.getBottom() - 4.0f;
    if (fillBottom > fillTop) {
        auto fillRect = juce::Rectangle<float>(midX - 1.0f, fillTop, 2.0f, fillBottom - fillTop);
        g.setColour(theme.accent.withAlpha(0.6f));
        g.fillRect(fillRect);
    }

    // 2. Hardware Console Fader Cap
    float capW = std::clamp(bounds.getWidth() - 2.0f, 16.0f, 26.0f);
    float capH = 13.0f;
    float capY = std::clamp(sliderPos - capH * 0.5f, bounds.getY(), bounds.getBottom() - capH);
    auto capRect = juce::Rectangle<float>(midX - capW * 0.5f, capY, capW, capH);

    // Metallic beveled fader body
    juce::ColourGradient capGrad(theme.cardHeader.brighter(0.15f), capRect.getX(), capRect.getY(),
                                 theme.cardBg.darker(0.2f), capRect.getX(), capRect.getBottom(), false);
    g.setGradientFill(capGrad);
    g.fillRect(capRect);

    // Fader border
    g.setColour(hasFocus ? juce::Colours::white : (isHovered ? theme.accent : theme.cardBorder));
    g.drawRect(capRect, 1.0f);

    // Grip texture lines
    g.setColour(theme.cardBorder.darker(0.3f));
    g.drawHorizontalLine((int)(capRect.getY() + 3.0f), capRect.getX() + 2.0f, capRect.getRight() - 2.0f);
    g.drawHorizontalLine((int)(capRect.getBottom() - 4.0f), capRect.getX() + 2.0f, capRect.getRight() - 2.0f);

    // Center illuminated indicator line
    g.setColour(theme.accent);
    g.fillRect(capRect.getX() + 2.0f, capRect.getCentreY() - 1.0f, capRect.getWidth() - 4.0f, 2.0f);
}

ModernVoiceMeterPanel::ModernVoiceMeterPanel(SynthEngine& eng) : engine(eng) {
    faderLnf.setTheme(ModernTheme::getPresetThemes()[0]);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voiceFaders[v] = std::make_unique<juce::Slider>(juce::Slider::LinearVertical, juce::Slider::NoTextBox);
        voiceFaders[v]->setLookAndFeel(&faderLnf);
        voiceFaders[v]->setRange(0.0, 1.25, 0.01);
        voiceFaders[v]->setValue(engine.getVoiceFader(v), juce::dontSendNotification);
        voiceFaders[v]->setComponentID("voiceMeterPanel_fader[" + juce::String(v) + "]");
        voiceFaders[v]->onValueChange = [this, v]() {
            engine.setVoiceFader(v, (float)voiceFaders[v]->getValue());
            repaint();
        };
        addAndMakeVisible(*voiceFaders[v]);

        voicePans[v] = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        voicePans[v]->setLookAndFeel(&faderLnf);
        voicePans[v]->setRange(-1.0, 1.0, 0.01);
        voicePans[v]->setValue(engine.getVoicePan(v), juce::dontSendNotification);
        voicePans[v]->setDoubleClickReturnValue(true, 0.0);
        voicePans[v]->setComponentID("voiceMeterPanel_pan[" + juce::String(v) + "]");
        voicePans[v]->setTooltip("Voice " + juce::String(v + 1) + " pan");
        voicePans[v]->onValueChange = [this, v]() {
            engine.setVoicePan(v, (float)voicePans[v]->getValue());
            repaint();
        };
        addAndMakeVisible(*voicePans[v]);
    }

    masterFader = std::make_unique<juce::Slider>(juce::Slider::LinearVertical, juce::Slider::NoTextBox);
    masterFader->setLookAndFeel(&faderLnf);
    masterFader->setRange(0.0, 999.0, 1.0);
    masterFader->setValue(scan_potFrom16bits(engine.getCurrentPreset().continuousParams[cpMackityOutPad]), juce::dontSendNotification);
    masterFader->setComponentID("voiceMeterPanel_masterFader");
    masterFader->onValueChange = [this]() {
        engine.setContinuousParam(cpMackityOutPad, (uint16_t)scan_potTo16bits((int)masterFader->getValue()));
        repaint();
    };
    addAndMakeVisible(*masterFader);
}

ModernVoiceMeterPanel::~ModernVoiceMeterPanel() {
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (voiceFaders[v]) voiceFaders[v]->setLookAndFeel(nullptr);
        if (voicePans[v]) voicePans[v]->setLookAndFeel(nullptr);
    }
    if (masterFader) masterFader->setLookAndFeel(nullptr);
}

void ModernVoiceMeterPanel::resized() {
    auto bounds = getLocalBounds().toFloat();
    float startY = 32.0f;
    float footerH = 22.0f;
    float usableH = bounds.getHeight() - startY - footerH;

    float marginX = 8.0f;
    float totalW = bounds.getWidth() - marginX * 2.0f;
    int numStrips = SYNTH_VOICE_COUNT + 1; // 6 Voices + 1 Master Strip
    float stripW = totalW / (float)numStrips;

    float faderTop = startY + 58.0f;
    float faderH = std::max(50.0f, usableH - 84.0f);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        float sx = marginX + (float)v * stripW;
        if (voicePans[v]) {
            float panSize = std::clamp(stripW - 16.0f, 24.0f, 32.0f);
            voicePans[v]->setBounds((int)(sx + (stripW - panSize) * 0.5f - 1.5f),
                                    (int)(startY + 20.0f), (int)panSize, (int)panSize);
        }
        if (voiceFaders[v]) {
            // Position fader on right side of strip, meter will be drawn on left
            float faderW = std::clamp(stripW * 0.48f, 22.0f, 32.0f);
            float faderX = sx + stripW - faderW - 4.0f;
            voiceFaders[v]->setBounds((int)faderX, (int)faderTop, (int)faderW, (int)faderH);
        }
    }

    if (masterFader) {
        float sx = marginX + (float)SYNTH_VOICE_COUNT * stripW;
        float faderW = std::clamp(stripW * 0.48f, 22.0f, 32.0f);
        float faderX = sx + stripW - faderW - 4.0f;
        masterFader->setBounds((int)faderX, (int)faderTop, (int)faderW, (int)faderH);
    }
}

void ModernVoiceMeterPanel::updateLevels(const float* levels) {
    float peakSum = 0.0f;
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        currentLevels[v] = levels[v];
        peakSum += levels[v];
        if (voicePans[v] && !voicePans[v]->isMouseButtonDown())
            voicePans[v]->setValue(engine.getVoicePan(v), juce::dontSendNotification);
    }
    masterPeakL = masterPeakL * 0.7f + (peakSum * 0.22f) * 0.3f;
    masterPeakR = masterPeakR * 0.7f + (peakSum * 0.22f) * 0.3f;
    repaint();
}

void ModernVoiceMeterPanel::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];
    faderLnf.setTheme(theme);

    // Chassis frame
    g.setColour(theme.cardBg);
    g.fillRect(bounds);
    g.setColour(theme.cardBorder);
    g.drawRect(bounds, 1.0f);

    // Header bar matching ModernSectionCard styling
    auto headerRect = bounds.withHeight(24.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(headerRect);

    // Accent line underneath header
    g.setColour(theme.accent);
    g.fillRect(bounds.getX(), 23.0f, bounds.getWidth(), 1.5f);

    // Header title
    g.setFont(lnf ? lnf->getCustomFont(11.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::bold));
    g.setColour(theme.textTitle);
    g.drawText("VOICE CONSOLE MIXER & TELEMETRY", 10, 2, (int)bounds.getWidth() - 200, 20, juce::Justification::centredLeft, false);

    // Badge indicating master summing engine
    uint8_t cModel = engine.getCurrentPreset().steppedParams[spConsoleModel];
    juce::String badge = (cModel == cmConsoleX) ? "AIRWINDOWS CONSOLEX PHI BUS" :
                         ((cModel == cmMackity) ? "AIRWINDOWS MACKITY BUS" : "CLEAN MIX BUS");
    g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::bold));
    int badgeW = (int)g.getCurrentFont().getStringWidth(badge) + 12;
    auto badgeRect = juce::Rectangle<float>(bounds.getRight() - badgeW - 6.0f, 4.0f, (float)badgeW, 16.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(badgeRect);
    g.setColour(cModel == cmConsoleX ? theme.accent : theme.cardBorder);
    g.drawRect(badgeRect, 1.0f);
    g.setColour(cModel == cmConsoleX ? theme.accent : theme.textMuted);
    g.drawText(badge, badgeRect, juce::Justification::centred, false);

    // 7 Channel Strips (6 Voices + 1 Master Bus)
    float startY = 30.0f;
    float footerH = 22.0f;
    float usableH = bounds.getHeight() - startY - footerH;

    float marginX = 8.0f;
    float totalW = bounds.getWidth() - marginX * 2.0f;
    int numStrips = SYNTH_VOICE_COUNT + 1;
    float stripW = totalW / (float)numStrips;

    float faderTop = startY + 58.0f;
    float faderH = std::max(50.0f, usableH - 84.0f);

    const int numSegments = 16;
    float segGap = 1.5f;

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        float sx = marginX + (float)v * stripW;
        auto stripRect = juce::Rectangle<float>(sx, startY + 2.0f, stripW - 3.0f, usableH);
        // The engine supplies a real post-fader audio peak. Map amplitude to the
        // labels painted alongside the meter: -6 dB at 50%, 0 dB at 80%, +2 dB
        // at the top. This keeps a full amp envelope from masquerading as clip.
        const float meterDb = std::max(-60.0f, 20.0f * std::log10(std::max(currentLevels[v], 0.000001f)));
        float lvl = 0.0f;
        if (meterDb <= -6.0f)
            lvl = juce::jmap(meterDb, -60.0f, -6.0f, 0.0f, 0.5f);
        else if (meterDb <= 0.0f)
            lvl = juce::jmap(meterDb, -6.0f, 0.0f, 0.5f, 0.8f);
        else
            lvl = juce::jmap(std::min(meterDb, 2.0f), 0.0f, 2.0f, 0.8f, 1.0f);
        lvl = std::clamp(lvl, 0.0f, 1.0f);
        bool isActive = (lvl > 0.015f);

        // Strip background
        g.setColour(v % 2 == 0 ? theme.cardBg : theme.windowBg);
        g.fillRect(stripRect);
        g.setColour(theme.cardBorder);
        g.drawRect(stripRect, 1.0f);

        // Channel header: Voice label & Gate LED
        g.setFont(lnf ? lnf->getCustomFont(9.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.5f, juce::Font::bold));
        g.setColour(isActive ? theme.textTitle : theme.textMuted);
        g.drawText("CH " + juce::String(v + 1), (int)sx + 4, (int)startY + 4, (int)stripW - 20, 14, juce::Justification::left, false);

        // Gate LED indicator
        float ledSz = 7.0f;
        float ledX = sx + stripW - ledSz - 6.0f;
        float ledY = startY + 7.0f;
        g.setColour(isActive ? theme.accent : theme.knobTrack);
        g.fillRect(ledX, ledY, ledSz, ledSz);
        g.setColour(theme.cardBorder);
        g.drawRect(ledX, ledY, ledSz, ledSz, 0.8f);

        const float pan = voicePans[v] ? (float)voicePans[v]->getValue() : 0.0f;
        const juce::String panText = std::abs(pan) < 0.005f ? "C"
            : (pan < 0.0f ? "L" : "R") + juce::String((int)std::round(std::abs(pan) * 100.0f));
        g.setFont(lnf ? lnf->getCustomFont(7.5f, juce::Font::bold) : juce::Font(7.5f));
        g.setColour(theme.textMuted);
        g.drawText(panText, (int)sx + 2, (int)(faderTop - 10.0f), (int)stripW - 7, 9,
                   juce::Justification::centred, false);

        // Vertical LED Meter Bar (16 Segments)
        float meterX = sx + 6.0f;
        float meterW = 7.0f;
        float segH = (faderH - (numSegments - 1) * segGap) / (float)numSegments;
        int activeSegments = (int)std::round(lvl * (float)numSegments);

        for (int s = 0; s < numSegments; ++s) {
            // Segment 0 is at bottom, segment 15 at top
            float sy = faderTop + (float)(numSegments - 1 - s) * (segH + segGap);
            bool lit = (s < activeSegments);

            juce::Colour segCol;
            if (s < 10) {
                segCol = lit ? theme.accentDark : theme.cardBg;
            } else if (s < 14) {
                segCol = lit ? theme.accent : theme.knobTrack;
            } else {
                segCol = lit ? juce::Colours::white : theme.cardBorder;
            }

            g.setColour(segCol);
            g.fillRect(meterX, sy, meterW, segH);
        }

        // dB Tick Marks along fader
        g.setFont(lnf ? lnf->getCustomFont(7.5f, juce::Font::plain) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 7.5f, juce::Font::plain));
        g.setColour(theme.textMuted);
        float tickX = meterX + meterW + 2.0f;
        g.drawText("+2", (int)tickX, (int)faderTop - 2, 14, 10, juce::Justification::left, false);
        g.drawText("0", (int)tickX, (int)(faderTop + faderH * 0.2f) - 5, 14, 10, juce::Justification::left, false);
        g.drawText("-6", (int)tickX, (int)(faderTop + faderH * 0.5f) - 5, 14, 10, juce::Justification::left, false);
        g.drawText("-inf", (int)tickX, (int)(faderTop + faderH - 8), 16, 10, juce::Justification::left, false);

        // Fader Readout Text below
        float faderVal = (voiceFaders[v] ? (float)voiceFaders[v]->getValue() : 1.0f);
        juce::String valText;
        if (faderVal < 0.02f) valText = "MUTE";
        else {
            float db = (faderVal >= 1.0f) ? (faderVal - 1.0f) * 8.0f : (1.0f - faderVal) * -36.0f;
            valText = (db >= 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
        }
        g.setFont(lnf ? lnf->getCustomFont(8.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.0f, juce::Font::bold));
        g.setColour(isActive ? theme.accent : theme.textMuted);
        g.drawText(valText, (int)sx + 2, (int)(faderTop + faderH + 4.0f), (int)stripW - 7, 12, juce::Justification::centred, false);

        // ConsoleX Phi Drive Status
        if (cModel == cmConsoleX && faderVal > 0.95f) {
            g.setFont(lnf ? lnf->getCustomFont(7.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 7.0f, juce::Font::bold));
            g.setColour(faderVal > 1.05f ? theme.accent : theme.accentDark);
            g.drawText("PHI DRIVE", (int)sx + 2, (int)(faderTop + faderH + 16.0f), (int)stripW - 7, 10, juce::Justification::centred, false);
        }
    }

    // Strip 7: Master Buss Strip
    {
        float sx = marginX + (float)SYNTH_VOICE_COUNT * stripW;
        auto stripRect = juce::Rectangle<float>(sx, startY + 2.0f, stripW - 3.0f, usableH);

        g.setColour(theme.cardBg.darker(0.15f));
        g.fillRect(stripRect);
        g.setColour(cModel == cmConsoleX ? theme.accentDark : theme.cardBorder);
        g.drawRect(stripRect, 1.0f);

        // Master Title
        g.setFont(lnf ? lnf->getCustomFont(9.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.5f, juce::Font::bold));
        g.setColour(theme.accent);
        g.drawText("MASTER", (int)sx + 4, (int)startY + 4, (int)stripW - 8, 14, juce::Justification::centred, false);

        // Dual Stereo Peak Meters (L and R)
        float mMeterX = sx + 5.0f;
        float mMeterW = 4.0f;
        float segH = (faderH - (numSegments - 1) * segGap) / (float)numSegments;
        int activeL = (int)std::round(std::clamp(masterPeakL, 0.0f, 1.0f) * (float)numSegments);
        int activeR = (int)std::round(std::clamp(masterPeakR, 0.0f, 1.0f) * (float)numSegments);

        for (int s = 0; s < numSegments; ++s) {
            float sy = faderTop + (float)(numSegments - 1 - s) * (segH + segGap);
            bool litL = (s < activeL);
            bool litR = (s < activeR);

            juce::Colour colL = (s < 10) ? (litL ? theme.accentDark : theme.cardBg) :
                               ((s < 14) ? (litL ? theme.accent : theme.knobTrack) : (litL ? juce::Colours::white : theme.cardBorder));
            juce::Colour colR = (s < 10) ? (litR ? theme.accentDark : theme.cardBg) :
                               ((s < 14) ? (litR ? theme.accent : theme.knobTrack) : (litR ? juce::Colours::white : theme.cardBorder));

            g.setColour(colL);
            g.fillRect(mMeterX, sy, mMeterW, segH);
            g.setColour(colR);
            g.fillRect(mMeterX + mMeterW + 2.0f, sy, mMeterW, segH);
        }

        // Master Readout
        float mVal = masterFader ? (float)masterFader->getValue() : 999.0f;
        int mPct = (int)std::round((mVal / 999.0f) * 100.0f);
        g.setFont(lnf ? lnf->getCustomFont(8.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.0f, juce::Font::bold));
        g.setColour(theme.accent);
        g.drawText(juce::String(mPct) + " %", (int)sx + 2, (int)(faderTop + faderH + 4.0f), (int)stripW - 7, 12, juce::Justification::centred, false);

        g.setFont(lnf ? lnf->getCustomFont(7.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 7.0f, juce::Font::bold));
        g.setColour(theme.textMuted);
        g.drawText("BUS OUT", (int)sx + 2, (int)(faderTop + faderH + 16.0f), (int)stripW - 7, 10, juce::Justification::centred, false);
    }

    // Telemetry Footer
    g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::plain) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::plain));
    g.setColour(theme.textMuted);
    juce::String footerInfo = (cModel == cmConsoleX)
        ? "Airwindows ConsoleX Golden Ratio (Phi = 1.618) Multi-Voice Summing & Discontinuity Acoustic Air Modeling"
        : "Discrete Dual LM13700 OTA Linear VCA with Continuous Analog RC Slew Limiter (Click-Free)";
    g.drawText(footerInfo, 14, (int)bounds.getBottom() - 18, (int)bounds.getWidth() - 28, 14, juce::Justification::left, false);
}


// ==============================================================================
// LfoWavePreviewComponent Implementation
// ==============================================================================
LfoWavePreviewComponent::LfoWavePreviewComponent(SynthEngine& eng, int lfoIndex)
    : engine(eng), lfoNum(lfoIndex) {}

void LfoWavePreviewComponent::setShape(int shapeIndex) {
    currentShape = shapeIndex;
    repaint();
}

void LfoWavePreviewComponent::setPhase(float phase) {
    currentPhase = phase;
    repaint();
}

void LfoWavePreviewComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];

    // Sharp dark frame
    g.setColour(theme.cardBg);
    g.fillRect(bounds);
    g.setColour(theme.cardBorder);
    g.drawRect(bounds, 1.0f);

    // Header bar matching ModernSectionCard styling
    auto headerRect = bounds.withHeight(24.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(headerRect);

    // Accent line underneath header
    g.setColour(theme.accent);
    g.fillRect(bounds.getX(), 23.0f, bounds.getWidth(), 1.5f);

    // Header title
    g.setFont(lnf ? lnf->getCustomFont(11.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::bold));
    g.setColour(theme.textTitle);
    g.drawText("LFO " + juce::String(lfoNum) + " OSCILLOSCOPE", 10, 2, (int)bounds.getWidth() - 130, 20, juce::Justification::centredLeft, false);

    // Badge
    const char* shapeNames[] = { "PULSE / SQUARE", "TRIANGLE", "RANDOM S&H", "SINE", "NOISE", "SAWTOOTH", "INVERTED SAW" };
    juce::String shapeBadge = (currentShape >= 0 && currentShape < 7) ? shapeNames[currentShape] : "TRIANGLE";
    g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::bold));
    int badgeW = (int)g.getCurrentFont().getStringWidth(shapeBadge) + 12;
    auto badgeRect = juce::Rectangle<float>(bounds.getRight() - badgeW - 6.0f, 4.0f, (float)badgeW, 16.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(badgeRect);
    g.setColour(theme.accentDark);
    g.drawRect(badgeRect, 1.0f);
    g.setColour(theme.accent);
    g.drawText(shapeBadge, badgeRect, juce::Justification::centred, false);

    // Canvas area
    auto disp = bounds.reduced(6.0f).withTrimmedTop(30.0f);
    g.setColour(theme.windowBg);
    g.fillRect(disp);
    g.setColour(theme.cardBorder);
    g.drawRect(disp, 1.0f);

    if (disp.getWidth() <= 10.0f || disp.getHeight() <= 10.0f) return;

    // Grid center line
    float midY = disp.getCentreY();
    g.setColour(theme.visualizerGrid);
    g.drawHorizontalLine((int)midY, disp.getX(), disp.getRight());

    // Calculate waveform shape points
    int plotW = (int)disp.getWidth();
    juce::Path wavePath;
    juce::Path fillPath;

    for (int x = 0; x < plotW; ++x) {
        float t = (float)x / (float)plotW; // [0.0, 1.0]
        float val = 0.0f; // [-1.0, 1.0]

        switch (currentShape) {
        case 0: // Pulse
            val = (t < 0.5f) ? 1.0f : -1.0f;
            break;
        case 1: // Triangle
            val = (t < 0.25f) ? (t * 4.0f) : (t < 0.75f) ? (2.0f - t * 4.0f) : (t * 4.0f - 4.0f);
            break;
        case 2: { // Random S&H (stepped pattern)
            int step = (int)(t * 8.0f);
            float pseudoRandom[] = { 0.3f, -0.7f, 0.9f, 0.1f, -0.4f, 0.8f, -0.9f, 0.5f };
            val = pseudoRandom[step % 8];
            break;
        }
        case 3: // Sine
            val = std::sin(t * 6.2831853f);
            break;
        case 4: // Noise
            val = std::sin(t * 31.4159f) * 0.6f + std::sin(t * 83.2f) * 0.4f;
            break;
        case 5: // Saw
            val = 2.0f * t - 1.0f;
            break;
        case 6: // Inverted Saw
            val = 1.0f - 2.0f * t;
            break;
        default:
            val = std::sin(t * 6.2831853f);
            break;
        }

        float py = midY - val * (disp.getHeight() * 0.42f);
        if (x == 0) {
            wavePath.startNewSubPath(disp.getX() + (float)x, py);
            fillPath.startNewSubPath(disp.getX() + (float)x, midY);
            fillPath.lineTo(disp.getX() + (float)x, py);
        } else {
            wavePath.lineTo(disp.getX() + (float)x, py);
            fillPath.lineTo(disp.getX() + (float)x, py);
        }
    }
    fillPath.lineTo(disp.getRight(), midY);
    fillPath.closeSubPath();

    // Shaded fill under curve
    juce::ColourGradient fillGrad(theme.accent.withAlpha(0.20f), 0, disp.getY(),
                                 theme.accent.withAlpha(0.02f), 0, disp.getBottom(), false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    // Glowing accent line
    g.setColour(theme.accent);
    g.strokePath(wavePath, juce::PathStrokeType(1.6f));

    // Animated Phase Indicator (sharp square)
    float markerNorm = std::fmod(currentPhase, 1.0f);
    if (markerNorm < 0.0f) markerNorm += 1.0f;
    float markerX = disp.getX() + markerNorm * disp.getWidth();
    g.setColour(theme.accent.withAlpha(0.35f));
    g.drawVerticalLine((int)markerX, disp.getY(), disp.getBottom());
    g.setColour(juce::Colours::white);
    g.fillRect(markerX - 2.5f, midY - 2.5f, 5.0f, 5.0f);
}

// ==============================================================================
// ArpVisualizerComponent Implementation
// ==============================================================================
ArpVisualizerComponent::ArpVisualizerComponent(SynthEngine& eng) : engine(eng) {
    startTimerHz(60);
}

void ArpVisualizerComponent::mouseDown(const juce::MouseEvent& e) {
    auto bounds = getLocalBounds().toFloat();
    auto disp = bounds.reduced(10.0f).withTrimmedTop(30.0f).withTrimmedBottom(36.0f);
    if (disp.contains(e.position)) {
        float colW = disp.getWidth() / 16.0f;
        int step = (int)((e.x - disp.getX()) / colW);
        if (step >= 0 && step < 16) {
            if (e.mods.isRightButtonDown() || e.mods.isShiftDown() || engine.getArpeggiator().getMode() == amDegree || engine.getArpeggiator().getMode() == amStrum) {
                engine.getArpeggiator().cycleStepDegree(step);
            } else {
                engine.getArpeggiator().cycleStepPattern(step);
            }
            repaint();
        }
    }
}

void ArpVisualizerComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];

    // Sharp dark frame
    g.setColour(theme.cardBg);
    g.fillRect(bounds);
    g.setColour(theme.cardBorder);
    g.drawRect(bounds, 1.0f);

    auto& arp = engine.getArpeggiator();
    arpMode_t mode = arp.getMode();
    const auto& liveState = engine.getArpVisualizationState();
    uint32_t tick = liveState.valid ? liveState.tick : engine.getCurrentTick();
    uint32_t divTicks = arp.getStepDivisionTicks();
    int currentStep = liveState.valid ? liveState.currentStep
                                     : ((divTicks > 0) ? (int)((tick / divTicks) % 16) : 0);
    float stepPhase = (divTicks > 0) ? (float)(tick % divTicks) / (float)divTicks : 0.0f;

    // Mode and Status Telemetry
    juce::String modeName = "OFF";
    switch (mode) {
        case amUp: modeName = "UP"; break;
        case amDown: modeName = "DOWN"; break;
        case amUpDown: modeName = "UP / DOWN"; break;
        case amRandom: modeName = "RANDOM"; break;
        case amAssign: modeName = "AS PLAYED"; break;
        case amChord: modeName = "CHORD"; break;
        case amConverge: modeName = "CONVERGE"; break;
        case amDegree: modeName = "CHORD DEGREE"; break;
        case amStrum: modeName = "POLY STRUM"; break;
        default: modeName = "OFF"; break;
    }

    uint8_t liveNotes[16]{};
    int liveCount = 0;
    if (liveState.valid) {
        liveCount = liveState.activeCount;
        std::copy_n(liveState.activeNotes.begin(), liveCount, liveNotes);
    } else {
        liveCount = arp.getActiveNotes(liveNotes, 16);
    }

    const int numSteps = 16;
    const int numOctaveRows = 4;
    uint8_t patternNotes[numSteps]{};
    if (liveState.valid)
        std::copy_n(liveState.patternNotes.begin(), numSteps, patternNotes);
    else
        arp.getPattern(patternNotes, numSteps);

    auto midiNoteName = [](uint8_t note) -> juce::String {
        if (note == ASSIGNER_NO_NOTE) return "-";
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int oct = ((int)note / 12) - 1;
        return juce::String(noteNames[note % 12]) + juce::String(oct);
    };

    juce::String curNoteStr = (liveCount > 0 && currentStep < numSteps) ? midiNoteName(patternNotes[currentStep]) : "-";
    juce::String octStr = juce::String(arp.getOctaves()) + " OCT";

    // Header bar matching ModernSectionCard styling
    auto headerRect = bounds.withHeight(24.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(headerRect);

    // Accent line underneath header
    g.setColour(theme.accent);
    g.fillRect(bounds.getX(), 23.0f, bounds.getWidth(), 1.5f);

    // Header title
    g.setFont(lnf ? lnf->getCustomFont(11.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::bold));
    g.setColour(theme.textTitle);
    g.drawText("ARPEGGIATOR MATRIX", 10, 2, (int)bounds.getWidth() - 140, 20, juce::Justification::centredLeft, false);

    // Badge (Right-aligned in header bar)
    juce::String arpBadge = (mode == amOff) ? "OFF" : (modeName + " (" + octStr + ")");
    g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::bold));
    int badgeW = (int)g.getCurrentFont().getStringWidth(arpBadge) + 12;
    auto badgeRect = juce::Rectangle<float>(bounds.getRight() - (float)badgeW - 8.0f, 4.0f, (float)badgeW, 16.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(badgeRect);
    g.setColour(theme.accentDark);
    g.drawRect(badgeRect, 1.0f);
    g.setColour(theme.accent);
    g.drawText(arpBadge, badgeRect, juce::Justification::centred, false);

    // Matrix display canvas
    auto disp = bounds.reduced(10.0f).withTrimmedTop(30.0f).withTrimmedBottom(36.0f);
    g.setColour(theme.windowBg);
    g.fillRect(disp);
    g.setColour(theme.cardBorder);
    g.drawRect(disp, 1.0f);

    if (disp.getWidth() <= 20.0f || disp.getHeight() <= 20.0f) return;

    float colW = disp.getWidth() / (float)numSteps;
    const float pitchHeight = disp.getHeight() - 4.0f; // leave 4px at bottom for gate meter
    float octaveH = pitchHeight / (float)numOctaveRows;

    // Four fixed octave lanes. Pitch nodes retain semitone resolution inside
    // each lane while octave boundaries remain readable at compact UI sizes.
    for (int r = 1; r < numOctaveRows; ++r) {
        float y = disp.getY() + (float)r * octaveH;
        g.setColour(theme.cardBorder.brighter(0.12f));
        g.drawHorizontalLine((int)y, disp.getX(), disp.getRight());
    }
    for (int semitone = 1; semitone < 48; ++semitone) {
        if (semitone % 12 == 0) continue;
        const float y = disp.getBottom() - 4.0f - ((float)semitone / 48.0f) * pitchHeight;
        g.setColour(theme.visualizerGrid.withAlpha(0.20f));
        g.drawHorizontalLine((int)y, disp.getX(), disp.getRight());
    }

    // Vertical Step lines (with beat accents on steps 4, 8, 12)
    for (int s = 1; s < numSteps; ++s) {
        float x = disp.getX() + (float)s * colW;
        bool isBeat = (s % 4 == 0);
        g.setColour(isBeat ? theme.cardBorder.brighter(0.2f) : theme.visualizerGrid);
        g.drawVerticalLine((int)x, disp.getY(), disp.getBottom() - 4.0f);
    }

    if (mode == amOff) {
        g.setFont(lnf ? lnf->getCustomFont(10.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.5f, juce::Font::bold));
        g.setColour(theme.textMuted.withAlpha(0.6f));
        g.drawText(bounds.getWidth() < 420.0f ? "ARPEGGIATOR OFF\nSELECT MODE ABOVE" : "ARPEGGIATOR OFF  —  SELECT A PLAYBACK MODE ABOVE TO ACTIVATE",
                   disp.toNearestInt(), juce::Justification::centred, false);
    } else if (liveCount == 0) {
        g.setFont(lnf ? lnf->getCustomFont(10.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.5f, juce::Font::bold));
        g.setColour(theme.textMuted.withAlpha(0.6f));
        g.drawText(bounds.getWidth() < 420.0f ? "HOLD KEYS\nTO ARPEGGIATE" : "HOLD OR LATCH KEYS TO ARPEGGIATE",
                   disp.toNearestInt(), juce::Justification::centred, false);
    } else {
        // Highlight active step column
        float curColX = disp.getX() + (float)currentStep * colW;
        g.setColour(theme.accent.withAlpha(0.08f));
        g.fillRect(curColX, disp.getY(), colW, disp.getHeight() - 4.0f);

        uint8_t minNote = 127;
        for (int i = 0; i < liveCount; ++i) {
            if (liveNotes[i] < minNote) minNote = liveNotes[i];
        }
        const int rangeBase = ((int)minNote / 12) * 12;

        float patternPitch[numSteps]{};
        for (int s = 0; s < numSteps; ++s) {
            uint8_t n = patternNotes[s];
            patternPitch[s] = n == ASSIGNER_NO_NOTE ? 0.0f
                : std::clamp(((float)n - (float)rangeBase) / 47.0f, 0.0f, 1.0f);
        }

        // Render Step Nodes / Blocks
        for (int s = 0; s < numSteps; ++s) {
            uint8_t stepPattern = arp.getStepPattern(s);
            if (stepPattern == 3) {
                // Rest / Mute step: small muted center dot
                float cellX = disp.getX() + (float)s * colW;
                float dotY = disp.getY() + (disp.getHeight() - 4.0f) * 0.5f;
                g.setColour(theme.textMuted.withAlpha(0.35f));
                g.fillRect(cellX + colW * 0.5f - 2.0f, dotY - 2.0f, 4.0f, 4.0f);
                continue;
            }

            const float nodeH = std::clamp(octaveH * 0.34f, 4.0f, 12.0f);
            float centreY = disp.getBottom() - 4.0f - patternPitch[s] * pitchHeight;
            float cellY = std::clamp(centreY - nodeH * 0.5f, disp.getY() + 1.0f,
                                     disp.getBottom() - 5.0f - nodeH);
            float cellX = disp.getX() + (float)s * colW;
            auto cellRect = juce::Rectangle<float>(cellX + 2.0f, cellY, colW - 4.0f, nodeH);

            bool isCurrent = (s == currentStep);
            if (isCurrent) {
                g.setColour(theme.accent);
                g.fillRect(cellRect);
                if (stepPhase < 0.45f) {
                    g.setColour(juce::Colours::white);
                    g.fillRect(cellRect.reduced(2.0f));
                }
                g.setColour(juce::Colours::white);
                g.drawRect(cellRect, 1.0f);
            } else {
                if (stepPattern == 1) { // Accent
                    g.setColour(theme.accent.withAlpha(0.55f));
                    g.fillRect(cellRect);
                    g.setColour(theme.accent);
                    g.drawRect(cellRect, 1.2f);
                } else if (stepPattern == 2) { // Tie
                    g.setColour(theme.accentDark.withAlpha(0.40f));
                    g.fillRect(cellRect);
                    g.setColour(theme.cardBorder);
                    g.drawRect(cellRect, 1.0f);
                    g.setColour(theme.accent);
                    g.drawHorizontalLine((int)(cellRect.getCentreY()), cellRect.getX() - 2.0f, cellRect.getRight() + 2.0f);
                } else { // Play (Normal)
                    g.setColour(theme.accent.withAlpha(0.22f));
                    g.fillRect(cellRect);
                    g.setColour(theme.accentDark.withAlpha(0.6f));
                    g.drawRect(cellRect, 1.0f);
                }
            }
        }

        // Draw Smooth Animated Tempo Playhead
        float playheadX = disp.getX() + ((float)currentStep + stepPhase) * colW;
        g.setColour(theme.accent.withAlpha(0.20f));
        g.fillRect(playheadX - 3.0f, disp.getY(), 6.0f, disp.getHeight() - 4.0f);
        g.setColour(juce::Colours::white);
        g.drawVerticalLine((int)playheadX, disp.getY(), disp.getBottom() - 4.0f);

        // Real-time Gate Pulse Bar at bottom of display (syncs with gate length)
        float gateY = disp.getBottom() - 4.0f;
        g.setColour(theme.cardBg);
        g.fillRect(disp.getX(), gateY, disp.getWidth(), 4.0f);

        bool isGateActive = liveState.valid ? liveState.gateActive : (stepPhase < arp.getGateLength());
        if (isGateActive && arp.getStepPattern(currentStep) != 3) {
            g.setColour(theme.accent);
            g.fillRect(curColX + 1.0f, gateY, colW - 2.0f, 4.0f);
        }
    }

    // Step Numbers & Pattern/Degree tags below matrix (Interactive clickable buttons)
    g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::plain) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::plain));
    float labelY = disp.getBottom() + 2.0f;
    for (int s = 0; s < numSteps; ++s) {
        float x = disp.getX() + (float)s * colW;
        bool isCurrent = (s == currentStep && mode != amOff);
        g.setColour(isCurrent ? theme.accent : theme.textMuted);
        juce::String stepNum = (s < 9) ? ("0" + juce::String(s + 1)) : juce::String(s + 1);
        uint8_t stepPattern = arp.getStepPattern(s);
        if (stepPattern == 1) stepNum += "!"; // Accent
        else if (stepPattern == 2) stepNum += "~"; // Tie
        else if (stepPattern == 3) stepNum += "x"; // Mute

        if (mode == amDegree || mode == amStrum) {
            uint8_t deg = arp.getStepDegree(s);
            stepNum += " d" + juce::String(deg + 1);
        }

        g.drawText(stepNum, (int)x, (int)labelY, (int)colW, 14, juce::Justification::centred, false);
    }

    // Telemetry and Status Bar at the bottom
    float effectiveBpm = engine.getEffectiveBpm();
    juce::String syncStr = engine.isHostSyncEnabled() ? "SYNC" : "FREE";
    juce::String statusStr;
    if (mode == amOff) {
        statusStr = "STATE: DISABLED  |  " + syncStr + ": " + juce::String((int)std::round(effectiveBpm)) + " BPM";
    } else if (liveCount == 0) {
        statusStr = "IDLE: WAITING FOR NOTE  |  " + syncStr + ": " + juce::String((int)std::round(effectiveBpm)) + " BPM";
    } else {
        statusStr = "NOTE: " + curNoteStr + "  |  STEP: " + juce::String(currentStep + 1) + "/16  |  " + syncStr + ": " + juce::String((int)std::round(effectiveBpm)) + " BPM";
    }
    float statusY = disp.getBottom() + 18.0f;
    g.setFont(lnf ? lnf->getCustomFont(9.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::bold));
    g.setColour(mode != amOff ? theme.accent : theme.textMuted);
    g.drawText(statusStr, (int)disp.getX(), (int)statusY, (int)disp.getWidth(), 15, juce::Justification::centred, false);
}
