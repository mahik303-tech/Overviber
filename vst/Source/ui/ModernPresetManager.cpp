#include "ModernPresetManager.h"
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
#include "../PluginProcessor.h"
#endif
#include "../data/OverviberPaths.h"

// ==============================================================================
// Category Detection Helper
// ==============================================================================
juce::String getPresetCategoryTag(const juce::String& name) {
    juce::String lower = name.toLowerCase();
    if (lower.contains("init")) return "INIT";
    if (lower.contains("bass") || lower.contains("sub") || lower.contains("acid")) return "BASS";
    if (lower.contains("lead") || lower.contains("saw") || lower.contains("sync") || lower.contains("mini")) return "LEAD";
    if (lower.contains("pad") || lower.contains("string") || lower.contains("swell") || lower.contains("drift") || lower.contains("choir") || lower.contains("chorus")) return "PAD";
    if (lower.contains("key") || lower.contains("piano") || lower.contains("organ") || lower.contains("bell") || lower.contains("pluck")) return "KEYS";
    if (lower.contains("arp") || lower.contains("seq") || lower.contains("slam")) return "ARP/SEQ";
    if (lower.contains("brass")) return "BRASS";
    if (lower.contains("fractal") || lower.contains("lo-fi") || lower.contains("eerie") || lower.contains("dark")) return "FX";
    return "SYNTH";
}

// ==============================================================================
// ModernHeaderButton Implementation
// ==============================================================================
ModernHeaderButton::ModernHeaderButton(const juce::String& name, const juce::String& text)
    : juce::Button(name), standardText(text) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

ModernHeaderButton::~ModernHeaderButton() {
    stopTimer();
}

void ModernHeaderButton::setButtonText(const juce::String& newText) {
    standardText = newText;
    repaint();
}

void ModernHeaderButton::setFlashText(const juce::String& text, int durationMs) {
    flashText = text;
    isFlashing = true;
    repaint();
    startTimer(durationMs);
}

void ModernHeaderButton::timerCallback() {
    stopTimer();
    isFlashing = false;
    repaint();
}

void ModernHeaderButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);

    juce::Colour bgCol = juce::Colour(0xff22252a);
    juce::Colour borderCol = juce::Colour(0xff3a3d45);

    if (isFlashing) {
        bgCol = juce::Colour(0xff185438);
        borderCol = juce::Colour(0xff2bb673);
    } else if (shouldDrawButtonAsDown) {
        bgCol = juce::Colour(0xff16181b);
        borderCol = accentCol;
    } else if (shouldDrawButtonAsHighlighted) {
        bgCol = juce::Colour(0xff2c3037);
        borderCol = accentCol.withAlpha(0.85f);
    }

    // Sharp rectangular buttons (no rounded corners)
    g.setColour(bgCol);
    g.fillRect(bounds);

    g.setColour(borderCol);
    g.drawRect(bounds, 1.0f);

    juce::String textToDraw = isFlashing ? flashText : (standardText.isNotEmpty() ? standardText : getName());
    juce::Colour textCol = isFlashing ? juce::Colour(0xff39d98a) : (shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colour(0xffdcdfe4));

    g.setColour(textCol);
    g.setFont(ModernFontManager::createFont("D-DIN", 11.5f, juce::Font::bold));
    g.drawText(textToDraw, getLocalBounds(), juce::Justification::centred, false);
}

