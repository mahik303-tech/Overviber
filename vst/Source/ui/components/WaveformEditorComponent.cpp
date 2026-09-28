#include "WaveformEditorComponent.h"
#include "ModernLookAndFeel.h"
#include <cmath>
#include <algorithm>

// ==============================================================================
// WaveformSaveModal Implementation
// ==============================================================================
WaveformSaveModal::WaveformSaveModal() {
    titleLabel.setText("SAVE CUSTOM WAVEFORM", juce::dontSendNotification);
    titleLabel.setFont(ModernFontManager::createFont("D-DIN", 13.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    descLabel.setText("Enter waveform name to save as .wav into disk/WAVEDATA/User/:", juce::dontSendNotification);
    descLabel.setFont(ModernFontManager::createFont("D-DIN", 11.0f, juce::Font::plain));
    descLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ea2aa));
    descLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(descLabel);

    nameEditor.setMultiLine(false);
    nameEditor.setReturnKeyStartsNewLine(false);
    nameEditor.setFont(ModernFontManager::createFont("D-DIN", 13.0f, juce::Font::bold));
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff121417));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3f434c));
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, accentCol);
    nameEditor.onReturnKey = [this]() {
        if (saveCallback && nameEditor.getText().trim().isNotEmpty()) {
            saveCallback(nameEditor.getText().trim());
            hide();
        }
    };
    nameEditor.onEscapeKey = [this]() { hide(); };
    addAndMakeVisible(nameEditor);

    saveBtn.setButtonText("SAVE");
    saveBtn.onClick = [this]() {
        if (saveCallback && nameEditor.getText().trim().isNotEmpty()) {
            saveCallback(nameEditor.getText().trim());
            hide();
        }
    };
    addAndMakeVisible(saveBtn);

    cancelBtn.setButtonText("CANCEL");
    cancelBtn.onClick = [this]() { hide(); };
    addAndMakeVisible(cancelBtn);

    setVisible(false);
}

void WaveformSaveModal::show(const juce::String& currentName, std::function<void(const juce::String& newName)> onSave) {
    saveCallback = onSave;
    nameEditor.setText(currentName);
    nameEditor.selectAll();
    setVisible(true);
    toFront(true);
    nameEditor.grabKeyboardFocus();
}

void WaveformSaveModal::hide() {
    setVisible(false);
}

void WaveformSaveModal::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xaa000000));

    int cardW = 410;
    int cardH = 175;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;
    juce::Rectangle<float> cardRect((float)cardX, (float)cardY, (float)cardW, (float)cardH);

    g.setColour(juce::Colour(0xff1e2126));
    g.fillRect(cardRect);

    g.setColour(accentCol);
    g.drawRect(cardRect, 1.0f);

    g.setColour(accentCol);
    g.fillRect(cardRect.getX() + 20.0f, cardRect.getY() + 38.0f, cardRect.getWidth() - 40.0f, 1.0f);
}

void WaveformSaveModal::resized() {
    int cardW = 410;
    int cardH = 175;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;

    titleLabel.setBounds(cardX + 20, cardY + 10, cardW - 40, 24);
    descLabel.setBounds(cardX + 20, cardY + 46, cardW - 40, 20);
    nameEditor.setBounds(cardX + 20, cardY + 74, cardW - 40, 32);

    int btnW = 90;
    int btnH = 26;
    int btnY = cardY + 124;
    saveBtn.setBounds(cardX + cardW - 20 - btnW, btnY, btnW, btnH);
    cancelBtn.setBounds(cardX + cardW - 20 - btnW * 2 - 10, btnY, btnW, btnH);
}

// ==============================================================================
// WaveformEditorComponent::SaveDisketteButton Implementation
// ==============================================================================
WaveformEditorComponent::SaveDisketteButton::SaveDisketteButton()
    : juce::Button("saveDiskette") {
    setTooltip("Save Custom Waveform As...");
}

