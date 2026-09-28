#include "LcdDisplay.h"
#include <iomanip>
#include <sstream>

LcdDisplay::LcdDisplay() {
    startTimerHz(25); // 40ms refresh rate for LCD animations and timeouts
}

LcdDisplay::~LcdDisplay() {
    stopTimer();
}

void LcdDisplay::setPage(ClassicUI::PageId page) {
    currentPage = page;
    if (currentPage == ClassicUI::PageId::Help) {
        currentMode = DisplayMode::Help;
    } else if (currentMode == DisplayMode::Help) {
        currentMode = DisplayMode::Normal;
    }
    repaint();
}

void LcdDisplay::setPotParam(int index, const std::string& name, const std::string& valueStr, float rawNormalized) {
    if (index >= 0 && index < 10) {
        potData[index].name = name;
        potData[index].value = valueStr;
        potData[index].normalized = juce::jlimit(0.0f, 1.0f, rawNormalized);
        repaint();
    }
}

void LcdDisplay::setButtonParam(int index, const std::string& name, const std::string& valueStr) {
    if (index >= 0 && index < 4) {
        buttonData[index].name = name;
        buttonData[index].value = valueStr;
        repaint();
    }
}

void LcdDisplay::setPresetInfo(const std::string& name, int number, bool isModified) {
    if (presetName != name || presetNumber != number || presetModified != isModified) {
        presetName = name;
        presetNumber = number;
        presetModified = isModified;
        repaint();
    }
}

void LcdDisplay::setVoiceActivity(int voiceIdx, bool active, float level) {
    if (voiceIdx >= 0 && voiceIdx < SYNTH_VOICE_COUNT) {
        voiceActive[voiceIdx] = active;
        voiceLevel[voiceIdx] = level;
    }
}

void LcdDisplay::showPotEdit(int knobIndex, const std::string& longName, const std::string& valueStr,
                             float currentVal, float minVal, float maxVal, bool showLeftArrow, bool showRightArrow) {
    currentMode = DisplayMode::PotEdit;
    potEditState.knobIndex = knobIndex;
    potEditState.longName = longName;
    potEditState.valueStr = valueStr;
    potEditState.currentVal = currentVal;
    potEditState.minVal = minVal;
    potEditState.maxVal = maxVal;
    potEditState.leftArrow = showLeftArrow;
    potEditState.rightArrow = showRightArrow;
    overlayHoldTicks = 35; // ~1.4 seconds at 25Hz
    repaint();
}

void LcdDisplay::showWaveformPreview(abx_t osc, const std::string& bankName, const std::string& waveName,
                                     const uint16_t* waveSamples, int sampleCount) {
    currentMode = DisplayMode::WavePreview;
    wavePreviewState.osc = osc;
    wavePreviewState.bankName = bankName;
    wavePreviewState.waveName = waveName;
    if (waveSamples != nullptr && sampleCount > 0) {
        wavePreviewState.samples.assign(waveSamples, waveSamples + sampleCount);
    } else {
        wavePreviewState.samples.clear();
    }
    overlayHoldTicks = 45; // ~1.8 seconds
    repaint();
}

void LcdDisplay::showButtonEdit(int buttonIndex, const std::string& longName, const std::string& activeValue,
                                const std::vector<std::string>& allOptions, int selectedOptionIndex) {
    currentMode = DisplayMode::ButtonEdit;
    buttonEditState.buttonIndex = buttonIndex;
    buttonEditState.longName = longName;
    buttonEditState.activeValue = activeValue;
    buttonEditState.options = allOptions;
    buttonEditState.selectedIndex = selectedOptionIndex;
    overlayHoldTicks = 35; // ~1.4 seconds
    repaint();
}

void LcdDisplay::showNumericInput(const std::string& prompt, const std::string& enteredDigits) {
    currentMode = DisplayMode::NumericInput;
    numericInputState.prompt = prompt;
    numericInputState.digits = enteredDigits;
    overlayHoldTicks = 200; // longer hold for manual input
    repaint();
}

void LcdDisplay::showHelpScreen() {
    currentMode = DisplayMode::Help;
    repaint();
}

void LcdDisplay::resetToNormalScreen() {
    currentMode = (currentPage == ClassicUI::PageId::Help) ? DisplayMode::Help : DisplayMode::Normal;
    overlayHoldTicks = 0;
    repaint();
}