// ==============================================================================
// ModernPresetDisplayButton Implementation
// ==============================================================================
ModernPresetDisplayButton::ModernPresetDisplayButton() {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void ModernPresetDisplayButton::setPreset(int presetNumber, int totalSlots, const juce::String& name, const juce::String& cat) {
    if (slot == presetNumber && total == totalSlots && presetName == name && category == cat) return;
    slot = presetNumber;
    total = totalSlots;
    presetName = name;
    category = cat;
    repaint();
}

void ModernPresetDisplayButton::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    bool isCompact = bounds.getHeight() <= 22.0f;

    juce::Colour bgCol = isHovered ? juce::Colour(0xff22262d) : juce::Colour(0xff191c21);
    juce::Colour borderCol = isHovered ? accentCol.withAlpha(0.85f) : juce::Colour(0xff373b43);

    // Sharp rectangular display box
    g.setColour(bgCol);
    g.fillRect(bounds);

    g.setColour(borderCol);
    g.drawRect(bounds, 1.0f);

    float padY = isCompact ? 2.0f : 4.0f;
    float boxH = bounds.getHeight() - (padY * 2.0f);

    // Left Slot Number Box (sharp)
    float slotW = isCompact ? 22.0f : 32.0f;
    juce::Rectangle<float> slotRect(bounds.getX() + (isCompact ? 3.0f : 5.0f), bounds.getY() + padY, slotW, boxH);
    g.setColour(accentCol.withAlpha(0.20f));
    g.fillRect(slotRect);
    g.setColour(accentCol.withAlpha(0.55f));
    g.drawRect(slotRect, 1.0f);

    g.setColour(accentCol);
    g.setFont(ModernFontManager::createFont("D-DIN", isCompact ? 9.5f : 10.5f, juce::Font::bold));
    juce::String slotStr = slot >= 0 ? juce::String::formatted("%04d", slot) : "----";
    g.drawText(slotStr, slotRect, juce::Justification::centred, false);

    // Right Chevron icon on far right
    float arrowW = isCompact ? 14.0f : 20.0f;
    float arrowCenterX = bounds.getRight() - (arrowW * 0.5f);
    float arrowCenterY = bounds.getCentreY();
    juce::Path arrow;
    float aw = isCompact ? 3.0f : 4.0f;
    float ah = isCompact ? 2.0f : 2.5f;
    arrow.startNewSubPath(arrowCenterX - aw, arrowCenterY - (ah * 0.6f));
    arrow.lineTo(arrowCenterX, arrowCenterY + ah);
    arrow.lineTo(arrowCenterX + aw, arrowCenterY - (ah * 0.6f));
    g.setColour(isHovered ? accentCol : juce::Colour(0xff8a8e98));
    g.strokePath(arrow, juce::PathStrokeType(1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

    // Right Category (only if space permits)
    float rightMargin = arrowW + (isCompact ? 2.0f : 4.0f);
    float minSpaceForCategory = isCompact ? 140.0f : 180.0f;
    if (category.isNotEmpty() && bounds.getWidth() >= minSpaceForCategory) {
        float tagW = isCompact ? 40.0f : 48.0f;
        juce::Rectangle<float> tagRect(bounds.getRight() - rightMargin - tagW, bounds.getY() + padY, tagW, boxH);
        g.setColour(juce::Colour(0xff2d3138));
        g.fillRect(tagRect);
        g.setColour(juce::Colour(0xff8a8e98));
        g.setFont(ModernFontManager::createFont("D-DIN", isCompact ? 8.5f : 9.5f, juce::Font::bold));
        g.drawText(category, tagRect, juce::Justification::centred, false);
        rightMargin += tagW + (isCompact ? 4.0f : 6.0f);
    }

    // Preset Name in Center
    float textX = slotRect.getRight() + (isCompact ? 5.0f : 8.0f);
    float textW = (bounds.getRight() - rightMargin) - textX;
    if (textW > 10.0f) {
        g.setColour(juce::Colours::white);
        g.setFont(ModernFontManager::createFont("D-DIN", isCompact ? 11.0f : 13.0f, juce::Font::bold));
        g.drawText(presetName, (int)textX, (int)bounds.getY(), (int)textW, (int)bounds.getHeight(), juce::Justification::centredLeft, true);
    }
}

void ModernPresetDisplayButton::mouseDown(const juce::MouseEvent&) {
    if (onClick) onClick();
}

// ==============================================================================
// ModernPresetBar Implementation
// ==============================================================================
ModernPresetBar::ModernPresetBar(SynthEngine& eng, OvercyclerAudioProcessor* p)
    : engine(eng), processor(p) {

    setComponentID("modernPresetBar");

    prevBtn.setComponentID("presetPrevBtn");
    prevBtn.onClick = [this]() { prevPreset(); };
    addAndMakeVisible(prevBtn);

    displayBtn.setComponentID("presetDisplayBtn");
    displayBtn.onClick = [this]() {
        if (onToggleBrowser) onToggleBrowser();
    };
    addAndMakeVisible(displayBtn);

    nextBtn.setComponentID("presetNextBtn");
    nextBtn.onClick = [this]() { nextPreset(); };
    addAndMakeVisible(nextBtn);

    saveBtn.setComponentID("presetSaveBtn");
    saveBtn.onClick = [this]() {
        if (onOpenSaveAs) onOpenSaveAs();
        else quickSave();
    };
    addAndMakeVisible(saveBtn);

    initBtn.setComponentID("presetInitBtn");
    initBtn.onClick = [this]() { initPatch(); };
    addAndMakeVisible(initBtn);

    updateDisplay();
}

ModernPresetBar::~ModernPresetBar() = default;

void ModernPresetBar::setAccentColour(juce::Colour c) {
    accentCol = c;
    prevBtn.setAccentColour(c);
    displayBtn.setAccentColour(c);
    nextBtn.setAccentColour(c);
    saveBtn.setAccentColour(c);
    initBtn.setAccentColour(c);
    repaint();
}

void ModernPresetBar::paint(juce::Graphics&) {
}

void ModernPresetBar::resized() {
    int h = getHeight();
    int btnH = std::min(h, 26);
    int y = (h - btnH) / 2;

    int curX = 0;
    int gap = 5;

    // Previous Preset (<)
    prevBtn.setBounds(curX, y, 26, btnH);
    curX += 26 + gap;

    // Preset Display (flexibly sized, fills space; clicking opens browser)
    int fixedRightButtonsW = 26 + 10 + 60 + gap + 50; // next + (10px section gap) + save + init
    int displayW = std::max(140, getWidth() - curX - fixedRightButtonsW - 5);
    displayBtn.setBounds(curX, y, displayW, btnH);
    curX += displayW + gap;

    // Next Preset (>)
    nextBtn.setBounds(curX, y, 26, btnH);
    curX += 26 + 10; // 10px clear separation gap between navigation and action buttons (5px grid: 2x 5px)

    // Combined Save Button (Save / Save As)
    saveBtn.setBounds(curX, y, 60, btnH);
    curX += 60 + gap;

    // Init Button
    initBtn.setBounds(curX, y, 50, btnH);
}

void ModernPresetBar::setSaveFlashText(const juce::String& text) {
    saveBtn.setFlashText(text);
}

void ModernPresetBar::updateDisplay() {
    auto& pm = engine.getPresetManager();
    int curProg = 0;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (processor != nullptr) {
        curProg = processor->getCurrentProgram();
    }
#endif
    int count = pm.getPresetCount();
    // The loaded state is authoritative, also for Init and restored .ovm files.
    const juce::String name(engine.getCurrentPreset().presetName);
    if (curProg < 0 || curProg >= count || name != juce::String(pm.getPresetName(curProg))) curProg = -1;
    const int presetNumber = curProg >= 0 ? pm.getPresetNumber(curProg) : -1;
    displayBtn.setPreset(presetNumber, count, name, getPresetCategoryTag(name));
}

void ModernPresetBar::selectPreset(int index) {
    auto& pm = engine.getPresetManager();
    int count = pm.getPresetCount();
    if (index >= 0 && index < count) {
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
        if (processor != nullptr) {
            processor->setCurrentProgram(index);
        } else {
            engine.loadPreset(index);
        }
#else
        engine.loadPreset(index);
#endif
        updateDisplay();
        if (onPresetChanged) onPresetChanged(index);
    }
}

void ModernPresetBar::prevPreset() {
    int curProg = 0;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (processor != nullptr) curProg = processor->getCurrentProgram();
#endif
    if (curProg > 0) {
        selectPreset(curProg - 1);
    }
}

void ModernPresetBar::nextPreset() {
    auto& pm = engine.getPresetManager();
    int curProg = 0;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (processor != nullptr) curProg = processor->getCurrentProgram();
#endif
    if (curProg + 1 < pm.getPresetCount()) {
        selectPreset(curProg + 1);
    }
}

void ModernPresetBar::quickSave() {
    auto& pm = engine.getPresetManager();
    int curProg = 0;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (processor != nullptr) curProg = processor->getCurrentProgram();
#endif
    if (curProg >= 0 && curProg < pm.getPresetCount()) {
        auto& current = engine.getCurrentPreset();
        if (pm.savePreset(curProg, current)) {
            saveBtn.setFlashText("SAVED!");
            updateDisplay();
        }
    }
}

void ModernPresetBar::initPatch() {
    engine.getCurrentPreset().setDefaults();
    engine.applyPreset();
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (processor) processor->updateAPVTSFromEngine();
#endif
    saveBtn.setFlashText("INITIALIZED");
    updateDisplay();
    if (onPresetChanged) onPresetChanged(-1);
}

// ==============================================================================
// ModernSaveAsModal Implementation
// ==============================================================================
ModernSaveAsModal::ModernSaveAsModal() {
    setAlwaysOnTop(true);
    setInterceptsMouseClicks(true, true);
    setComponentID("saveAsModal");

    titleLabel.setText("SAVE PRESET", juce::dontSendNotification);
    titleLabel.setFont(ModernFontManager::createFont("D-DIN", 14.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setComponentID("saveAsTitleLabel");
    addAndMakeVisible(titleLabel);

    descLabel.setText("Save changes to current preset or enter a new name:", juce::dontSendNotification);
    descLabel.setFont(ModernFontManager::createFont("D-DIN", 11.0f, juce::Font::plain));
    descLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ea2aa));
    descLabel.setJustificationType(juce::Justification::centredLeft);
    descLabel.setComponentID("saveAsDescLabel");
    addAndMakeVisible(descLabel);

    nameEditor.setMultiLine(false);
    nameEditor.setReturnKeyStartsNewLine(false);
    nameEditor.setFont(ModernFontManager::createFont("D-DIN", 13.0f, juce::Font::bold));
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff121417));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3f434c));
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, accentCol);
    nameEditor.setComponentID("saveAsNameEditor");
    nameEditor.onReturnKey = [this]() {
        if (saveCallback && nameEditor.getText().isNotEmpty()) {
            saveCallback(nameEditor.getText());
            hide();
        }
    };
    nameEditor.onEscapeKey = [this]() { hide(); };
    addAndMakeVisible(nameEditor);

    saveBtn.setButtonText("SAVE");
    saveBtn.setComponentID("saveAsConfirmBtn");
    saveBtn.onClick = [this]() {
        if (saveCallback && nameEditor.getText().isNotEmpty()) {
            saveCallback(nameEditor.getText());
            hide();
        }
    };
    addAndMakeVisible(saveBtn);

    cancelBtn.setButtonText("CANCEL");
    cancelBtn.setComponentID("saveAsCancelBtn");
    cancelBtn.onClick = [this]() { hide(); };
    addAndMakeVisible(cancelBtn);

    setVisible(false);
}

