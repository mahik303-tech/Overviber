#include "OvercyclerKeypad.h"

OvercyclerKeypad::KeyButton::KeyButton(char keyChar, const juce::String& subLabelText)
    : juce::Button(juce::String(keyChar)), key(keyChar), subLabel(subLabelText) {}

void OvercyclerKeypad::KeyButton::setKeyInfo(char newKey, const juce::String& newLabel) {
    key = newKey;
    subLabel = newLabel;
    repaint();
}

void OvercyclerKeypad::KeyButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    juce::Colour btnCol = shouldDrawButtonAsDown ? juce::Colour(0xff18191c) :
                         shouldDrawButtonAsHighlighted ? juce::Colour(0xff2d2f34) :
                         juce::Colour(0xff222327);

    // Drop shadow
    g.setColour(juce::Colour(0x60000000));
    g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), 4.0f);

    // Button body
    g.setColour(btnCol);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Active or idle border
    if (isActive) {
        g.setColour(juce::Colour(0xff00d5ee));
        g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    } else {
        g.setColour(juce::Colour(0x28ffffff));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    // Top subtle specular highlight
    g.setColour(juce::Colour(0x25ffffff));
    g.drawHorizontalLine((int)bounds.getY() + 1, bounds.getX() + 3.0f, bounds.getRight() - 3.0f);

    float totalH = bounds.getHeight();
    float numH = totalH * 0.54f;
    float subH = totalH * 0.40f;

    // Draw main key character
    g.setColour(isActive ? juce::Colour(0xff00e5ff) : juce::Colours::white);
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 14.0f, juce::Font::bold));
    g.drawText(juce::String(key),
               (int)bounds.getX(), (int)(bounds.getY() + 2.0f),
               (int)bounds.getWidth(), (int)numH,
               juce::Justification::centred, false);

    // Draw sublabel
    juce::Colour subCol = isActive ? juce::Colour(0xff00e5ff) :
                          (key >= 'A' && key <= 'D') ? juce::Colour(0xffffb74d) :
                          (key == '*' || key == '#') ? juce::Colour(0xffef5350) :
                          juce::Colour(0xff00d5ee);

    g.setColour(subCol);
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::bold));
    g.drawText(subLabel,
               (int)bounds.getX(), (int)(bounds.getY() + numH - 2.0f),
               (int)bounds.getWidth(), (int)subH,
               juce::Justification::centred, false);
}

OvercyclerKeypad::OvercyclerKeypad() {
    layoutToggleBtn.setButtonText("LAYOUT: NUMPAD (7-8-9)");
    layoutToggleBtn.setClickingTogglesState(false);
    layoutToggleBtn.onClick = [this]() {
        setLayoutMode(currentLayout == LayoutPcNumpad ? LayoutHardware : LayoutPcNumpad);
    };
    addAndMakeVisible(layoutToggleBtn);

    for (int i = 0; i < 16; ++i) {
        buttons[i] = std::make_unique<KeyButton>(' ', "");
        buttons[i]->onClick = [this, i]() {
            char k = buttons[i]->getKey();
            if (onKeyPress) onKeyPress(k);
        };
        addAndMakeVisible(buttons[i].get());
    }

    updateButtonLabels();
}

void OvercyclerKeypad::setLayoutMode(LayoutMode mode) {
    currentLayout = mode;
    layoutToggleBtn.setButtonText(currentLayout == LayoutPcNumpad ? "LAYOUT: NUMPAD (7-8-9)" : "LAYOUT: HW (1-2-3)");
    updateButtonLabels();
}

void OvercyclerKeypad::setActiveKey(char keyChar) {
    activeKey = keyChar;
    for (int i = 0; i < 16; ++i) {
        if (buttons[i] != nullptr) {
            buttons[i]->setActive(buttons[i]->getKey() == activeKey);
        }
    }
}

void OvercyclerKeypad::setActionSublabels(const std::string& a, const std::string& b, const std::string& c, const std::string& d) {
    subLabelA = a;
    subLabelB = b;
    subLabelC = c;
    subLabelD = d;
    updateButtonLabels();
}

