#include "ModernTelemetryComponents.h"
#include "../../dsp/MasterBus.h"
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

namespace {
constexpr float kFallDbPerSecond = 24.0f;   // meter release
constexpr double kHoldMs = 1500.0;          // peak hold
constexpr int kMeterSegments = 16;          // the top one is the over indicator
// Over (+2 dB) is always red, independent of the skin.
const juce::Colour kOverColour(0xffe53935);

// Voices: share of the console bus load re the knee. Master: output peak re
// kOutputMeterReference, so +2 dB (red) is where the output ceiling starts.
float meterToDb(int value, bool output) {
    const float reference = output ? MasterBus::kOutputMeterReference : MasterBus::kConsoleKnee;
    const float level = (float)value / 65535.0f / reference;
    return std::max(ModernVoiceMeterPanel::kMeterFloorDb, 20.0f * std::log10(std::max(level, 1.0e-6f)));
}
} // namespace

float ModernVoiceMeterPanel::meterPosition(float db) {
    if (db <= kMeterZoneDb)
        return juce::jmap(std::max(db, kMeterFloorDb), kMeterFloorDb, kMeterZoneDb, 0.0f, kMeterZonePosition);
    const float t = std::clamp((db - kMeterZoneDb) / (kMeterOverDb - kMeterZoneDb), 0.0f, 1.0f);
    return kMeterZonePosition + (1.0f - kMeterZonePosition) * std::pow(t, kMeterDegression);
}

float ModernVoiceMeterPanel::faderGain(double position) {
    const float p = std::clamp((float)position, 0.0f, 1.0f);
    if (p <= 0.001f) return 0.0f;
    const float db = p <= kFaderUnityPosition
        ? juce::jmap(p, 0.0f, kFaderUnityPosition, kFaderMinDb, 0.0f)
        : juce::jmap(p, kFaderUnityPosition, 1.0f, 0.0f, kFaderMaxDb);
    return std::pow(10.0f, db / 20.0f);
}

double ModernVoiceMeterPanel::faderPosition(float gain) {
    if (gain <= 0.0f) return 0.0;
    const float db = std::clamp(20.0f * std::log10(gain), kFaderMinDb, kFaderMaxDb);
    return db <= 0.0f ? juce::jmap(db, kFaderMinDb, 0.0f, 0.0f, kFaderUnityPosition)
                      : juce::jmap(db, 0.0f, kFaderMaxDb, kFaderUnityPosition, 1.0f);
}

