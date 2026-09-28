#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// ==============================================================================
// Modern Section Card Component (Visual Grouping Container with Sharp Dividers)
// ==============================================================================
class ModernSectionCard : public juce::Component {
public:
    struct Divider {
        int y = 0;
        juce::String label;
        int x = -1;
        int width = -1;
    };

    struct VerticalDivider {
        int x = 0;
        int yTop = 0;
        int yBottom = 0;
    };

    ModernSectionCard() = default;
    explicit ModernSectionCard(const juce::String& titleText, const juce::String& badgeText = "");

    void setHeader(const juce::String& titleText, const juce::String& badgeText = "");
    void addDivider(int yPos, const juce::String& label = "");
    void addDivider(int xPos, int yPos, int width, const juce::String& label = "");
    void addVerticalDivider(int xPos, int yTop, int yBottom);
    void clearDividers();
    void setAccentBar(bool enable, int width = 3, juce::Colour col = juce::Colour());

    void paint(juce::Graphics& g) override;

private:
    juce::String title;
    juce::String badge;
    std::vector<Divider> dividers;
    std::vector<VerticalDivider> verticalDividers;
    bool showAccentBar = false;
    int accentBarWidth = 3;
    juce::Colour customAccentBarColor;
};
