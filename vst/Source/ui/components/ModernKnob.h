#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// ==============================================================================
// A knob whose right click (or Ctrl click) opens a menu, the modulation menu
// (ModernEditorView::showModulationMenu), instead of turning the knob. When
// onPopupMenu is unset or declines, it behaves like a plain juce::Slider.
// ==============================================================================
class ModernKnob : public juce::Slider {
public:
    using juce::Slider::Slider;

    // Returns true when it showed a menu.
    std::function<bool()> onPopupMenu;

    void mouseDown(const juce::MouseEvent& e) override {
        menuGesture = e.mods.isPopupMenu() && onPopupMenu && onPopupMenu();
        if (!menuGesture) juce::Slider::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!menuGesture) juce::Slider::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if (!menuGesture) juce::Slider::mouseUp(e);
        menuGesture = false;
    }

private:
    bool menuGesture = false;
};
