#include "FilterCurveComponent.h"
#include "../../dsp/OvercyclerTypes.h"
#include <cmath>
#include <algorithm>

FilterCurveComponent::FilterCurveComponent(SynthModel& eng, juce::Slider& cKnob, juce::Slider& rKnob)
    : model(eng), cutoff(cKnob), reso(rKnob) {
    cutoff.addListener(this);
    reso.addListener(this);
    startTimerHz((int)RetroSpectrum::kUpdateHz);
}

FilterCurveComponent::~FilterCurveComponent() {
    stopTimer();
    cutoff.removeListener(this);
    reso.removeListener(this);
}

void FilterCurveComponent::timerCallback() {
    // The timer always runs (tab switches don't tell the children about
    // visibility); off screen the spectrum parts only drop their history.
    if (!isShowing()) {
        spectrum.reset();
        waterfall.reset();
        return;
    }
    // Each part only while switched on in the settings.
    const bool wasLit = spectrum.isLit() || waterfall.isActive();
    bool lit = false;
    if (model.isRetroSpectrumShown()) lit = spectrum.update();
    else spectrum.reset();
    if (model.isSpectrumWaterfallShown()) waterfall.update();
    else waterfall.reset();
    if (lit || wasLit || waterfall.isActive()) repaint();
}

void FilterCurveComponent::mouseDown(const juce::MouseEvent& e) {
    uint8_t fModel = model.getCurrentPreset().steppedParams[spFilterModel];
    uint8_t fMode = model.getCurrentPreset().steppedParams[spFilterMode];

    if (fModel == 2 && fMode == 0) { // Shelves 4-Band EQ
        auto bounds = getLocalBounds().toFloat();
        auto disp = bounds.reduced(10.0f).withTrimmedTop(26.0f).withTrimmedLeft(28.0f);
        if (disp.getWidth() <= 0 || disp.getHeight() <= 0) return;

        const auto& preset = model.getCurrentPreset();
        float fVals[4] = {
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesLsFreq]),
            (float)scan_potFrom16bits(preset.continuousParams[cpCutoff]),
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Freq]),
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesHsFreq])
        };
        float gVals[4] = {
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesLsGain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesP1Gain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Gain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesHsGain]) - 500.0f) / 499.0f * 18.0f
        };

        // Find nearest band node
        int closestBand = -1;
        float minDistSq = 1e9f;
        for (int i = 0; i < 4; ++i) {
            float nx = disp.getX() + (fVals[i] / 999.0f) * disp.getWidth();
            float ny = disp.getY() + ((24.0f - gVals[i]) / 48.0f) * disp.getHeight();
            float dx = (float)e.x - nx;
            float dy = (float)e.y - ny;
            float distSq = dx * dx + dy * dy;
            if (distSq < minDistSq) {
                minDistSq = distSq;
                closestBand = i;
            }
        }

        if (closestBand >= 0) {
            activeBand = closestBand;
            draggedNode = closestBand;
            if (onBandSelected) onBandSelected(activeBand);
            mouseDrag(e);
        }
    } else {
        mouseDrag(e);
    }
}

void FilterCurveComponent::mouseDrag(const juce::MouseEvent& e) {
    auto disp = getLocalBounds().toFloat().reduced(10.0f).withTrimmedTop(26.0f).withTrimmedLeft(28.0f);
    if (disp.getWidth() <= 0 || disp.getHeight() <= 0) return;

    float normX = (e.x - disp.getX()) / disp.getWidth();
    float normY = (e.y - disp.getY()) / disp.getHeight();
    normX = std::clamp(normX, 0.0f, 1.0f);
    normY = std::clamp(normY, 0.0f, 1.0f);

    uint8_t fModel = model.getCurrentPreset().steppedParams[spFilterModel];
    uint8_t fMode = model.getCurrentPreset().steppedParams[spFilterMode];

    if (fModel == 2 && fMode == 0) { // Shelves 4-Band EQ
        if (draggedNode >= 0 && draggedNode < 4) {
            float potFreq = normX * 999.0f;
            float gainDb = 24.0f - normY * 48.0f;
            gainDb = std::clamp(gainDb, -18.0f, 18.0f);
            float potGain = 500.0f + (gainDb / 18.0f) * 499.0f;
            potGain = std::clamp(potGain, 1.0f, 999.0f);

            if (onBandParamChanged) {
                onBandParamChanged(draggedNode, potFreq, potGain);
            }
            repaint();
        }
    } else {
        int newCutoff = (int)std::round(normX * 999.0f);
        int newReso = (int)std::round((1.0f - normY) * 999.0f);

        cutoff.setValue(newCutoff, juce::sendNotificationSync);
        reso.setValue(newReso, juce::sendNotificationSync);
        repaint();
    }
}