void ModernSaveAsModal::show(const juce::String& currentName, std::function<void(const juce::String& newName)> onSave) {
    saveCallback = onSave;
    nameEditor.setText(currentName);
    nameEditor.selectAll();
    setVisible(true);
    nameEditor.grabKeyboardFocus();
}

void ModernSaveAsModal::hide() {
    setVisible(false);
}

void ModernSaveAsModal::paint(juce::Graphics& g) {
    // Semi-transparent backdrop overlay
    g.fillAll(juce::Colour(0xaa000000));

    // Centered dialog card (sharp industrial container, 0.0px corner radius, 1.0px border)
    int cardW = 390;
    int cardH = 175;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;
    juce::Rectangle<float> cardRect((float)cardX, (float)cardY, (float)cardW, (float)cardH);

    g.setColour(juce::Colour(0xff1e2126));
    g.fillRect(cardRect);

    g.setColour(accentCol);
    g.drawRect(cardRect, 1.0f);

    // Accent line below title
    g.setColour(accentCol);
    g.fillRect(cardRect.getX() + 20.0f, cardRect.getY() + 38.0f, cardRect.getWidth() - 40.0f, 1.0f);
}

void ModernSaveAsModal::resized() {
    int cardW = 390;
    int cardH = 175;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;

    titleLabel.setBounds(cardX + 20, cardY + 12, cardW - 40, 24);
    descLabel.setBounds(cardX + 25, cardY + 48, cardW - 50, 20);
    nameEditor.setBounds(cardX + 25, cardY + 72, cardW - 50, 28);

    cancelBtn.setBounds(cardX + cardW - 200, cardY + 120, 85, 26);
    saveBtn.setBounds(cardX + cardW - 105, cardY + 120, 85, 26);
}