void LcdDisplay::setContrast(float c) {
    contrast = juce::jlimit(0.2f, 1.0f, c);
    repaint();
}

void LcdDisplay::timerCallback() {
    if (overlayHoldTicks > 0) {
        overlayHoldTicks--;
        if (overlayHoldTicks == 0 && currentMode != DisplayMode::Help) {
            currentMode = DisplayMode::Normal;
            repaint();
        }
    }
    // Repaint periodically to animate voice activity meters
    repaint();
}

void LcdDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Outer Chassis Bezel
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(bounds, 5.0f);

    // Bezel Border
    g.setColour(juce::Colour(0xff0c0d0f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 2.0f);

    // LCD Display Window (authentic green/amber character LCD)
    auto screenArea = bounds.reduced(8.0f);

    // Base background modulated by contrast
    float r = 0.545f * contrast;
    float gr = 0.631f * contrast;
    float b = 0.349f * contrast;
    juce::Colour lcdBg = juce::Colour::fromFloatRGBA(r, gr, b, 1.0f);
    juce::Colour lcdChar(0xff131a0b); // dark monochrome liquid crystal ink

    g.setColour(lcdBg);
    g.fillRect(screenArea);

    // Subtle LCD pixel grid / scanlines
    g.setColour(juce::Colour(0x0a000000));
    for (float y = screenArea.getY(); y < screenArea.getBottom(); y += 2.0f) {
        g.drawHorizontalLine((int)y, screenArea.getX(), screenArea.getRight());
    }

    // Inner Bezel Shadow
    g.setColour(juce::Colour(0x35000000));
    g.drawRect(screenArea, 1.5f);

    // Render active display mode
    switch (currentMode) {
    case DisplayMode::PotEdit:
        renderPotEditScreen(g, screenArea);
        break;
    case DisplayMode::WavePreview:
        renderWavePreviewScreen(g, screenArea);
        break;
    case DisplayMode::ButtonEdit:
        renderButtonEditScreen(g, screenArea);
        break;
    case DisplayMode::Help:
        renderHelpScreen(g, screenArea);
        break;
    case DisplayMode::NumericInput:
        renderNumericInputScreen(g, screenArea);
        break;
    case DisplayMode::Normal:
    default:
        renderMatrixText(g, screenArea);
        break;
    }
}