void FilterCurveComponent::mouseUp(const juce::MouseEvent&) {
    draggedNode = -1;
}

void FilterCurveComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    juce::ignoreUnused(e);
    uint8_t fModel = model.getCurrentPreset().steppedParams[spFilterModel];
    uint8_t fMode = model.getCurrentPreset().steppedParams[spFilterMode];

    if (fModel == 2 && fMode == 0) { // Shelves 4-Band EQ
        if (activeBand == 1 || activeBand == 2) {
            if (onBandQChanged) {
                onBandQChanged(activeBand, wheel.deltaY);
            }
            repaint();
        }
    }
}

void FilterCurveComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];

    // Sharp dark frame (no rounded corners)
    g.setColour(theme.cardBg);
    g.fillRect(bounds);
    g.setColour(theme.cardBorder);
    g.drawRect(bounds, 1.0f);

    uint8_t fModel = model.getCurrentPreset().steppedParams[spFilterModel];
    uint8_t fMode = model.getCurrentPreset().steppedParams[spFilterMode];
    bool isShelvesEQ = (fModel == 2 && fMode == 0);

    const uint8_t semVariant = model.getCurrentPreset().steppedParams[spSemModel];
    const bool isLiquid = fModel == 1 && semVariant == 4;
    juce::Colour curveColour = theme.accent; // Default to theme accent for SSI2144
    if (fModel == 1) { // SEM
        curveColour = juce::Colour(0xff00e676).interpolatedWith(theme.accent, 0.4f);
    } else if (fModel == 2) { // Shelves
        curveColour = juce::Colour(0xffff9100).interpolatedWith(theme.accent, 0.3f);
    }

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
    g.drawText("FREQUENCY RESPONSE", 10, 2, 180, 20, juce::Justification::centredLeft, false);

    // Filter badge (Right-aligned in header bar)
    static const char* semBadges[] = { "SEM OB-XD 12 DB", "SEM OBERHEIM", "SEM VULT SVF", "SEM CYTOMIC SVF", "LIQUID RIPPLES" };
    juce::String filterBadge = isShelvesEQ ? "SHELVES 4-BAND EQ" :
                               (fModel == 1 ? semBadges[std::min<int>(semVariant, 4)] :
                               (fModel == 3 ? "SST VINTAGE LADDER" : "SSI2144 LADDER"));
    g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::bold));
    int badgeW = (int)g.getCurrentFont().getStringWidth(filterBadge) + 12;
    auto badgeRect = juce::Rectangle<float>(bounds.getRight() - (float)badgeW - 8.0f, 4.0f, (float)badgeW, 16.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(badgeRect);
    g.setColour(theme.accentDark);
    g.drawRect(badgeRect, 1.0f);
    g.setColour(theme.accent);
    g.drawText(filterBadge, badgeRect, juce::Justification::centred, false);

    // Telemetry and readout values
    const auto& preset = model.getCurrentPreset();
    float cVal = (float)cutoff.getValue();
    float rVal = (float)reso.getValue();

    juce::String telemetryStr;
    if (isShelvesEQ) {
        float fVals[4] = {
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesLsFreq]),
            (float)scan_potFrom16bits(preset.continuousParams[cpCutoff]),
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Freq]),
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesHsFreq])
        };
        float gVals[4] = {
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesLsGain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesP1Gain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Gain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesHsGain]) - 500.0f) / 499.0f * 18.0f
        };
        float qVals[2] = {
            shelvesQ((float)scan_potFrom16bits(preset.continuousParams[cpResonance])),
            shelvesQ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Q]))
        };

        auto formatHz = [](float normPot) -> juce::String {
            float hz = 20.0f * std::pow(10.0f, (normPot / 999.0f) * 3.0f);
            if (hz >= 1000.0f) return juce::String(hz / 1000.0f, 2) + " kHz";
            return juce::String((int)std::round(hz)) + " Hz";
        };
        auto formatDb = [](float db) -> juce::String {
            int r = (int)std::round(db);
            return (r > 0 ? "+" : "") + juce::String(r) + " dB";
        };

        if (activeBand == 0) {
            telemetryStr = "LOW SHELF: " + formatHz(fVals[0]) + " | Gain: " + formatDb(gVals[0]);
        } else if (activeBand == 1) {
            telemetryStr = "MID LOW: " + formatHz(fVals[1]) + " | Gain: " + formatDb(gVals[1]) + " | Q: " + juce::String(qVals[0], 2);
        } else if (activeBand == 2) {
            telemetryStr = "MID HIGH: " + formatHz(fVals[2]) + " | Gain: " + formatDb(gVals[2]) + " | Q: " + juce::String(qVals[1], 2);
        } else {
            telemetryStr = "HIGH SHELF: " + formatHz(fVals[3]) + " | Gain: " + formatDb(gVals[3]);
        }
    } else {
        float fHz = 20.0f * std::pow(1300.0f, cVal / 999.0f);   // filter cutoff range
        juce::String hzStr = (fHz >= 1000.0f)
            ? juce::String(fHz / 1000.0f, 2) + " kHz"
            : juce::String((int)std::round(fHz)) + " Hz";
        int resoPct = (int)std::round((rVal / 999.0f) * 100.0f);
        telemetryStr = "Cutoff: " + hzStr + "   |   Resonance: " + juce::String(resoPct) + "%";
    }

    // Display area
    auto disp = bounds.reduced(10.0f).withTrimmedTop(30.0f).withTrimmedLeft(28.0f);
    g.setColour(theme.windowBg);
    g.fillRect(disp);
    g.setColour(theme.cardBorder);
    g.drawRect(disp, 1.0f);

    if (disp.getWidth() <= 20.0f || disp.getHeight() <= 20.0f) return;

    // Grid: Frequency markers (50, 100, 250, 500, 1k, 2k, 5k, 10k, 20k)
    float freqs[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    const char* fLabels[] = { "50", "100", "250", "500", "1k", "2k", "5k", "10k", "20k" };

    g.setFont(lnf ? lnf->getCustomFont(9.0f, juce::Font::plain) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::plain));
    for (int i = 0; i < 9; ++i) {
        float normX = std::log10(freqs[i] / 20.0f) / 3.0f;
        float fx = disp.getX() + normX * disp.getWidth();
        g.setColour(theme.visualizerGrid);
        g.drawVerticalLine((int)fx, disp.getY(), disp.getBottom());
        g.setColour(theme.textMuted);
        g.drawText(fLabels[i], (int)fx - 14, (int)disp.getBottom() - 14, 28, 12, juce::Justification::centred, false);
    }

    // Grid: dB markers
    if (isShelvesEQ) {
        float dBs[] = { 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f };
        for (float db : dBs) {
            float normY = (24.0f - db) / 48.0f;
            float fy = disp.getY() + normY * disp.getHeight();
            g.setColour(juce::Colour((db == 0.0f) ? (curveColour.withAlpha(0.35f)) : theme.visualizerGrid));
            g.drawHorizontalLine((int)fy, disp.getX(), disp.getRight());

            g.setColour(theme.textMuted);
            juce::String dbText = (db > 0) ? ("+" + juce::String((int)db)) : juce::String((int)db);
            g.drawText(dbText, (int)bounds.getX() + 2, (int)fy - 6, 24, 12, juce::Justification::right, false);
        }
    } else {
        float dBs[] = { 12.0f, 0.0f, -12.0f, -24.0f, -36.0f };
        for (float db : dBs) {
            float normY = (18.0f - db) / 60.0f;
            float fy = disp.getY() + normY * disp.getHeight();
            g.setColour(juce::Colour((db == 0.0f) ? (curveColour.withAlpha(0.25f)) : theme.visualizerGrid));
            g.drawHorizontalLine((int)fy, disp.getX(), disp.getRight());

            g.setColour(theme.textMuted);
            juce::String dbText = (db > 0) ? ("+" + juce::String((int)db)) : juce::String((int)db);
            g.drawText(dbText, (int)bounds.getX() + 2, (int)fy - 6, 24, 12, juce::Justification::right, false);
        }
    }

    // Master output behind the filter curve: the 8-bit spectrum, the line
    // waterfall above it
    waterfall.setWidth(disp.getWidth());
    spectrum.setWidth(disp.getWidth());
    if (model.isRetroSpectrumShown()) spectrum.draw(g, disp.reduced(2.0f), theme, model.retroOpacityFilter,
                                                    model.retroRandomness);
    if (model.isSpectrumWaterfallShown()) waterfall.draw(g, disp.reduced(1.0f), theme, model.waterfallOpacity);

    // Filter curve calculation
    juce::Path curvePath;
    juce::Path fillPath;
    int plotW = (int)disp.getWidth();
    float dispX = disp.getX();
    float dispY = disp.getY();
    float dispH = disp.getHeight();

    if (isShelvesEQ) {
        float lsFreqVal = (float)scan_potFrom16bits(preset.continuousParams[cpShelvesLsFreq]);
        float lsGainVal = (float)scan_potFrom16bits(preset.continuousParams[cpShelvesLsGain]);
        float p1FreqVal = (float)scan_potFrom16bits(preset.continuousParams[cpCutoff]);
        float p1GainVal = (float)scan_potFrom16bits(preset.continuousParams[cpShelvesP1Gain]);
        float p1QVal    = (float)scan_potFrom16bits(preset.continuousParams[cpResonance]);
        float p2FreqVal = (float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Freq]);
        float p2GainVal = (float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Gain]);
        float p2QVal    = (float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Q]);
        float hsFreqVal = (float)scan_potFrom16bits(preset.continuousParams[cpShelvesHsFreq]);
        float hsGainVal = (float)scan_potFrom16bits(preset.continuousParams[cpShelvesHsGain]);

        float fLs = 20.0f * std::pow(10.0f, (lsFreqVal / 999.0f) * 3.0f);
        float dbLs = ((lsGainVal - 500.0f) / 499.0f) * 18.0f;

        float fP1 = 20.0f * std::pow(10.0f, (p1FreqVal / 999.0f) * 3.0f);
        float dbP1 = ((p1GainVal - 500.0f) / 499.0f) * 18.0f;
        float qP1 = shelvesQ(p1QVal);

        float fP2 = 20.0f * std::pow(10.0f, (p2FreqVal / 999.0f) * 3.0f);
        float dbP2 = ((p2GainVal - 500.0f) / 499.0f) * 18.0f;
        float qP2 = shelvesQ(p2QVal);

        float fHs = 20.0f * std::pow(10.0f, (hsFreqVal / 999.0f) * 3.0f);
        float dbHs = ((hsGainVal - 500.0f) / 499.0f) * 18.0f;

        for (int x = 0; x < plotW; ++x) {
            float normFreq = (float)x / (float)plotW;
            float f = 20.0f * std::pow(10.0f, normFreq * 3.0f);

            // Low shelf
            float rLs = f / std::max(10.0f, fLs);
            float dbLow = dbLs / (1.0f + rLs * rLs);

            // Mid 1 bell
            float r1 = f / std::max(10.0f, fP1);
            float bell1 = 1.0f / (1.0f + qP1 * qP1 * std::pow(r1 - 1.0f / r1, 2.0f));
            float dbMid1 = dbP1 * bell1;

            // Mid 2 bell
            float r2 = f / std::max(10.0f, fP2);
            float bell2 = 1.0f / (1.0f + qP2 * qP2 * std::pow(r2 - 1.0f / r2, 2.0f));
            float dbMid2 = dbP2 * bell2;

            // High shelf
            float rHs = f / std::max(10.0f, fHs);
            float dbHigh = dbHs * (rHs * rHs) / (1.0f + rHs * rHs);

            float totalDb = dbLow + dbMid1 + dbMid2 + dbHigh;
            float py = dispY + ((24.0f - totalDb) / 48.0f) * dispH;
            py = std::clamp(py, dispY, disp.getBottom());

            if (x == 0) {
                curvePath.startNewSubPath(dispX + (float)x, py);
                fillPath.startNewSubPath(dispX + (float)x, disp.getBottom());
                fillPath.lineTo(dispX + (float)x, py);
            } else {
                curvePath.lineTo(dispX + (float)x, py);
                fillPath.lineTo(dispX + (float)x, py);
            }
        }
    } else {
        float fHz = 20.0f * std::pow(1300.0f, cVal / 999.0f);   // filter cutoff range
        float Q = 0.5f + (rVal / 999.0f) * 9.5f;

        for (int x = 0; x < plotW; ++x) {
            float normFreq = (float)x / (float)plotW;
            float f = 20.0f * std::pow(10.0f, normFreq * 3.0f);
            float r = f / fHz;
            float r2 = r * r;

            float mag = 1.0f;
            if (fModel == 1 && !isLiquid) { // SEM 2-pole SVF, resonance curve as in SemFilter
                const float rs = rVal / 999.0f;
                const float qs = 0.5f + 19.5f * rs * rs * rs;
                const float den = std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (qs * qs)));
                if (fMode == 1) mag = (r / qs) / den;                           // bandpass
                else if (fMode == 2) mag = r2 / den;                            // highpass
                else if (fMode == 3) mag = std::fabs(1.0f - r2) / den;          // notch
                else mag = 1.0f / den;                                          // lowpass
            } else if (fModel == 1) { // Liquid Filter (Mutable Instruments Ripples)
                if (fMode == 1) { // LP2 (12 dB/oct)
                    mag = 1.0f / std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q)));
                } else if (fMode == 2) { // BP2 (12 dB/oct)
                    mag = (r / std::max(0.2f, Q)) / std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q)));
                } else { // LP4 (24 dB/oct Liquid)
                    mag = 1.0f / (std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q))) * std::sqrt(1.0f + r2 * r2));
                }
            } else if (fModel == 2) { // Shelves SVF
                if (fMode == 1) { // SVF LP
                    mag = 1.0f / std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q)));
                } else if (fMode == 2) { // SVF BP
                    mag = (r / std::max(0.2f, Q)) / std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q)));
                } else { // SVF HP
                    mag = r2 / std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q)));
                }
            } else if (fModel == 3) { // SST Vintage Moog Ladder
                if (fMode == 1) { // 18 dB / 3-Pole
                    mag = 1.0f / (std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q))) * std::sqrt(1.0f + r2));
                } else if (fMode == 2) { // 12 dB / 2-Pole
                    mag = 1.0f / std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q)));
                } else if (fMode == 3) { // 6 dB / 1-Pole
                    mag = 1.0f / std::sqrt(1.0f + r2);
                } else { // 24 dB / 4-Pole
                    mag = 1.0f / (std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q))) * (1.0f + r2));
                }
            } else { // SSI2144
                mag = 1.0f / (std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q))) * std::sqrt(1.0f + r2 * r2));
            }

            float magDb = 20.0f * std::log10(std::max(1e-4f, mag));
            float py = dispY + ((18.0f - magDb) / 60.0f) * dispH;
            py = std::clamp(py, dispY, disp.getBottom());

            if (x == 0) {
                curvePath.startNewSubPath(dispX + (float)x, py);
                fillPath.startNewSubPath(dispX + (float)x, disp.getBottom());
                fillPath.lineTo(dispX + (float)x, py);
            } else {
                curvePath.lineTo(dispX + (float)x, py);
                fillPath.lineTo(dispX + (float)x, py);
            }
        }
    }

    fillPath.lineTo(disp.getRight(), disp.getBottom());
    fillPath.closeSubPath();

    // Shaded fill with model-specific hue
    juce::ColourGradient fillGrad(curveColour.withAlpha(0.20f), 0, dispY,
                                 curveColour.withAlpha(0.02f), 0, disp.getBottom(), false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    // Glowing curve line
    g.setColour(curveColour);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    // Interactive handles
    if (isShelvesEQ) {
        float fVals[4] = {
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesLsFreq]),
            (float)scan_potFrom16bits(preset.continuousParams[cpCutoff]),
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Freq]),
            (float)scan_potFrom16bits(preset.continuousParams[cpShelvesHsFreq])
        };
        float gVals[4] = {
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesLsGain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesP1Gain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesP2Gain]) - 500.0f) / 499.0f * 18.0f,
            ((float)scan_potFrom16bits(preset.continuousParams[cpShelvesHsGain]) - 500.0f) / 499.0f * 18.0f
        };

        const char* nodeLabels[] = { "1", "2", "3", "4" };

        for (int i = 0; i < 4; ++i) {
            float nx = dispX + (fVals[i] / 999.0f) * disp.getWidth();
            float ny = dispY + ((24.0f - gVals[i]) / 48.0f) * disp.getHeight();
            ny = std::clamp(ny, dispY + 8.0f, disp.getBottom() - 8.0f);

            bool isActive = (i == activeBand);

            // Crosshairs for active node
            if (isActive) {
                g.setColour(curveColour.withAlpha(0.28f));
                g.drawVerticalLine((int)nx, dispY, disp.getBottom());
                g.drawHorizontalLine((int)ny, dispX, disp.getRight());
            }

            // Node tag box (16x16 square)
            float sz = isActive ? 18.0f : 14.0f;
            auto nodeRect = juce::Rectangle<float>(nx - sz * 0.5f, ny - sz * 0.5f, sz, sz);

            if (isActive) {
                g.setColour(theme.accent);
                g.fillRect(nodeRect);
                g.setColour(juce::Colours::white);
                g.drawRect(nodeRect, 1.5f);
                g.setColour(juce::Colours::black);
            } else {
                g.setColour(theme.cardBg);
                g.fillRect(nodeRect);
                g.setColour(theme.cardBorder);
                g.drawRect(nodeRect, 1.0f);
                g.setColour(theme.textTitle);
            }

            g.setFont(lnf ? lnf->getCustomFont(isActive ? 10.0f : 9.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), isActive ? 10.0f : 9.0f, juce::Font::bold));
            g.drawText(nodeLabels[i], nodeRect.toNearestInt(), juce::Justification::centred, false);
        }
    } else {
        float nodeX = dispX + (cVal / 999.0f) * disp.getWidth();
        float nodeY = dispY + (1.0f - (rVal / 999.0f)) * disp.getHeight();

        // Crosshairs
        g.setColour(curveColour.withAlpha(0.25f));
        g.drawVerticalLine((int)nodeX, dispY, disp.getBottom());
        g.drawHorizontalLine((int)nodeY, dispX, disp.getRight());

        // Sharp square node
        g.setColour(theme.accentDark);
        g.fillRect(nodeX - 5.0f, nodeY - 5.0f, 10.0f, 10.0f);
        g.setColour(theme.accent);
        g.drawRect(nodeX - 5.0f, nodeY - 5.0f, 10.0f, 10.0f, 1.5f);
    }

    // Telemetry and live readout text (top-right of display canvas)
    g.setFont(lnf ? lnf->getCustomFont(9.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.5f, juce::Font::bold));
    g.setColour(curveColour);
    g.drawText(telemetryStr, (int)disp.getRight() - 325, (int)disp.getY() + 3, 315, 16, juce::Justification::right, false);
}