// ==============================================================================
// ==============================================================================
// ModernPresetBrowserOverlay Implementation
// ==============================================================================
ModernPresetBrowserOverlay::ModernPresetBrowserOverlay(SynthEngine& eng, OvercyclerAudioProcessor* p)
    : engine(eng), processor(p) {

    setAlwaysOnTop(true);
    setInterceptsMouseClicks(true, true);
    setComponentID("presetBrowserOverlay");

    titleLabel.setText("PRESET & WAVE BROWSER", juce::dontSendNotification);
    titleLabel.setFont(ModernFontManager::createFont("D-DIN", 15.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, accentCol);
    titleLabel.setComponentID("presetBrowserTitle");
    addAndMakeVisible(titleLabel);

    modePresetsBtn.setConnectedEdges(juce::Button::ConnectedOnRight);
    modeWavesBtn.setConnectedEdges(juce::Button::ConnectedOnLeft);
    modePresetsBtn.setToggleState(true, juce::dontSendNotification);

    modePresetsBtn.onClick = [this]() { setMode(BrowserMode::PatchPresets); };
    modeWavesBtn.onClick = [this]() { setMode(BrowserMode::Waveforms, targetOsc); };

    addAndMakeVisible(modePresetsBtn);
    addAndMakeVisible(modeWavesBtn);

    searchBox.setTextToShowWhenEmpty("Search by name, bank or category...", juce::Colour(0xff777a84));
    searchBox.setFont(ModernFontManager::createFont("D-DIN", 12.5f, juce::Font::plain));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff14161a));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3f434c));
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, accentCol);
    searchBox.setComponentID("presetSearchBox");
    searchBox.onTextChange = [this]() { filterList(); };
    addAndMakeVisible(searchBox);

    closeBtn.setComponentID("presetBrowserCloseBtn");
    closeBtn.onClick = [this]() {
        setVisible(false);
        if (onClose) onClose();
    };
    addAndMakeVisible(closeBtn);

    setupCategoryChips();

    listBox.setModel(this);
    listBox.setRowHeight(36);
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff15171b));
    listBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff2d3138));
    listBox.setComponentID("presetListBox");
    addAndMakeVisible(listBox);

    statusLabel.setFont(ModernFontManager::createFont("D-DIN", 11.5f, juce::Font::plain));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8e929c));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setComponentID("presetStatusLabel");
    addAndMakeVisible(statusLabel);

    openFolderBtn.setComponentID("presetOpenFolderBtn");
    openFolderBtn.onClick = [this]() {
        if (currentMode == BrowserMode::PatchPresets) {
            auto basePath = engine.getPresetManager().getBaseDirectory();
            if (basePath.empty()) basePath = OverviberPaths::getPresetsDirectory().getFullPathName().toStdString();
            juce::File f(basePath);
            if (f.exists()) f.startAsProcess();
        } else {
            auto basePath = engine.getWaveManager().getBaseDirectory();
            if (basePath.empty()) basePath = OverviberPaths::getWaveDataDirectory().getFullPathName().toStdString();
            juce::File f(basePath);
            if (f.exists()) f.startAsProcess();
        }
    };
    addAndMakeVisible(openFolderBtn);

    reloadBtn.setComponentID("presetReloadBtn");
    reloadBtn.onClick = [this]() {
        if (currentMode == BrowserMode::PatchPresets) {
            engine.getPresetManager().scanPresets();
        } else {
            engine.getWaveManager().scanDirectory();
        }
        refreshList();
    };
    addAndMakeVisible(reloadBtn);

    closeBottomBtn.setComponentID("presetCloseBottomBtn");
    closeBottomBtn.onClick = [this]() {
        setVisible(false);
        if (onClose) onClose();
    };
    addAndMakeVisible(closeBottomBtn);

    refreshList();
    setVisible(false);
}

