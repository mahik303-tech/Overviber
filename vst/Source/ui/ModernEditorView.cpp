#include "ModernEditorView.h"
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
#include "../PluginProcessor.h"
#endif
#include "../data/OverviberPaths.h"
#include <cmath>
#include <algorithm>

// ==============================================================================
// ModernColorPickerModal Implementation (Doubled size & Modern Industrial Look)
// ==============================================================================
ModernEditorView::ModernColorPickerModal::ModernColorPickerModal() {
    setAlwaysOnTop(true);
    setInterceptsMouseClicks(true, true);

    titleLabel.setText("COLOR PALETTE PICKER — ACCENT & THEME TUNING", juce::dontSendNotification);
    titleLabel.setFont(ModernFontManager::createFont("D-DIN", 13.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    roleLabel.setText("EDITING: [ACCENT]", juce::dontSendNotification);
    roleLabel.setFont(ModernFontManager::createFont("D-DIN", 10.5f, juce::Font::bold));
    roleLabel.setColour(juce::Label::textColourId, accentCol);
    roleLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(roleLabel);

    colourSelector = std::make_unique<juce::ColourSelector>(
        juce::ColourSelector::showColourAtTop
        | juce::ColourSelector::showSliders
        | juce::ColourSelector::showColourspace);
    colourSelector->addChangeListener(this);
    addAndMakeVisible(*colourSelector);

    applyBtn.setButtonText("APPLY & CLOSE");
    applyBtn.onClick = [this]() {
        if (applyCallback) applyCallback(currentColour);
        hide();
    };
    addAndMakeVisible(applyBtn);

    cancelBtn.setButtonText("CANCEL");
    cancelBtn.onClick = [this]() {
        if (changeCallback) changeCallback(originalColour);
        hide();
    };
    addAndMakeVisible(cancelBtn);

    setVisible(false);
}

ModernEditorView::ModernColorPickerModal::~ModernColorPickerModal() {
    if (colourSelector) {
        colourSelector->removeChangeListener(this);
    }
}

void ModernEditorView::ModernColorPickerModal::show(juce::Colour initialColour, const juce::String& roleTitle,
                                                   std::function<void(juce::Colour)> onColourChanged,
                                                   std::function<void(juce::Colour)> onApply) {
    originalColour = initialColour;
    currentColour = initialColour;
    changeCallback = onColourChanged;
    applyCallback = onApply;

    roleLabel.setText("EDITING: [" + roleTitle.toUpperCase() + "]", juce::dontSendNotification);
    if (colourSelector) {
        colourSelector->setCurrentColour(initialColour, juce::dontSendNotification);
    }

    setVisible(true);
    toFront(true);
}

void ModernEditorView::ModernColorPickerModal::hide() {
    setVisible(false);
}

void ModernEditorView::ModernColorPickerModal::setAccentColour(juce::Colour c) {
    accentCol = c;
    roleLabel.setColour(juce::Label::textColourId, c);
    applyBtn.setAccentColour(c);
    repaint();
}

void ModernEditorView::ModernColorPickerModal::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == colourSelector.get() && colourSelector != nullptr) {
        currentColour = colourSelector->getCurrentColour();
        if (changeCallback) {
            changeCallback(currentColour);
        }
    }
}

void ModernEditorView::ModernColorPickerModal::mouseDown(const juce::MouseEvent& e) {
    int cardW = 600;
    int cardH = 490;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;
    juce::Rectangle<int> cardRect(cardX, cardY, cardW, cardH);

    if (!cardRect.contains(e.getPosition())) {
        if (applyCallback) applyCallback(currentColour);
        hide();
    }
}

bool ModernEditorView::ModernColorPickerModal::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) {
        if (changeCallback) changeCallback(originalColour);
        hide();
        return true;
    }
    if (key == juce::KeyPress::returnKey) {
        if (applyCallback) applyCallback(currentColour);
        hide();
        return true;
    }
    return false;
}