void WaveformEditorComponent::SaveDisketteButton::paintButton(juce::Graphics& g,
                                                              bool shouldDrawButtonAsHighlighted,
                                                              bool shouldDrawButtonAsDown) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];

    juce::Colour bg = theme.buttonBg;
    if (shouldDrawButtonAsHighlighted) bg = theme.accentDark.brighter(0.15f);
    if (shouldDrawButtonAsDown) bg = theme.accentDark.darker(0.15f);

    g.setColour(bg);
    g.fillRect(bounds);

    g.setColour(shouldDrawButtonAsHighlighted ? theme.accent : theme.buttonBorder);
    g.drawRect(bounds, 1.0f);

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown) {
        g.setColour(theme.accent);
        g.fillRect(bounds.getX(), bounds.getBottom() - 2.0f, bounds.getWidth(), 2.0f);
    }

    juce::Colour iconCol = shouldDrawButtonAsHighlighted ? juce::Colours::white : theme.accent;
    g.setColour(iconCol);

    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float diskW = 13.0f;
    float diskH = 13.0f;
    float diskX = cx - diskW * 0.5f;
    float diskY = cy - diskH * 0.5f;

    juce::Path disk;
    disk.startNewSubPath(diskX, diskY);
    disk.lineTo(diskX + diskW - 3.0f, diskY);
    disk.lineTo(diskX + diskW, diskY + 3.0f);
    disk.lineTo(diskX + diskW, diskY + diskH);
    disk.lineTo(diskX, diskY + diskH);
    disk.closeSubPath();
    g.strokePath(disk, juce::PathStrokeType(1.2f));

    juce::Rectangle<float> shutter(diskX + 2.5f, diskY + 1.2f, 6.0f, 4.2f);
    g.fillRect(shutter);
    g.setColour(bg);
    g.fillRect(diskX + 4.0f, diskY + 2.0f, 1.2f, 2.4f);

    g.setColour(iconCol);
    juce::Rectangle<float> labelRect(diskX + 2.0f, diskY + 7.0f, diskW - 4.0f, 4.5f);
    g.drawRect(labelRect, 1.0f);
    g.drawHorizontalLine((int)(diskY + 9.0f), diskX + 3.5f, diskX + diskW - 3.5f);
}

// ==============================================================================
// WaveformEditorComponent Implementation
// ==============================================================================
WaveformEditorComponent::WaveformEditorComponent(SynthModel& eng, abx_t targetOsc)
    : model(eng), currentOsc(targetOsc) {
    auto setupBtn = [this](juce::TextButton& b, const juce::String& text, auto callback) {
        b.setButtonText(text);
        b.onClick = callback;
        addAndMakeVisible(b);
    };

    titleText = (currentOsc == abxAMain) ? "WAVEFORM A" : "WAVEFORM B";
    badgeText = (currentOsc == abxAMain) ? "CORE" : "SYNC / DETUNE";

    waveDisplayBtn.onClick = [this]() { showPresetMenu(); };
    addAndMakeVisible(waveDisplayBtn);
    refreshPresetDisplay();

    saveDisketteBtn.onClick = [this]() { openSaveModal(); };
    saveDisketteBtn.setVisible(false);
    addAndMakeVisible(saveDisketteBtn);

    setupBtn(prevFrameBtn, "<", [this]() { stepFrame(-1); });
    frameLabel.setText("1 / 1", juce::dontSendNotification);
    frameLabel.setFont(ModernFontManager::createFont("D-DIN", 10.0f, juce::Font::bold));
    frameLabel.setJustificationType(juce::Justification::centred);
    frameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ea2aa));
    addAndMakeVisible(frameLabel);
    setupBtn(nextFrameBtn, ">", [this]() { stepFrame(1); });

    setupBtn(invertBtn, "INVERT", [this]() { invertWave(); });

    setupBtn(smoothBtn, "SMOOTH", [this]() {
        int passes = smoothKnob ? (int)smoothKnob->getValue() : 5;
        smoothWave(passes);
    });

    smoothKnob = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    smoothKnob->setRange(1.0, 50.0, 1.0);
    smoothKnob->setValue(5.0);
    smoothKnob->setTextValueSuffix("x");
    smoothKnob->setTextBoxStyle(juce::Slider::TextBoxRight, false, 28, 18);
    smoothKnob->setTooltip("Smooth Multiplier (1x - 50x passes)");
    addAndMakeVisible(*smoothKnob);

    setupBtn(normBtn, "NORMALIZE", [this]() {
        float gain = normKnob ? ((float)normKnob->getValue() / 100.0f) : 1.0f;
        normalizeWave(gain);
    });

    normKnob = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    normKnob->setRange(25.0, 150.0, 1.0);
    normKnob->setValue(100.0);
    normKnob->setTextValueSuffix("%");
    normKnob->setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 18);
    normKnob->setTooltip("Normalize Target Level (25% - 150%)");
    addAndMakeVisible(*normKnob);

    addChildComponent(saveModal);
    setupSubComponentIDs((currentOsc == abxAMain) ? "waveformEditorA" : "waveformEditorB");
    updateFrameControls();
}

