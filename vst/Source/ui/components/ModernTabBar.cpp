#include "ModernTabBar.h"
#include "../theme/ModernTheme.h"

ModernTabBar::ModernTabBar() {
    const char* titles[tabCount] = {
        "OSC", "FILTER / VCA", "ENV", "LFO / ARP", "AFX", "MOD MATRIX", "SETTINGS", "LUA"
    };

    for (int i = 0; i < tabCount; ++i) {
        buttons[i].setButtonText(titles[i]);
        buttons[i].setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        buttons[i].onClick = [this, i]() {
            setSelectedTab(static_cast<Tab>(i));
            if (onTabSelected)
                onTabSelected(selectedTab);
        };
        addAndMakeVisible(buttons[i]);
    }
    setSelectedTab(Tab::Osc);
}

void ModernTabBar::setSelectedTab(Tab tab) {
    const int selected = juce::jlimit(0, tabCount - 1, static_cast<int>(tab));
    selectedTab = static_cast<Tab>(selected);
    for (int i = 0; i < tabCount; ++i)
        buttons[i].setToggleState(i == selected, juce::dontSendNotification);
}

void ModernTabBar::resized() {
    constexpr int tabGap = 5;
    constexpr int tabH = 34;
    constexpr int startX = 16; // left edge of the tab content (ModernEditorView insets it by 16 px)
    // The tabs end before the voice meter in the top right corner.
    const int maxTabAreaW = getWidth() - startX - ComponentTokens::protectedVoiceMonitorWidth;
    const int tabW = juce::jlimit(60, 115, (maxTabAreaW - (tabCount - 1) * tabGap) / tabCount);
    const int startY = juce::jmax(0, (getHeight() - tabH) / 2);

    for (int i = 0; i < tabCount; ++i)
        buttons[i].setBounds(startX + i * (tabW + tabGap), startY, tabW, tabH);
}