void ModernEditorView::ModernColorPickerModal::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xaa000000));

    int cardW = 600;
    int cardH = 490;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;
    juce::Rectangle<float> cardRect((float)cardX, (float)cardY, (float)cardW, (float)cardH);

    // Card background (0px corner radius, strict industrial look)
    g.setColour(juce::Colour(0xff1c2026));
    g.fillRect(cardRect);

    // 1px border
    g.setColour(accentCol);
    g.drawRect(cardRect, 1.0f);

    // Header bar (height 32px)
    juce::Rectangle<float> headerRect((float)cardX, (float)cardY, (float)cardW, 32.0f);
    g.setColour(juce::Colour(0xff242a33));
    g.fillRect(headerRect);
    g.setColour(accentCol);
    g.fillRect((float)cardX, (float)cardY + 31.0f, (float)cardW, 1.0f);

    // Bottom action bar separator
    g.setColour(juce::Colour(0xff333c47));
    g.fillRect((float)cardX, (float)cardY + (float)cardH - 46.0f, (float)cardW, 1.0f);
}

void ModernEditorView::ModernColorPickerModal::resized() {
    int cardW = 600;
    int cardH = 490;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;

    titleLabel.setBounds(cardX + 16, cardY + 4, 340, 24);
    roleLabel.setBounds(cardX + cardW - 220, cardY + 4, 204, 24);

    if (colourSelector) {
        colourSelector->setBounds(cardX + 16, cardY + 40, cardW - 32, cardH - 96);
    }

    cancelBtn.setBounds(cardX + cardW - 250, cardY + cardH - 38, 100, 28);
    applyBtn.setBounds(cardX + cardW - 140, cardY + cardH - 38, 124, 28);
}

// ==============================================================================
// ModernPaletteSaveModal Implementation
// ==============================================================================
ModernEditorView::ModernPaletteSaveModal::ModernPaletteSaveModal() {
    setAlwaysOnTop(true);
    setInterceptsMouseClicks(true, true);

    titleLabel.setText("SAVE USER PALETTE", juce::dontSendNotification);
    titleLabel.setFont(ModernFontManager::createFont("D-DIN", 14.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    descLabel.setText("Enter a name for your custom theme palette:", juce::dontSendNotification);
    descLabel.setFont(ModernFontManager::createFont("D-DIN", 11.0f, juce::Font::plain));
    descLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ea2aa));
    descLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(descLabel);

    nameEditor.setMultiLine(false);
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

    saveBtn.onClick = [this]() {
        if (saveCallback && nameEditor.getText().trim().isNotEmpty()) {
            saveCallback(nameEditor.getText().trim());
            hide();
        }
    };
    addAndMakeVisible(saveBtn);

    cancelBtn.onClick = [this]() { hide(); };
    addAndMakeVisible(cancelBtn);

    setVisible(false);
}

void ModernEditorView::ModernPaletteSaveModal::show(const juce::String& currentName, std::function<void(const juce::String& newName)> onSave) {
    saveCallback = onSave;
    nameEditor.setText(currentName.isNotEmpty() ? currentName : "My Palette");
    nameEditor.selectAll();
    setVisible(true);
    toFront(true);
    nameEditor.grabKeyboardFocus();
}

void ModernEditorView::ModernPaletteSaveModal::hide() {
    setVisible(false);
}

void ModernEditorView::ModernPaletteSaveModal::setAccentColour(juce::Colour c) {
    accentCol = c;
    saveBtn.setAccentColour(c);
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, c);
    repaint();
}

void ModernEditorView::ModernPaletteSaveModal::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xaa000000));

    int cardW = 390;
    int cardH = 175;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;
    juce::Rectangle<float> cardRect((float)cardX, (float)cardY, (float)cardW, (float)cardH);

    g.setColour(juce::Colour(0xff1e2126));
    g.fillRect(cardRect);

    g.setColour(accentCol.withAlpha(0.65f));
    g.drawRect(cardRect, 1.5f);

    g.setColour(accentCol);
    g.fillRect(cardRect.getX() + 20.0f, cardRect.getY() + 38.0f, cardRect.getWidth() - 40.0f, 2.0f);
}