ModernPresetBrowserOverlay::~ModernPresetBrowserOverlay() = default;

void ModernPresetBrowserOverlay::setMode(BrowserMode mode, abx_t osc) {
    currentMode = mode;
    targetOsc = osc;
    modePresetsBtn.setToggleState(currentMode == BrowserMode::PatchPresets, juce::dontSendNotification);
    modeWavesBtn.setToggleState(currentMode == BrowserMode::Waveforms, juce::dontSendNotification);

    if (currentMode == BrowserMode::PatchPresets) {
        titleLabel.setText("PATCH PRESET BROWSER", juce::dontSendNotification);
    } else {
        juce::String oscStr = (targetOsc == abxAMain) ? "OSC A" : "OSC B";
        titleLabel.setText("WAVEFORM BROWSER [" + oscStr + "]", juce::dontSendNotification);
    }

    selectedCategory = "ALL";
    setupCategoryChips();
    refreshList();
    resized();
    repaint();
}

void ModernPresetBrowserOverlay::setupCategoryChips() {
    categoryChips.clear();
    if (currentMode == BrowserMode::PatchPresets) {
        categories = { "ALL", "INIT", "BASS", "LEAD", "PAD", "KEYS", "SYNTH", "ARP/SEQ", "FX" };
    } else {
        categories = { "ALL", "SHAPES", "USER", "AKWF" };
    }

    for (size_t i = 0; i < categories.size(); ++i) {
        auto chip = std::make_unique<ModernHeaderButton>("cat_" + categories[i], categories[i]);
        chip->setComponentID("presetCatChip_" + categories[i]);
        chip->setAccentColour(accentCol);
        chip->onClick = [this, cat = categories[i]]() {
            selectedCategory = cat;
            filterList();
            repaint();
        };
        addAndMakeVisible(*chip);
        categoryChips.push_back(std::move(chip));
    }
}

void ModernPresetBrowserOverlay::setAccentColour(juce::Colour c) {
    accentCol = c;
    titleLabel.setColour(juce::Label::textColourId, c);
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, c);
    closeBtn.setAccentColour(c);
    reloadBtn.setAccentColour(c);
    openFolderBtn.setAccentColour(c);
    closeBottomBtn.setAccentColour(c);
    modePresetsBtn.setAccentColour(c);
    modeWavesBtn.setAccentColour(c);
    for (auto& chip : categoryChips) chip->setAccentColour(c);
    repaint();
}

void ModernPresetBrowserOverlay::visibilityChanged() {
    if (isVisible()) {
        refreshList();
        searchBox.grabKeyboardFocus();
    }
}

