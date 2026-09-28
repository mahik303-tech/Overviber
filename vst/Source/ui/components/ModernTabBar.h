#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class ModernTabBar : public juce::Component {
public:
    enum class Tab {
        Osc = 0,
        FilterVca,
        Envelopes,
        LfoArp,
        Afx,
        ModMatrix,
        Settings,
        Count
    };

    ModernTabBar();

    std::function<void(Tab)> onTabSelected;
    void setSelectedTab(Tab tab);
    Tab getSelectedTab() const noexcept { return selectedTab; }
    juce::TextButton& getButton(Tab tab) { return buttons[static_cast<int>(tab)]; }

    void resized() override;

private:
    static constexpr int tabCount = static_cast<int>(Tab::Count);
    juce::TextButton buttons[tabCount];
    Tab selectedTab = Tab::Osc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModernTabBar)
};
