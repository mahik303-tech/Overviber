#include "ModernLookAndFeel.h"
#include <cmath>
#include <algorithm>

// ==============================================================================
// Modern Look & Feel Implementation (Sharp Industrial Hardware Styling)
// ==============================================================================
ModernLookAndFeel::ModernLookAndFeel() {
    setTheme(ModernTheme::getPresetThemes()[0]);
    setFontFamily("D-DIN");
}

void ModernLookAndFeel::setTheme(const ModernTheme& newTheme) {
    currentTheme = newTheme;
    setColour(juce::Slider::rotarySliderFillColourId, currentTheme.accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, currentTheme.knobTrack);
    setColour(juce::Slider::thumbColourId, currentTheme.knobNeedle);
    setColour(juce::ComboBox::backgroundColourId, currentTheme.cardBg);
    setColour(juce::ComboBox::outlineColourId, currentTheme.cardBorder);
    setColour(juce::ComboBox::textColourId, currentTheme.textBody);
    setColour(juce::ComboBox::arrowColourId, currentTheme.accent);
    setColour(juce::TextButton::buttonColourId, currentTheme.buttonBg);
    setColour(juce::TextButton::buttonOnColourId, currentTheme.accentDark);
    setColour(juce::TextButton::textColourOffId, currentTheme.textBody);
    setColour(juce::TextButton::textColourOnId, currentTheme.textTitle);
    setColour(juce::PopupMenu::backgroundColourId, currentTheme.cardBg);
    setColour(juce::PopupMenu::textColourId, currentTheme.textBody);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, currentTheme.accentDark);
    setColour(juce::PopupMenu::highlightedTextColourId, currentTheme.textTitle);
    setColour(juce::Label::textColourId, currentTheme.textMuted);
}

void ModernLookAndFeel::setFontFamily(const juce::String& familyName) {
    currentFontFamily = familyName;
}

void ModernLookAndFeel::setFontScale(float scale) {
    currentFontScale = std::clamp(scale, 0.7f, 1.5f);
}

juce::Font ModernLookAndFeel::getCustomFont(float size, int style) const {
    return ModernFontManager::createFont(currentFontFamily, size, style, currentFontScale);
}

juce::Font ModernLookAndFeel::getLabelFont(juce::Label&) {
    return getCustomFont(9.0f, juce::Font::bold);
}

juce::Font ModernLookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return getCustomFont(11.0f, juce::Font::plain);
}

juce::Font ModernLookAndFeel::getTextButtonFont(juce::TextButton& button, int /*buttonHeight*/) {
    // "compactFont": the toggle size, for small button rows such as the EQ bands.
    if (button.getProperties()["compactFont"]) return getCustomFont(9.0f, juce::Font::bold);
    return getCustomFont(10.5f, juce::Font::bold);
}

void ModernLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider& slider) {
    if (width <= 0 || height <= 0) return;

    if (!slider.isEnabled())
        g.setOpacity(0.45f);

    bool isHovered = slider.isMouseOverOrDragging();

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius < 6.0f) return;

    sliderPosProportional = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
    auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = 3.5f;
    auto arcRadius = radius - lineW * 0.5f;
    if (arcRadius <= 1.0f) return;

    auto center = bounds.getCentre();

    // Hardware knob in the style of the Elements panel (knobStyle "elements").
    if (slider.getProperties()["knobStyle"].toString() == "elements") {
        const auto capProperty = slider.getProperties()["capColour"];
        const juce::Colour cap = capProperty.isVoid() ? juce::Colour(0xfff2f2f2)
                                                      : juce::Colour((juce::uint32)(juce::int64)capProperty);
        drawElementsKnob(g, center, radius, toAngle, cap, isHovered);
        return;
    }

    // 1. Outer background track arc
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(currentTheme.knobTrack);
    g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // 2. Active value arc: the theme accent, or the knob's own "arcColour"
    // property (ARGB), e.g. the Elements exciter colours.
    const auto arcProperty = slider.getProperties()["arcColour"];
    const juce::Colour arcColour = arcProperty.isVoid() ? currentTheme.accent
                                                        : juce::Colour((juce::uint32)(juce::int64)arcProperty);
    bool isBipolar = (slider.getMinimum() < 0.0);
    float midAngle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;

    if (isBipolar) {
        if (std::abs(toAngle - midAngle) > 0.015f) {
            juce::Path valueArc;
            if (toAngle >= midAngle) {
                valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, midAngle, toAngle, true);
            } else {
                valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, toAngle, midAngle, true);
            }
            g.setColour(arcColour);
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
    } else {
        if (sliderPosProportional > 0.005f && toAngle > rotaryStartAngle + 0.01f) {
            juce::Path valueArc;
            valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(arcColour);
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
    }

    // 3. Dial body with hardware radial gradient
    float dialRadius = radius * 0.72f;
    juce::ColourGradient dialGrad(currentTheme.knobBodyTop, center.x, center.y - dialRadius,
                                 currentTheme.knobBodyBot, center.x, center.y + dialRadius, false);
    g.setGradientFill(dialGrad);
    g.fillEllipse(center.x - dialRadius, center.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

    // No focus highlight after a click: hover and value show the state.
    g.setColour(isHovered ? currentTheme.knobBorder.brighter(0.25f) : currentTheme.knobBorder);
    g.drawEllipse(center.x - dialRadius, center.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f, 1.2f);

    // 4. Center 12 o'clock orientation tick mark (unambiguous neutral index)
    {
        float tickY1 = center.y - radius;
        float tickY2 = center.y - radius + 3.0f;
        g.setColour(isBipolar ? currentTheme.accent : currentTheme.textMuted);
        g.drawVerticalLine((int)center.x, tickY1, tickY2);
    }

    // 5. Pointer line (Sharp rectangular industrial pointer needle)
    juce::Path p;
    auto pointerLength = dialRadius * 0.75f;
    auto pointerThickness = 2.2f;
    p.addRectangle(-pointerThickness * 0.5f, -dialRadius, pointerThickness, pointerLength * 0.6f);
    p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));
    g.setColour(isHovered ? currentTheme.accent : currentTheme.knobNeedle);
    g.fillPath(p);
}