void ModernEditorView::ModernPaletteSaveModal::resized() {
    int cardW = 380;
    int cardH = 170;
    int cardX = (getWidth() - cardW) / 2;
    int cardY = (getHeight() - cardH) / 2;

    titleLabel.setBounds(cardX + 20, cardY + 12, cardW - 40, 24);
    descLabel.setBounds(cardX + 24, cardY + 48, cardW - 48, 20);
    nameEditor.setBounds(cardX + 24, cardY + 72, cardW - 48, 30);

    cancelBtn.setBounds(cardX + cardW - 220, cardY + 120, 95, 30);
    saveBtn.setBounds(cardX + cardW - 115, cardY + 120, 95, 30);
}

// ==============================================================================
// ModernDebugTooltipWindow Implementation
// ==============================================================================
class ModernEditorView::ModernDebugTooltipWindow : public juce::TooltipWindow {
public:
    ModernDebugTooltipWindow(juce::Component* parent, std::function<bool()> isDebugFn)
        : juce::TooltipWindow(parent, 100), isDebugActive(std::move(isDebugFn)) {
        setOpaque(false);
        // A child TooltipWindow normally participates in hit-testing. Near a
        // window edge its constrained bounds can overlap the inspected control
        // and consume the first click before mouseEnter has hidden the tip.
        setInterceptsMouseClicks(false, false);
        setWantsKeyboardFocus(false);
    }

    juce::String getTipFor(juce::Component& c) override {
        if (isDebugActive && isDebugActive()) {
            for (auto* comp = &c; comp != nullptr; comp = comp->getParentComponent()) {
                if (comp->getComponentID().isNotEmpty()) {
                    return ModernEditorView::getDebugHoverTextFor(*comp);
                }
            }
        }
        return juce::TooltipWindow::getTipFor(c);
    }

private:
    std::function<bool()> isDebugActive;
};

juce::String ModernEditorView::getDebugHoverTextFor(juce::Component& component) {
    const auto componentId = component.getComponentID();
    juce::String currentValue;

    if (auto* slider = dynamic_cast<juce::Slider*>(&component)) {
        currentValue = slider->getTextFromValue(slider->getValue()).trim();
    } else if (auto* toggle = dynamic_cast<juce::ToggleButton*>(&component)) {
        currentValue = toggle->getToggleState() ? "On" : "Off";
    } else if (auto* combo = dynamic_cast<juce::ComboBox*>(&component)) {
        currentValue = combo->getText().trim();
    } else if (auto* editor = dynamic_cast<juce::TextEditor*>(&component)) {
        currentValue = editor->getText().trim();
    }

    return currentValue.isNotEmpty() ? componentId + "  |  " + currentValue : componentId;
}

// ==============================================================================
// ModernDebugHighlightOverlay Implementation
// ==============================================================================
class ModernEditorView::ModernDebugHighlightOverlay : public juce::Component, private juce::Timer, public juce::KeyListener {
public:
    explicit ModernDebugHighlightOverlay(std::function<bool()> isDebugFn)
        : isDebugActive(std::move(isDebugFn)) {
        setInterceptsMouseClicks(false, false);
        setAlwaysOnTop(true);
        if (isDebugActive && isDebugActive()) {
            startTimerHz(40);
        }
    }

    void updateTimerState() {
        if (isDebugActive && isDebugActive()) {
            if (!isTimerRunning()) startTimerHz(40);
        } else {
            stopTimer();
            clearCurrentTarget();
        }
    }

    ~ModernDebugHighlightOverlay() override {
        stopTimer();
        if (auto* top = getTopLevelComponent()) {
            top->removeKeyListener(this);
        }
    }

    void clearCurrentTarget() {
        currentTarget = nullptr;
        hoverStartTimeMs = 0;
        hasCopiedForCurrentTarget = false;
        copiedId.clear();
        copiedFlash = 0;
        repaint();
    }