ModernVoiceMeterPanel::ModernVoiceMeterPanel(SynthModel& eng) : model(eng) {
    shownDb.fill(kMeterFloorDb);
    holdDb.fill(kMeterFloorDb);
    faderLnf.setTheme(ModernTheme::getPresetThemes()[0]);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        voiceFaders[v] = std::make_unique<juce::Slider>(juce::Slider::LinearVertical, juce::Slider::NoTextBox);
        voiceFaders[v]->setLookAndFeel(&faderLnf);
        // Position 0..1; the model keeps the linear gain (faderGain()).
        voiceFaders[v]->setRange(0.0, 1.0, 0.0);
        voiceFaders[v]->setValue(faderPosition(model.getVoiceFader(v)), juce::dontSendNotification);
        voiceFaders[v]->setDoubleClickReturnValue(true, kFaderUnityPosition);
        voiceFaders[v]->setComponentID("voiceMeterPanel_fader[" + juce::String(v) + "]");
        voiceFaders[v]->onValueChange = [this, v]() {
            model.setVoiceFader(v, faderGain(voiceFaders[v]->getValue()));
            repaint();
        };
        addAndMakeVisible(*voiceFaders[v]);

        voicePans[v] = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        voicePans[v]->setLookAndFeel(&faderLnf);
        voicePans[v]->setRange(-1.0, 1.0, 0.01);
        voicePans[v]->setValue(model.getVoicePan(v), juce::dontSendNotification);
        voicePans[v]->setDoubleClickReturnValue(true, 0.0);
        voicePans[v]->setComponentID("voiceMeterPanel_pan[" + juce::String(v) + "]");
        voicePans[v]->setTooltip("Voice " + juce::String(v + 1) + " pan");
        voicePans[v]->textFromValueFunction = [](double value) { return juce::String((int)std::round(value * 100.0)); };
        voicePans[v]->onValueChange = [this, v]() {
            model.setVoicePan(v, (float)voicePans[v]->getValue());
            repaint();
        };
        addAndMakeVisible(*voicePans[v]);
    }

    masterFader = std::make_unique<juce::Slider>(juce::Slider::LinearVertical, juce::Slider::NoTextBox);
    masterFader->setLookAndFeel(&faderLnf);
    masterFader->setRange(0.0, 999.0, 1.0);
    masterFader->setValue(scan_potFrom16bits(model.getCurrentPreset().continuousParams[cpConsolePad]), juce::dontSendNotification);
    masterFader->setComponentID("voiceMeterPanel_masterFader");
    masterFader->onValueChange = [this]() {
        writeContinuous(cpConsolePad, (float)masterFader->getValue());
        repaint();
    };
    addAndMakeVisible(*masterFader);

    // Master strip encoder in the voice pan position: Mackity parallel send.
    mackitySendKnob = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    mackitySendKnob->setLookAndFeel(&faderLnf);
    mackitySendKnob->setRange(0.0, 999.0, 1.0);
    mackitySendKnob->setValue(scan_potFrom16bits(model.getCurrentPreset().continuousParams[cpMackitySend]), juce::dontSendNotification);
    mackitySendKnob->setDoubleClickReturnValue(true, 0.0);
    mackitySendKnob->setComponentID("voiceMeterPanel_mackitySend");
    mackitySendKnob->setTooltip("Mackity send (parallel saturation)");
    mackitySendKnob->textFromValueFunction = [](double value) { return juce::String((int)std::round(value / 999.0 * 100.0)); };
    mackitySendKnob->onValueChange = [this]() {
        writeContinuous(cpMackitySend, (float)mackitySendKnob->getValue());
        repaint();
    };
    addAndMakeVisible(*mackitySendKnob);

    mackityPadToggle.getProperties().set("labelFirst", true);
    mackityPadToggle.setToggleState(model.getCurrentPreset().steppedParams[spMackityReturnPad] != 0,
                                    juce::dontSendNotification);
    mackityPadToggle.setComponentID("voiceMeterPanel_mackityPad");
    mackityPadToggle.setTooltip("Mackity send return -6 dB");
    mackityPadToggle.onClick = [this]() {
        writeStepped(spMackityReturnPad, mackityPadToggle.getToggleState() ? 1 : 0);
        repaint();
    };
    addAndMakeVisible(mackityPadToggle);
}

void ModernVoiceMeterPanel::writeContinuous(continuousParameter_t cp, float potValue) {
    if (onContinuousParam) onContinuousParam(cp, potValue);
    else model.setContinuousParam(cp, (uint16_t)scan_potTo16bits((int)std::round(potValue)));
}

void ModernVoiceMeterPanel::writeStepped(steppedParameter_t sp, uint8_t value) {
    if (onSteppedParam) onSteppedParam(sp, value);
    else model.setSteppedParam(sp, value);
}

ModernVoiceMeterPanel::~ModernVoiceMeterPanel() {
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (voiceFaders[v]) voiceFaders[v]->setLookAndFeel(nullptr);
        if (voicePans[v]) voicePans[v]->setLookAndFeel(nullptr);
    }
    if (masterFader) masterFader->setLookAndFeel(nullptr);
    if (mackitySendKnob) mackitySendKnob->setLookAndFeel(nullptr);
}