void LcdDisplay::renderMatrixText(juce::Graphics& g, const juce::Rectangle<float>& screenArea) {
    juce::Colour lcdChar(0xff131a0b);
    g.setColour(lcdChar);

    float totalW = screenArea.getWidth();
    float totalH = screenArea.getHeight();
    float rowH = totalH / 4.0f;

    // 40 columns total:
    // Columns 0..27 (5 pot columns, ~5.2 chars per column)
    // Columns 28..29 (Voice monitors & modified flag)
    // Columns 30..39 (Buttons A, B, C, D)
    float colPotW = (totalW - 130.0f) / 5.0f;
    float divX = screenArea.getX() + colPotW * 5.0f + 6.0f;
    float btnX = divX + 24.0f;
    float btnW = screenArea.getRight() - btnX - 6.0f;

    juce::Font font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold);
    g.setFont(font);

    // Special layout for Page Presets
    if (currentPage == ClassicUI::PageId::Presets) {
        std::string line0 = std::string("PRST #") + (presetNumber < 10 ? "00" : presetNumber < 100 ? "0" : "") +
                            std::to_string(presetNumber) + ": " + presetName;
        g.drawText(line0, (int)screenArea.getX() + 6, (int)screenArea.getY() + 2, (int)(divX - screenArea.getX() - 10), (int)rowH, juce::Justification::left, true);

        // Line 1: Quick hint
        g.drawText("Use Buttons to Load/Save/Browse", (int)screenArea.getX() + 6, (int)(screenArea.getY() + rowH), (int)(divX - screenArea.getX() - 10), (int)rowH, juce::Justification::left, true);

        // Line 2: Pot 8 and 9 values
        std::string line2 = "Type: " + potData[8].value + "  Styl: " + potData[9].value;
        g.drawText(line2, (int)screenArea.getX() + 6, (int)(screenArea.getY() + rowH * 2.0f), (int)(divX - screenArea.getX() - 10), (int)rowH, juce::Justification::left, true);

        // Line 3: Pot 8 and 9 names & * hint
        std::string line3 = "Type: [P8]  Styl: [P9]   *:Set Digits";
        g.drawText(line3, (int)screenArea.getX() + 6, (int)(screenArea.getY() + rowH * 3.0f - 2), (int)(divX - screenArea.getX() - 10), (int)rowH, juce::Justification::left, true);
    } else {
        // Line 0: Top pots names (Pots 0..4)
        // Line 1: Top pots values (Pots 0..4)
        // Line 2: Bottom pots values (Pots 5..9)
        // Line 3: Bottom pots names (Pots 5..9)
        for (int i = 0; i < 5; ++i) {
            float px = screenArea.getX() + i * colPotW + 6.0f;
            int colW = (int)colPotW - 4;

            // Row 0: Pot name
            g.drawText(potData[i].name, (int)px, (int)(screenArea.getY() + 2), colW, (int)rowH, juce::Justification::left, true);

            // Row 1: Pot value
            g.drawText(potData[i].value, (int)px, (int)(screenArea.getY() + rowH), colW, (int)rowH, juce::Justification::left, true);

            // Row 2: Bottom pot value
            g.drawText(potData[i + 5].value, (int)px, (int)(screenArea.getY() + rowH * 2.0f), colW, (int)rowH, juce::Justification::left, true);

            // Row 3: Bottom pot name
            g.drawText(potData[i + 5].name, (int)px, (int)(screenArea.getY() + rowH * 3.0f - 2), colW, (int)rowH, juce::Justification::left, true);
        }
    }

    // Divider Line
    g.setColour(juce::Colour(0x35131a0b));
    g.drawVerticalLine((int)divX - 2, screenArea.getY() + 4.0f, screenArea.getBottom() - 4.0f);

    // Delimiter column (Preset modified flag & Voice Activity)
    g.setColour(lcdChar);
    if (presetModified) {
        g.drawText("*", (int)divX + 2, (int)screenArea.getY() + 2, 16, (int)rowH, juce::Justification::centred, false);
    }

    // Voice activity meters
    renderVoiceMonitors(g, divX + 2.0f, screenArea.getY() + rowH * 0.8f, 16.0f, rowH * 3.0f);

    // Buttons Column (A, B, C, D)
    const char* btnTags[4] = { "A:", "B:", "C:", "D:" };
    for (int b = 0; b < 4; ++b) {
        float by = screenArea.getY() + b * rowH;
        std::string txt = std::string(btnTags[b]) + " " + buttonData[b].name;
        if (!buttonData[b].value.empty()) {
            txt += " " + buttonData[b].value;
        }
        g.drawText(txt, (int)btnX, (int)by, (int)btnW, (int)rowH, juce::Justification::centredLeft, true);
    }
}

void LcdDisplay::renderVoiceMonitors(juce::Graphics& g, float x, float y, float w, float h) {
    float pairH = h / 3.0f;
    for (int pair = 0; pair < 3; ++pair) {
        int vLeft = pair * 2;
        int vRight = pair * 2 + 1;

        float py = y + pair * pairH;

        // Voice Left dot/bar
        if (voiceActive[vLeft]) {
            g.fillRect(x + 2.0f, py + 2.0f, 4.0f, pairH - 4.0f);
        } else {
            g.drawRect(x + 2.0f, py + 2.0f, 4.0f, pairH - 4.0f, 1.0f);
        }

        // Voice Right dot/bar
        if (voiceActive[vRight]) {
            g.fillRect(x + 9.0f, py + 2.0f, 4.0f, pairH - 4.0f);
        } else {
            g.drawRect(x + 9.0f, py + 2.0f, 4.0f, pairH - 4.0f, 1.0f);
        }
    }
}