    void parentHierarchyChanged() override {
        if (auto* top = getTopLevelComponent()) {
            top->removeKeyListener(this);
            top->addKeyListener(this);
        }
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/) override {
        if (!isDebugActive || !isDebugActive())
            return false;

        if (key == juce::KeyPress('c', juce::ModifierKeys::ctrlModifier, 0) ||
            key == juce::KeyPress('C', juce::ModifierKeys::ctrlModifier, 0) ||
            key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0) ||
            key == juce::KeyPress('C', juce::ModifierKeys::commandModifier, 0)) {
            if (currentTarget != nullptr) {
                // Same text as the hover tooltip: component ID and current value
                juce::String compID = ModernEditorView::getDebugHoverTextFor(*currentTarget);
                if (compID.isNotEmpty()) {
                    juce::SystemClipboard::copyTextToClipboard(compID);
                    copiedId = compID;
                    copiedFlash = 45;
                    hasCopiedForCurrentTarget = true;
                    repaint();
                    return true;
                }
            }
        }
        return false;
    }

    void paint(juce::Graphics& g) override {
        if (!isDebugActive || !isDebugActive())
            return;

        if (currentTarget != nullptr && currentTarget->isShowing()) {
            auto targetBounds = getLocalArea(currentTarget.getComponent(), currentTarget->getLocalBounds());
            if (!targetBounds.isEmpty() && targetBounds.getWidth() >= 2 && targetBounds.getHeight() >= 2) {
                auto r = targetBounds.toFloat().expanded(1.5f);

                // Dark outer contrast border (shadow)
                g.setColour(juce::Colours::black.withAlpha(0.8f));
                g.drawRect(r.expanded(1.0f), 1.0f);

                // Crisp pure white 2.0px highlight frame (or green if copied)
                g.setColour((copiedFlash > 0) ? juce::Colour(0xff2ecc71) : juce::Colours::white);
                g.drawRect(r, 2.0f);

                // Hardware corner accent markers
                float markerSize = 4.0f;
                g.fillRect(juce::Rectangle<float>(r.getX() - 1.0f, r.getY() - 1.0f, markerSize, markerSize));
                g.fillRect(juce::Rectangle<float>(r.getRight() - markerSize + 1.0f, r.getY() - 1.0f, markerSize, markerSize));
                g.fillRect(juce::Rectangle<float>(r.getX() - 1.0f, r.getBottom() - markerSize + 1.0f, markerSize, markerSize));
                g.fillRect(juce::Rectangle<float>(r.getRight() - markerSize + 1.0f, r.getBottom() - markerSize + 1.0f, markerSize, markerSize));
            }
        }

        // Copied toast banner
        if (copiedFlash > 0 && copiedId.isNotEmpty()) {
            auto b = getLocalBounds().toFloat();
            auto toastRect = juce::Rectangle<float>(b.getCentreX() - 250.0f, 12.0f, 500.0f, 44.0f);
            g.setColour(juce::Colour(0xff121418));
            g.fillRect(toastRect);
            g.setColour(juce::Colour(0xff2ecc71));
            g.drawRect(toastRect, 2.0f);
            g.setFont(ModernFontManager::createFont("D-DIN", 15.0f, juce::Font::bold));
            g.setColour(juce::Colours::white);
            g.drawText("COPIED TO CLIPBOARD: " + copiedId, toastRect, juce::Justification::centred, false);
        }
    }