void ModernVoiceMeterPanel::resized() {
    const auto geo = getStripGeometry();
    auto placeEncoder = [&](juce::Slider* knob, float stripX) {
        if (knob) knob->setBounds((int)(stripX + (geo.stripW - 3.0f - geo.knobSize) * 0.5f), (int)geo.knobY,
                                  (int)geo.knobSize, (int)geo.knobSize);
    };
    auto placeFader = [&](juce::Slider* fader, float stripX) {
        // Fader on the right side of the strip; the meter is drawn on the left
        const float faderW = std::clamp(geo.stripW * 0.48f, 22.0f, 32.0f);
        if (fader) fader->setBounds((int)(stripX + geo.stripW - faderW - 4.0f), (int)geo.faderTop,
                                    (int)faderW, (int)geo.faderH);
    };

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        const float sx = geo.marginX + (float)v * geo.stripW;
        placeEncoder(voicePans[v].get(), sx);
        placeFader(voiceFaders[v].get(), sx);
    }
    placeEncoder(mackitySendKnob.get(), geo.masterX());
    placeFader(masterFader.get(), geo.masterX());

    // Footer row: PAD right-aligned, caption first then the LED
    const int footerRowY = getHeight() - kFooterRowH - 8;
    mackityPadToggle.setBounds(getWidth() - 8 - kPadToggleW, footerRowY, kPadToggleW, kFooterRowH);
}

ModernVoiceMeterPanel::StripGeometry ModernVoiceMeterPanel::getStripGeometry() const {
    StripGeometry geo{};
    geo.startY = 30.0f;
    geo.usableH = (float)getHeight() - geo.startY - kFooterH;
    geo.marginX = 8.0f;
    geo.stripW = ((float)getWidth() - geo.marginX * 2.0f) / (float)(SYNTH_VOICE_COUNT + 1);
    // Same ring size as the tab encoders: a standard knob cell minus its
    // built-in value text box (the readout is drawn below instead).
    geo.knobSize = std::min((float)ComponentTokens::KnobSizes::Standard - 16.0f, geo.stripW - 10.0f);
    geo.knobY = geo.startY + 20.0f;
    geo.readoutY = geo.knobY + geo.knobSize + 1.0f;
    geo.faderTop = geo.readoutY + 13.0f;
    // Leave room below the fader for the level readout (+4 .. +16)
    geo.faderH = std::max(40.0f, geo.startY + 2.0f + geo.usableH - geo.faderTop - 20.0f);
    return geo;
}

juce::Rectangle<int> ModernVoiceMeterPanel::getFooterControlArea() const {
    const int footerRowY = getHeight() - kFooterRowH - 8;
    return { 10, footerRowY, getWidth() - 20 - kPadToggleW - 10, kFooterRowH };
}

