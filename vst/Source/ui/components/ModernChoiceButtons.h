#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>

// ==============================================================================
// A choice among a few options as a row or grid of buttons (one radio group):
// the Modern skin's replacement for combo boxes. Every option stays visible
// and one click selects it. The buttons are children with the component IDs
// "<prefix>[<index>]"; the group itself is "<prefix>Choices".
// ==============================================================================
class ModernChoiceButtons : public juce::Component {
public:
    // Draws an option's symbol instead of its text (LFO shapes).
    using GlyphPainter = std::function<void(juce::Graphics&, juce::Rectangle<float> area, int index,
                                            juce::Colour colour)>;

    // columns 0: all options in one row.
    explicit ModernChoiceButtons(const juce::StringArray& labels, int columns = 0);
    ~ModernChoiceButtons() override;

    void setIdPrefix(const juce::String& prefix);
    void setGap(int newGap) { gap = newGap; resized(); }
    void setTooltips(const juce::StringArray& tooltips);
    void setGlyphPainter(GlyphPainter painter);

    // Selects an option without notifying (-1: none); ignored while the user
    // presses one of the buttons.
    void setSelected(int index);
    int getSelected() const noexcept { return selected; }
    int getNumOptions() const noexcept { return static_cast<int>(buttons.size()); }
    juce::Button* getButton(int index);

    // Called when the user picks an option.
    std::function<void(int index)> onSelect;

    void resized() override;

private:
    class Option;
    std::vector<std::unique_ptr<Option>> buttons;
    GlyphPainter glyphPainter;
    int columns = 0, gap = 4, selected = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModernChoiceButtons)
};