private:
    void timerCallback() override {
        if (auto* parent = getParentComponent()) {
            if (getBounds() != parent->getLocalBounds()) {
                setBounds(parent->getLocalBounds());
            }
        }

        if (copiedFlash > 0) {
            --copiedFlash;
            repaint();
        }

        if (!isDebugActive || !isDebugActive()) {
            if (currentTarget != nullptr) {
                clearCurrentTarget();
            }
            return;
        }

        const auto mouseSource = juce::Desktop::getInstance().getMainMouseSource();
        auto* underMouse = mouseSource.isTouch() ? nullptr : mouseSource.getComponentUnderMouse();
        juce::Component* validTarget = nullptr;

        if (underMouse != nullptr && underMouse != this) {
            for (auto* comp = underMouse; comp != nullptr; comp = comp->getParentComponent()) {
                if (comp->getComponentID().isNotEmpty()) {
                    validTarget = comp;
                    break;
                }
            }
        }

        if (currentTarget.getComponent() != validTarget) {
            currentTarget = validTarget;
            hoverStartTimeMs = juce::Time::getMillisecondCounter();
            hasCopiedForCurrentTarget = false;
            repaint();
        } else if (currentTarget != nullptr && !hasCopiedForCurrentTarget) {
            // 1.0-second hover delay before updating clipboard
            uint32_t elapsed = juce::Time::getMillisecondCounter() - hoverStartTimeMs;
            if (elapsed >= 1000) {
                juce::String compID = ModernEditorView::getDebugHoverTextFor(*currentTarget);
                if (compID.isNotEmpty()) {
                    juce::SystemClipboard::copyTextToClipboard(compID);
                    copiedId = compID;
                    copiedFlash = 45;
                    hasCopiedForCurrentTarget = true;
                    repaint();
                }
            }
        }
    }

    std::function<bool()> isDebugActive;
    juce::Component::SafePointer<juce::Component> currentTarget;
    uint32_t hoverStartTimeMs = 0;
    bool hasCopiedForCurrentTarget = false;
    juce::String copiedId;
    int copiedFlash = 0;
};

// ==============================================================================
// ModernEditorView Implementation
// ==============================================================================
ModernEditorView::ModernEditorView(SynthEngine& eng, OvercyclerAudioProcessor* p)
    : engine(eng), processor(p), tabContext{engine, processor, modernLnf} {
    tabContext.setContinuousParam = [this](continuousParameter_t cp, float potVal) { setContinuousParam(cp, potVal); };
    tabContext.setSteppedParam = [this](steppedParameter_t sp, uint8_t stepVal) { setSteppedParam(sp, stepVal); };
    tabContext.openWaveBrowser = [this](abx_t osc) {
        if (onOpenWaveBrowser) onOpenWaveBrowser(osc);
    };
    tabContext.refreshFromEngine = [this] { updateFromEngine(); };
    setLookAndFeel(&modernLnf);

    // Navigation Tab Titles without numbers, cleanly organized (6 tabs)
    tabBar.onTabSelected = [this](TabIndex tab) { selectTab(tab); };
    addAndMakeVisible(tabBar);

    addAndMakeVisible(oscillatorTab);
    addChildComponent(filterTab);
    addChildComponent(envTab);
    addChildComponent(lfoTab);
    addChildComponent(afxTab);
    addChildComponent(modMatrixTab);
    addChildComponent(settingsTab);
    addChildComponent(paletteSaveModal);
    addChildComponent(colorPickerModal);

    oscillatorTab.setup();
    filterTab.setup();
    envTab.setup();
    lfoTab.setup();
    afxTab.setup();
    modMatrixTab.setup();
    settingsTab.setup();
    setupComponentIDs();
    settingsTab.loadSkinConfig();

    highlightOverlay = std::make_unique<ModernDebugHighlightOverlay>([this]() {
        return settingsTab.isDebugModeEnabled();
    });

    tooltipWindow = std::make_unique<ModernDebugTooltipWindow>(this, [this]() {
        return settingsTab.isDebugModeEnabled();
    });
    tooltipWindow->setLookAndFeel(&modernLnf);

    selectTab(TabIndex::Osc);
    updateFromEngine();
    startTimerHz(30);
}

ModernEditorView::~ModernEditorView() {
    stopTimer();
    if (highlightOverlay != nullptr) {
        highlightOverlay.reset();
    }
    if (tooltipWindow != nullptr) {
        tooltipWindow->setLookAndFeel(nullptr);
        tooltipWindow.reset();
    }
    setLookAndFeel(nullptr);
}

void ModernEditorView::parentHierarchyChanged() {
    juce::Component::parentHierarchyChanged();
    if (auto* top = getTopLevelComponent()) {
        if (tooltipWindow != nullptr && tooltipWindow->getParentComponent() != top) {
            top->addChildComponent(tooltipWindow.get());
        }
        if (highlightOverlay != nullptr && highlightOverlay->getParentComponent() != top) {
            top->addChildComponent(highlightOverlay.get());
            highlightOverlay->setBounds(top->getLocalBounds());
            highlightOverlay->setVisible(true);
            highlightOverlay->toFront(false);
        }
    }
}