void ModernVoiceMeterPanel::updateLevels(const SynthModel::MeterLevels& peaks) {
    // Ballistics: rise at once, fall by kFallDbPerSecond, hold the peak.
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float elapsed = lastUpdateMs > 0.0 ? (float)std::min(0.25, (now - lastUpdateMs) / 1000.0) : 0.0f;
    lastUpdateMs = now;
    for (int i = 0; i < SynthModel::kMeterCount; ++i) {
        const float db = meterToDb(peaks[i], i >= SYNTH_VOICE_COUNT);
        shownDb[i] = std::max(db, std::max(kMeterFloorDb, shownDb[i] - kFallDbPerSecond * elapsed));
        if (db >= holdDb[i] || now > holdUntilMs[i]) {
            holdDb[i] = db;
            holdUntilMs[i] = now + kHoldMs;
        }
    }
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        if (voicePans[v] && !voicePans[v]->isMouseButtonDown())
            voicePans[v]->setValue(model.getVoicePan(v), juce::dontSendNotification);
        if (voiceFaders[v] && !voiceFaders[v]->isMouseButtonDown()
            && std::abs(faderGain(voiceFaders[v]->getValue()) - model.getVoiceFader(v)) > 1.0e-4f)
            voiceFaders[v]->setValue(faderPosition(model.getVoiceFader(v)), juce::dontSendNotification);
    }
    const auto& preset = model.getCurrentPreset();
    if (mackitySendKnob && !mackitySendKnob->isMouseButtonDown())
        mackitySendKnob->setValue(scan_potFrom16bits(preset.continuousParams[cpMackitySend]), juce::dontSendNotification);
    if (masterFader && !masterFader->isMouseButtonDown())
        masterFader->setValue(scan_potFrom16bits(preset.continuousParams[cpConsolePad]), juce::dontSendNotification);
    mackityPadToggle.setToggleState(preset.steppedParams[spMackityReturnPad] != 0, juce::dontSendNotification);
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
    g.drawText("VOICE CONSOLE MIXER", 10, 2, (int)bounds.getWidth() - 200, 20, juce::Justification::centredLeft, false);

    // Badge indicating the master summing engine
    const juce::String badge = "AIRWINDOWS CONSOLEX PHI BUS";
    g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::bold));
    int badgeW = (int)g.getCurrentFont().getStringWidth(badge) + 12;
    auto badgeRect = juce::Rectangle<float>(bounds.getRight() - badgeW - 6.0f, 4.0f, (float)badgeW, 16.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(badgeRect);
    g.setColour(theme.accent);
    g.drawRect(badgeRect, 1.0f);
    g.drawText(badge, badgeRect, juce::Justification::centred, false);

    // 7 Channel Strips (6 Voices + 1 Master Bus), all in the same dark style
    const auto geo = getStripGeometry();
    const float startY = geo.startY, usableH = geo.usableH, marginX = geo.marginX, stripW = geo.stripW;
    const float faderTop = geo.faderTop, faderH = geo.faderH;

    // level 0..1: the strip's LED, like the voice LEDs in the title bar
    // (brightness follows the level, accent border above 10 %).
    auto drawStrip = [&](float sx, const juce::String& title, float level) {
        const bool isActive = level > 0.0015f;
        auto stripRect = juce::Rectangle<float>(sx, startY + 2.0f, stripW - 3.0f, usableH);
        g.setColour(theme.windowBg);
        g.fillRect(stripRect);
        g.setColour(theme.cardBorder);
        g.drawRect(stripRect, 1.0f);

        // Channel header: label & activity LED
        g.setFont(lnf ? lnf->getCustomFont(9.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.5f, juce::Font::bold));
        g.setColour(isActive ? theme.textTitle : theme.textMuted);
        g.drawText(title, (int)sx + 4, (int)startY + 4, (int)stripW - 20, 14, juce::Justification::left, false);
        const float ledSz = 7.0f, ledX = sx + stripW - ledSz - 6.0f, ledY = startY + 7.0f;
        g.setColour(theme.cardBg.interpolatedWith(theme.accent, std::clamp(level, 0.0f, 1.0f)));
        g.fillRect(ledX, ledY, ledSz, ledSz);
        g.setColour(level > 0.1f ? theme.accent : theme.cardBorder);
        g.drawRect(ledX, ledY, ledSz, ledSz, 0.8f);
    };

    // Value text centred under an encoder
    auto drawEncoderReadout = [&](float sx, const juce::String& text, bool highlighted) {
        g.setFont(lnf ? lnf->getCustomFont(8.0f, juce::Font::bold) : juce::Font(8.0f));
        g.setColour(highlighted ? theme.accent : theme.textMuted);
        g.drawText(text, (int)sx, (int)geo.readoutY, (int)stripW - 3, 11, juce::Justification::centred, false);
    };

    // LED level of a meter: linear, full at 0 dB (the knee; for the master
    // the output reference), nothing at the meter floor.
    auto ledLevel = [](float db) { return db <= kMeterFloorDb + 1.0f ? 0.0f : std::min(1.0f, std::pow(10.0f, db / 20.0f)); };

    const int numSegments = kMeterSegments;
    const float segGap = 1.5f;
    const float segH = (faderH - (numSegments - 1) * segGap) / (float)numSegments;
    // The lower segments show the level; the top one lights red from +2 dB.
    const float lowerTop = faderTop + segH + segGap, lowerH = faderH - segH - segGap;
    auto yForPosition = [&](float pos) { return lowerTop + lowerH * (1.0f - pos); };

    // One meter column: level segments, over segment, peak hold line.
    auto drawMeter = [&](float x, float w, int meter) {
        const float pos = meterPosition(shownDb[meter]);
        const int lowerCount = numSegments - 1;
        const int lit = (int)std::round(pos * (float)lowerCount);
        for (int s = 0; s < lowerCount; ++s) {
            const float sy = faderTop + (float)(numSegments - 1 - s) * (segH + segGap);
            const bool on = s < lit;
            const float centreDb = [&] {   // zone of the segment centre
                const float p = ((float)s + 0.5f) / (float)lowerCount;
                return p <= kMeterZonePosition ? -100.0f : (p <= meterPosition(0.0f) ? -3.0f : 1.0f);
            }();
            juce::Colour colour;
            if (centreDb < kMeterZoneDb) colour = on ? theme.accentDark : theme.cardBg;
            else if (centreDb < 0.0f) colour = on ? theme.accent : theme.knobTrack;
            else colour = on ? juce::Colours::white : theme.cardBorder;
            g.setColour(colour);
            g.fillRect(x, sy, w, segH);
        }
        const bool over = shownDb[meter] >= kMeterOverDb || holdDb[meter] >= kMeterOverDb;
        g.setColour(over ? kOverColour : kOverColour.withAlpha(0.18f));
        g.fillRect(x, faderTop, w, segH);
        if (holdDb[meter] > kMeterFloorDb + 1.0f && holdDb[meter] < kMeterOverDb) {
            g.setColour(holdDb[meter] >= 0.0f ? juce::Colours::white : theme.accent);
            g.fillRect(x, yForPosition(meterPosition(holdDb[meter])) - 1.0f, w, 2.0f);
        }
    };
    // Scale labels right of a meter: +2 (red), 0 (console knee), -6, -inf.
    auto drawScale = [&](float x) {
        g.setFont(lnf ? lnf->getCustomFont(7.5f, juce::Font::plain) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 7.5f, juce::Font::plain));
        g.setColour(kOverColour);
        g.drawText("+2", (int)x, (int)faderTop - 2, 14, 10, juce::Justification::left, false);
        g.setColour(theme.textMuted);
        g.drawText("0", (int)x, (int)yForPosition(meterPosition(0.0f)) - 5, 14, 10, juce::Justification::left, false);
        g.drawText("-6", (int)x, (int)yForPosition(kMeterZonePosition) - 5, 14, 10, juce::Justification::left, false);
        g.drawText("-inf", (int)x, (int)(faderTop + faderH - 8), 16, 10, juce::Justification::left, false);
    };

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        float sx = marginX + (float)v * stripW;
        // The voice's share of the console bus load after drive and pan.
        const bool isActive = shownDb[v] > kMeterFloorDb + 1.0f;
        drawStrip(sx, "VOICE " + juce::String(v + 1), ledLevel(shownDb[v]));

        // Pan ring: L / R at the ring ends, the value as a plain number
        // (-100 = hard left, 0 = centre, 100 = hard right)
        if (voicePans[v]) {
            const auto ring = voicePans[v]->getBounds().toFloat();
            g.setFont(lnf ? lnf->getCustomFont(7.5f, juce::Font::bold) : juce::Font(7.5f));
            g.setColour(theme.textMuted);
            g.drawText("L", (int)ring.getX() - 2, (int)ring.getBottom() - 11, 8, 10, juce::Justification::centred, false);
            g.drawText("R", (int)ring.getRight() - 6, (int)ring.getBottom() - 11, 8, 10, juce::Justification::centred, false);
        }
        const float pan = voicePans[v] ? (float)voicePans[v]->getValue() : 0.0f;
        drawEncoderReadout(sx, juce::String((int)std::round(pan * 100.0f)), false);

        // Meter and scale
        const float meterX = sx + 6.0f, meterW = 7.0f;
        drawMeter(meterX, meterW, v);
        drawScale(meterX + meterW + 2.0f);

        // Fader Readout Text below
        const float faderVal = voiceFaders[v] ? faderGain(voiceFaders[v]->getValue()) : 1.0f;
        juce::String valText;
        if (faderVal <= 0.0f) valText = "MUTE";
        else {
            // Gain of the fader position (0 .. +12 dB above unity).
            const float db = 20.0f * std::log10(faderVal);
            valText = (db >= 0.05f ? "+" : "") + juce::String(db, 1) + " dB";
        }
        g.setFont(lnf ? lnf->getCustomFont(8.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.0f, juce::Font::bold));
        g.setColour(isActive ? theme.accent : theme.textMuted);
        g.drawText(valText, (int)sx + 2, (int)(faderTop + faderH + 4.0f), (int)stripW - 7, 12, juce::Justification::centred, false);
    }

    // Strip 7: Master Buss Strip
    {
        const float sx = geo.masterX();
        const int busL = SYNTH_VOICE_COUNT, busR = SYNTH_VOICE_COUNT + 1;
        drawStrip(sx, "MASTER", ledLevel(std::max(shownDb[busL], shownDb[busR])));

        // Mackity send readout: "MACKITY" when off, otherwise 1 .. 100
        const int send = mackitySendKnob ? (int)std::round(mackitySendKnob->getValue() / 999.0 * 100.0) : 0;
        drawEncoderReadout(sx, send > 0 ? juce::String(send) : juce::String("MACKITY"), false);

        // Output after pad, Mackity send and ceiling, left and right
        const float mMeterX = sx + 5.0f, mMeterW = 4.0f;
        drawMeter(mMeterX, mMeterW, busL);
        drawMeter(mMeterX + mMeterW + 2.0f, mMeterW, busR);
        drawScale(mMeterX + 2.0f * mMeterW + 4.0f);

        // Master Readout
        float mVal = masterFader ? (float)masterFader->getValue() : 999.0f;
        int mPct = (int)std::round((mVal / 999.0f) * 100.0f);
        g.setFont(lnf ? lnf->getCustomFont(8.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.0f, juce::Font::bold));
        g.setColour(theme.textMuted);
        g.drawText(juce::String(mPct), (int)sx + 2, (int)(faderTop + faderH + 4.0f), (int)stripW - 7, 12, juce::Justification::centred, false);
    }

    // Footer divider above the footer controls (owner controls left, PAD right)
    const int dividerY = getHeight() - kFooterRowH - 8 - 9;
    g.setFont(lnf ? lnf->getCustomFont(9.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::bold));
    const int captionW = footerCaption.isEmpty() ? 0 : (int)g.getCurrentFont().getStringWidth(footerCaption) + 10;
    g.setColour(theme.cardBorder);
    if (captionW > 0) g.drawHorizontalLine(dividerY, 6.0f, 10.0f);
    g.drawHorizontalLine(dividerY, captionW > 0 ? 14.0f + (float)captionW : 6.0f, bounds.getRight() - 6.0f);
    if (captionW > 0) {
        g.setColour(theme.textMuted);
        g.drawText(footerCaption, 12, dividerY - 7, captionW, 14, juce::Justification::centredLeft, false);
    }
}