void OvercyclerKeypad::updateButtonLabels() {
    // 4 rows of 4 keys:
    // PC Numpad order:
    // Row 0: 7, 8, 9, A
    // Row 1: 4, 5, 6, B
    // Row 2: 1, 2, 3, C
    // Row 3: *, 0, #, D
    //
    // Hardware Matrix order:
    // Row 0: 1, 2, 3, A
    // Row 1: 4, 5, 6, B
    // Row 2: 7, 8, 9, C
    // Row 3: *, 0, #, D

    struct KeyTemplate {
        char key;
        const char* defaultLabel;
    };

    static const KeyTemplate pcKeys[16] = {
        { '7', "ARP" },   { '8', "SEQ" },  { '9', "MISC" }, { 'A', "" },
        { '4', "AMP" },   { '5', "LFO1" }, { '6', "LFO2" }, { 'B', "" },
        { '1', "OSC" },   { '2', "WMOD" }, { '3', "FIL" },  { 'C', "" },
        { '*', "SET" },   { '0', "PRST" }, { '#', "TRSP" }, { 'D', "" }
    };

    static const KeyTemplate hwKeys[16] = {
        { '1', "OSC" },   { '2', "WMOD" }, { '3', "FIL" },  { 'A', "" },
        { '4', "AMP" },   { '5', "LFO1" }, { '6', "LFO2" }, { 'B', "" },
        { '7', "ARP" },   { '8', "SEQ" },  { '9', "MISC" }, { 'C', "" },
        { '*', "SET" },   { '0', "PRST" }, { '#', "TRSP" }, { 'D', "" }
    };

    const auto* tmpl = (currentLayout == LayoutPcNumpad) ? pcKeys : hwKeys;

    for (int i = 0; i < 16; ++i) {
        if (buttons[i] != nullptr) {
            char k = tmpl[i].key;
            std::string label = tmpl[i].defaultLabel;
            if (k == 'A') label = subLabelA;
            else if (k == 'B') label = subLabelB;
            else if (k == 'C') label = subLabelC;
            else if (k == 'D') label = subLabelD;

            buttons[i]->setKeyInfo(k, label);
            buttons[i]->setActive(k == activeKey);
        }
    }
}

void OvercyclerKeypad::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Dark inset bezel plate
    g.setColour(juce::Colour(0xff141517));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour(0xff0d0e10));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.5f);

    // 4 Corner Allen screws
    g.setColour(juce::Colour(0xffa0a0a5));
    float sRadius = 2.5f;
    g.fillEllipse(bounds.getX() + 4.0f, bounds.getY() + 4.0f, sRadius * 2.0f, sRadius * 2.0f);
    g.fillEllipse(bounds.getRight() - 9.0f, bounds.getY() + 4.0f, sRadius * 2.0f, sRadius * 2.0f);
    g.fillEllipse(bounds.getX() + 4.0f, bounds.getBottom() - 9.0f, sRadius * 2.0f, sRadius * 2.0f);
    g.fillEllipse(bounds.getRight() - 9.0f, bounds.getBottom() - 9.0f, sRadius * 2.0f, sRadius * 2.0f);
}

void OvercyclerKeypad::resized() {
    auto bounds = getLocalBounds().reduced(6);
    int toggleH = 20;
    layoutToggleBtn.setBounds(bounds.getX() + 8, bounds.getY() + 4, bounds.getWidth() - 16, toggleH);

    int keyAreaY = bounds.getY() + toggleH + 6;
    int keyAreaH = bounds.getBottom() - keyAreaY;

    int cols = 4;
    int rows = 4;
    float gap = 4.0f;
    float btnW = (bounds.getWidth() - gap * (cols - 1)) / cols;
    float btnH = (keyAreaH - gap * (rows - 1)) / rows;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int idx = r * cols + c;
            int bx = (int)(bounds.getX() + c * (btnW + gap));
            int by = (int)(keyAreaY + r * (btnH + gap));
            if (buttons[idx] != nullptr) {
                buttons[idx]->setBounds(bx, by, (int)btnW, (int)btnH);
            }
        }
    }
}