void ModernEditorView::setContinuousParam(continuousParameter_t cp, float potVal) {
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (processor) {
        processor->setContinuousParamFromUI(cp, potVal);
        return;
    }
#endif
    uint16_t u16 = (uint16_t)scan_potTo16bits((int)std::round(potVal));
    engine.setContinuousParam(cp, u16);
}

void ModernEditorView::setSteppedParam(steppedParameter_t sp, uint8_t stepVal) {
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (processor) {
        processor->setSteppedParamFromUI(sp, stepVal);
        return;
    }
#endif
    engine.setSteppedParam(sp, stepVal);
}

void ModernEditorView::selectTab(int tab) {
    tab = juce::jlimit(0, static_cast<int>(TabIndex::Count) - 1, tab);
    currentTab = static_cast<TabIndex>(tab);
    if (highlightOverlay) highlightOverlay->clearCurrentTarget();
    tabBar.setSelectedTab(currentTab);

    oscillatorTab.setVisible(currentTab == TabIndex::Osc);
    filterTab.setVisible(currentTab == TabIndex::FilterVca);
    envTab.setVisible(currentTab == TabIndex::Envelopes);
    lfoTab.setVisible(currentTab == TabIndex::LfoArp);
    afxTab.setVisible(currentTab == TabIndex::Afx);
    modMatrixTab.setVisible(currentTab == TabIndex::ModMatrix);
    settingsTab.setVisible(currentTab == TabIndex::Settings);
}

void ModernEditorView::selectTab(TabIndex tab) {
    selectTab(static_cast<int>(tab));
}

int ModernEditorView::getSelectedTabIndex() const {
    return (int)currentTab;
}

void ModernEditorView::setTheme(const ModernTheme& theme) {
    modernLnf.setTheme(theme);
    settingsTab.themeApplied(theme);
    paletteSaveModal.setAccentColour(theme.accent);
    colorPickerModal.setAccentColour(theme.accent);
    sendLookAndFeelChange();
    repaint();
    if (onThemeChanged) onThemeChanged(theme);
}

void ModernEditorView::setFontFamily(const juce::String& familyName) {
    modernLnf.setFontFamily(familyName);
    sendLookAndFeelChange();
    repaint();
}

void ModernEditorView::setFontScale(float scale) {
    modernLnf.setFontScale(scale);
    sendLookAndFeelChange();
    repaint();
}

// ------------------------------------------------------------------------------
// SettingsTab::Host: editor-level services driven by the settings page
// ------------------------------------------------------------------------------
void ModernEditorView::applyTheme(const ModernTheme& theme) { setTheme(theme); }
void ModernEditorView::applyFontFamily(const juce::String& familyName) { setFontFamily(familyName); }
void ModernEditorView::applyFontScale(float scale) { setFontScale(scale); }

void ModernEditorView::windowScaleChanged(float scale) {
    if (onWindowScaleChanged) onWindowScaleChanged(scale);
}

void ModernEditorView::switchToClassicSkin() {
    if (onSkinModeChanged) onSkinModeChanged(false);
}

void ModernEditorView::debugModeChanged(bool enabled) {
    if (highlightOverlay) highlightOverlay->updateTimerState();
    if (tooltipWindow && !enabled) tooltipWindow->hideTip();
}

void ModernEditorView::showColourPicker(juce::Colour initialColour, const juce::String& roleTitle,
                                        std::function<void(juce::Colour)> onColourChanged,
                                        std::function<void(juce::Colour)> onApply) {
    colorPickerModal.show(initialColour, roleTitle, std::move(onColourChanged), std::move(onApply));
}

void ModernEditorView::showPaletteSaveDialog(const juce::String& initialName,
                                             std::function<void(const juce::String&)> onSave) {
    paletteSaveModal.show(initialName, std::move(onSave));
}