void WaveformEditorComponent::setupSubComponentIDs(const juce::String& prefix) {
    setComponentID(prefix);
    waveDisplayBtn.setComponentID(prefix + "_presetBtn");
    saveDisketteBtn.setComponentID(prefix + "_saveDisketteBtn");
    prevFrameBtn.setComponentID(prefix + "_prevFrameBtn");
    frameLabel.setComponentID(prefix + "_frameLabel");
    nextFrameBtn.setComponentID(prefix + "_nextFrameBtn");
    invertBtn.setComponentID(prefix + "_invertBtn");
    smoothBtn.setComponentID(prefix + "_smoothBtn");
    if (smoothKnob) smoothKnob->setComponentID(prefix + "_smoothKnob");
    normBtn.setComponentID(prefix + "_normBtn");
    if (normKnob) normKnob->setComponentID(prefix + "_normKnob");
}

void WaveformEditorComponent::setModified(bool modified) {
    if (isWaveModified != modified) {
        isWaveModified = modified;
        saveDisketteBtn.setVisible(modified);
        resized();
        repaint();
    }
}

void WaveformEditorComponent::setTargetOsc(abx_t osc) {
    currentOsc = osc;
    titleText = (currentOsc == abxAMain) ? "WAVEFORM A" : "WAVEFORM B";
    badgeText = (currentOsc == abxAMain) ? "CORE" : "SYNC / DETUNE";
    setModified(false);
    updateFrameControls();
    refreshPresetDisplay();
    repaint();
}

void WaveformEditorComponent::showPresetMenu() {
    juce::PopupMenu menu;
    menu.addItem(1, "Open Waveform Browser...", true, false);
    menu.addSeparator();

    juce::PopupMenu stdShapes;
    const char* shapeNames[] = {
        "Sine Wave", "Triangle Wave", "Sawtooth Wave", "Square Wave",
        "Pulse 25%", "Pulse 10%", "Half Sine", "Harmonics 1-8",
        "White Noise", "Parabolic"
    };
    for (int i = 0; i < 10; ++i) {
        stdShapes.addItem(10 + i, shapeNames[i]);
    }
    menu.addSubMenu("Standard Shapes", stdShapes);

    auto& wm = model.getWaveManager();
    const auto& banks = wm.getBankNames();
    int itemId = 100;
    std::map<int, std::pair<std::string, std::string>> itemMap;

    for (const auto& b : banks) {
        auto waves = wm.getWaveNames(b);
        if (waves.empty()) continue;
        juce::PopupMenu bankMenu;
        for (const auto& w : waves) {
            bankMenu.addItem(itemId, w);
            itemMap[itemId] = { b, w };
            itemId++;
            if (itemId > 800) break;
        }
        menu.addSubMenu(b, bankMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&waveDisplayBtn), [this, itemMap](int result) {
        if (result == 1) {
            if (onOpenWaveBrowser) onOpenWaveBrowser(currentOsc);
        } else if (result >= 10 && result < 20) {
            WaveManager::StandardShape shapes[] = {
                WaveManager::StandardShape::Sine,
                WaveManager::StandardShape::Triangle,
                WaveManager::StandardShape::Sawtooth,
                WaveManager::StandardShape::Square,
                WaveManager::StandardShape::Pulse25,
                WaveManager::StandardShape::Pulse10,
                WaveManager::StandardShape::HalfSine,
                WaveManager::StandardShape::Harmonics1to8,
                WaveManager::StandardShape::WhiteNoise,
                WaveManager::StandardShape::Parabolic
            };
            model.getWaveManager().generateStandardShape(currentOsc, shapes[result - 10]);
            model.getCurrentPreset().oscBank[currentOsc] = "Standard";
            model.getCurrentPreset().oscWave[currentOsc] = "Shape";
            setModified(false);
            updateFrameControls();
            refreshPresetDisplay();
            repaint();
            if (onWaveformChanged) onWaveformChanged();
        } else if (result >= 100) {
            auto it = itemMap.find(result);
            if (it != itemMap.end()) {
                model.getWaveManager().loadWave(currentOsc, it->second.first, it->second.second, 0);
                model.getCurrentPreset().oscBank[currentOsc] = it->second.first;
                model.getCurrentPreset().oscWave[currentOsc] = it->second.second;
                setModified(false);
                updateFrameControls();
                refreshPresetDisplay();
                repaint();
                if (onWaveformChanged) onWaveformChanged();
            }
        }
    });
}

