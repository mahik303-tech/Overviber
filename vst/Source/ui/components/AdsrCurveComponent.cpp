#include "AdsrCurveComponent.h"
#include <algorithm>

AdsrCurveComponent::AdsrCurveComponent(SynthModel& eng,
                                       juce::Slider& aKnob, juce::Slider& dKnob,
                                       juce::Slider& sKnob, juce::Slider& relKnob,
                                       const juce::String& titleText)
    : model(eng), att(aKnob), dec(dKnob), sus(sKnob), rel(relKnob), title(titleText) {
    att.addListener(this);
    dec.addListener(this);
    sus.addListener(this);
    rel.addListener(this);
}

AdsrCurveComponent::~AdsrCurveComponent() {
    att.removeListener(this);
    dec.removeListener(this);
    sus.removeListener(this);
    rel.removeListener(this);
}

void AdsrCurveComponent::mouseDown(const juce::MouseEvent& e) {
    auto disp = getLocalBounds().toFloat().reduced(10.0f).withTrimmedTop(24.0f).withTrimmedBottom(18.0f);
    float dW = disp.getWidth();
    float dH = disp.getHeight();

    float aFrac = (float)att.getValue() / 999.0f;
    float dFrac = (float)dec.getValue() / 999.0f;
    float sFrac = (float)sus.getValue() / 999.0f;
    float rFrac = (float)rel.getValue() / 999.0f;

    float wA = 6.0f + aFrac * (dW * 0.28f);
    float wD = 6.0f + dFrac * (dW * 0.28f);
    float wS = dW * 0.22f;
    float wR = 6.0f + rFrac * (dW * 0.28f);

    juce::Point<float> pA(disp.getX() + wA, disp.getY());
    juce::Point<float> pD(disp.getX() + wA + wD, disp.getBottom() - sFrac * dH);
    juce::Point<float> pR(disp.getX() + wA + wD + wS + wR, disp.getBottom());

    float distA = pA.getDistanceFrom(e.position);
    float distD = pD.getDistanceFrom(e.position);
    float distR = pR.getDistanceFrom(e.position);

    if (distA <= 16.0f) activeHandle = 1;
    else if (distD <= 16.0f) activeHandle = 2;
    else if (distR <= 16.0f) activeHandle = 3;
    else {
        if (distA < distD && distA < distR) activeHandle = 1;
        else if (distD < distR) activeHandle = 2;
        else activeHandle = 3;
    }
}

void AdsrCurveComponent::mouseDrag(const juce::MouseEvent& e) {
    auto disp = getLocalBounds().toFloat().reduced(10.0f).withTrimmedTop(24.0f).withTrimmedBottom(18.0f);
    float dW = disp.getWidth();
    float dH = disp.getHeight();

    if (activeHandle == 1) { // Attack
        float aX = (e.x - disp.getX() - 6.0f) / (dW * 0.28f);
        att.setValue(std::clamp(aX, 0.0f, 1.0f) * 999.0f, juce::sendNotificationSync);
    } else if (activeHandle == 2) { // Decay (X) and Sustain (Y)
        float aFrac = (float)att.getValue() / 999.0f;
        float wA = 6.0f + aFrac * (dW * 0.28f);
        float dX = (e.x - (disp.getX() + wA) - 6.0f) / (dW * 0.28f);
        float sY = (disp.getBottom() - e.y) / dH;

        dec.setValue(std::clamp(dX, 0.0f, 1.0f) * 999.0f, juce::sendNotificationSync);
        sus.setValue(std::clamp(sY, 0.0f, 1.0f) * 999.0f, juce::sendNotificationSync);
    } else if (activeHandle == 3) { // Release
        float aFrac = (float)att.getValue() / 999.0f;
        float dFrac = (float)dec.getValue() / 999.0f;
        float wA = 6.0f + aFrac * (dW * 0.28f);
        float wD = 6.0f + dFrac * (dW * 0.28f);
        float wS = dW * 0.22f;
        float rX = (e.x - (disp.getX() + wA + wD + wS) - 6.0f) / (dW * 0.28f);

        rel.setValue(std::clamp(rX, 0.0f, 1.0f) * 999.0f, juce::sendNotificationSync);
    }
    repaint();
}

void AdsrCurveComponent::mouseUp(const juce::MouseEvent& /*e*/) {
    activeHandle = 0;
}

void AdsrCurveComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];

    // Sharp dark frame (no rounded corners)
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
    g.drawText(title.toUpperCase(), 10, 2, (int)bounds.getWidth() - 110, 20, juce::Justification::centredLeft, false);

    // Badge
    juce::String envBadge = "ADSR STAGES";
    g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::bold));
    int badgeW = (int)g.getCurrentFont().getStringWidth(envBadge) + 12;
    auto badgeRect = juce::Rectangle<float>(bounds.getRight() - badgeW - 6.0f, 4.0f, (float)badgeW, 16.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(badgeRect);
    g.setColour(theme.accentDark);
    g.drawRect(badgeRect, 1.0f);
    g.setColour(theme.accent);
    g.drawText(envBadge, badgeRect, juce::Justification::centred, false);

    // Display area
    auto disp = bounds.reduced(10.0f).withTrimmedTop(30.0f).withTrimmedBottom(18.0f);
    g.setColour(theme.windowBg);
    g.fillRect(disp);
    g.setColour(theme.cardBorder);
    g.drawRect(disp, 1.0f);

    if (disp.getWidth() <= 10.0f || disp.getHeight() <= 10.0f) return;

    // Grid lines
    g.setColour(theme.visualizerGrid);
    g.drawHorizontalLine((int)disp.getCentreY(), disp.getX(), disp.getRight());

    // Envelope calculations
    float dW = disp.getWidth();
    float dH = disp.getHeight();

    float aFrac = (float)att.getValue() / 999.0f;
    float dFrac = (float)dec.getValue() / 999.0f;
    float sFrac = (float)sus.getValue() / 999.0f;
    float rFrac = (float)rel.getValue() / 999.0f;

    float wA = 6.0f + aFrac * (dW * 0.28f);
    float wD = 6.0f + dFrac * (dW * 0.28f);
    float wS = dW * 0.22f;
    float wR = 6.0f + rFrac * (dW * 0.28f);

    juce::Point<float> p0(disp.getX(), disp.getBottom());
    juce::Point<float> pA(disp.getX() + wA, disp.getY());
    juce::Point<float> pD(disp.getX() + wA + wD, disp.getBottom() - sFrac * dH);
    juce::Point<float> pS(disp.getX() + wA + wD + wS, disp.getBottom() - sFrac * dH);
    juce::Point<float> pR(disp.getX() + wA + wD + wS + wR, disp.getBottom());

    // Path
    juce::Path envPath;
    envPath.startNewSubPath(p0);
    envPath.lineTo(pA);
    envPath.lineTo(pD);
    envPath.lineTo(pS);
    envPath.lineTo(pR);

    juce::Path fillPath = envPath;
    fillPath.lineTo(pR.x, disp.getBottom());
    fillPath.lineTo(p0.x, disp.getBottom());
    fillPath.closeSubPath();

    // Shaded gradient fill
    juce::ColourGradient fillGrad(theme.accent.withAlpha(0.22f), 0, disp.getY(),
                                 theme.accent.withAlpha(0.02f), 0, disp.getBottom(), false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    // Glowing accent stroke
    g.setColour(theme.accent);
    g.strokePath(envPath, juce::PathStrokeType(2.0f));

    // Sharp square handles for A, D/S, R (strictly no rounded corners)
    auto drawHandle = [&g, &theme](const juce::Point<float>& pt, bool isHighlighted) {
        g.setColour(isHighlighted ? theme.accent : theme.accentDark);
        g.fillRect(pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f);
        g.setColour(isHighlighted ? juce::Colours::white : theme.accent);
        g.drawRect(pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f, 1.2f);
    };

    drawHandle(pA, activeHandle == 1);
    drawHandle(pD, activeHandle == 2);
    drawHandle(pR, activeHandle == 3);

    // Stage labels at the bottom (A, D, S, R)
    g.setFont(lnf ? lnf->getCustomFont(9.5f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.5f, juce::Font::bold));
    g.setColour(theme.textMuted);
    float yLbl = bounds.getBottom() - 15.0f;
    g.drawText("A", (int)(disp.getX() + wA * 0.5f) - 10, (int)yLbl, 20, 14, juce::Justification::centred, false);
    g.drawText("D", (int)(pA.x + wD * 0.5f) - 10, (int)yLbl, 20, 14, juce::Justification::centred, false);
    g.drawText("S", (int)(pD.x + wS * 0.5f) - 10, (int)yLbl, 20, 14, juce::Justification::centred, false);
    g.drawText("R", (int)(pS.x + wR * 0.5f) - 10, (int)yLbl, 20, 14, juce::Justification::centred, false);
}
