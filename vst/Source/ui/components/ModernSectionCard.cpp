#include "ModernSectionCard.h"
#include "ModernLookAndFeel.h"
#include <algorithm>

// ==============================================================================
// ModernSectionCard Implementation
// ==============================================================================
ModernSectionCard::ModernSectionCard(const juce::String& titleText, const juce::String& badgeText)
    : title(titleText), badge(badgeText) {}

void ModernSectionCard::setHeader(const juce::String& titleText, const juce::String& badgeText) {
    title = titleText;
    badge = badgeText;
    repaint();
}

void ModernSectionCard::addDivider(int yPos, const juce::String& label) {
    dividers.push_back({ yPos, label, -1, -1 });
    repaint();
}

void ModernSectionCard::addDivider(int xPos, int yPos, int width, const juce::String& label) {
    dividers.push_back({ yPos, label, xPos, width });
    repaint();
}

void ModernSectionCard::addVerticalDivider(int xPos, int yTop, int yBottom) {
    verticalDividers.push_back({ xPos, yTop, yBottom });
    repaint();
}

void ModernSectionCard::clearDividers() {
    dividers.clear();
    verticalDividers.clear();
    repaint();
}

void ModernSectionCard::setAccentBar(bool enable, int width, juce::Colour col) {
    showAccentBar = enable;
    accentBarWidth = std::clamp(width, 3, 5);
    customAccentBarColor = col;
    repaint();
}

void ModernSectionCard::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];

    // Sharp dark container body (no rounded corners)
    g.setColour(theme.cardBg);
    g.fillRect(bounds);

    // Optional Accent Bar (Specification Section 3: width 3-5px, full section height)
    if (showAccentBar) {
        g.setColour(customAccentBarColor.isTransparent() ? theme.accent : customAccentBarColor);
        g.fillRect(bounds.getX(), bounds.getY(), (float)accentBarWidth, bounds.getHeight());
    }

    // Frame outline (sharp)
    g.setColour(theme.cardBorder);
    g.drawRect(bounds, 1.0f);

    // Header bar
    auto headerRect = bounds.withHeight(24.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(headerRect);

    // Accent line underneath header
    g.setColour(theme.accent);
    g.fillRect(bounds.getX(), 23.0f, bounds.getWidth(), 1.5f);

    // Header title
    g.setFont(lnf ? lnf->getCustomFont(11.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::bold));
    g.setColour(theme.textTitle);
    g.drawText(title, 10, 2, (int)bounds.getWidth() - 90, 20, juce::Justification::centredLeft, false);

    // Optional badge
    if (badge.isNotEmpty()) {
        g.setFont(lnf ? lnf->getCustomFont(9.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::bold));
        int badgeW = (int)g.getCurrentFont().getStringWidth(badge) + 12;
        auto badgeRect = juce::Rectangle<float>(bounds.getRight() - badgeW - 6.0f, 4.0f, (float)badgeW, 16.0f);
        g.setColour(theme.cardHeader);
        g.fillRect(badgeRect);
        g.setColour(theme.accentDark);
        g.drawRect(badgeRect, 1.0f);
        g.setColour(theme.accent);
        g.drawText(badge, badgeRect, juce::Justification::centred, false);
    }

    // Dividers with sub-labels
    for (const auto& div : dividers) {
        if (div.y < 30) continue; // Safety guard: ignore any divider that would collide with the header bar
        float y = (float)div.y;
        float startX = (div.x >= 0) ? (bounds.getX() + (float)div.x) : (bounds.getX() + 4.0f);
        float endX = (div.width > 0) ? (startX + (float)div.width) : (bounds.getRight() - 4.0f);

        if (div.label.isNotEmpty()) {
            g.setFont(lnf ? lnf->getCustomFont(9.0f, juce::Font::bold) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::bold));
            int lblW = (int)g.getCurrentFont().getStringWidth(div.label) + 10;
            float textX = startX + 6.0f;
            float lineRightX = textX + (float)lblW + 4.0f;

            g.setColour(theme.cardBorder);
            g.drawHorizontalLine((int)y, startX, textX - 4.0f);
            g.drawHorizontalLine((int)y, lineRightX, endX);

            g.setColour(theme.textMuted);
            g.drawText(div.label, (int)textX, (int)y - 7, lblW, 14, juce::Justification::centredLeft, false);
        } else {
            g.setColour(theme.cardBorder);
            g.drawHorizontalLine((int)y, startX, endX);
        }
    }

    // Vertical Dividers
    for (const auto& vdiv : verticalDividers) {
        g.setColour(theme.cardBorder);
        g.drawVerticalLine(vdiv.x, (float)vdiv.yTop, (float)vdiv.yBottom);
    }
}