void WaveformEditorComponent::refreshPresetDisplay() {
    auto& wm = model.getWaveManager();
    int curF = wm.getCurrentFrame(currentOsc);
    int totF = wm.getTotalFrames(currentOsc);
    juce::String bankName = model.getCurrentPreset().oscBank[currentOsc];
    juce::String waveName = model.getCurrentPreset().oscWave[currentOsc];
    if (waveName.isEmpty()) waveName = "Default";
    if (bankName.isEmpty()) bankName = "SHAPES";
    waveDisplayBtn.setPreset(curF, totF, waveName, bankName.toUpperCase());
}

void WaveformEditorComponent::stepFrame(int delta) {
    auto& wm = model.getWaveManager();
    int curF = wm.getCurrentFrame(currentOsc);
    int totF = wm.getTotalFrames(currentOsc);
    if (totF > 1) {
        int nextF = std::clamp(curF + delta, 0, totF - 1);
        if (nextF != curF) {
            wm.setCurrentFrame(currentOsc, nextF);
            setModified(false);
            updateFrameControls();
            refreshPresetDisplay();
            repaint();
            if (onWaveformChanged) onWaveformChanged();
        }
    }
}

void WaveformEditorComponent::updateFrameControls() {
    auto& wm = model.getWaveManager();
    int curF = wm.getCurrentFrame(currentOsc);
    int totF = wm.getTotalFrames(currentOsc);
    frameLabel.setText(juce::String(curF + 1) + " / " + juce::String(totF), juce::dontSendNotification);
    prevFrameBtn.setEnabled(curF > 0);
    nextFrameBtn.setEnabled(curF < totF - 1);
    bool hasMultiFrames = (totF > 1);
    prevFrameBtn.setVisible(hasMultiFrames);
    frameLabel.setVisible(hasMultiFrames);
    nextFrameBtn.setVisible(hasMultiFrames);
}

void WaveformEditorComponent::openSaveModal() {
    juce::String curName = (currentOsc == abxAMain)
        ? model.getCurrentPreset().oscWave[abxAMain]
        : model.getCurrentPreset().oscWave[abxBMain];
    if (curName.endsWithIgnoreCase(".wav")) curName = curName.dropLastCharacters(4);
    if (curName.isEmpty()) curName = "MyCustomWave";

    saveModal.show(curName, [this](const juce::String& newName) {
        if (model.getWaveManager().saveUserWave(currentOsc, newName.toStdString())) {
            setModified(false);
            refreshPresetDisplay();
            repaint();
            if (onWaveformChanged) onWaveformChanged();
        }
    });
}

