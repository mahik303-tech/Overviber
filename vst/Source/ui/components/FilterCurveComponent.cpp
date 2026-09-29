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

namespace {
juce::Font curveFont(ModernLookAndFeel* lnf, float size, int style) {
    return lnf ? lnf->getCustomFont(size, style) : juce::Font(juce::Font::getDefaultSansSerifFontName(), size, style);
}

juce::String formatEqHz(float pot) {
    float hz = 20.0f * std::pow(10.0f, (pot / 999.0f) * 3.0f);
    if (hz >= 1000.0f) return juce::String(hz / 1000.0f, 2) + " kHz";
    return juce::String((int)std::round(hz)) + " Hz";
}

juce::String formatEqDb(float db) {
    int r = (int)std::round(db);
    return (r > 0 ? "+" : "") + juce::String(r) + " dB";
}
}

// The Shelves bands as drawn: low shelf, mid low (cutoff, resonance = Q),
// mid high, high shelf.
std::array<FilterCurveComponent::EqBandView, 4> FilterCurveComponent::eqBandViews() const {
    const auto& p = model.getCurrentPreset();
    auto pot = [&](continuousParameter_t cp) { return (float)scan_potFrom16bits(p.continuousParams[cp]); };
    auto band = [&](continuousParameter_t freq, continuousParameter_t gain, float q) {
        const float freqPot = pot(freq);
        return EqBandView{ freqPot, 20.0f * std::pow(10.0f, (freqPot / 999.0f) * 3.0f),
                           ((pot(gain) - 500.0f) / 499.0f) * 18.0f, q };
    };
    return { { band(cpShelvesLsFreq, cpShelvesLsGain, 0.0f),
               band(cpCutoff, cpShelvesP1Gain, shelvesQ(pot(cpResonance))),
               band(cpShelvesP2Freq, cpShelvesP2Gain, shelvesQ(pot(cpShelvesP2Q))),
               band(cpShelvesHsFreq, cpShelvesHsGain, 0.0f) } };
}

// Magnitude of a filter at r = f / cutoff; Q = 0.5 .. 10 from the resonance.
float FilterCurveComponent::filterMagnitude(int model, int mode, bool liquid, float resoPot, float Q, float r) {
    const float r2 = r * r;
    const float svf = std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (Q * Q)));   // 2-pole resonance
    if (model == 1 && !liquid) { // SEM 2-pole SVF, resonance curve as in SemFilter
        const float rs = resoPot / 999.0f;
        const float qs = 0.5f + 19.5f * rs * rs * rs;
        const float den = std::sqrt((1.0f - r2) * (1.0f - r2) + (r2 / (qs * qs)));
        if (mode == 1) return (r / qs) / den;                   // bandpass
        if (mode == 2) return r2 / den;                         // highpass
        if (mode == 3) return std::fabs(1.0f - r2) / den;       // notch
        return 1.0f / den;                                      // lowpass
    }
    if (model == 1) { // Liquid Filter (Mutable Instruments Ripples)
        if (mode == 1) return 1.0f / svf;                                   // LP2 (12 dB/oct)
        if (mode == 2) return (r / std::max(0.2f, Q)) / svf;               // BP2 (12 dB/oct)
        return 1.0f / (svf * std::sqrt(1.0f + r2 * r2));                    // LP4 (24 dB/oct)
    }
    if (model == 2) { // Shelves SVF
        if (mode == 1) return 1.0f / svf;                                   // LP
        if (mode == 2) return (r / std::max(0.2f, Q)) / svf;               // BP
        return r2 / svf;                                                    // HP
    }
    if (model == 3) { // SST Vintage Moog Ladder
        if (mode == 1) return 1.0f / (svf * std::sqrt(1.0f + r2));          // 18 dB / 3-pole
        if (mode == 2) return 1.0f / svf;                                   // 12 dB / 2-pole
        if (mode == 3) return 1.0f / std::sqrt(1.0f + r2);                  // 6 dB / 1-pole
        return 1.0f / (svf * (1.0f + r2));                                  // 24 dB / 4-pole
    }
    return 1.0f / (svf * std::sqrt(1.0f + r2 * r2));                        // SSI2144
}

