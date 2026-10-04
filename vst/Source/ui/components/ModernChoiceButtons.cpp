#include "ModernChoiceButtons.h"
#include "ModernLookAndFeel.h"

// One option: a toggling text button that can draw a symbol instead.
class ModernChoiceButtons::Option : public juce::TextButton {
public:
    Option(ModernChoiceButtons& group, const juce::String& label, int optionIndex)
        : juce::TextButton(label), owner(group), index(optionIndex) {}

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override {
        if (!owner.glyphPainter) {
            juce::TextButton::paintButton(g, highlighted, down);
            return;
        }
        auto& lnf = getLookAndFeel();
        lnf.drawButtonBackground(g, *this, findColour(getToggleState() ? buttonOnColourId : buttonColourId),
                                 highlighted, down);
        juce::Colour colour = getToggleState() ? juce::Colours::white : juce::Colours::grey;
        if (auto* modern = dynamic_cast<ModernLookAndFeel*>(&lnf))
            colour = getToggleState() ? modern->getTheme().textTitle : modern->getTheme().textMuted;
        owner.glyphPainter(g, getLocalBounds().toFloat().reduced(6.0f, 4.0f), index, colour);
    }

private:
    ModernChoiceButtons& owner;
    const int index;
};

ModernChoiceButtons::ModernChoiceButtons(const juce::StringArray& labels, int columnCount)
    : columns(columnCount) {
    for (int i = 0; i < labels.size(); ++i) {
        auto button = std::make_unique<Option>(*this, labels[i], i);
        button->setClickingTogglesState(true);
        button->setRadioGroupId(1);   // the group's buttons are siblings only of each other
        button->onClick = [this, i]() {
            if (!buttons[(size_t)i]->getToggleState()) return;   // the radio partner switching off
            selected = i;
            if (onSelect) onSelect(i);
        };
        addAndMakeVisible(*button);
        buttons.push_back(std::move(button));
    }
}

ModernChoiceButtons::~ModernChoiceButtons() = default;

void ModernChoiceButtons::setIdPrefix(const juce::String& prefix) {
    setComponentID(prefix + "Choices");
    for (size_t i = 0; i < buttons.size(); ++i) buttons[i]->setComponentID(prefix + "[" + juce::String((int)i) + "]");
}

void ModernChoiceButtons::setTooltips(const juce::StringArray& tooltips) {
    for (size_t i = 0; i < buttons.size() && (int)i < tooltips.size(); ++i) buttons[i]->setTooltip(tooltips[(int)i]);
}

void ModernChoiceButtons::setGlyphPainter(GlyphPainter painter) {
    glyphPainter = std::move(painter);
    repaint();
}

void ModernChoiceButtons::setSelected(int index) {
    for (auto& button : buttons)
        if (button->isMouseButtonDown()) return;
    selected = index;
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState((int)i == index, juce::dontSendNotification);
}

juce::Button* ModernChoiceButtons::getButton(int index) {
    return index >= 0 && index < (int)buttons.size() ? buttons[(size_t)index].get() : nullptr;
}

void ModernChoiceButtons::resized() {
    const int count = (int)buttons.size();
    if (count == 0) return;
    const int cols = columns > 0 ? std::min(columns, count) : count;
    const int rows = (count + cols - 1) / cols;
    const float cellW = (static_cast<float>(getWidth()) - static_cast<float>(gap * (cols - 1))) / static_cast<float>(cols);
    const float cellH = (static_cast<float>(getHeight()) - static_cast<float>(gap * (rows - 1))) / static_cast<float>(rows);
    for (int i = 0; i < count; ++i) {
        const int col = i % cols, row = i / cols;
        const int x0 = juce::roundToInt(static_cast<float>(col) * (cellW + static_cast<float>(gap)));
        const int x1 = juce::roundToInt(static_cast<float>(col) * (cellW + static_cast<float>(gap)) + cellW);
        const int y0 = juce::roundToInt(static_cast<float>(row) * (cellH + static_cast<float>(gap)));
        const int y1 = juce::roundToInt(static_cast<float>(row) * (cellH + static_cast<float>(gap)) + cellH);
        buttons[(size_t)i]->setBounds(x0, y0, x1 - x0, y1 - y0);
    }
}