void WaveformEditorComponent::openImportDialog() {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Import Wavetable / Waveform (.wav)...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav;*.WAV"
    );

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
            if (model.getWaveManager().loadWaveFromFile(currentOsc, file.getFullPathName().toStdString(), 0)) {
                setModified(false);
                updateFrameControls();
                repaint();
                if (onWaveformChanged) onWaveformChanged();
            }
        }
    });
}

void WaveformEditorComponent::invertWave() {
    uint16_t* wave = model.getWaveManager().getMutableWaveData(currentOsc);
    if (!wave) return;
    for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
        int32_t val = (int32_t)wave[i];
        int32_t inv = 65536 - val;
        wave[i] = (uint16_t)std::clamp(inv, 4600, 60935);
    }
    setModified(true);
    repaint();
    if (onWaveformChanged) onWaveformChanged();
}

void WaveformEditorComponent::smoothWave(int passes) {
    if (passes <= 0) passes = smoothKnob ? (int)smoothKnob->getValue() : 5;
    uint16_t* wave = model.getWaveManager().getMutableWaveData(currentOsc);
    if (!wave) return;

    for (int p = 0; p < passes; ++p) {
        std::vector<uint16_t> copy(wave, wave + WTOSC_SAMPLE_COUNT);
        for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
            int i_m2 = (i - 2 + WTOSC_SAMPLE_COUNT) % WTOSC_SAMPLE_COUNT;
            int i_m1 = (i - 1 + WTOSC_SAMPLE_COUNT) % WTOSC_SAMPLE_COUNT;
            int i_p1 = (i + 1) % WTOSC_SAMPLE_COUNT;
            int i_p2 = (i + 2) % WTOSC_SAMPLE_COUNT;
            int32_t sum = (int32_t)copy[i_m2] + 2 * (int32_t)copy[i_m1] + 3 * (int32_t)copy[i]
                        + 2 * (int32_t)copy[i_p1] + (int32_t)copy[i_p2];
            wave[i] = (uint16_t)(sum / 9);
        }
    }
    setModified(true);
    repaint();
    if (onWaveformChanged) onWaveformChanged();
}

void WaveformEditorComponent::normalizeWave(float targetGain) {
    if (targetGain <= 0.0f) targetGain = normKnob ? ((float)normKnob->getValue() / 100.0f) : 1.0f;
    uint16_t* wave = model.getWaveManager().getMutableWaveData(currentOsc);
    if (!wave) return;
    int32_t maxDev = 0;
    for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
        int32_t dev = std::abs((int32_t)wave[i] - 32768);
        if (dev > maxDev) maxDev = dev;
    }
    if (maxDev > 10) {
        float gain = (28167.0f * targetGain) / (float)maxDev;
        for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
            float dev = ((float)wave[i] - 32768.0f) * gain;
            wave[i] = (uint16_t)std::clamp(32768.0f + dev, 4600.0f, 60935.0f);
        }
        setModified(true);
        repaint();
        if (onWaveformChanged) onWaveformChanged();
    }
}

void WaveformEditorComponent::resized() {
    int topBtnH = 22;
    int topBtnY = 2;
    int titleLeftW = badgeText.isNotEmpty() ? (84 + (int)badgeText.length() * 6 + 12) : 90;
    int rightEdge = getWidth() - 6;

    if (saveDisketteBtn.isVisible()) {
        int diskBtnW = 24;
        saveDisketteBtn.setBounds(rightEdge - diskBtnW, topBtnY, diskBtnW, topBtnH);
        rightEdge -= (diskBtnW + 4);
    }

    int maxPresetW = rightEdge - titleLeftW;
    int presetW = std::max(160, maxPresetW);
    int presetX = rightEdge - presetW;
    waveDisplayBtn.setBounds(presetX, topBtnY, presetW, topBtnH);

    int botH = 22;
    int botY = getHeight() - botH - 3;
    int leftX = 6;

    if (prevFrameBtn.isVisible()) {
        prevFrameBtn.setBounds(leftX, botY, 18, botH); leftX += 20;
        frameLabel.setBounds(leftX, botY, 34, botH); leftX += 36;
        nextFrameBtn.setBounds(leftX, botY, 18, botH); leftX += 22;
    }

    int smoothBtnW = 62;
    int smoothKnobW = 95;
    smoothBtn.setBounds(leftX, botY, smoothBtnW, botH);
    if (smoothKnob) {
        smoothKnob->setBounds(leftX + smoothBtnW + 4, botY, smoothKnobW, botH);
    }

    int invertBtnW = 72;
    int invertX = (getWidth() - invertBtnW) / 2;
    invertBtn.setBounds(invertX, botY, invertBtnW, botH);

    int normBtnW = 82;
    int normKnobW = 100;
    int rEdge = getWidth() - 6;
    normBtn.setBounds(rEdge - normBtnW, botY, normBtnW, botH);
    if (normKnob) {
        normKnob->setBounds(rEdge - normBtnW - 4 - normKnobW, botY, normKnobW, botH);
    }

    saveModal.setBounds(getLocalBounds());
}