// The response in dB for each x of a plot `width` pixels wide, from 20 Hz
// to 20 kHz: the EQ's summed bands or the filter's magnitude. Computed again
// only when a setting or the width changed (the spectrum repaints often).
const std::vector<float>& FilterCurveComponent::responseDb(int width, bool shelvesEq, const std::array<EqBandView, 4>& bands) {
    const auto& p = model.getCurrentPreset();
    const float cVal = (float)cutoff.getValue(), rVal = (float)reso.getValue();
    const std::array<float, 17> key{ shelvesEq ? 1.0f : 0.0f, (float)p.steppedParams[spFilterModel],
                                     (float)p.steppedParams[spFilterMode], (float)p.steppedParams[spSemModel], cVal, rVal,
                                     bands[0].freqPot, bands[0].gainDb, bands[1].freqPot, bands[1].gainDb, bands[1].q,
                                     bands[2].freqPot, bands[2].gainDb, bands[2].q, bands[3].freqPot, bands[3].gainDb,
                                     (float)width };
    if (key == responseKey && (int)response.size() == width) return response;
    responseKey = key;
    response.resize((size_t)std::max(0, width));

    const int fModel = p.steppedParams[spFilterModel], fMode = p.steppedParams[spFilterMode];
    const bool liquid = fModel == 1 && p.steppedParams[spSemModel] == 4;
    const float fHz = 20.0f * std::pow(1300.0f, cVal / 999.0f);   // filter cutoff range
    const float Q = 0.5f + (rVal / 999.0f) * 9.5f;
    for (int x = 0; x < width; ++x) {
        const float normFreq = (float)x / (float)width;
        const float f = 20.0f * std::pow(10.0f, normFreq * 3.0f);
        if (shelvesEq) {
            const float rLs = f / std::max(10.0f, bands[0].hz);        // low shelf
            const float dbLow = bands[0].gainDb / (1.0f + rLs * rLs);
            const float r1 = f / std::max(10.0f, bands[1].hz);         // mid 1 bell
            const float dbMid1 = bands[1].gainDb * (1.0f / (1.0f + bands[1].q * bands[1].q * std::pow(r1 - 1.0f / r1, 2.0f)));
            const float r2 = f / std::max(10.0f, bands[2].hz);         // mid 2 bell
            const float dbMid2 = bands[2].gainDb * (1.0f / (1.0f + bands[2].q * bands[2].q * std::pow(r2 - 1.0f / r2, 2.0f)));
            const float rHs = f / std::max(10.0f, bands[3].hz);        // high shelf
            const float dbHigh = bands[3].gainDb * (rHs * rHs) / (1.0f + rHs * rHs);
            response[(size_t)x] = dbLow + dbMid1 + dbMid2 + dbHigh;
        } else {
            const float mag = filterMagnitude(fModel, fMode, liquid, rVal, Q, f / fHz);
            response[(size_t)x] = 20.0f * std::log10(std::max(1e-4f, mag));
        }
    }
    return response;
}