void ModernPresetBrowserOverlay::refreshList() {
    if (currentMode == BrowserMode::PatchPresets) {
        auto& pm = engine.getPresetManager();
        allPresets.clear();
        int count = pm.getPresetCount();

        for (int i = 0; i < count; ++i) {
            PresetEntry pe;
            pe.index = i;
            pe.number = pm.getPresetNumber(i);
            pe.name = pm.getPresetName(i);
            pe.category = getPresetCategoryTag(pe.name);
            allPresets.push_back(pe);
        }
    } else {
        auto& wm = engine.getWaveManager();
        allWaves.clear();
        int idx = 0;

        // 1. Standard shapes
        const char* stdShapes[] = { "Sine", "Triangle", "Sawtooth", "Square", "Pulse 25%", "Pulse 10%", "Half Sine", "Harmonics 1-8", "White Noise", "Parabolic" };
        for (int i = 0; i < 10; ++i) {
            WaveEntry we;
            we.index = idx++;
            we.bank = "_shapes";
            we.waveName = stdShapes[i];
            we.category = "SHAPES";
            we.displayName = stdShapes[i];
            allWaves.push_back(we);
        }

        // 2. Scan all banks from WaveManager
        const auto& banks = wm.getBankNames();
        for (const auto& b : banks) {
            auto waves = wm.getWaveNames(b);
            juce::String cat = "AKWF";
            juce::String bUpper = juce::String(b).toUpperCase();
            if (bUpper.contains("USER")) cat = "USER";
            else if (bUpper.contains("_BASIC")) cat = "SHAPES";

            for (const auto& w : waves) {
                WaveEntry we;
                we.index = idx++;
                we.bank = b;
                we.waveName = w;
                we.category = cat;
                we.displayName = juce::String(b) + " / " + juce::String(w);
                allWaves.push_back(we);
            }
        }
    }
    filterList();
}

void ModernPresetBrowserOverlay::filterList() {
    juce::String q = searchBox.getText().trim().toLowerCase();

    if (currentMode == BrowserMode::PatchPresets) {
        filteredPresets.clear();
        for (const auto& pe : allPresets) {
            if (selectedCategory != "ALL" && pe.category != selectedCategory) continue;
            if (q.isNotEmpty()) {
                if (!pe.name.toLowerCase().contains(q) && !pe.category.toLowerCase().contains(q)) continue;
            }
            filteredPresets.push_back(pe);
        }
        statusLabel.setText(juce::String(filteredPresets.size()) + " of " + juce::String(allPresets.size()) + " Presets showing", juce::dontSendNotification);
    } else {
        filteredWaves.clear();
        for (const auto& we : allWaves) {
            if (selectedCategory != "ALL" && we.category != selectedCategory) continue;
            if (q.isNotEmpty()) {
                if (!we.displayName.toLowerCase().contains(q) && !we.category.toLowerCase().contains(q) && !we.bank.toLowerCase().contains(q)) continue;
            }
            filteredWaves.push_back(we);
        }
        statusLabel.setText(juce::String(filteredWaves.size()) + " of " + juce::String(allWaves.size()) + " Waveforms showing", juce::dontSendNotification);
    }

    listBox.updateContent();
    listBox.repaint();
}

void ModernPresetBrowserOverlay::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xdd0b0d10));

    auto b = getLocalBounds().toFloat().reduced(12.0f);

    g.setColour(juce::Colour(0xff1a1c21));
    g.fillRect(b);

    g.setColour(accentCol.withAlpha(0.45f));
    g.drawRect(b, 1.5f);

    g.setColour(juce::Colour(0xff2d3138));
    g.drawHorizontalLine((int)(b.getY() + 94.0f), b.getX() + 10.0f, b.getRight() - 10.0f);
}

void ModernPresetBrowserOverlay::resized() {
    auto b = getLocalBounds().reduced(12);

    int topY = b.getY() + 12;
    titleLabel.setBounds(b.getX() + 18, topY, 210, 30);

    modePresetsBtn.setBounds(b.getX() + 235, topY, 110, 30);
    modeWavesBtn.setBounds(b.getX() + 345, topY, 130, 30);

    int searchX = b.getX() + 485;
    int searchW = b.getRight() - searchX - 50;
    searchBox.setBounds(searchX, topY, searchW, 30);
    closeBtn.setBounds(b.getRight() - 44, topY, 30, 30);

    // Category chips row
    int chipY = topY + 42;
    int chipX = b.getX() + 18;
    int chipW = 72;
    for (size_t i = 0; i < categoryChips.size(); ++i) {
        categoryChips[i]->setBounds(chipX + (int)i * (chipW + 6), chipY, chipW, 26);
    }

    // Bottom Bar
    int bottomH = 34;
    int bottomY = b.getBottom() - bottomH - 10;
    statusLabel.setBounds(b.getX() + 18, bottomY, 240, bottomH);
    openFolderBtn.setBounds(b.getRight() - 310, bottomY, 110, bottomH);
    reloadBtn.setBounds(b.getRight() - 192, bottomY, 80, bottomH);
    closeBottomBtn.setBounds(b.getRight() - 104, bottomY, 86, bottomH);

    // ListBox occupies the central space
    int listY = chipY + 36;
    int listH = bottomY - listY - 10;
    listBox.setBounds(b.getX() + 18, listY, b.getWidth() - 36, listH);
}