// ==============================================================================
// LfoWavePreviewComponent Implementation
// ==============================================================================
LfoWavePreviewComponent::LfoWavePreviewComponent(SynthModel& eng, int lfoIndex)
    : model(eng), lfoNum(lfoIndex) {}

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
ArpVisualizerComponent::ArpVisualizerComponent(SynthModel& eng) : model(eng) {
    startTimerHz(60);
}

void ArpVisualizerComponent::mouseDown(const juce::MouseEvent& e) {
    auto bounds = getLocalBounds().toFloat();
    auto disp = bounds.reduced(10.0f).withTrimmedTop(30.0f).withTrimmedBottom(36.0f);
    if (disp.contains(e.position)) {
        float colW = disp.getWidth() / 16.0f;
        int step = (int)((e.x - disp.getX()) / colW);
        if (step >= 0 && step < 16) {
            if (e.mods.isRightButtonDown() || e.mods.isShiftDown() || model.getArpeggiator().getMode() == amDegree || model.getArpeggiator().getMode() == amStrum) {
                model.getArpeggiator().cycleStepDegree(step);
            } else {
                model.getArpeggiator().cycleStepPattern(step);
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

    auto& arp = model.getArpeggiator();
    arpMode_t mode = arp.getMode();
    const auto& liveState = model.getArpVisualizationState();
    uint32_t tick = liveState.valid ? liveState.tick : model.getCurrentTick();
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
    float effectiveBpm = model.getEffectiveBpm();
    juce::String syncStr = model.isHostSyncEnabled() ? "SYNC" : "FREE";
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