void FilterCurveComponent::paintHeader(juce::Graphics& g, juce::Rectangle<float> bounds, const ModernTheme& theme,
                                       ModernLookAndFeel* lnf, const juce::String& badge) {
    auto headerRect = bounds.withHeight(24.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(headerRect);
    g.setColour(theme.accent);
    g.fillRect(bounds.getX(), 23.0f, bounds.getWidth(), 1.5f);   // accent line under the header

    g.setFont(curveFont(lnf, 11.0f, juce::Font::bold));
    g.setColour(theme.textTitle);
    g.drawText("FREQUENCY RESPONSE", 10, 2, 180, 20, juce::Justification::centredLeft, false);

    // Filter badge, right-aligned in the header
    g.setFont(curveFont(lnf, 8.5f, juce::Font::bold));
    int badgeW = (int)g.getCurrentFont().getStringWidth(badge) + 12;
    auto badgeRect = juce::Rectangle<float>(bounds.getRight() - (float)badgeW - 8.0f, 4.0f, (float)badgeW, 16.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(badgeRect);
    g.setColour(theme.accentDark);
    g.drawRect(badgeRect, 1.0f);
    g.setColour(theme.accent);
    g.drawText(badge, badgeRect, juce::Justification::centred, false);
}

// Frequency lines (50 Hz .. 20 kHz) and dB lines: +-18 dB for the EQ,
// +12 .. -36 dB for the filters.
void FilterCurveComponent::paintGrid(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Rectangle<float> disp,
                                     const ModernTheme& theme, ModernLookAndFeel* lnf, bool shelvesEq, juce::Colour curveColour) {
    const float freqs[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    const char* fLabels[] = { "50", "100", "250", "500", "1k", "2k", "5k", "10k", "20k" };
    g.setFont(curveFont(lnf, 9.0f, juce::Font::plain));
    for (int i = 0; i < 9; ++i) {
        float normX = std::log10(freqs[i] / 20.0f) / 3.0f;
        float fx = disp.getX() + normX * disp.getWidth();
        g.setColour(theme.visualizerGrid);
        g.drawVerticalLine((int)fx, disp.getY(), disp.getBottom());
        g.setColour(theme.textMuted);
        g.drawText(fLabels[i], (int)fx - 14, (int)disp.getBottom() - 14, 28, 12, juce::Justification::centred, false);
    }

    const std::vector<float> dBs = shelvesEq ? std::vector<float>{ 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f }
                                             : std::vector<float>{ 12.0f, 0.0f, -12.0f, -24.0f, -36.0f };
    const float top = shelvesEq ? 24.0f : 18.0f, range = shelvesEq ? 48.0f : 60.0f;
    const float zeroAlpha = shelvesEq ? 0.35f : 0.25f;
    for (float db : dBs) {
        float normY = (top - db) / range;
        float fy = disp.getY() + normY * disp.getHeight();
        g.setColour(juce::Colour((db == 0.0f) ? (curveColour.withAlpha(zeroAlpha)) : theme.visualizerGrid));
        g.drawHorizontalLine((int)fy, disp.getX(), disp.getRight());

        g.setColour(theme.textMuted);
        juce::String dbText = (db > 0) ? ("+" + juce::String((int)db)) : juce::String((int)db);
        g.drawText(dbText, (int)bounds.getX() + 2, (int)fy - 6, 24, 12, juce::Justification::right, false);
    }
}

// The four EQ band handles; the active one larger with crosshairs.
void FilterCurveComponent::paintEqHandles(juce::Graphics& g, juce::Rectangle<float> disp, const ModernTheme& theme,
                                          ModernLookAndFeel* lnf, const std::array<EqBandView, 4>& bands, juce::Colour curveColour) {
    const char* nodeLabels[] = { "1", "2", "3", "4" };
    for (int i = 0; i < 4; ++i) {
        float nx = disp.getX() + (bands[(size_t)i].freqPot / 999.0f) * disp.getWidth();
        float ny = disp.getY() + ((24.0f - bands[(size_t)i].gainDb) / 48.0f) * disp.getHeight();
        ny = std::clamp(ny, disp.getY() + 8.0f, disp.getBottom() - 8.0f);

        const bool isActive = (i == activeBand);
        if (isActive) {
            g.setColour(curveColour.withAlpha(0.28f));
            g.drawVerticalLine((int)nx, disp.getY(), disp.getBottom());
            g.drawHorizontalLine((int)ny, disp.getX(), disp.getRight());
        }

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
        g.setFont(curveFont(lnf, isActive ? 10.0f : 9.0f, juce::Font::bold));
        g.drawText(nodeLabels[i], nodeRect.toNearestInt(), juce::Justification::centred, false);
    }
}

// The filter's cutoff / resonance handle with crosshairs.
void FilterCurveComponent::paintFilterHandle(juce::Graphics& g, juce::Rectangle<float> disp, const ModernTheme& theme,
                                             juce::Colour curveColour) {
    const float nodeX = disp.getX() + ((float)cutoff.getValue() / 999.0f) * disp.getWidth();
    const float nodeY = disp.getY() + (1.0f - ((float)reso.getValue() / 999.0f)) * disp.getHeight();
    g.setColour(curveColour.withAlpha(0.25f));
    g.drawVerticalLine((int)nodeX, disp.getY(), disp.getBottom());
    g.drawHorizontalLine((int)nodeY, disp.getX(), disp.getRight());
    g.setColour(theme.accentDark);
    g.fillRect(nodeX - 5.0f, nodeY - 5.0f, 10.0f, 10.0f);
    g.setColour(theme.accent);
    g.drawRect(nodeX - 5.0f, nodeY - 5.0f, 10.0f, 10.0f, 1.5f);
}

// The live readout: the active EQ band, or cutoff and resonance.
juce::String FilterCurveComponent::telemetryText(bool shelvesEq, const std::array<EqBandView, 4>& bands) const {
    if (shelvesEq) {
        const auto& b = bands[(size_t)std::clamp(activeBand, 0, 3)];
        if (activeBand == 0) return "LOW SHELF: " + formatEqHz(b.freqPot) + " | Gain: " + formatEqDb(b.gainDb);
        if (activeBand == 1) return "MID LOW: " + formatEqHz(b.freqPot) + " | Gain: " + formatEqDb(b.gainDb) + " | Q: " + juce::String(b.q, 2);
        if (activeBand == 2) return "MID HIGH: " + formatEqHz(b.freqPot) + " | Gain: " + formatEqDb(b.gainDb) + " | Q: " + juce::String(b.q, 2);
        return "HIGH SHELF: " + formatEqHz(b.freqPot) + " | Gain: " + formatEqDb(b.gainDb);
    }
    const float fHz = 20.0f * std::pow(1300.0f, (float)cutoff.getValue() / 999.0f);   // filter cutoff range
    const juce::String hzStr = (fHz >= 1000.0f) ? juce::String(fHz / 1000.0f, 2) + " kHz"
                                                : juce::String((int)std::round(fHz)) + " Hz";
    const int resoPct = (int)std::round(((float)reso.getValue() / 999.0f) * 100.0f);
    return "Cutoff: " + hzStr + "   |   Resonance: " + juce::String(resoPct) + "%";
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

    const auto& preset = model.getCurrentPreset();
    const uint8_t fModel = preset.steppedParams[spFilterModel];
    const uint8_t fMode = preset.steppedParams[spFilterMode];
    const uint8_t semVariant = preset.steppedParams[spSemModel];
    const bool isShelvesEQ = (fModel == 2 && fMode == 0);
    juce::Colour curveColour = theme.accent; // SSI2144 and SST: the theme accent
    if (fModel == 1) curveColour = juce::Colour(0xff00e676).interpolatedWith(theme.accent, 0.4f);        // SEM
    else if (fModel == 2) curveColour = juce::Colour(0xffff9100).interpolatedWith(theme.accent, 0.3f);   // Shelves

    static const char* semBadges[] = { "SEM OB-XD 12 DB", "SEM OBERHEIM", "SEM VULT SVF", "SEM CYTOMIC SVF", "LIQUID RIPPLES" };
    const juce::String filterBadge = isShelvesEQ ? "SHELVES 4-BAND EQ" :
                                     (fModel == 1 ? semBadges[std::min<int>(semVariant, 4)] :
                                     (fModel == 3 ? "SST VINTAGE LADDER" : "SSI2144 LADDER"));
    paintHeader(g, bounds, theme, lnf, filterBadge);

    const auto bands = eqBandViews();
    const juce::String telemetry = telemetryText(isShelvesEQ, bands);

    // Display area
    auto disp = bounds.reduced(10.0f).withTrimmedTop(30.0f).withTrimmedLeft(28.0f);
    g.setColour(theme.windowBg);
    g.fillRect(disp);
    g.setColour(theme.cardBorder);
    g.drawRect(disp, 1.0f);
    if (disp.getWidth() <= 20.0f || disp.getHeight() <= 20.0f) return;

    paintGrid(g, bounds, disp, theme, lnf, isShelvesEQ, curveColour);

    // Master output behind the filter curve: the 8-bit spectrum, the line
    // waterfall above it
    waterfall.setWidth(disp.getWidth());
    spectrum.setWidth(disp.getWidth());
    if (model.isRetroSpectrumShown()) spectrum.draw(g, disp.reduced(2.0f), theme, model.retroOpacityFilter,
                                                    model.retroRandomness);
    if (model.isSpectrumWaterfallShown()) waterfall.draw(g, disp.reduced(1.0f), theme, model.waterfallOpacity);

    // The curve: EQ from +24 dB over 48 dB, filters from +18 dB over 60 dB
    const auto& db = responseDb((int)disp.getWidth(), isShelvesEQ, bands);
    const float top = isShelvesEQ ? 24.0f : 18.0f, range = isShelvesEQ ? 48.0f : 60.0f;
    juce::Path curvePath, fillPath;
    for (int x = 0; x < (int)db.size(); ++x) {
        float py = disp.getY() + ((top - db[(size_t)x]) / range) * disp.getHeight();
        py = std::clamp(py, disp.getY(), disp.getBottom());
        const float px = disp.getX() + (float)x;
        if (x == 0) {
            curvePath.startNewSubPath(px, py);
            fillPath.startNewSubPath(px, disp.getBottom());
            fillPath.lineTo(px, py);
        } else {
            curvePath.lineTo(px, py);
            fillPath.lineTo(px, py);
        }
    }
    fillPath.lineTo(disp.getRight(), disp.getBottom());
    fillPath.closeSubPath();

    // Shaded fill with model-specific hue, then the curve line
    juce::ColourGradient fillGrad(curveColour.withAlpha(0.20f), 0, disp.getY(),
                                  curveColour.withAlpha(0.02f), 0, disp.getBottom(), false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);
    g.setColour(curveColour);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    if (isShelvesEQ) paintEqHandles(g, disp, theme, lnf, bands, curveColour);
    else paintFilterHandle(g, disp, theme, curveColour);

    // Live readout (top right of the display)
    g.setFont(curveFont(lnf, 9.5f, juce::Font::bold));
    g.setColour(curveColour);
    g.drawText(telemetry, (int)disp.getRight() - 325, (int)disp.getY() + 3, 315, 16, juce::Justification::right, false);
}
