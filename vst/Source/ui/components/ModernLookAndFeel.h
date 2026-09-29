#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../theme/ModernTheme.h"
#include "../theme/ModernFontManager.h"

// ==============================================================================
// Modern Look & Feel with Sharp Industrial Hardware Styling
// ==============================================================================
//
// Focus rings show only while the keyboard is used to navigate: the editor
// passes its key presses and mouse clicks here (addKeyListener and
// addMouseListener); Tab turns the rings on, a click turns them off, so a
// clicked control keeps its normal look.
class ModernLookAndFeel : public juce::LookAndFeel_V4, public juce::KeyListener, public juce::MouseListener {
public:
    ModernLookAndFeel();

    bool showsKeyboardFocus(const juce::Component& c) const { return keyboardNavigation && c.hasKeyboardFocus(true); }
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void mouseDown(const juce::MouseEvent& e) override;

    void setTheme(const ModernTheme& newTheme);
    const ModernTheme& getTheme() const { return currentTheme; }

    void setFontFamily(const juce::String& familyName);
    const juce::String& getFontFamily() const { return currentFontFamily; }

    void setFontScale(float scale);
    float getFontScale() const { return currentFontScale; }

    juce::Font getCustomFont(float size, int style = juce::Font::plain) const;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;
    juce::Font getTooltipFont();
    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor&) override;
    juce::Label* createSliderTextBox(juce::Slider& slider) override;
    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor&) override;

private:
    void drawElementsKnob(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                          juce::Colour cap, bool hovered, bool focused);
    void setKeyboardNavigation(bool keyboard);
    bool keyboardNavigation = false;
    ModernTheme currentTheme = ModernTheme::getPresetThemes()[0];
    juce::String currentFontFamily = "D-DIN";
    float currentFontScale = 1.0f;
};