void WaveformEditorComponent::applySampleEdit(int mouseX, int mouseY) {
    uint16_t* wave = model.getWaveManager().getMutableWaveData(currentOsc);
    if (!wave) return;

    auto dispBounds = getLocalBounds().reduced(6).withTrimmedTop(30).withTrimmedBottom(28);
    if (dispBounds.getWidth() <= 0 || dispBounds.getHeight() <= 0) return;

    float normX = (float)(mouseX - dispBounds.getX()) / (float)dispBounds.getWidth();
    float normY = (float)(mouseY - dispBounds.getY()) / (float)dispBounds.getHeight();
    normX = std::clamp(normX, 0.0f, 1.0f);
    normY = std::clamp(normY, 0.0f, 1.0f);

    int targetIdx = std::clamp((int)(normX * (WTOSC_SAMPLE_COUNT - 1)), 0, WTOSC_SAMPLE_COUNT - 1);
    float amp = (0.5f - normY) * 2.0f; // [-1.0, 1.0]
    uint16_t targetVal = (uint16_t)std::clamp(32768.0f + amp * 28167.0f, 4600.0f, 60935.0f);

    if (lastEditX >= 0 && lastEditX != targetIdx) {
        int startIdx = std::min(lastEditX, targetIdx);
        int endIdx = std::max(lastEditX, targetIdx);
        uint16_t startVal = (lastEditX < targetIdx) ? (uint16_t)lastEditY : targetVal;
        uint16_t endVal = (lastEditX < targetIdx) ? targetVal : (uint16_t)lastEditY;

        for (int i = startIdx; i <= endIdx; ++i) {
            float frac = (float)(i - startIdx) / (float)(endIdx - startIdx);
            wave[i] = (uint16_t)(startVal + frac * (endVal - startVal));
        }
    } else {
        wave[targetIdx] = targetVal;
    }

    lastEditX = targetIdx;
    lastEditY = (int)targetVal;

    setModified(true);
    repaint();
    if (onWaveformChanged) onWaveformChanged();
}

void WaveformEditorComponent::mouseDown(const juce::MouseEvent& e) {
    auto dispBounds = getLocalBounds().reduced(6).withTrimmedTop(30).withTrimmedBottom(28);
    if (dispBounds.contains(e.getPosition())) {
        lastEditX = -1;
        applySampleEdit(e.x, e.y);
    }
}

void WaveformEditorComponent::mouseDrag(const juce::MouseEvent& e) {
    auto dispBounds = getLocalBounds().reduced(6).withTrimmedTop(30).withTrimmedBottom(28);
    if (dispBounds.contains(e.getPosition()) || lastEditX >= 0) {
        applySampleEdit(e.x, e.y);
    }
}

void WaveformEditorComponent::mouseUp(const juce::MouseEvent& /*e*/) {
    lastEditX = -1;
    lastEditY = -1;
}

void WaveformEditorComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];

    g.setColour(theme.cardBg);
    g.fillRect(bounds);
    g.setColour(theme.cardBorder);
    g.drawRect(bounds, 1.0f);

    auto headerRect = bounds.withHeight(26.0f);
    g.setColour(theme.cardHeader);
    g.fillRect(headerRect);

    g.setColour(theme.accent);
    g.fillRect(bounds.getX(), 25.0f, bounds.getWidth(), 1.5f);

    g.setFont(lnf ? lnf->getCustomFont(11.0f, juce::Font::bold) : juce::Font(11.0f, juce::Font::bold));
    g.setColour(theme.textTitle);
    g.drawText(titleText, 8, 3, 74, 20, juce::Justification::centredLeft, false);

    if (badgeText.isNotEmpty()) {
        g.setFont(lnf ? lnf->getCustomFont(8.5f, juce::Font::bold) : juce::Font(8.5f, juce::Font::bold));
        int badgeW = (int)g.getCurrentFont().getStringWidth(badgeText) + 8;
        auto badgeRect = juce::Rectangle<float>(82.0f, 4.0f, (float)badgeW, 16.0f);
        g.setColour(theme.cardHeader);
        g.fillRect(badgeRect);
        g.setColour(theme.accentDark);
        g.drawRect(badgeRect, 1.0f);
        g.setColour(theme.accent);
        g.drawText(badgeText, badgeRect, juce::Justification::centred, false);
    }

    auto disp = bounds.reduced(6.0f).withTrimmedTop(30.0f).withTrimmedBottom(28.0f);
    g.setColour(theme.windowBg);
    g.fillRect(disp);
    g.setColour(theme.cardBorder);
    g.drawRect(disp, 1.0f);

    g.setColour(theme.cardBorder);
    g.fillRect(bounds.getX() + 6.0f, bounds.getBottom() - 28.0f, bounds.getWidth() - 12.0f, 1.0f);

    if (disp.getWidth() <= 10.0f || disp.getHeight() <= 10.0f) return;

    float midY = disp.getCentreY();
    g.setColour(theme.accent.withAlpha(0.25f));
    g.drawHorizontalLine((int)midY, disp.getX(), disp.getRight());

    g.setColour(theme.visualizerGrid);
    g.drawHorizontalLine((int)(midY - disp.getHeight() * 0.25f), disp.getX(), disp.getRight());
    g.drawHorizontalLine((int)(midY + disp.getHeight() * 0.25f), disp.getX(), disp.getRight());

    for (int q = 1; q <= 3; ++q) {
        float qx = disp.getX() + disp.getWidth() * (q * 0.25f);
        g.drawVerticalLine((int)qx, disp.getY(), disp.getBottom());
    }

    const uint16_t* waveData = model.getWaveManager().getWaveData(currentOsc);
    if (!waveData) return;

    juce::Path wavePath;
    juce::Path fillPath;
    int plotW = (int)disp.getWidth();
    float dispY = disp.getY();
    float dispH = disp.getHeight();
    float halfH = dispH * 0.46f;

    for (int x = 0; x < plotW; ++x) {
        int sIdx = (x * WTOSC_SAMPLE_COUNT) / plotW;
        if (sIdx >= WTOSC_SAMPLE_COUNT) sIdx = WTOSC_SAMPLE_COUNT - 1;
        float normVal = ((float)waveData[sIdx] - 32768.0f) / 28167.0f;
        normVal = std::clamp(normVal, -1.0f, 1.0f);
        float py = midY - normVal * halfH;

        if (x == 0) {
            wavePath.startNewSubPath(disp.getX() + (float)x, py);
            fillPath.startNewSubPath(disp.getX() + (float)x, midY);
            fillPath.lineTo(disp.getX() + (float)x, py);
        } else {
            wavePath.lineTo(disp.getX() + (float)x, py);
            fillPath.lineTo(disp.getX() + (float)x, py);
        }
    }

    fillPath.lineTo(disp.getRight(), midY);
    fillPath.closeSubPath();

    juce::ColourGradient fillGrad(theme.accent.withAlpha(0.20f), 0, dispY,
                                 theme.accent.withAlpha(0.02f), 0, disp.getBottom(), false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    g.setColour(theme.accent);
    g.strokePath(wavePath, juce::PathStrokeType(1.8f));
}