void LcdDisplay::renderPotEditScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea) {
    juce::Colour lcdChar(0xff131a0b);
    g.setColour(lcdChar);

    float rowH = screenArea.getHeight() / 4.0f;

    // Line 0: Full Parameter Long Name (ALL CAPS)
    juce::Font titleFont(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold);
    g.setFont(titleFont);
    std::string title = "[ " + potEditState.longName + " ]";
    g.drawText(title, screenArea.getX(), screenArea.getY() + 2.0f, screenArea.getWidth(), rowH, juce::Justification::centred, true);

    // Line 1: Value with direction arrows
    juce::Font valFont(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold);
    g.setFont(valFont);
    std::string valStr = (potEditState.leftArrow ? "< " : "  ") + potEditState.valueStr + (potEditState.rightArrow ? " >" : "  ");
    g.drawText(valStr, screenArea.getX(), screenArea.getY() + rowH, screenArea.getWidth(), rowH, juce::Justification::centred, false);

    // Line 2: Horizontal Bar Graph
    float barW = screenArea.getWidth() - 80.0f;
    float barH = 12.0f;
    float barX = screenArea.getX() + 40.0f;
    float barY = screenArea.getY() + rowH * 2.0f + (rowH - barH) * 0.5f;

    g.drawRect(barX, barY, barW, barH, 1.0f);

    float norm = (potEditState.currentVal - potEditState.minVal) / std::max(1.0f, potEditState.maxVal - potEditState.minVal);
    norm = juce::jlimit(0.0f, 1.0f, norm);
    g.fillRect(barX + 2.0f, barY + 2.0f, (barW - 4.0f) * norm, barH - 4.0f);

    // Line 3: Min / Max Limits
    juce::Font limitFont(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain);
    g.setFont(limitFont);
    std::string limits = "MIN: " + std::to_string((int)potEditState.minVal) +
                         "                      MAX: " + std::to_string((int)potEditState.maxVal);
    g.drawText(limits, screenArea.getX() + 40.0f, screenArea.getY() + rowH * 3.0f - 2.0f, barW, rowH, juce::Justification::centred, false);
}

void LcdDisplay::renderWavePreviewScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea) {
    juce::Colour lcdChar(0xff131a0b);
    g.setColour(lcdChar);

    float rowH = screenArea.getHeight() / 4.0f;

    // Line 0: Header with Bank and Wave Name
    juce::Font headerFont(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold);
    g.setFont(headerFont);
    std::string oscStr = (wavePreviewState.osc == abxAMain) ? "OSC A" : "OSC B";
    std::string header = oscStr + " WAVEFORM: " + wavePreviewState.waveName + "  [BANK: " + wavePreviewState.bankName + "]";
    g.drawText(header, screenArea.getX() + 10.0f, screenArea.getY() + 2.0f, screenArea.getWidth() - 20.0f, rowH, juce::Justification::left, true);

    // Lines 1..3: Multi-line vector waveform
    float waveAreaY = screenArea.getY() + rowH + 4.0f;
    float waveAreaH = rowH * 3.0f - 10.0f;
    float waveAreaX = screenArea.getX() + 16.0f;
    float waveAreaW = screenArea.getWidth() - 32.0f;

    // Outer frame for the LCD oscilloscope
    g.drawRect(waveAreaX, waveAreaY, waveAreaW, waveAreaH, 1.0f);

    // Center Zero line
    float midY = waveAreaY + waveAreaH * 0.5f;
    g.setColour(juce::Colour(0x35131a0b));
    g.drawHorizontalLine((int)midY, waveAreaX, waveAreaX + waveAreaW);
    g.setColour(lcdChar);

    if (!wavePreviewState.samples.empty()) {
        juce::Path p;
        int numSmp = (int)wavePreviewState.samples.size();
        for (int x = 0; x < (int)waveAreaW; ++x) {
            int smpIdx = (x * numSmp) / (int)waveAreaW;
            smpIdx = juce::jlimit(0, numSmp - 1, smpIdx);
            float s = (float)wavePreviewState.samples[smpIdx] - 32768.0f;
            float normY = midY - (s / 32768.0f) * (waveAreaH * 0.44f);

            if (x == 0) p.startNewSubPath(waveAreaX + x, normY);
            else p.lineTo(waveAreaX + x, normY);
        }
        g.strokePath(p, juce::PathStrokeType(1.5f));
    } else {
        // Fallback flat line with loading text
        g.drawText("NO WAVE SAMPLES LOADED", waveAreaX, waveAreaY, waveAreaW, waveAreaH, juce::Justification::centred, false);
    }
}