// A knob like those of the Elements panel: a dark knurled body with its
// value notch on the rim and a coloured cap (white, red or teal); no value
// arc, the value is in the text below.
void ModernLookAndFeel::drawElementsKnob(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                                         juce::Colour cap, bool hovered) {
    const float bodyR = radius * 0.96f;
    auto circle = [&](float r) { return juce::Rectangle<float>(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f); };

    // Drop shadow and knurled body
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(circle(bodyR).translated(0.0f, 1.5f));
    const juce::Colour bodyTop(hovered ? 0xff4a4e55 : 0xff3c3f45), bodyBottom(0xff17181b);
    g.setGradientFill(juce::ColourGradient(bodyTop, centre.x, centre.y - bodyR, bodyBottom, centre.x, centre.y + bodyR, false));
    g.fillEllipse(circle(bodyR));
    constexpr int ridges = 28;
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    for (int i = 0; i < ridges; ++i) {
        const float a = juce::MathConstants<float>::twoPi * (float)i / (float)ridges;
        const juce::Point<float> dir(std::sin(a), -std::cos(a));
        g.drawLine(juce::Line<float>(centre + dir * (bodyR * 0.80f), centre + dir * (bodyR * 0.99f)), 1.0f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawEllipse(circle(bodyR), 1.0f);

    // Value notch on the rim, in the cap colour (white caps: light grey)
    const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
    const juce::Colour notch = cap.getBrightness() > 0.9f ? juce::Colour(0xffd8d8d8) : cap;
    g.setColour(notch);
    g.drawLine(juce::Line<float>(centre + dir * (bodyR * 0.66f), centre + dir * (bodyR * 0.97f)), juce::jmax(2.0f, radius * 0.09f));

    // Cap with a soft top light
    const float capR = bodyR * 0.60f;
    g.setGradientFill(juce::ColourGradient(cap.brighter(0.25f), centre.x, centre.y - capR,
                                           cap.darker(0.35f), centre.x, centre.y + capR, false));
    g.fillEllipse(circle(capR));
    g.setColour(cap.darker(0.6f));
    g.drawEllipse(circle(capR), 1.0f);
}

void ModernLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float minSliderPos, float maxSliderPos,
                                         const juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (width <= 0 || height <= 0) return;

    if (!slider.isEnabled())
        g.setOpacity(0.45f);

    bool isHovered = slider.isMouseOverOrDragging();
    bool isBipolar = (slider.getMinimum() < 0.0);
    bool isHorizontal = (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar);

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    float trackThickness = 2.0f;

    if (isHorizontal) {
        float midY = bounds.getCentreY();
        g.setColour(currentTheme.cardBorder);
        g.fillRect(bounds.getX(), midY - trackThickness * 0.5f, bounds.getWidth(), trackThickness);

        if (isBipolar) {
            float midX = (minSliderPos + maxSliderPos) * 0.5f;
            float leftX = std::min(midX, sliderPos);
            float fillW = std::abs(sliderPos - midX);
            g.setColour(currentTheme.accent);
            g.fillRect(leftX, midY - trackThickness * 0.5f, fillW, trackThickness);

            g.setColour(currentTheme.accent);
            g.drawVerticalLine((int)midX, midY - 6.0f, midY + 6.0f);
        } else {
            g.setColour(currentTheme.accent);
            g.fillRect(bounds.getX(), midY - trackThickness * 0.5f, sliderPos - bounds.getX(), trackThickness);
        }

        float thumbW = 8.0f;
        float thumbH = std::min(bounds.getHeight() - 4.0f, 18.0f);
        auto thumbRect = juce::Rectangle<float>(sliderPos - thumbW * 0.5f, midY - thumbH * 0.5f, thumbW, thumbH);

        g.setColour(currentTheme.accent);
        g.fillRect(thumbRect);
        g.setColour(isHovered ? currentTheme.textTitle : currentTheme.cardBorder);
        g.drawRect(thumbRect, 1.0f);
    } else {
        float midX = bounds.getCentreX();
        g.setColour(currentTheme.cardBorder);
        g.fillRect(midX - trackThickness * 0.5f, bounds.getY(), trackThickness, bounds.getHeight());

        if (isBipolar) {
            float midY = (minSliderPos + maxSliderPos) * 0.5f;
            float topY = std::min(midY, sliderPos);
            float fillH = std::abs(sliderPos - midY);
            g.setColour(currentTheme.accent);
            g.fillRect(midX - trackThickness * 0.5f, topY, trackThickness, fillH);

            g.setColour(currentTheme.accent);
            g.drawHorizontalLine((int)midY, midX - 6.0f, midX + 6.0f);
        } else {
            g.setColour(currentTheme.accent);
            g.fillRect(midX - trackThickness * 0.5f, sliderPos, trackThickness, bounds.getBottom() - sliderPos);
        }

        float thumbW = std::min(bounds.getWidth() - 4.0f, 18.0f);
        float thumbH = 8.0f;
        auto thumbRect = juce::Rectangle<float>(midX - thumbW * 0.5f, sliderPos - thumbH * 0.5f, thumbW, thumbH);

        g.setColour(currentTheme.accent);
        g.fillRect(thumbRect);
        g.setColour(isHovered ? currentTheme.textTitle : currentTheme.cardBorder);
        g.drawRect(thumbRect, 1.0f);
    }
}

void ModernLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    if (!button.isEnabled())
        g.setOpacity(0.45f);

    auto baseCol = button.getToggleState() ? currentTheme.accentDark : backgroundColour;
    if (shouldDrawButtonAsHighlighted) baseCol = baseCol.brighter(0.15f);
    if (shouldDrawButtonAsDown) baseCol = baseCol.darker(0.20f);

    g.setColour(baseCol);
    g.fillRect(bounds);

    g.setColour(button.getToggleState() ? currentTheme.accent
                                        : (shouldDrawButtonAsHighlighted ? currentTheme.cardBorder.brighter(0.25f) : currentTheme.buttonBorder));
    g.drawRect(bounds, 1.0f);

    if (button.getToggleState() || shouldDrawButtonAsDown) {
        g.setColour(currentTheme.accent);
        g.fillRect(bounds.getX(), bounds.getBottom() - 2.0f, bounds.getWidth(), 2.0f);
    }
}

void ModernLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool /*shouldDrawButtonAsDown*/) {
    auto bounds = button.getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;

    if (!button.isEnabled())
        g.setOpacity(0.45f);

    // Property "labelFirst": caption on the left, LED at the right edge
    const bool labelFirst = (bool)button.getProperties().getWithDefault("labelFirst", false);
    float ledSize = 13.0f;
    auto ledRect = juce::Rectangle<float>(labelFirst ? bounds.getRight() - 3.0f - ledSize : bounds.getX() + 3.0f,
                                          bounds.getCentreY() - ledSize * 0.5f, ledSize, ledSize);

    g.setColour(currentTheme.knobBodyTop);
    g.fillRect(ledRect);
    g.setColour(shouldDrawButtonAsHighlighted ? currentTheme.accent : currentTheme.cardBorder);
    g.drawRect(ledRect, 1.0f);

    if (button.getToggleState()) {
        auto innerLed = ledRect.reduced(2.5f);
        g.setColour(currentTheme.accent);
        g.fillRect(innerLed);
        g.setColour(juce::Colours::white);
        g.fillRect(innerLed.reduced(2.0f));
    } else {
        auto innerLed = ledRect.reduced(3.5f);
        g.setColour(currentTheme.knobTrack);
        g.fillRect(innerLed);
    }

    g.setFont(getCustomFont(9.0f, juce::Font::bold));
    g.setColour(button.getToggleState() ? currentTheme.textTitle : currentTheme.textMuted);
    if (labelFirst)
        g.drawText(button.getButtonText().toUpperCase(), 0, 0, (int)ledRect.getX() - 6, (int)bounds.getHeight(),
                   juce::Justification::centredRight, false);
    else
        g.drawText(button.getButtonText().toUpperCase(),
                   (int)ledRect.getRight() + 8, 0,
                   (int)(bounds.getWidth() - ledRect.getRight() - 10), (int)bounds.getHeight(),
                   juce::Justification::centredLeft, false);
}

void ModernLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                     int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                     juce::ComboBox& box) {
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);

    if (!box.isEnabled())
        g.setOpacity(0.45f);

    g.setColour(currentTheme.cardBg);
    g.fillRect(bounds);

    bool isHovered = box.isMouseOver(true);
    g.setColour(isHovered ? currentTheme.cardBorder.brighter(0.25f) : currentTheme.cardBorder);
    g.drawRect(bounds, 1.0f);

    juce::Path arrow;
    float arrowX = width - 16.0f;
    float arrowY = height * 0.5f - 2.0f;
    arrow.startNewSubPath(arrowX, arrowY);
    arrow.lineTo(arrowX + 4.0f, arrowY + 5.0f);
    arrow.lineTo(arrowX + 8.0f, arrowY);
    g.setColour(isHovered ? currentTheme.accent.brighter(0.2f) : currentTheme.accent);
    g.strokePath(arrow, juce::PathStrokeType(1.6f));
}

void ModernLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    g.setColour(currentTheme.cardBg);
    g.fillRect(bounds);
    g.setColour(currentTheme.accent);
    g.drawRect(bounds, 1.0f);
}

void ModernLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive, bool isHighlighted,
                                          bool isTicked, bool /*hasSubMenu*/, const juce::String& text,
                                          const juce::String& /*shortcutKeyText*/,
                                          const juce::Drawable* /*icon*/, const juce::Colour* textColour) {
    if (isSeparator) {
        g.setColour(currentTheme.cardBorder);
        g.drawHorizontalLine(area.getCentreY(), (float)area.getX() + 4.0f, (float)area.getRight() - 4.0f);
        return;
    }

    if (isHighlighted && isActive) {
        g.setColour(currentTheme.accentDark);
        g.fillRect(area);
        g.setColour(currentTheme.accent);
        g.drawRect(area.toFloat(), 1.0f);
    }

    g.setFont(getCustomFont(11.0f, juce::Font::plain));
    g.setColour(textColour != nullptr ? *textColour : (isActive ? currentTheme.textTitle : currentTheme.textMuted));

    int textX = area.getX() + (isTicked ? 22 : 12);
    g.drawText(text, textX, area.getY(), area.getWidth() - textX - 6, area.getHeight(), juce::Justification::centredLeft, false);

    if (isTicked) {
        g.setColour(currentTheme.accent);
        g.fillRect(area.getX() + 6, area.getCentreY() - 3, 6, 6);
    }
}

juce::Font ModernLookAndFeel::getTooltipFont() {
    return getCustomFont(24.0f, juce::Font::bold);
}