int ModernPresetBrowserOverlay::getNumRows() {
    if (currentMode == BrowserMode::PatchPresets) {
        return (int)filteredPresets.size();
    }
    return (int)filteredWaves.size();
}

void ModernPresetBrowserOverlay::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) {
    juce::Rectangle<float> rowRect(2.0f, 2.0f, (float)width - 4.0f, (float)height - 4.0f);

    if (currentMode == BrowserMode::PatchPresets) {
        if (rowNumber < 0 || rowNumber >= (int)filteredPresets.size()) return;
        const auto& entry = filteredPresets[rowNumber];
        int currentProgram = 0;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
        if (processor != nullptr) currentProgram = processor->getCurrentProgram();
#endif
        bool isCurrentPatch = (entry.index == currentProgram);

        if (isCurrentPatch) {
            g.setColour(accentCol.withAlpha(0.22f));
            g.fillRect(rowRect);
            g.setColour(accentCol.withAlpha(0.85f));
            g.drawRect(rowRect, 1.2f);
        } else if (rowIsSelected) {
            g.setColour(juce::Colour(0xff292d34));
            g.fillRect(rowRect);
        } else if (rowNumber % 2 == 1) {
            g.setColour(juce::Colour(0xff181a1e));
            g.fillRect(rowRect);
        }

        // Slot Number Box (sharp)
        juce::Rectangle<float> numPill(rowRect.getX() + 8.0f, rowRect.getY() + 6.0f, 34.0f, rowRect.getHeight() - 12.0f);
        g.setColour(isCurrentPatch ? accentCol.withAlpha(0.35f) : juce::Colour(0xff22252a));
        g.fillRect(numPill);
        g.setColour(isCurrentPatch ? accentCol : juce::Colour(0xff8a8e98));
        g.setFont(ModernFontManager::createFont("D-DIN", 10.5f, juce::Font::bold));
        g.drawText(juce::String::formatted("#%02d", entry.number), numPill, juce::Justification::centred, false);

        // Active Indicator Icon
        float textX = numPill.getRight() + 10.0f;
        if (isCurrentPatch) {
            g.setColour(accentCol);
            juce::Path tri;
            float triY = rowRect.getCentreY();
            tri.startNewSubPath(textX, triY - 4.0f);
            tri.lineTo(textX + 6.0f, triY);
            tri.lineTo(textX, triY + 4.0f);
            tri.closeSubPath();
            g.fillPath(tri);
            textX += 12.0f;
        }

        // Preset Name
        g.setFont(ModernFontManager::createFont("D-DIN", 12.5f, isCurrentPatch ? juce::Font::bold : juce::Font::plain));
        g.setColour(isCurrentPatch ? juce::Colours::white : (rowIsSelected ? juce::Colour(0xffeaebee) : juce::Colour(0xffcfd2d9)));
        g.drawText(entry.name, (int)textX, (int)rowRect.getY(), width - (int)textX - 85, (int)rowRect.getHeight(), juce::Justification::centredLeft, true);

        // Category Tag (sharp)
        juce::Rectangle<float> tagRect((float)width - 76.0f, rowRect.getY() + 7.0f, 58.0f, rowRect.getHeight() - 14.0f);
        g.setColour(juce::Colour(0xff22262c));
        g.fillRect(tagRect);
        g.setColour(isCurrentPatch ? accentCol : juce::Colour(0xff777a84));
        g.setFont(ModernFontManager::createFont("D-DIN", 9.0f, juce::Font::bold));
        g.drawText(entry.category, tagRect, juce::Justification::centred, false);
    } else {
        if (rowNumber < 0 || rowNumber >= (int)filteredWaves.size()) return;
        const auto& entry = filteredWaves[rowNumber];
        juce::String curWaveName = (targetOsc == abxAMain)
            ? engine.getCurrentPreset().oscWave[abxAMain]
            : engine.getCurrentPreset().oscWave[abxBMain];
        bool isCurrentWave = (entry.waveName == curWaveName);

        if (isCurrentWave) {
            g.setColour(accentCol.withAlpha(0.22f));
            g.fillRect(rowRect);
            g.setColour(accentCol.withAlpha(0.85f));
            g.drawRect(rowRect, 1.2f);
        } else if (rowIsSelected) {
            g.setColour(juce::Colour(0xff292d34));
            g.fillRect(rowRect);
        } else if (rowNumber % 2 == 1) {
            g.setColour(juce::Colour(0xff181a1e));
            g.fillRect(rowRect);
        }

        // Wave Tag Box (sharp)
        juce::Rectangle<float> numPill(rowRect.getX() + 8.0f, rowRect.getY() + 6.0f, 44.0f, rowRect.getHeight() - 12.0f);
        g.setColour(isCurrentWave ? accentCol.withAlpha(0.35f) : juce::Colour(0xff22252a));
        g.fillRect(numPill);
        g.setColour(isCurrentWave ? accentCol : juce::Colour(0xff8a8e98));
        g.setFont(ModernFontManager::createFont("D-DIN", 9.5f, juce::Font::bold));
        g.drawText("WAVE", numPill, juce::Justification::centred, false);

        float textX = numPill.getRight() + 10.0f;
        if (isCurrentWave) {
            g.setColour(accentCol);
            juce::Path tri;
            float triY = rowRect.getCentreY();
            tri.startNewSubPath(textX, triY - 4.0f);
            tri.lineTo(textX + 6.0f, triY);
            tri.lineTo(textX, triY + 4.0f);
            tri.closeSubPath();
            g.fillPath(tri);
            textX += 12.0f;
        }

        // Wave Name / Bank
        g.setFont(ModernFontManager::createFont("D-DIN", 12.5f, isCurrentWave ? juce::Font::bold : juce::Font::plain));
        g.setColour(isCurrentWave ? juce::Colours::white : (rowIsSelected ? juce::Colour(0xffeaebee) : juce::Colour(0xffcfd2d9)));
        g.drawText(entry.displayName, (int)textX, (int)rowRect.getY(), width - (int)textX - 90, (int)rowRect.getHeight(), juce::Justification::centredLeft, true);

        // Category Tag
        juce::Rectangle<float> tagRect((float)width - 80.0f, rowRect.getY() + 7.0f, 64.0f, rowRect.getHeight() - 14.0f);
        g.setColour(juce::Colour(0xff22262c));
        g.fillRect(tagRect);
        g.setColour(isCurrentWave ? accentCol : juce::Colour(0xff777a84));
        g.setFont(ModernFontManager::createFont("D-DIN", 9.0f, juce::Font::bold));
        g.drawText(entry.category, tagRect, juce::Justification::centred, false);
    }
}

