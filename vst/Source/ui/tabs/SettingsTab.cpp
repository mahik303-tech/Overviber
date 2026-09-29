#include "SettingsTab.h"

#include "../../data/OverviberPaths.h"
#include <algorithm>

namespace {
// Confirmation text on a button, reset after a moment unless the button is
// gone by then (window closed, skin switched).
void resetButtonTextLater(juce::Button& button, const juce::String& text) {
    juce::Timer::callAfterDelay(1500, [safe = juce::Component::SafePointer<juce::Button>(&button), text]() {
        if (safe != nullptr) safe->setButtonText(text);
    });
}
}

SettingsTab::SettingsTab(ModernTabContext& context, Host& editorHost)
    : ModernTabModule(context), host(editorHost) {}

void SettingsTab::setup() {
    // The cards sit in an invisible frame that scrolls vertically once the
    // settings outgrow the tab.
    viewport.setViewedComponent(&scrollContent, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    scrollContent.addAndMakeVisible(themeCard);
    themeCard.toBack();
    scrollContent.addAndMakeVisible(debugCard);
    debugCard.toBack();
    scrollContent.addAndMakeVisible(behaviourCard);
    behaviourCard.toBack();

    createControllerToggles();
    createThemeControls();
    createTypographyControls();
    createWindowControls();
    createDebugControls();
    createBehaviourControls();

    assignComponentIDs();
}

// A radio group that sets a stepped parameter to the toggle's index.
void SettingsTab::createStepToggles(std::unique_ptr<juce::ToggleButton>* toggles, const char* const* names, int count,
                                    int radioGroup, steppedParameter_t sp) {
    for (int i = 0; i < count; ++i) {
        toggles[i] = createToggle(names[i]);
        toggles[i]->setRadioGroupId(radioGroup);
        toggles[i]->onClick = [this, sp, i]() { setSteppedParam(sp, (uint8_t)i); };
        scrollContent.addAndMakeVisible(*toggles[i]);
    }
}

// Muted explanation text next to a setting.
void SettingsTab::setupInfoLabel(juce::Label& label, const juce::String& text) {
    label.setText(text, juce::dontSendNotification);
    label.setFont(ModernFontManager::createFont("D-DIN", 11.0f, juce::Font::plain));
    label.setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
    label.setJustificationType(juce::Justification::centredLeft);
    scrollContent.addAndMakeVisible(label);
}

// An editor setting (skin_config.conf): applied and saved on click.
std::unique_ptr<juce::ToggleButton> SettingsTab::createSettingToggle(const juce::String& text, bool state,
                                                                     std::function<void(bool)> apply) {
    auto toggle = createToggle(text);
    toggle->setToggleState(state, juce::dontSendNotification);
    auto* raw = toggle.get();
    toggle->onClick = [this, raw, apply]() {
        apply(raw->getToggleState());
        saveSkinConfig();
    };
    scrollContent.addAndMakeVisible(*toggle);
    return toggle;
}

// A 0..1 editor setting shown as 0..100 % (pot 0..999), saved on change.
std::unique_ptr<juce::Slider> SettingsTab::createSettingKnob(const juce::String& name, float value, const juce::String& caption,
                                                             std::unique_ptr<juce::Label>& label, std::function<void(float)> apply) {
    auto knob = createKnob(name, 0, 999, 0, KnobMode::Percent);
    knob->setValue(std::round(value * 999.0f), juce::dontSendNotification);
    knob->updateText();
    auto* raw = knob.get();
    knob->onValueChange = [this, raw, apply]() {
        apply((float)raw->getValue() / 999.0f);
        saveSkinConfig();
    };
    addPageKnob(*knob);
    label = createLabel(caption, scrollContent);
    return knob;
}

// Hue (0..360 degrees) or saturation / brightness (0..100 %) of the
// palette role being edited.
std::unique_ptr<juce::Slider> SettingsTab::createColourKnob(const juce::String& name, bool hue, double init,
                                                            const juce::String& caption, std::unique_ptr<juce::Label>& label) {
    const double max = hue ? 360.0 : 100.0;
    auto knob = createKnob(name, 0.0, max, init, KnobMode::Raw);
    knob->setRange(0.0, max, hue ? 0.5 : 0.1);
    knob->textFromValueFunction = [hue](double val) -> juce::String {
        return juce::String(val, 1) + (hue ? "°" : " %");
    };
    knob->valueFromTextFunction = [hue, max](const juce::String& text) -> double {
        const juce::String number = hue ? text.replace("°", "").replace("deg", "") : text.replace("%", "");
        return std::clamp(number.trim().getDoubleValue(), 0.0, max);
    };
    knob->updateText();
    knob->onValueChange = [this]() { applyColorToActiveRole(); };
    addPageKnob(*knob);
    label = createLabel(caption, scrollContent);
    return knob;
}

// A knob on the scrolling page: a touch drag on it turns the knob only.
void SettingsTab::addPageKnob(juce::Slider& knob) {
    knob.setViewportIgnoreDragFlag(true);
    scrollContent.addAndMakeVisible(knob);
    pageKnobs.push_back(&knob);
}

// A colour from the picker or the swatch into the hue/saturation/brightness knobs.
void SettingsTab::setColourKnobs(juce::Colour c) {
    if (customHueKnob) customHueKnob->setValue(c.getHue() * 360.0f, juce::dontSendNotification);
    if (customSatKnob) customSatKnob->setValue(c.getSaturation() * 100.0f, juce::dontSendNotification);
    if (customBriKnob) customBriKnob->setValue(c.getBrightness() * 100.0f, juce::dontSendNotification);
    applyColorToActiveRole();
}

// MPE, timbre target and release velocity
void SettingsTab::createControllerToggles() {
    static const char* const timbreTargetNames[7] = { "Off", "Pitch", "Cutoff", "Volume", "WaveMod", "LFO 1", "LFO 2" };
    static const char* const mpeModeNames[3] = { "Off (Std MIDI)", "MPE 6-Voice (Ch 2-7)", "MPE Full (Ch 2-15)" };
    static const char* const mpeBendRangeNames[5] = { "+/-2 st", "+/-12 st", "+/-24 st", "+/-48 st", "+/-96 st" };
    static const char* const relVelNames[4] = { "Off", "Low", "Mid", "High" };
    createStepToggles(timbreTargetToggles, timbreTargetNames, 7, 1107, spTimbreTarget);
    createStepToggles(mpeModeToggles, mpeModeNames, 3, 1108, spMPEMode);
    createStepToggles(mpeBendRangeToggles, mpeBendRangeNames, 5, 1109, spMPEPitchBendRange);
    createStepToggles(releaseVelocityToggles, relVelNames, 4, 1110, spReleaseVelocityAmt);
}

// Skin & palette: presets and user palettes, the role strip, HSB knobs,
// the colour picker swatch and saving a palette.
void SettingsTab::createThemeControls() {
    loadUserPalettes();
    refreshThemePresetCombo();
    themePresetCombo.setSelectedId(1, juce::dontSendNotification);
    themePresetCombo.onChange = [this]() {
        int id = themePresetCombo.getSelectedId();
        auto presetThemes = ModernTheme::getPresetThemes();
        if (id >= 1 && id <= (int)presetThemes.size()) customTheme = presetThemes[id - 1];
        else if (id >= 101 && id <= 100 + (int)userThemes.size()) customTheme = userThemes[id - 101];
        else if (id != 100) return;
        host.applyTheme(customTheme);
        swatchStrip.setTheme(customTheme);
        updateRoleColorInSliders();
        saveSkinConfig();
    };
    scrollContent.addAndMakeVisible(themePresetCombo);

    // Clickable Palette Swatch Strip (7 distinct roles - direct modern selection)
    swatchStrip.onRoleSelected = [this](int r) {
        if (r >= 0 && r < ModernTheme::NumColorRoles) {
            currentEditingRole = r;
            updateRoleColorInSliders();
        }
    };
    scrollContent.addAndMakeVisible(swatchStrip);

    customHueKnob = createColourKnob("Hue", true, 187.0, "COLOR HUE", customHueLabel);
    customSatKnob = createColourKnob("Sat", false, 85.0, "SATURATION", customSatLabel);
    customBriKnob = createColourKnob("Bri", false, 80.0, "BRIGHTNESS", customBriLabel);

    swatchButton.setSwatchColour(ModernTheme::getPresetThemes()[0].accent);
    swatchButton.onOpenColorPicker = [this]() {
        juce::String roleName = ModernTheme::getRoleName(currentEditingRole);
        host.showColourPicker(swatchButton.getSwatchColour(), roleName,
            [this](juce::Colour c) { setColourKnobs(c); },
            [this](juce::Colour c) {
                swatchButton.setSwatchColour(c);
                saveSkinConfig();
            });
    };
    swatchButton.onColourChanged = [this](juce::Colour c) { setColourKnobs(c); };
    scrollContent.addAndMakeVisible(swatchButton);

    // Palette Saving via Modal Dialog
    savePaletteBtn.setButtonText("Save Palette As...");
    savePaletteBtn.onClick = [this]() {
        juce::String initialName = customTheme.name;
        if (initialName.isEmpty() || initialName == "Custom User Palette") {
            initialName = "User Palette " + juce::String((int)userThemes.size() + 1);
        }
        host.showPaletteSaveDialog(initialName, [this](const juce::String& name) {
            customTheme.name = name;

            int existingIdx = -1;
            for (size_t i = 0; i < userThemes.size(); ++i) {
                if (userThemes[i].name.equalsIgnoreCase(name)) {
                    existingIdx = (int)i;
                    break;
                }
            }

            if (existingIdx >= 0) {
                userThemes[existingIdx] = customTheme;
            } else {
                userThemes.push_back(customTheme);
                existingIdx = (int)userThemes.size() - 1;
            }

            saveUserPalettes();
            refreshThemePresetCombo();
            themePresetCombo.setSelectedId(101 + existingIdx, juce::dontSendNotification);

            savePaletteBtn.setButtonText("Palette Saved!");
            resetButtonTextLater(savePaletteBtn, "Save Palette As...");

            saveSkinConfig();
        });
    };
    scrollContent.addAndMakeVisible(savePaletteBtn);
}

// Typography and saving the appearance as the startup default
void SettingsTab::createTypographyControls() {
    auto curatedFonts = ModernFontManager::getCuratedFonts();
    for (size_t i = 0; i < curatedFonts.size(); ++i) {
        fontSelectorCombo.addItem(curatedFonts[i].displayName, (int)i + 1);
    }
    fontSelectorCombo.setSelectedId(1, juce::dontSendNotification);
    fontSelectorCombo.onChange = [this]() {
        int idx = fontSelectorCombo.getSelectedId() - 1;
        auto curated = ModernFontManager::getCuratedFonts();
        if (idx >= 0 && idx < (int)curated.size()) {
            host.applyFontFamily(curated[idx].fontName);
            saveSkinConfig();
        }
    };
    scrollContent.addAndMakeVisible(fontSelectorCombo);

    fontScaleCombo.addItem("85% (Compact)", 1);
    fontScaleCombo.addItem("90%", 2);
    fontScaleCombo.addItem("100% (Standard)", 3);
    fontScaleCombo.addItem("110%", 4);
    fontScaleCombo.addItem("120% (Large)", 5);
    fontScaleCombo.setSelectedId(3, juce::dontSendNotification);
    fontScaleCombo.onChange = [this]() {
        float scales[] = { 0.85f, 0.90f, 1.0f, 1.10f, 1.20f };
        int idx = fontScaleCombo.getSelectedId() - 1;
        if (idx >= 0 && idx < 5) {
            host.applyFontScale(scales[idx]);
            saveSkinConfig();
        }
    };
    scrollContent.addAndMakeVisible(fontScaleCombo);

    setupInfoLabel(defaultInfoLabel, "Save current theme palette, font and window scale as permanent startup default:");
    saveDefaultBtn.setButtonText("Set as Default");
    saveDefaultBtn.onClick = [this]() {
        saveSkinConfig();
        saveDefaultBtn.setButtonText("Default Saved!");
        resetButtonTextLater(saveDefaultBtn, "Set as Default");
    };
    scrollContent.addAndMakeVisible(saveDefaultBtn);
}

// Skin switch and window size
void SettingsTab::createWindowControls() {
    skinSwitchBtn.setButtonText("SWITCH TO CLASSIC SKIN");
    skinSwitchBtn.onClick = [this]() { host.switchToClassicSkin(); };
    scrollContent.addAndMakeVisible(skinSwitchBtn);

    windowScaleCombo.addItem("Window Size: 87% (960 x 610)", 1);
    windowScaleCombo.addItem("Window Size: 100% (1100 x 700)", 2);
    windowScaleCombo.addItem("Window Size: 115% (1265 x 805)", 3);
    windowScaleCombo.addItem("Window Size: 125% (1375 x 875)", 4);
    windowScaleCombo.addItem("Window Size: 140% (1540 x 980)", 5);
    windowScaleCombo.addItem("Window Size: 160% (1760 x 1120)", 6);
    windowScaleCombo.setSelectedId(2, juce::dontSendNotification);
    windowScaleCombo.onChange = [this]() {
        float scales[] = { 0.8727f, 1.0f, 1.15f, 1.25f, 1.40f, 1.60f };
        int idx = windowScaleCombo.getSelectedId() - 1;
        if (idx >= 0 && idx < 6) {
            savedWindowScale = scales[idx];
            host.windowScaleChanged(scales[idx]);
            saveSkinConfig();
        }
    };
    scrollContent.addAndMakeVisible(windowScaleCombo);
}

// Debug card: code names on hover, the current state for test scenarios
void SettingsTab::createDebugControls() {
    debugModeToggle = createToggle("DEBUG MODE (SHOW CODE NAMES ON HOVER)");
    debugModeToggle->setToggleState(debugMode, juce::dontSendNotification);
    debugModeToggle->onClick = [this]() { setDebugMode(debugModeToggle->getToggleState()); };
    scrollContent.addAndMakeVisible(*debugModeToggle);
    setupInfoLabel(debugInfoLabel, "Displays program code element names as mouse-over text for exact identification.");

    copyStateBtn.onClick = [this]() {
        const auto text = describeState(model);
        juce::SystemClipboard::copyTextToClipboard(text);
        copyStateBtn.setFlashText("COPIED", 1200);
        copyStateInfoLabel.setText("Copied " + juce::String(text.getNumBytesAsUTF8()) + " characters: preset parameters, mixer and routing.",
                                   juce::dontSendNotification);
    };
    scrollContent.addAndMakeVisible(copyStateBtn);
    setupInfoLabel(copyStateInfoLabel, "Copies all parameter values of the current sound (preset file format) plus mixer and routing.");
}

// Editor behaviour: filter family switch, spectrum displays and their opacity
void SettingsTab::createBehaviourControls() {
    const char* switchNames[2] = { "SAME FILTER (FOR COMPARING)", "LAST CHOSEN FILTER OF THE FAMILY" };
    for (int i = 0; i < 2; ++i) {
        filterSwitchToggles[i] = createToggle(switchNames[i]);
        filterSwitchToggles[i]->setRadioGroupId(1901);
        filterSwitchToggles[i]->onClick = [this, i]() {
            setFilterSwitchMatch(i == 0);
            saveSkinConfig();
        };
        scrollContent.addAndMakeVisible(*filterSwitchToggles[i]);
    }
    filterSwitchToggles[0]->setToggleState(true, juce::dontSendNotification);
    setupInfoLabel(filterSwitchInfoLabel, "Filter families (Ladder, SEM, Ripples, Shelves): the entry preselected when switching.");

    retroSpectrumToggle = createSettingToggle("8-BIT SPECTRUM (FILTER, ENV, LFO)", model.isRetroSpectrumShown(),
                                              [this](bool on) { model.setRetroSpectrumShown(on); });
    spectrumWaterfallToggle = createSettingToggle("SPECTRUM WATERFALL (FILTER)", model.isSpectrumWaterfallShown(),
                                                  [this](bool on) { model.setSpectrumWaterfallShown(on); });
    curvesWaterfallToggle = createSettingToggle("SPECTRUM WATERFALL (ENV, LFO)", model.spectrumWaterfallCurvesShown,
                                                [this](bool on) { model.spectrumWaterfallCurvesShown = on; });
    setupInfoLabel(spectrumInfoLabel, "Spectrum of the master output behind the filter, envelope and LFO curves.");

    // Opacity of the spectrum displays (0..999 = 0..100 %)
    retroFilterOpacityKnob = createSettingKnob("RetroFilterOpacity", model.retroOpacityFilter, "8-BIT FILTER",
                                               retroFilterOpacityLabel, [this](float v) { model.retroOpacityFilter = v; });
    retroCurvesOpacityKnob = createSettingKnob("RetroCurvesOpacity", model.retroOpacityCurves, "8-BIT ENV/LFO",
                                               retroCurvesOpacityLabel, [this](float v) { model.retroOpacityCurves = v; });
    waterfallOpacityKnob = createSettingKnob("WaterfallOpacity", model.waterfallOpacity, "WATERFALL",
                                             waterfallOpacityLabel, [this](float v) { model.waterfallOpacity = v; });
    retroRandomKnob = createSettingKnob("RetroRandom", model.retroRandomness, "8-BIT RANDOM",
                                        retroRandomLabel, [this](float v) { model.retroRandomness = v; });
}

void SettingsTab::assignComponentIDs() {
    themeCard.setComponentID("themeCard");

    // Tab 6: Settings & Controllers
    for (int i = 0; i < 7; ++i) {
        if (timbreTargetToggles[i]) timbreTargetToggles[i]->setComponentID("timbreTargetToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 3; ++i) {
        if (mpeModeToggles[i]) mpeModeToggles[i]->setComponentID("mpeModeToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 5; ++i) {
        if (mpeBendRangeToggles[i]) mpeBendRangeToggles[i]->setComponentID("mpeBendRangeToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 4; ++i) {
        if (releaseVelocityToggles[i]) releaseVelocityToggles[i]->setComponentID("releaseVelocityToggle[" + juce::String(i) + "]");
    }
    themePresetCombo.setComponentID("themePresetCombo");
    swatchStrip.setComponentID("swatchStrip");
    swatchButton.setComponentID("swatchButton");
    if (customHueKnob) customHueKnob->setComponentID("customHueKnob");
    if (customHueLabel) customHueLabel->setComponentID("customHueLabel");
    if (customSatKnob) customSatKnob->setComponentID("customSatKnob");
    if (customSatLabel) customSatLabel->setComponentID("customSatLabel");
    if (customBriKnob) customBriKnob->setComponentID("customBriKnob");
    if (customBriLabel) customBriLabel->setComponentID("customBriLabel");
    savePaletteBtn.setComponentID("savePaletteBtn");
    fontSelectorCombo.setComponentID("fontSelectorCombo");
    fontScaleCombo.setComponentID("fontScaleCombo");
    saveDefaultBtn.setComponentID("saveDefaultBtn");
    skinSwitchBtn.setComponentID("skinSwitchBtn");
    windowScaleCombo.setComponentID("windowScaleCombo");
    if (debugModeToggle) debugModeToggle->setComponentID("debugModeToggle");
    debugInfoLabel.setComponentID("debugInfoLabel");
    debugCard.setComponentID("debugCard");
    copyStateBtn.setComponentID("copyStateBtn");
    copyStateInfoLabel.setComponentID("copyStateInfoLabel");
    behaviourCard.setComponentID("behaviourCard");
    for (int i = 0; i < 2; ++i)
        if (filterSwitchToggles[i]) filterSwitchToggles[i]->setComponentID("filterSwitchToggle[" + juce::String(i) + "]");
    filterSwitchInfoLabel.setComponentID("filterSwitchInfoLabel");
    if (retroSpectrumToggle) retroSpectrumToggle->setComponentID("retroSpectrumToggle");
    if (spectrumWaterfallToggle) spectrumWaterfallToggle->setComponentID("spectrumWaterfallToggle");
    if (curvesWaterfallToggle) curvesWaterfallToggle->setComponentID("curvesWaterfallToggle");
    spectrumInfoLabel.setComponentID("spectrumInfoLabel");
    if (retroFilterOpacityKnob) retroFilterOpacityKnob->setComponentID("retroFilterOpacityKnob");
    if (retroCurvesOpacityKnob) retroCurvesOpacityKnob->setComponentID("retroCurvesOpacityKnob");
    if (waterfallOpacityKnob) waterfallOpacityKnob->setComponentID("waterfallOpacityKnob");
    if (retroRandomKnob) retroRandomKnob->setComponentID("retroRandomKnob");
}

SettingsTab::ColorSwatchButton::ColorSwatchButton() : juce::Button("swatchColorButton") {}
SettingsTab::ColorSwatchButton::~ColorSwatchButton() = default;

void SettingsTab::ColorSwatchButton::clicked() {
    if (onOpenColorPicker) {
        onOpenColorPicker();
    }
}

void SettingsTab::ColorSwatchButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto b = getLocalBounds().toFloat();
    g.setColour(colour);
    g.fillRect(b);

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown) {
        g.setColour(juce::Colours::white);
        g.drawRect(b, 2.0f);
    } else {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawRect(b, 1.0f);
    }
}

void SettingsTab::applyColorToActiveRole() {
    if (!customHueKnob || !customSatKnob || !customBriKnob) return;
    float h = (float)customHueKnob->getValue() / 360.0f;
    float s = (float)customSatKnob->getValue() / 100.0f;
    float b = (float)customBriKnob->getValue() / 100.0f;
    juce::Colour col = juce::Colour::fromHSV(h, s, b, 1.0f);

    customTheme.setColorForRole(currentEditingRole, col);
    customTheme.name = "Custom User Palette";
    host.applyTheme(customTheme);
    swatchButton.setSwatchColour(col);
    swatchStrip.setTheme(customTheme);

    themePresetCombo.setSelectedId(100, juce::dontSendNotification);
    saveSkinConfig();
}

void SettingsTab::updateRoleColorInSliders() {
    juce::Colour c = customTheme.getColorForRole(currentEditingRole);
    if (customHueKnob) customHueKnob->setValue(c.getHue() * 360.0f, juce::dontSendNotification);
    if (customSatKnob) customSatKnob->setValue(c.getSaturation() * 100.0f, juce::dontSendNotification);
    if (customBriKnob) customBriKnob->setValue(c.getBrightness() * 100.0f, juce::dontSendNotification);
    swatchButton.setSwatchColour(c);

    if (customHueLabel) {
        switch (currentEditingRole) {
            case ModernTheme::RoleAccent:     customHueLabel->setText("ACCENT HUE", juce::dontSendNotification); break;
            case ModernTheme::RoleWindowBg:   customHueLabel->setText("CHASSIS HUE", juce::dontSendNotification); break;
            case ModernTheme::RoleCardBg:     customHueLabel->setText("PANEL HUE", juce::dontSendNotification); break;
            case ModernTheme::RoleCardHeader: customHueLabel->setText("HEADER HUE", juce::dontSendNotification); break;
            case ModernTheme::RoleCardBorder: customHueLabel->setText("BORDER HUE", juce::dontSendNotification); break;
            case ModernTheme::RoleKnobs:      customHueLabel->setText("DIALS HUE", juce::dontSendNotification); break;
            case ModernTheme::RoleText:       customHueLabel->setText("TEXT HUE", juce::dontSendNotification); break;
            default:                          customHueLabel->setText("COLOR HUE", juce::dontSendNotification); break;
        }
    }
}

void SettingsTab::saveSkinConfig() {
    juce::File confDir = OverviberPaths::getAppConfigDirectory();
    confDir.createDirectory();
    juce::File confFile = confDir.getChildFile("skin_config.conf");

    juce::String content;
    content << "themeId=" << themePresetCombo.getSelectedId() << "\n";
    content << "customThemeName=" << customTheme.name << "\n";
    content << "fontId=" << fontSelectorCombo.getSelectedId() << "\n";
    content << "scaleId=" << fontScaleCombo.getSelectedId() << "\n";
    content << "windowScaleId=" << windowScaleCombo.getSelectedId() << "\n";
    content << "windowScale=" << juce::String(savedWindowScale, 4) << "\n";

    // Save full custom palette colors in hex
    content << "customAccent=" << customTheme.accent.toDisplayString(true) << "\n";
    content << "customWindow=" << customTheme.windowBg.toDisplayString(true) << "\n";
    content << "customCard=" << customTheme.cardBg.toDisplayString(true) << "\n";
    content << "customHeader=" << customTheme.cardHeader.toDisplayString(true) << "\n";
    content << "customBorder=" << customTheme.cardBorder.toDisplayString(true) << "\n";
    content << "customKnobs=" << customTheme.knobBodyTop.toDisplayString(true) << "\n";
    content << "customText=" << customTheme.textTitle.toDisplayString(true) << "\n";
    content << "debugMode=" << (debugMode ? "1" : "0") << "\n";
    content << "filterFamilySwitch=" << (filterSwitchMatch ? "same" : "last") << "\n";
    content << "spectrum8bit=" << (model.isRetroSpectrumShown() ? 1 : 0) << "\n";
    content << "spectrumWaterfall=" << (model.isSpectrumWaterfallShown() ? 1 : 0) << "\n";
    content << "spectrumWaterfallCurves=" << (model.spectrumWaterfallCurvesShown ? 1 : 0) << "\n";
    content << "spectrumOpacity=" << model.retroOpacityFilter << "," << model.retroOpacityCurves << ","
            << model.waterfallOpacity << "\n";
    content << "spectrumRandom=" << model.retroRandomness << "\n";

    confFile.replaceWithText(content);
 
    // Also mirror to local disk folder if present
    juce::File localDisk("disk");
    if (localDisk.isDirectory()) {
        localDisk.getChildFile("skin_config.conf").replaceWithText(content);
    }
}

void SettingsTab::loadSkinConfig() {
    loadUserPalettes();

    juce::File confFile = OverviberPaths::getAppConfigDirectory().getChildFile("skin_config.conf");
    if (!confFile.existsAsFile()) {
        auto factory = OverviberPaths::findFactoryDiskDirectory();
        if (factory.exists()) confFile = factory.getChildFile("skin_config.conf");
        if (!confFile.existsAsFile()) confFile = juce::File("disk/skin_config.conf");
    }

    // Initialize customTheme with default theme
    customTheme = ModernTheme::getPresetThemes()[0];
    customTheme.name = "Custom User Palette";

    int themeId = 1, fontId = 1, scaleId = 3, windowScaleId = 2;
    float winScale = 1.0f;

    if (confFile.existsAsFile()) {
        juce::StringArray lines;
        confFile.readLines(lines);

        for (const auto& line : lines) {
            if (line.startsWith("themeId=")) themeId = line.fromFirstOccurrenceOf("themeId=", false, false).getIntValue();
            else if (line.startsWith("customThemeName=")) customTheme.name = line.fromFirstOccurrenceOf("customThemeName=", false, false).trim();
            else if (line.startsWith("fontId=")) fontId = line.fromFirstOccurrenceOf("fontId=", false, false).getIntValue();
            else if (line.startsWith("scaleId=")) scaleId = line.fromFirstOccurrenceOf("scaleId=", false, false).getIntValue();
            else if (line.startsWith("windowScaleId=")) windowScaleId = line.fromFirstOccurrenceOf("windowScaleId=", false, false).getIntValue();
            else if (line.startsWith("windowScale=")) winScale = (float)line.fromFirstOccurrenceOf("windowScale=", false, false).getDoubleValue();
            else if (line.startsWith("customAccent=")) customTheme.setColorForRole(ModernTheme::RoleAccent, juce::Colour::fromString(line.fromFirstOccurrenceOf("customAccent=", false, false)));
            else if (line.startsWith("customWindow=")) customTheme.setColorForRole(ModernTheme::RoleWindowBg, juce::Colour::fromString(line.fromFirstOccurrenceOf("customWindow=", false, false)));
            else if (line.startsWith("customCard=")) customTheme.setColorForRole(ModernTheme::RoleCardBg, juce::Colour::fromString(line.fromFirstOccurrenceOf("customCard=", false, false)));
            else if (line.startsWith("customHeader=")) customTheme.setColorForRole(ModernTheme::RoleCardHeader, juce::Colour::fromString(line.fromFirstOccurrenceOf("customHeader=", false, false)));
            else if (line.startsWith("customBorder=")) customTheme.setColorForRole(ModernTheme::RoleCardBorder, juce::Colour::fromString(line.fromFirstOccurrenceOf("customBorder=", false, false)));
            else if (line.startsWith("customKnobs=")) customTheme.setColorForRole(ModernTheme::RoleKnobs, juce::Colour::fromString(line.fromFirstOccurrenceOf("customKnobs=", false, false)));
            else if (line.startsWith("customText=")) customTheme.setColorForRole(ModernTheme::RoleText, juce::Colour::fromString(line.fromFirstOccurrenceOf("customText=", false, false)));
            else if (line.startsWith("spectrumRandom=")) {
                model.retroRandomness = juce::jlimit(0.0f, 1.0f, line.fromFirstOccurrenceOf("=", false, false).getFloatValue());
                if (retroRandomKnob) retroRandomKnob->setValue(std::round(model.retroRandomness * 999.0f), juce::dontSendNotification);
            }
            else if (line.startsWith("spectrumOpacity=")) {
                juce::StringArray values;
                values.addTokens(line.fromFirstOccurrenceOf("=", false, false), ",", "");
                if (values.size() == 3) {
                    auto value = [&](int i) { return juce::jlimit(0.0f, 1.0f, values[i].getFloatValue()); };
                    model.retroOpacityFilter = value(0);
                    model.retroOpacityCurves = value(1);
                    model.waterfallOpacity = value(2);
                    if (retroFilterOpacityKnob) retroFilterOpacityKnob->setValue(std::round(model.retroOpacityFilter * 999.0f), juce::dontSendNotification);
                    if (retroCurvesOpacityKnob) retroCurvesOpacityKnob->setValue(std::round(model.retroOpacityCurves * 999.0f), juce::dontSendNotification);
                    if (waterfallOpacityKnob) waterfallOpacityKnob->setValue(std::round(model.waterfallOpacity * 999.0f), juce::dontSendNotification);
                }
            }
            else if (line.startsWith("spectrum8bit=")) {
                model.setRetroSpectrumShown(line.fromFirstOccurrenceOf("=", false, false).getIntValue() != 0);
                if (retroSpectrumToggle) retroSpectrumToggle->setToggleState(model.isRetroSpectrumShown(), juce::dontSendNotification);
            }
            else if (line.startsWith("spectrumWaterfallCurves=")) {
                model.spectrumWaterfallCurvesShown = line.fromFirstOccurrenceOf("=", false, false).getIntValue() != 0;
                if (curvesWaterfallToggle) curvesWaterfallToggle->setToggleState(model.spectrumWaterfallCurvesShown, juce::dontSendNotification);
            }
            else if (line.startsWith("spectrumWaterfall=")) {
                model.setSpectrumWaterfallShown(line.fromFirstOccurrenceOf("=", false, false).getIntValue() != 0);
                if (spectrumWaterfallToggle) spectrumWaterfallToggle->setToggleState(model.isSpectrumWaterfallShown(), juce::dontSendNotification);
            }
            else if (line.startsWith("filterFamilySwitch=")) {
                setFilterSwitchMatch(line.fromFirstOccurrenceOf("filterFamilySwitch=", false, false).trim() != "last");
            }
            else if (line.startsWith("debugMode=")) {
                debugMode = (line.fromFirstOccurrenceOf("debugMode=", false, false).getIntValue() != 0);
                if (debugModeToggle) debugModeToggle->setToggleState(debugMode, juce::dontSendNotification);
                host.debugModeChanged(debugMode);
            }
        }
    }

    savedWindowScale = (winScale >= 0.5f && winScale <= 2.5f) ? winScale : 1.0f;
    windowScaleCombo.setSelectedId(windowScaleId, juce::dontSendNotification);

    fontSelectorCombo.setSelectedId(fontId, juce::sendNotificationSync);
    fontScaleCombo.setSelectedId(scaleId, juce::sendNotificationSync);

    refreshThemePresetCombo();

    if (themeId >= 101 && themeId <= 100 + (int)userThemes.size()) {
        customTheme = userThemes[themeId - 101];
        host.applyTheme(customTheme);
        themePresetCombo.setSelectedId(themeId, juce::dontSendNotification);
    } else if (themeId == 100) {
        host.applyTheme(customTheme);
        themePresetCombo.setSelectedId(100, juce::dontSendNotification);
    } else {
        int clampedThemeId = std::clamp(themeId, 1, (int)ModernTheme::getPresetThemes().size());
        auto chosenPreset = ModernTheme::getPresetThemes()[clampedThemeId - 1];
        host.applyTheme(chosenPreset);
        themePresetCombo.setSelectedId(clampedThemeId, juce::dontSendNotification);
    }

    swatchStrip.setSelectedRole(currentEditingRole);
    updateRoleColorInSliders();
}

void SettingsTab::loadUserPalettes() {
    userThemes.clear();
    juce::File file = OverviberPaths::getAppConfigDirectory().getChildFile("user_palettes.conf");
    if (!file.existsAsFile()) {
        auto factory = OverviberPaths::findFactoryDiskDirectory();
        if (factory.exists()) file = factory.getChildFile("user_palettes.conf");
        if (!file.existsAsFile()) file = juce::File("disk/user_palettes.conf");
    }
    if (!file.existsAsFile()) return;

    juce::StringArray lines;
    file.readLines(lines);
    for (const auto& line : lines) {
        auto tokens = juce::StringArray::fromTokens(line, ";", "");
        if (tokens.size() >= 8) {
            ModernTheme ut = ModernTheme::getPresetThemes()[0];
            ut.name = tokens[0].trim();
            ut.description = "User Custom Palette";
            ut.setColorForRole(ModernTheme::RoleAccent,     juce::Colour::fromString(tokens[1]));
            ut.setColorForRole(ModernTheme::RoleWindowBg,   juce::Colour::fromString(tokens[2]));
            ut.setColorForRole(ModernTheme::RoleCardBg,     juce::Colour::fromString(tokens[3]));
            ut.setColorForRole(ModernTheme::RoleCardHeader, juce::Colour::fromString(tokens[4]));
            ut.setColorForRole(ModernTheme::RoleCardBorder, juce::Colour::fromString(tokens[5]));
            ut.setColorForRole(ModernTheme::RoleKnobs,      juce::Colour::fromString(tokens[6]));
            ut.setColorForRole(ModernTheme::RoleText,       juce::Colour::fromString(tokens[7]));
            userThemes.push_back(ut);
        }
    }
}

void SettingsTab::saveUserPalettes() {
    juce::File confDir = OverviberPaths::getAppConfigDirectory();
    confDir.createDirectory();
    juce::File file = confDir.getChildFile("user_palettes.conf");

    juce::String content;
    for (const auto& ut : userThemes) {
        content << ut.name << ";"
                << ut.accent.toDisplayString(true) << ";"
                << ut.windowBg.toDisplayString(true) << ";"
                << ut.cardBg.toDisplayString(true) << ";"
                << ut.cardHeader.toDisplayString(true) << ";"
                << ut.cardBorder.toDisplayString(true) << ";"
                << ut.knobBodyTop.toDisplayString(true) << ";"
                << ut.textTitle.toDisplayString(true) << "\n";
    }
    file.replaceWithText(content);

    juce::File localDisk("disk");
    if (localDisk.isDirectory()) {
        localDisk.getChildFile("user_palettes.conf").replaceWithText(content);
    }
}

void SettingsTab::refreshThemePresetCombo() {
    int prevSelectedId = themePresetCombo.getSelectedId();
    themePresetCombo.clear(juce::dontSendNotification);

    auto presetThemes = ModernTheme::getPresetThemes();
    for (size_t i = 0; i < presetThemes.size(); ++i) {
        themePresetCombo.addItem(presetThemes[i].name, (int)i + 1);
    }

    for (size_t u = 0; u < userThemes.size(); ++u) {
        themePresetCombo.addItem("[ User: " + userThemes[u].name + " ]", (int)(101 + u));
    }

    juce::String customLabel = "[ Custom: " + customTheme.name + " ]";
    themePresetCombo.addItem(customLabel, 100);

    if (prevSelectedId > 0) {
        themePresetCombo.setSelectedId(prevSelectedId, juce::dontSendNotification);
    }
}

void SettingsTab::setFilterSwitchMatch(bool match) {
    filterSwitchMatch = match;
    for (int i = 0; i < 2; ++i)
        if (filterSwitchToggles[i]) filterSwitchToggles[i]->setToggleState(match == (i == 0), juce::dontSendNotification);
    host.filterFamilySwitchChanged(match);
}

void SettingsTab::setDebugMode(bool enabled) {
    debugMode = enabled;
    if (debugModeToggle) debugModeToggle->setToggleState(enabled, juce::dontSendNotification);
    host.debugModeChanged(enabled);
    saveSkinConfig();
}

void SettingsTab::themeApplied(const ModernTheme& theme) {
    customTheme = theme;
    saveDefaultBtn.setAccentColour(theme.accent);
    savePaletteBtn.setAccentColour(theme.accent);
    skinSwitchBtn.setAccentColour(theme.accent);
    for (auto* label : { &defaultInfoLabel, &debugInfoLabel, &copyStateInfoLabel, &filterSwitchInfoLabel, &spectrumInfoLabel })
        label->setColour(juce::Label::textColourId, theme.textMuted);
    swatchStrip.setTheme(theme);
    swatchButton.setSwatchColour(theme.getColorForRole(currentEditingRole));
}

void SettingsTab::updateFromEngine() {
    const auto& preset = model.getCurrentPreset();

    // MPE & release velocity
    int tTarget = preset.steppedParams[spTimbreTarget];
    for (int i = 0; i < 7; ++i) {
        if (timbreTargetToggles[i]) timbreTargetToggles[i]->setToggleState(i == tTarget, juce::dontSendNotification);
    }
    int mpeMode = preset.steppedParams[spMPEMode];
    for (int i = 0; i < 3; ++i) {
        if (mpeModeToggles[i]) mpeModeToggles[i]->setToggleState(i == mpeMode, juce::dontSendNotification);
    }
    int mpeBend = preset.steppedParams[spMPEPitchBendRange];
    for (int i = 0; i < 5; ++i) {
        if (mpeBendRangeToggles[i]) mpeBendRangeToggles[i]->setToggleState(i == mpeBend, juce::dontSendNotification);
    }
    int relVel = preset.steppedParams[spReleaseVelocityAmt];
    for (int i = 0; i < 4; ++i) {
        if (releaseVelocityToggles[i]) releaseVelocityToggles[i]->setToggleState(i == relVel, juce::dontSendNotification);
    }
}

juce::String SettingsTab::describeState(SynthModel& model) {
    juce::String text;
    text << "# Overviber state for test scenarios\n"
         << "# Main preset in the preset file format (PresetManager::parsePresetString)\n";
    text << juce::String(model.getPresetManager().serializePresetToString(model.getCurrentPreset()));
    // The preset format stores continuous parameters in pot units (0..999);
    // the exact 16-bit values follow as comments, which the parser skips.
    text << "\n# Exact 16-bit values of the continuous parameters\n";
    for (int cp = 0; cp < cpCount; ++cp) {
        const char* name = PresetManager::getContinuousParamName(static_cast<continuousParameter_t>(cp));
        if (name != nullptr && *name != '\0')
            text << "# raw " << name << " = " << (int)model.getCurrentPreset().continuousParams[cp] << "\n";
    }
    text << "\n# Mixer (not part of a preset)\n";
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
        text << "voiceFader" << v << " = " << juce::String(model.getVoiceFader(v), 4) << "\n";
        text << "voicePan" << v << " = " << juce::String(model.getStoredVoicePan(v), 4)
             << (model.isVoicePanCustomized(v) ? " (customized)" : "") << "\n";
    }
    text << "masterMute = " << (model.isMasterMuted() ? 1 : 0) << "\n";
    text << "\n# Routing: parts 1-16 (enabled, MIDI channel, key range)\n";
    text << "customRouting = " << (model.usesCustomRouting() ? 1 : 0) << "\n";
    for (int part = 0; part < 16; ++part) {
        const auto& route = model.getPartRoute(part);
        text << "part" << (part + 1) << " = " << (int)route.enabled << ", " << (int)route.channel << ", "
             << (int)route.low << ".." << (int)route.high << "\n";
    }
    return text;
}

void SettingsTab::resized() {
    // The appearance card ends after the skin row; the debug card follows.
    constexpr int themeCardH = 304, cardGap = 12, debugCardH = 100, behaviourCardH = 140;
    constexpr int contentH = themeCardH + cardGap + debugCardH + cardGap + behaviourCardH;

    // Content as wide as the tab, less the scroll bar when it is needed.
    viewport.setBounds(getLocalBounds());
    const bool scrolls = contentH > getHeight();
    const int contentW = getWidth() - (scrolls ? viewport.getScrollBarThickness() : 0);
    scrollContent.setSize(contentW, contentH);
    // While the page scrolls, the mouse wheel scrolls it, also over a knob.
    for (auto* knob : pageKnobs) knob->setScrollWheelEnabled(!scrolls);
    const juce::Rectangle<int> tabBounds(contentW, contentH);

    themeCard.setBounds(0, 0, tabBounds.getWidth(), themeCardH);
    const int debugY = themeCardH + cardGap;
    debugCard.setBounds(0, debugY, tabBounds.getWidth(), debugCardH);
    debugCard.clearDividers();
    themeCard.clearDividers();

    int themeY = 0;

    // Row 1: Interactive Palette Swatch Strip with generous breathing space before and after
    int swatchW = tabBounds.getWidth() - 40;
    int swatchY = themeY + 40; // 18px space after header
    int swatchH = 32;
    swatchStrip.setBounds(20, swatchY, swatchW, swatchH);

    // Optical separation below swatch strip with generous spacing after swatchStrip
    int dividerY = 92;
    themeCard.addDivider(dividerY, "PALETTE PRESET & COLOR TUNING");

    // Row 2: Centered vertically with standard 55px knobs
    int knobSz = getStandardKnobSize();
    int knobY = themeY + dividerY + 14;

    int btnH = 28;
    int btnY = knobY + (knobSz - btnH) / 2;

    int btnW = 150;
    savePaletteBtn.setBounds(20, btnY, btnW, btnH);

    int comboW = 180;
    themePresetCombo.setBounds(20 + btnW + 10, btnY, comboW, btnH);

    int swatchBtnX = 20 + btnW + 10 + comboW + 10;
    swatchButton.setBounds(swatchBtnX, btnY, btnH, btnH);

    // Graphical separation: Vertical divider between Palette Controls and Color Tuning Knobs
    int sepX = swatchBtnX + btnH + 16;
    themeCard.addVerticalDivider(sepX, dividerY + 8, dividerY + 88);

    int knobStartX = sepX + 20;
    int knobSpacing = 115;

    layoutKnob(customHueKnob.get(), customHueLabel, knobStartX, knobY, knobSz);
    layoutKnob(customSatKnob.get(), customSatLabel, knobStartX + knobSpacing, knobY, knobSz);
    layoutKnob(customBriKnob.get(), customBriLabel, knobStartX + knobSpacing * 2, knobY, knobSz);

    // Row 3: Display & Typography
    int dispDividerY = 194;
    themeCard.addDivider(dispDividerY, "DISPLAY & TYPOGRAPHY");
    fontSelectorCombo.setBounds(20, themeY + dispDividerY + 12, 210, 26);
    fontScaleCombo.setBounds(240, themeY + dispDividerY + 12, 160, 26);
    windowScaleCombo.setBounds(410, themeY + dispDividerY + 12, 240, 26);

    // Row 4: Combined Section "DEFAULTS & INTERFACE SKIN"
    int skinDividerY = 250;
    themeCard.addDivider(skinDividerY, "STARTUP DEFAULTS & INTERFACE SKIN");
    saveDefaultBtn.setBounds(20, themeY + skinDividerY + 12, 140, 28);
    skinSwitchBtn.setBounds(170, themeY + skinDividerY + 12, 190, 28);
    defaultInfoLabel.setBounds(375, themeY + skinDividerY + 12, std::max(200, tabBounds.getWidth() - 390), 28);

    // Debug card: inspector switch, then the state copy for test scenarios
    const int labelX = 340, labelW = std::max(200, tabBounds.getWidth() - 355);
    const int behaviourLabelW = std::max(200, tabBounds.getWidth() - 355 - 380);   // room for the spectrum knobs
    if (debugModeToggle != nullptr) debugModeToggle->setBounds(20, debugY + 32, 310, 28);
    debugInfoLabel.setBounds(labelX, debugY + 32, labelW, 28);
    copyStateBtn.setBounds(20, debugY + 64, 230, 28);
    copyStateInfoLabel.setBounds(labelX, debugY + 64, labelW, 28);

    // Editor behaviour card below the debug card
    const int behaviourY = debugY + debugCardH + cardGap;
    behaviourCard.setBounds(0, behaviourY, tabBounds.getWidth(), behaviourCardH);
    behaviourCard.clearDividers();
    for (int i = 0; i < 2; ++i)
        if (filterSwitchToggles[i]) filterSwitchToggles[i]->setBounds(20, behaviourY + 32 + i * 20, 300, 18);
    filterSwitchInfoLabel.setBounds(labelX, behaviourY + 32, behaviourLabelW, 38);
    if (retroSpectrumToggle) retroSpectrumToggle->setBounds(20, behaviourY + 76, 300, 18);
    if (spectrumWaterfallToggle) spectrumWaterfallToggle->setBounds(20, behaviourY + 96, 300, 18);
    if (curvesWaterfallToggle) curvesWaterfallToggle->setBounds(20, behaviourY + 116, 300, 18);
    spectrumInfoLabel.setBounds(labelX, behaviourY + 86, behaviourLabelW, 38);
    // Opacity knobs at the card's right end
    const int opacityKnobSz = getStandardKnobSize(), opacitySlot = 90;
    const int opacityX = tabBounds.getWidth() - 20 - 4 * opacitySlot;
    const int opacityY = behaviourY + 40;
    layoutKnob(retroFilterOpacityKnob.get(), retroFilterOpacityLabel, opacityX + (opacitySlot - opacityKnobSz) / 2, opacityY, opacityKnobSz);
    layoutKnob(retroCurvesOpacityKnob.get(), retroCurvesOpacityLabel, opacityX + opacitySlot + (opacitySlot - opacityKnobSz) / 2, opacityY, opacityKnobSz);
    layoutKnob(waterfallOpacityKnob.get(), waterfallOpacityLabel, opacityX + 2 * opacitySlot + (opacitySlot - opacityKnobSz) / 2, opacityY, opacityKnobSz);
    layoutKnob(retroRandomKnob.get(), retroRandomLabel, opacityX + 3 * opacitySlot + (opacitySlot - opacityKnobSz) / 2, opacityY, opacityKnobSz);
}