void LcdDisplay::renderButtonEditScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea) {
    juce::Colour lcdChar(0xff131a0b);
    g.setColour(lcdChar);

    float rowH = screenArea.getHeight() / 4.0f;

    // Line 0: Full Parameter Name
    juce::Font titleFont(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold);
    g.setFont(titleFont);
    std::string title = "[ " + buttonEditState.longName + " ]";
    g.drawText(title, screenArea.getX(), screenArea.getY() + 2.0f, screenArea.getWidth(), rowH, juce::Justification::centred, true);

    // Line 1: Active Value
    juce::Font valFont(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain);
    g.setFont(valFont);
    std::string valStr = "CURRENT: " + buttonEditState.activeValue;
    g.drawText(valStr, screenArea.getX(), screenArea.getY() + rowH, screenArea.getWidth(), rowH, juce::Justification::centred, false);

    // Lines 2 & 3: Options List with Selection Arrow
    std::string optsStr = "OPTIONS: ";
    for (size_t i = 0; i < buttonEditState.options.size(); ++i) {
        if ((int)i == buttonEditState.selectedIndex) {
            optsStr += ">" + buttonEditState.options[i] + "<  ";
        } else {
            optsStr += " " + buttonEditState.options[i] + "   ";
        }
    }
    g.drawText(optsStr, screenArea.getX() + 20.0f, screenArea.getY() + rowH * 2.0f, screenArea.getWidth() - 40.0f, rowH * 2.0f, juce::Justification::centred, true);
}

void LcdDisplay::renderHelpScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea) {
    juce::Colour lcdChar(0xff131a0b);
    g.setColour(lcdChar);

    float rowH = screenArea.getHeight() / 4.0f;
    juce::Font font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold);
    g.setFont(font);

    // 4 Lines of Original Startup/Help Page
    g.drawText("1:Oscillators   2:WaveMod    3:Filter   ", (int)screenArea.getX() + 6, (int)(screenArea.getY() + 2), (int)screenArea.getWidth(), (int)rowH, juce::Justification::left, false);
    g.drawText("4:Amplifier     5:LFO1       6:LFO2     ", (int)screenArea.getX() + 6, (int)(screenArea.getY() + rowH), (int)screenArea.getWidth(), (int)rowH, juce::Justification::left, false);
    g.drawText("7:Arpeggiator   8:Sequencer  9:Misc.    ", (int)screenArea.getX() + 6, (int)(screenArea.getY() + rowH * 2.0f), (int)screenArea.getWidth(), (int)rowH, juce::Justification::left, false);
    g.drawText("*:Set digits    0:Presets    #:Transpose", (int)screenArea.getX() + 6, (int)(screenArea.getY() + rowH * 3.0f - 2), (int)screenArea.getWidth(), (int)rowH, juce::Justification::left, false);
}

void LcdDisplay::renderNumericInputScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea) {
    juce::Colour lcdChar(0xff131a0b);
    g.setColour(lcdChar);

    float rowH = screenArea.getHeight() / 4.0f;
    juce::Font font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold);
    g.setFont(font);

    g.drawText("=== NUMERIC ENTRY (*:Set Digits) ===", juce::Rectangle<float>(screenArea.getX(), screenArea.getY() + 2.0f, screenArea.getWidth(), rowH), juce::Justification::centred, false);
    g.drawText("TARGET: " + numericInputState.prompt, juce::Rectangle<float>(screenArea.getX() + 20.0f, screenArea.getY() + rowH, screenArea.getWidth() - 40.0f, rowH), juce::Justification::centred, false);

    std::string valStr = "ENTER VALUE: [ " + numericInputState.digits + " ]";
    g.drawText(valStr, juce::Rectangle<float>(screenArea.getX(), screenArea.getY() + rowH * 2.0f, screenArea.getWidth(), rowH), juce::Justification::centred, false);

    juce::Font hintFont(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain);
    g.setFont(hintFont);
    g.drawText("Press 3 digits (0-9) to confirm, or press * to cancel", juce::Rectangle<float>(screenArea.getX(), screenArea.getY() + rowH * 3.0f - 2.0f, screenArea.getWidth(), rowH), juce::Justification::centred, false);
}

void LcdDisplay::resized() {
}
