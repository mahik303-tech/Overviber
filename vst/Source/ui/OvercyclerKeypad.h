#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <string>

class OvercyclerKeypad : public juce::Component {
public:
    using KeyPressCallback = std::function<void(char key)>;

    enum LayoutMode {
        LayoutPcNumpad,   // 7 8 9 on top (standard PC numeric keypad layout)
        LayoutHardware    // 1 2 3 on top (original GliGli hardware matrix)
    };

    struct KeyDef {
        char key;
        std::string label;
    };

    OvercyclerKeypad();
    ~OvercyclerKeypad() override = default;

    void setOnKeyPress(KeyPressCallback cb) { onKeyPress = cb; }
    void setLayoutMode(LayoutMode mode);
    LayoutMode getLayoutMode() const { return currentLayout; }
    void setActiveKey(char keyChar);

    // Dynamic context-sensitive sublabels for action buttons A, B, C, D
    void setActionSublabels(const std::string& a, const std::string& b, const std::string& c, const std::string& d);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class KeyButton : public juce::Button {
    public:
        KeyButton(char keyChar, const juce::String& subLabelText);
        void setKeyInfo(char newKey, const juce::String& newLabel);
        void setActive(bool active) { isActive = active; repaint(); }
        char getKey() const { return key; }

        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    private:
        char key;
        juce::String subLabel;
        bool isActive{ false };
    };

    LayoutMode currentLayout{ LayoutPcNumpad };
    char activeKey{ '1' };

    std::string subLabelA{ "AXoS" };
    std::string subLabelB{ "BXoS" };
    std::string subLabelC{ "FrqM" };
    std::string subLabelD{ "Sync" };

    juce::TextButton layoutToggleBtn;
    std::unique_ptr<KeyButton> buttons[16];
    KeyPressCallback onKeyPress;

    void updateButtonLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OvercyclerKeypad)
};