juce::Rectangle<int> ModernLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) {
    auto font = getTooltipFont();
    int textW = (int)std::ceil(font.getStringWidth(tipText));
    int w = std::max(260, textW + 48);
    int h = 60;

    int x = (screenPos.x > parentArea.getCentreX()) ? (screenPos.x - w - 16) : (screenPos.x + 20);
    int y = (screenPos.y > parentArea.getCentreY()) ? (screenPos.y - h - 16) : (screenPos.y + 20);

    return juce::Rectangle<int>(x, y, w, h).constrainedWithin(parentArea);
}

void ModernLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);

    g.setColour(juce::Colour(0xff121316));
    g.fillRect(bounds);

    g.setColour(currentTheme.accent);
    g.drawRect(bounds, 2.0f);

    g.fillRect(bounds.getX(), bounds.getY(), 6.0f, 6.0f);
    g.fillRect(bounds.getRight() - 6.0f, bounds.getBottom() - 6.0f, 6.0f, 6.0f);

    g.setColour(juce::Colours::white);
    g.setFont(getTooltipFont());
    g.drawText(text, bounds.reduced(12.0f, 3.0f).withTrimmedBottom(18.0f), juce::Justification::centred, false);

    g.setFont(getCustomFont(11.5f, juce::Font::bold));
    g.setColour(currentTheme.accent.withAlpha(0.85f));
    g.drawText("HOVER 1S TO COPY TO CLIPBOARD", bounds.reduced(8.0f, 4.0f), juce::Justification::centredBottom, false);
}

void ModernLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor&) {
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(currentTheme.cardBg);
    g.fillRect(bounds);
}

void ModernLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor&) {
    // Same outline while typing: no focus highlight.
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    g.setColour(currentTheme.cardBorder);
    g.drawRect(bounds, 1.0f);
}

namespace {
// A knob's value box: empty when clicked, so a value can be typed at once;
// confirming or leaving it empty keeps the old value.
class ValueBoxLabel final : public juce::Label {
public:
    explicit ValueBoxLabel(bool barSlider) : juce::Label({}, {}), wheelViaListener(barSlider) {}

    // The mouse wheel goes to the knob: a bar slider hears the box as its
    // mouse listener, any other slider gets the wheel passed up from here
    // (and a knob that ignores it passes it on to a scrolling page).
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (!wheelViaListener) juce::Label::mouseWheelMove(e, wheel);
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override {
        return createIgnoredAccessibilityHandler(*this);
    }

protected:
    void editorShown(juce::TextEditor* editor) override {
        // A listener may hide the editor or delete the box.
        juce::Component::BailOutChecker checker(this);
        juce::Label::editorShown(editor);
        if (checker.shouldBailOut()) return;
        if (auto* current = getCurrentTextEditor()) current->clear();
    }

    // Closed while still empty (the slider's mouse wheel commits the box):
    // the old text goes back in, so the value stays.
    void editorAboutToBeHidden(juce::TextEditor* editor) override {
        if (editor != nullptr && editor->getText().trim().isEmpty()) editor->setText(getText(), false);
        juce::Label::editorAboutToBeHidden(editor);
    }

public:
    // Also reached on focus loss, after JUCE's own focus check.
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override {
        if (editor.getText().trim().isEmpty()) hideEditor(true);
        else juce::Label::textEditorReturnKeyPressed(editor);
    }

private:
    const bool wheelViaListener;
};
}

juce::Label* ModernLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    const bool bar = slider.getSliderStyle() == juce::Slider::LinearBar || slider.getSliderStyle() == juce::Slider::LinearBarVertical;
    auto* l = new ValueBoxLabel(bar);
    l->setJustificationType(juce::Justification::centred);
    l->setKeyboardType(juce::TextInputTarget::decimalKeyboard);
    l->setColour(juce::Label::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
    l->setColour(juce::Label::backgroundColourId, bar ? juce::Colours::transparentBlack
                                                      : slider.findColour(juce::Slider::textBoxBackgroundColourId));
    l->setColour(juce::Label::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
    l->setColour(juce::TextEditor::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
    l->setColour(juce::TextEditor::backgroundColourId,
                 slider.findColour(juce::Slider::textBoxBackgroundColourId).withAlpha(bar ? 0.7f : 1.0f));
    l->setColour(juce::TextEditor::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
    l->setColour(juce::TextEditor::highlightColourId, slider.findColour(juce::Slider::textBoxHighlightColourId));
    return l;
}