void ModernEditorView::setupComponentIDs() {
    // Navigation Tabs
    tabBar.getButton(TabIndex::Osc).setComponentID("tabButtons[TabOsc]");
    tabBar.getButton(TabIndex::FilterVca).setComponentID("tabButtons[TabFilterVca]");
    tabBar.getButton(TabIndex::Envelopes).setComponentID("tabButtons[TabEnvelopes]");
    tabBar.getButton(TabIndex::LfoArp).setComponentID("tabButtons[TabLfoArp]");
    tabBar.getButton(TabIndex::Afx).setComponentID("tabButtons[TabAfx]");
    tabBar.getButton(TabIndex::ModMatrix).setComponentID("tabButtons[TabModMatrix]");
    tabBar.getButton(TabIndex::Settings).setComponentID("tabButtons[TabSettings]");
    colorPickerModal.setComponentID("colorPickerModal");
}

void ModernEditorView::updateFromEngine() {
    oscillatorTab.updateFromEngine();
    filterTab.updateFromEngine();
    envTab.updateFromEngine();
    lfoTab.updateFromEngine();
    afxTab.updateFromEngine();
    modMatrixTab.updateFromEngine();
    settingsTab.updateFromEngine();
}

void ModernEditorView::timerCallback() {
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (isVisible() && processor && processor->checkAndResetHostParamsChanged()) {
        updateFromEngine();
    }
#endif

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        float rawLvl = (float)engine.getVoiceAmpLevel(v) / 65535.0f;
        voiceLevels[v] = voiceLevels[v] * 0.65f + rawLvl * 0.35f;
    }

    if (auto* meter = filterTab.getVoiceMeterPanel()) {
        meter->updateLevels(voiceLevels);
    }

    // Animate LFO preview traces
    lfoTab.advancePreviewAnimation();

    // Targeted repaint of only the top-right LED voice indicators rather than the entire 1100x700 window
    int meterRightMargin = 20;
    int meterW = 152;
    int meterX = getWidth() - meterW - meterRightMargin;
    repaint(meterX - 60, 0, meterW + 80, 45);
}

void ModernEditorView::paint(juce::Graphics& g) {
    auto theme = modernLnf.getTheme();
    g.fillAll(theme.windowBg);

    // Draw active voice LEDs in top right (Sharp rectangular styling - strictly no rounded corners)
    int meterRightMargin = 20;
    int meterW = 152;
    int meterX = getWidth() - meterW - meterRightMargin;
    int meterY = 15;
    g.setFont(modernLnf.getCustomFont(10.0f, juce::Font::bold));
    g.setColour(theme.textMuted);
    g.drawText("VOICES:", meterX - 54, meterY - 1, 48, 14, juce::Justification::right, false);

    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        float lvl = std::clamp(voiceLevels[v], 0.0f, 1.0f);
        juce::Colour ledCol = theme.cardBg.interpolatedWith(theme.accent, lvl);
        float vx = (float)(meterX + v * 28);

        // Sharp square LED
        g.setColour(ledCol);
        g.fillRect(vx, (float)meterY, 12.0f, 12.0f);
        g.setColour(lvl > 0.1f ? theme.accent : theme.cardBorder);
        g.drawRect(vx, (float)meterY, 12.0f, 12.0f, 1.0f);

        g.setColour(theme.textBody);
        g.drawText(juce::String(v + 1), (int)vx, meterY + 13, 12, 12, juce::Justification::centred, false);
    }
}

void ModernEditorView::resized() {
    const int tabH = 24;
    tabBar.setBounds(0, 0, getWidth(), tabH + 28);

    auto tabBounds = getLocalBounds().withTrimmedTop(52).reduced(16, 12);
    oscillatorTab.setBounds(tabBounds);
    filterTab.setBounds(tabBounds);
    envTab.setBounds(tabBounds);
    lfoTab.setBounds(tabBounds);
    afxTab.setBounds(tabBounds);
    modMatrixTab.setBounds(tabBounds);
    settingsTab.setBounds(tabBounds);

    paletteSaveModal.setBounds(getLocalBounds());
    paletteSaveModal.toFront(true);
    colorPickerModal.setBounds(getLocalBounds());
    colorPickerModal.toFront(true);
}