void ModernPresetBrowserOverlay::listBoxItemClicked(int row, const juce::MouseEvent&) {
    if (currentMode == BrowserMode::PatchPresets) {
        if (row >= 0 && row < (int)filteredPresets.size()) {
            int originalIdx = filteredPresets[row].index;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
            if (processor != nullptr) {
                processor->setCurrentProgram(originalIdx);
            } else {
                engine.loadPreset(originalIdx);
            }
#else
            engine.loadPreset(originalIdx);
#endif
            if (onPresetSelected) onPresetSelected(originalIdx);
            listBox.repaint();
        }
    } else {
        if (row >= 0 && row < (int)filteredWaves.size()) {
            const auto& we = filteredWaves[row];
            if (we.bank == "_shapes") {
                // Map standard shape names
                if (we.waveName == "Sine") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::Sine);
                else if (we.waveName == "Triangle") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::Triangle);
                else if (we.waveName == "Sawtooth") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::Sawtooth);
                else if (we.waveName == "Square") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::Square);
                else if (we.waveName == "Pulse 25%") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::Pulse25);
                else if (we.waveName == "Pulse 10%") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::Pulse10);
                else if (we.waveName == "Half Sine") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::HalfSine);
                else if (we.waveName == "Harmonics 1-8") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::Harmonics1to8);
                else if (we.waveName == "White Noise") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::WhiteNoise);
                else if (we.waveName == "Parabolic") engine.getWaveManager().generateStandardShape(targetOsc, WaveManager::StandardShape::Parabolic);
            } else {
                engine.getWaveManager().loadWave(targetOsc, we.bank.toStdString(), we.waveName.toStdString(), 0);
            }
            engine.refreshOscWaves();
            if (onWaveSelected) onWaveSelected(targetOsc, we.bank, we.waveName);
            listBox.repaint();
        }
    }
}

void ModernPresetBrowserOverlay::listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) {
    listBoxItemClicked(row, e);
    setVisible(false);
    if (onClose) onClose();
}
