#include "SettingsTab.h"
#include "AfxTab.h"

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
    viewport.setWantsKeyboardFocus(false);  // no Tab stop of its own
    addAndMakeVisible(viewport);

    scrollContent.addAndMakeVisible(midiCard);
    midiCard.toBack();
    scrollContent.addAndMakeVisible(themeCard);
    themeCard.toBack();
    scrollContent.addAndMakeVisible(debugCard);
    debugCard.toBack();
    scrollContent.addAndMakeVisible(behaviourCard);
    behaviourCard.toBack();
    scrollContent.addAndMakeVisible(routingCard);
    routingCard.toBack();

    createControllerToggles();
    createThemeControls();
    createTypographyControls();
    createWindowControls();
    createDebugControls();
    createBehaviourControls();
    createRoutingControls();

    assignComponentIDs();
}

// Buttons that set a stepped parameter to the chosen index.
std::unique_ptr<ModernChoiceButtons> SettingsTab::createStepChoice(const juce::StringArray& labels, steppedParameter_t sp) {
    auto choice = std::make_unique<ModernChoiceButtons>(labels);
    choice->onSelect = [this, sp](int i) { setSteppedParam(sp, (uint8_t)i); };
    scrollContent.addAndMakeVisible(*choice);
    return choice;
}

// An editor setting as buttons (skin_config.conf): applied and saved on click.
std::unique_ptr<ModernChoiceButtons> SettingsTab::createSettingChoice(const juce::StringArray& labels, int columns,
                                                                      std::function<void(int)> apply) {
    auto choice = std::make_unique<ModernChoiceButtons>(labels, columns);
    choice->onSelect = [this, apply](int i) {
        apply(i);
        saveSkinConfig();
    };
    scrollContent.addAndMakeVisible(*choice);
    return choice;
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

// MIDI & MPE: MPE mode and bend range, the timbre (slide) target and the
// release velocity, preset parameters
void SettingsTab::createControllerToggles() {
    mpeModeChoice = createStepChoice({ "OFF (STANDARD MIDI)", "MPE 6 VOICES (CH 2-7)", "MPE FULL (CH 2-15)" }, spMPEMode);
    bendRangeChoice = createStepChoice({ "2 ST", "12 ST", "24 ST", "48 ST", "96 ST" }, spMPEPitchBendRange);
    bendRangeChoice->setTooltips({ "+/- 2 semitones", "+/- 12 semitones", "+/- 24 semitones", "+/- 48 semitones",
                                   "+/- 96 semitones" });
    timbreTargetChoice = createStepChoice({ "OFF", "PITCH", "CUTOFF", "VOLUME", "WAVEMOD", "LFO 1", "LFO 2" }, spTimbreTarget);
    releaseVelocityChoice = createStepChoice({ "OFF", "LOW", "MID", "HIGH" }, spReleaseVelocityAmt);
    releaseVelocityChoice->setTooltips({ "The key release speed does not change the release",
                                         "A fast key release shortens the amp release a little",
                                         "A fast key release shortens the amp release",
                                         "A fast key release shortens the amp release strongly" });
}

// Skin & palette: presets and user palettes, the role strip, HSB knobs,
// the colour picker swatch and saving a palette.
void SettingsTab::createThemeControls() {
    loadUserPalettes();
    rebuildPaletteChoice();

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
                userThemes[(size_t)existingIdx] = customTheme;
            } else {
                userThemes.push_back(customTheme);
                existingIdx = (int)userThemes.size() - 1;
            }

            saveUserPalettes();
            themeId = 101 + existingIdx;
            rebuildPaletteChoice();
            resized();   // the palette buttons may need another row

            savePaletteBtn.setButtonText("Palette Saved!");
            resetButtonTextLater(savePaletteBtn, "Save Palette As...");

            saveSkinConfig();
        });
    };
    scrollContent.addAndMakeVisible(savePaletteBtn);
}

// Typography and saving the appearance as the startup default
void SettingsTab::createTypographyControls() {
    // The fonts by their family name, the description as tooltip
    juce::StringArray fontNames, fontTips;
    for (const auto& font : ModernFontManager::getCuratedFonts()) {
        fontNames.add(font.displayName.upToFirstOccurrenceOf(" (", false, false).toUpperCase());
        fontTips.add(font.displayName);
    }
    fontChoice = createSettingChoice(fontNames, 6, [this](int i) { applyFont(i + 1); });
    fontChoice->setTooltips(fontTips);
    fontChoice->setSelected(fontId - 1);

    fontSizeChoice = createSettingChoice({ "85 %", "90 %", "100 %", "110 %", "120 %" }, 0,
                                         [this](int i) { applyFontScale(i + 1); });
    fontSizeChoice->setTooltips({ "Compact", "", "Standard", "", "Large" });
    fontSizeChoice->setSelected(scaleId - 1);

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

    windowSizeChoice = createSettingChoice({ "87 %", "100 %", "115 %", "125 %", "140 %", "160 %" }, 0, [this](int i) {
        static constexpr float scales[] = { 0.8727f, 1.0f, 1.15f, 1.25f, 1.40f, 1.60f };
        windowScaleId = i + 1;
        savedWindowScale = scales[i];
        host.windowScaleChanged(scales[i]);
    });
    windowSizeChoice->setTooltips({ "960 x 610", "1100 x 700", "1265 x 805", "1375 x 875", "1540 x 980", "1760 x 1120" });
    windowSizeChoice->setSelected(windowScaleId - 1);
}

// Font (1-based as in skin_config.conf) and font size (1: 85 % .. 5: 120 %)
void SettingsTab::applyFont(int id) {
    const auto curated = ModernFontManager::getCuratedFonts();
    if (id < 1 || id > (int)curated.size()) return;
    fontId = id;
    if (fontChoice) fontChoice->setSelected(id - 1);
    host.applyFontFamily(curated[(size_t)id - 1].fontName);
}

void SettingsTab::applyFontScale(int id) {
    static constexpr float scales[] = { 0.85f, 0.90f, 1.0f, 1.10f, 1.20f };
    if (id < 1 || id > 5) return;
    scaleId = id;
    if (fontSizeChoice) fontSizeChoice->setSelected(id - 1);
    host.applyFontScale(scales[id - 1]);
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
    filterSwitchChoice = createSettingChoice({ "SAME FILTER", "LAST CHOSEN" }, 0,
                                             [this](int i) { setFilterSwitchMatch(i == 0); });
    filterSwitchChoice->setTooltips({ "The same filter as before, for comparing", "The family's last chosen filter" });
    filterSwitchChoice->setSelected(0);
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

// Split / layer routing (VoiceAllocator::assign): with it on, a note plays
// every part whose MIDI channel and key range match.
void SettingsTab::createRoutingControls() {
    routingToggle = createToggle("SPLIT / LAYER ROUTING");
    routingToggle->onClick = [this]() {
        const bool on = routingToggle->getToggleState();
        model.setCustomRouting(on);
        // The routes replace the AFX key map, which would otherwise be
        // ignored without a hint (the AFX switch turns routing off).
        if (on && model.getCurrentPreset().steppedParams[spEngineMode] == emAFX)
            setSteppedParam(spEngineMode, static_cast<uint8_t>(emMultiChannel));
        updateRoutingControls();
    };
    scrollContent.addAndMakeVisible(*routingToggle);
    setupInfoLabel(routingInfoLabel,
        "A note plays every part whose MIDI channel and key range match: separate ranges split the keyboard, "
        "overlapping ranges layer sounds. The parts are the 16 sounds of the AFX tab; on, AFX mode is off.");
    scrollContent.addAndMakeVisible(routingInfoLabel);

    routeMap = std::make_unique<RouteMap>(model, modernLnf);
    routeMap->onSelect = [this](int part) { selectRoutePart(part); };
    scrollContent.addAndMakeVisible(*routeMap);

    // The part selected on the lanes, by number and sound
    routePartLabel.setFont(modernLnf.getCustomFont(13.0f, juce::Font::bold));
    routePartLabel.setJustificationType(juce::Justification::centredLeft);
    scrollContent.addAndMakeVisible(routePartLabel);

    routeEnabledToggle = createToggle("PART ON");
    routeEnabledToggle->onClick = [this]() { storeRoute(); };
    scrollContent.addAndMakeVisible(*routeEnabledToggle);

    juce::StringArray channels{ "ANY" };
    for (int ch = 1; ch <= 16; ++ch) channels.add(juce::String(ch));
    routeChannelChoice = std::make_unique<ModernChoiceButtons>(channels);
    routeChannelChoice->setGap(3);
    routeChannelChoice->getButton(0)->setTooltip("The part listens on every MIDI channel");
    routeChannelChoice->onSelect = [this](int) { storeRoute(); };
    scrollContent.addAndMakeVisible(*routeChannelChoice);

    for (auto* slider : { &routeLowSlider, &routeHighSlider }) {
        const juce::String prefix = slider == &routeLowSlider ? "LOWEST KEY  " : "HIGHEST KEY  ";
        slider->setSliderStyle(juce::Slider::LinearBar);
        slider->setRange(0.0, 127.0, 1.0);
        slider->textFromValueFunction = [prefix](double v) { return prefix + AfxTab::noteName(static_cast<int>(v)); };
        slider->valueFromTextFunction = [](const juce::String&) { return 0.0; };
        slider->setTextBoxIsEditable(false);
        slider->onValueChange = [this]() { storeRoute(); };
        addPageKnob(*slider);
    }
    routeLowSlider.setTooltip("Lowest key the part plays");
    routeHighSlider.setTooltip("Highest key the part plays");
    selectRoutePart(0);
}

void SettingsTab::selectRoutePart(int part) {
    selectedRoutePart = std::clamp(part, 0, 15);
    updateRoutingControls();
}

// The selected part's route into the controls.
void SettingsTab::updateRoutingControls() {
    if (!routeMap) return;
    const bool on = model.usesCustomRouting();
    safeSetToggle(routingToggle.get(), on);
    routePartLabel.setText("PART " + juce::String(selectedRoutePart + 1) + "   "
                               + juce::String(model.getAfxKit().getSlot(selectedRoutePart).name).toUpperCase(),
                           juce::dontSendNotification);
    routePartLabel.setColour(juce::Label::textColourId, AfxTab::padColour(selectedRoutePart));
    const auto& route = model.getPartRoute(selectedRoutePart);
    safeSetToggle(routeEnabledToggle.get(), route.enabled != 0);
    routeChannelChoice->setSelected(route.channel);
    safeSetKnob(&routeLowSlider, route.low);
    safeSetKnob(&routeHighSlider, route.high);
    for (juce::Component* c : std::initializer_list<juce::Component*>{ routeEnabledToggle.get(), routeChannelChoice.get(),
                                                                       &routeLowSlider, &routeHighSlider })
        c->setAlpha(on ? 1.0f : 0.5f);
    routeMap->setSelectedPart(selectedRoutePart);
    routeMap->setAlpha(on ? 1.0f : 0.5f);
}

// The controls into the selected part's route (a range with low above high
// takes the two keys in order).
void SettingsTab::storeRoute() {
    auto& route = model.getPartRoute(selectedRoutePart);
    const auto low = static_cast<int>(routeLowSlider.getValue()), high = static_cast<int>(routeHighSlider.getValue());
    route.enabled = routeEnabledToggle->getToggleState() ? 1 : 0;
    route.channel = static_cast<uint8_t>(std::clamp(routeChannelChoice->getSelected(), 0, 16));
    route.low = static_cast<uint8_t>(std::min(low, high));
    route.high = static_cast<uint8_t>(std::max(low, high));
    if (routeMap) routeMap->repaint();
}

int SettingsTab::RouteMap::partAt(float y) const {
    const float laneH = (static_cast<float>(getHeight()) - kAxisHeight) / 16.0f;
    const int lane = static_cast<int>(std::floor(y / laneH));
    return lane >= 0 && lane < 16 ? lane : -1;
}

void SettingsTab::RouteMap::mouseDown(const juce::MouseEvent& e) {
    const int part = partAt(e.position.y);
    if (part >= 0 && onSelect) onSelect(part);
}

void SettingsTab::RouteMap::paint(juce::Graphics& g) {
    const auto& theme = lnf.getTheme();
    const auto bounds = getLocalBounds().toFloat();
    const float laneH = (bounds.getHeight() - kAxisHeight) / 16.0f;
    const float keysX = kNumberWidth, keysW = bounds.getWidth() - kNumberWidth - kChannelWidth;
    const auto keyX = [keysX, keysW](int note) { return keysX + keysW * static_cast<float>(note) / 128.0f; };

    g.setColour(theme.visualizerGrid.withAlpha(0.35f));
    for (int octave = 0; octave <= 10; ++octave) g.fillRect(keyX(octave * 12), 0.0f, 1.0f, laneH * 16.0f);
    g.setFont(lnf.getCustomFont(9.0f, juce::Font::plain));
    for (int part = 0; part < 16; ++part) {
        const float y = laneH * static_cast<float>(part);
        const bool isSelected = part == selected;
        if (isSelected) {
            g.setColour(theme.accent.withAlpha(0.15f));
            g.fillRect(0.0f, y, bounds.getWidth(), laneH);
        }
        const auto& route = model.getPartRoute(part);
        g.setColour(isSelected ? theme.textTitle : theme.textMuted);
        g.drawText(juce::String(part + 1), juce::Rectangle<float>(0.0f, y, kNumberWidth - 5.0f, laneH),
                   juce::Justification::centredRight, false);
        g.drawText(!route.enabled ? juce::String("OFF") : route.channel == 0 ? juce::String("ANY")
                                                                                : "CH " + juce::String(route.channel),
                   juce::Rectangle<float>(keysX + keysW + 6.0f, y, kChannelWidth - 6.0f, laneH),
                   juce::Justification::centredLeft, false);
        if (!route.enabled) continue;
        const juce::Rectangle<float> bar(keyX(route.low), y + 2.0f, keyX(route.high + 1) - keyX(route.low),
                                         std::max(1.0f, laneH - 4.0f));
        g.setColour(AfxTab::padColour(part).withAlpha(isSelected ? 1.0f : 0.55f));
        g.fillRect(bar);
    }
    // Key axis: the C of every octave, labelled every second one.
    g.setColour(theme.textMuted);
    for (int octave = 0; octave <= 10; octave += 2)
        g.drawText(AfxTab::noteName(octave * 12), juce::Rectangle<float>(keyX(octave * 12), laneH * 16.0f, 40.0f, kAxisHeight),
                   juce::Justification::centredLeft, false);
}

void SettingsTab::assignComponentIDs() {
    themeCard.setComponentID("themeCard");

    // Tab 6: Settings & Controllers
    midiCard.setComponentID("midiCard");
    mpeModeChoice->setIdPrefix("mpeModeButton");
    bendRangeChoice->setIdPrefix("mpeBendRangeButton");
    timbreTargetChoice->setIdPrefix("timbreTargetButton");
    releaseVelocityChoice->setIdPrefix("releaseVelocityButton");
    swatchStrip.setComponentID("swatchStrip");
    swatchButton.setComponentID("swatchButton");
    if (customHueKnob) customHueKnob->setComponentID("customHueKnob");
    if (customHueLabel) customHueLabel->setComponentID("customHueLabel");
    if (customSatKnob) customSatKnob->setComponentID("customSatKnob");
    if (customSatLabel) customSatLabel->setComponentID("customSatLabel");
    if (customBriKnob) customBriKnob->setComponentID("customBriKnob");
    if (customBriLabel) customBriLabel->setComponentID("customBriLabel");
    savePaletteBtn.setComponentID("savePaletteBtn");
    fontChoice->setIdPrefix("fontButton");
    fontSizeChoice->setIdPrefix("fontSizeButton");
    saveDefaultBtn.setComponentID("saveDefaultBtn");
    skinSwitchBtn.setComponentID("skinSwitchBtn");
    windowSizeChoice->setIdPrefix("windowSizeButton");
    if (debugModeToggle) debugModeToggle->setComponentID("debugModeToggle");
    debugInfoLabel.setComponentID("debugInfoLabel");
    debugCard.setComponentID("debugCard");
    copyStateBtn.setComponentID("copyStateBtn");
    copyStateInfoLabel.setComponentID("copyStateInfoLabel");
    behaviourCard.setComponentID("behaviourCard");
    routingCard.setComponentID("routingCard");
    if (routingToggle) routingToggle->setComponentID("splitLayerToggle");
    routingInfoLabel.setComponentID("routingInfoLabel");
    if (routeMap) routeMap->setComponentID("routeMap");
    routePartLabel.setComponentID("routePartLabel");
    if (routeEnabledToggle) routeEnabledToggle->setComponentID("routeEnabledToggle");
    routeChannelChoice->setIdPrefix("routeChannelButton");
    routeLowSlider.setComponentID("routeLowSlider");
    routeHighSlider.setComponentID("routeHighSlider");
    filterSwitchChoice->setIdPrefix("filterSwitchButton");
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

    selectPalette(100);
    if (paletteChoice) paletteChoice->repaint();   // the CUSTOM chip
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
    content << "themeId=" << themeId << "\n";
    content << "customThemeName=" << customTheme.name << "\n";
    content << "fontId=" << fontId << "\n";
    content << "scaleId=" << scaleId << "\n";
    content << "windowScaleId=" << windowScaleId << "\n";
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

    int storedThemeId = 1, storedFontId = 1, storedScaleId = 3, storedWindowScaleId = 2;
    float winScale = 1.0f;

    if (confFile.existsAsFile()) {
        juce::StringArray lines;
        confFile.readLines(lines);

        for (const auto& line : lines) {
            if (line.startsWith("themeId=")) storedThemeId = line.fromFirstOccurrenceOf("themeId=", false, false).getIntValue();
            else if (line.startsWith("customThemeName=")) customTheme.name = line.fromFirstOccurrenceOf("customThemeName=", false, false).trim();
            else if (line.startsWith("fontId=")) storedFontId = line.fromFirstOccurrenceOf("fontId=", false, false).getIntValue();
            else if (line.startsWith("scaleId=")) storedScaleId = line.fromFirstOccurrenceOf("scaleId=", false, false).getIntValue();
            else if (line.startsWith("windowScaleId=")) storedWindowScaleId = line.fromFirstOccurrenceOf("windowScaleId=", false, false).getIntValue();
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
    windowScaleId = std::clamp(storedWindowScaleId, 1, 6);
    if (windowSizeChoice) windowSizeChoice->setSelected(windowScaleId - 1);

    applyFont(storedFontId);
    applyFontScale(storedScaleId);

    if (storedThemeId >= 101 && storedThemeId <= 100 + (int)userThemes.size()) {
        customTheme = userThemes[(size_t)storedThemeId - 101];
        host.applyTheme(customTheme);
        themeId = storedThemeId;
    } else if (storedThemeId == 100) {
        host.applyTheme(customTheme);
        themeId = 100;
    } else {
        themeId = std::clamp(storedThemeId, 1, (int)ModernTheme::getPresetThemes().size());
        host.applyTheme(ModernTheme::getPresetThemes()[(size_t)themeId - 1]);
    }
    rebuildPaletteChoice();   // the user palettes as loaded
    resized();

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

// One button per palette, each with its accent and panel colours as a chip
void SettingsTab::rebuildPaletteChoice() {
    std::vector<ModernTheme> themes;
    juce::StringArray labels, tips;
    paletteIds.clear();
    const auto presetThemes = ModernTheme::getPresetThemes();
    for (size_t i = 0; i < presetThemes.size(); ++i) {
        themes.push_back(presetThemes[i]);
        labels.add(presetThemes[i].name.upToFirstOccurrenceOf(" (", false, false).toUpperCase());
        tips.add(presetThemes[i].description);
        paletteIds.push_back((int)i + 1);
    }
    for (size_t u = 0; u < userThemes.size(); ++u) {
        themes.push_back(userThemes[u]);
        labels.add(userThemes[u].name.toUpperCase());
        tips.add("User palette");
        paletteIds.push_back(101 + (int)u);
    }
    themes.push_back(customTheme);
    labels.add("CUSTOM");
    tips.add("The colours as edited below");
    paletteIds.push_back(100);

    paletteChoice = std::make_unique<ModernChoiceButtons>(labels, 5);
    paletteChoice->setTooltips(tips);
    paletteChoice->setGlyphPainter([this, themes, labels](juce::Graphics& g, juce::Rectangle<float> area, int index,
                                                         juce::Colour text) {
        // CUSTOM shows the colours as they are being edited
        const auto& theme = paletteIds[(size_t)index] == 100 ? customTheme : themes[(size_t)index];
        const auto chip = area.removeFromLeft(area.getHeight() * 1.6f).reduced(0.0f, 1.0f);
        g.setColour(theme.cardBg);
        g.fillRect(chip);
        g.setColour(theme.accent);
        g.fillRect(chip.withWidth(chip.getWidth() * 0.5f));
        g.setColour(text.withAlpha(0.5f));
        g.drawRect(chip, 1.0f);
        g.setColour(text);
        g.setFont(modernLnf.getCustomFont(10.5f, juce::Font::bold));
        g.drawFittedText(labels[index], area.withTrimmedLeft(8.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    });
    paletteChoice->onSelect = [this](int i) {
        const int id = paletteIds[(size_t)i];
        const auto presets = ModernTheme::getPresetThemes();
        if (id >= 1 && id <= (int)presets.size()) customTheme = presets[(size_t)id - 1];
        else if (id >= 101 && id <= 100 + (int)userThemes.size()) customTheme = userThemes[(size_t)id - 101];
        themeId = id;
        host.applyTheme(customTheme);
        swatchStrip.setTheme(customTheme);
        updateRoleColorInSliders();
        saveSkinConfig();
    };
    paletteChoice->setIdPrefix("paletteButton");
    scrollContent.addAndMakeVisible(*paletteChoice);
    selectPalette(themeId);
}

void SettingsTab::selectPalette(int id) {
    themeId = id;
    if (!paletteChoice) return;
    const auto it = std::find(paletteIds.begin(), paletteIds.end(), id);
    paletteChoice->setSelected(it == paletteIds.end() ? -1 : (int)(it - paletteIds.begin()));
}

void SettingsTab::setFilterSwitchMatch(bool match) {
    filterSwitchMatch = match;
    if (filterSwitchChoice) filterSwitchChoice->setSelected(match ? 0 : 1);
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

    // MIDI & MPE
    mpeModeChoice->setSelected(preset.steppedParams[spMPEMode]);
    bendRangeChoice->setSelected(preset.steppedParams[spMPEPitchBendRange]);
    timbreTargetChoice->setSelected(preset.steppedParams[spTimbreTarget]);
    releaseVelocityChoice->setSelected(preset.steppedParams[spReleaseVelocityAmt]);

    // Split / layer: the AFX switch or a loaded session may have changed it.
    updateRoutingControls();
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
    // Top to bottom: MIDI & MPE, split / layer, appearance, editor behaviour,
    // debug. The appearance card grows by a row per five palettes.
    const int paletteRows = paletteChoice ? (paletteChoice->getNumOptions() + 4) / 5 : 2;
    constexpr int cardGap = 12, midiCardH = 128, routingCardH = 340, behaviourCardH = 140, debugCardH = 100;
    const int themeCardH = 310 + 26 * paletteRows;
    const int contentH = midiCardH + routingCardH + themeCardH + behaviourCardH + debugCardH + 4 * cardGap;

    // The viewport shows its scroll bar when needed; the content takes the
    // width left beside it.
    viewport.setBounds(getLocalBounds());
    scrollContent.setSize(getWidth(), contentH);
    const int contentW = viewport.getMaximumVisibleWidth();
    scrollContent.setSize(contentW, contentH);
    // While the page scrolls, the mouse wheel scrolls it, also over a knob.
    const bool scrolls = viewport.getVerticalScrollBar().isVisible();
    for (auto* knob : pageKnobs) knob->setScrollWheelEnabled(!scrolls);

    constexpr int left = 20;
    const int innerW = contentW - 2 * left;
    const int halfW = (innerW - 24) / 2, rightX = left + halfW + 24;
    // A named divider over a group of controls (x, w of the controls)
    auto divider = [](ModernSectionCard& card, int x, int y, int w, const juce::String& name) {
        card.addDivider(x - 6, y, w + 6, name);
    };

    // MIDI & MPE: two rows of two groups
    const int midiY = 0;
    midiCard.setBounds(0, midiY, contentW, midiCardH);
    midiCard.clearDividers();
    divider(midiCard, left, 30, halfW, "MPE MODE");
    divider(midiCard, rightX, 30, halfW, "PITCH BEND RANGE (MPE)");
    mpeModeChoice->setBounds(left, midiY + 40, halfW, 24);
    bendRangeChoice->setBounds(rightX, midiY + 40, halfW, 24);
    divider(midiCard, left, 78, halfW, "TIMBRE / SLIDE (CC 74) TO");
    divider(midiCard, rightX, 78, halfW, "RELEASE VELOCITY");
    timbreTargetChoice->setBounds(left, midiY + 88, halfW, 24);
    releaseVelocityChoice->setBounds(rightX, midiY + 88, halfW, 24);

    // Split / layer: the switch and its explanation, the part lanes, the
    // selected part's route, its MIDI channel at the bottom
    const int labelX = 340, labelW = std::max(200, contentW - 355);
    const int routingY = midiY + midiCardH + cardGap;
    routingCard.setBounds(0, routingY, contentW, routingCardH);
    routingCard.clearDividers();
    if (routingToggle) routingToggle->setBounds(left, routingY + 36, 300, 20);
    routingInfoLabel.setBounds(labelX, routingY + 30, labelW, 32);
    if (routeMap) routeMap->setBounds(left, routingY + 68, innerW, 16 * 10 + 14);
    const int rowY = routingY + 252;
    routePartLabel.setBounds(left, rowY, 300, 26);
    if (routeEnabledToggle) routeEnabledToggle->setBounds(left + 310, rowY + 2, 100, 22);
    const int sliderW = (halfW - 10) / 2;
    routeLowSlider.setBounds(rightX, rowY, sliderW, 26);
    routeHighSlider.setBounds(rightX + sliderW + 10, rowY, sliderW, 26);
    divider(routingCard, left, 290, innerW, "MIDI CHANNEL OF THE PART");
    routeChannelChoice->setBounds(left, routingY + 300, innerW, 24);

    // Skin & palette: the palettes on top, the colour of a role in the
    // middle, typography and window at the bottom
    const int themeY = routingY + routingCardH + cardGap;
    themeCard.setBounds(0, themeY, contentW, themeCardH);
    themeCard.clearDividers();
    divider(themeCard, left, 30, innerW, "PALETTE");
    paletteChoice->setBounds(left, themeY + 40, innerW, paletteRows * 26 - 4);

    const int roleY = 40 + paletteRows * 26 + 8;
    divider(themeCard, left, roleY, innerW, "COLOUR OF A ROLE");
    swatchStrip.setBounds(left, themeY + roleY + 10, innerW, 30);
    const int knobSz = getStandardKnobSize();
    const int knobY = themeY + roleY + 50;
    constexpr int btnH = 28, btnW = 150;
    const int btnY = knobY + (knobSz - btnH) / 2;
    savePaletteBtn.setBounds(left, btnY, btnW, btnH);
    swatchButton.setBounds(left + btnW + 10, btnY, btnH, btnH);
    const int sepX = left + btnW + 10 + btnH + 16;
    themeCard.addVerticalDivider(sepX, roleY + 48, roleY + 48 + knobSz + 14);
    constexpr int knobSpacing = 115;
    layoutKnob(customHueKnob.get(), customHueLabel, sepX + 20, knobY, knobSz);
    layoutKnob(customSatKnob.get(), customSatLabel, sepX + 20 + knobSpacing, knobY, knobSz);
    layoutKnob(customBriKnob.get(), customBriLabel, sepX + 20 + knobSpacing * 2, knobY, knobSz);

    const int fontY = roleY + 128;
    const int fontW = innerW * 2 / 3, sizeX = left + fontW + 24, sizeW = innerW - fontW - 24;
    divider(themeCard, left, fontY, fontW, "FONT");
    divider(themeCard, sizeX, fontY, sizeW, "FONT SIZE");
    fontChoice->setBounds(left, themeY + fontY + 10, fontW, 48);
    fontSizeChoice->setBounds(sizeX, themeY + fontY + 10, sizeW, 22);

    const int windowY = fontY + 68;
    divider(themeCard, left, windowY, halfW, "WINDOW SIZE");
    divider(themeCard, rightX, windowY, halfW, "STARTUP DEFAULT & SKIN");
    windowSizeChoice->setBounds(left, themeY + windowY + 10, halfW, 24);
    saveDefaultBtn.setBounds(rightX, themeY + windowY + 8, 140, 28);
    skinSwitchBtn.setBounds(rightX + 150, themeY + windowY + 8, 190, 28);
    defaultInfoLabel.setBounds(rightX, themeY + windowY + 38, halfW, 18);

    // Editor behaviour: the filter family switch on top, the spectrum
    // displays below, their opacity knobs on the right
    const int behaviourY = themeY + themeCardH + cardGap;
    const int behaviourLabelW = std::max(200, contentW - 355 - 380);   // room for the opacity knobs
    behaviourCard.setBounds(0, behaviourY, contentW, behaviourCardH);
    behaviourCard.clearDividers();
    filterSwitchChoice->setBounds(left, behaviourY + 36, 300, 24);
    filterSwitchInfoLabel.setBounds(labelX, behaviourY + 30, behaviourLabelW, 38);
    if (retroSpectrumToggle) retroSpectrumToggle->setBounds(left, behaviourY + 76, 300, 18);
    if (spectrumWaterfallToggle) spectrumWaterfallToggle->setBounds(left, behaviourY + 96, 300, 18);
    if (curvesWaterfallToggle) curvesWaterfallToggle->setBounds(left, behaviourY + 116, 300, 18);
    spectrumInfoLabel.setBounds(labelX, behaviourY + 86, behaviourLabelW, 38);
    const int opacitySlot = 90;
    const int opacityX = contentW - left - 4 * opacitySlot;
    const int opacityY = behaviourY + 40;
    auto opacityKnob = [&](juce::Slider* knob, std::unique_ptr<juce::Label>& label, int slot) {
        layoutKnob(knob, label, opacityX + slot * opacitySlot + (opacitySlot - knobSz) / 2, opacityY, knobSz);
    };
    opacityKnob(retroFilterOpacityKnob.get(), retroFilterOpacityLabel, 0);
    opacityKnob(retroCurvesOpacityKnob.get(), retroCurvesOpacityLabel, 1);
    opacityKnob(waterfallOpacityKnob.get(), waterfallOpacityLabel, 2);
    opacityKnob(retroRandomKnob.get(), retroRandomLabel, 3);

    // Debug: inspector switch, then the state copy for test scenarios
    const int debugY = behaviourY + behaviourCardH + cardGap;
    debugCard.setBounds(0, debugY, contentW, debugCardH);
    debugCard.clearDividers();
    if (debugModeToggle != nullptr) debugModeToggle->setBounds(left, debugY + 32, 310, 28);
    debugInfoLabel.setBounds(labelX, debugY + 32, labelW, 28);
    copyStateBtn.setBounds(left, debugY + 64, 230, 28);
    copyStateInfoLabel.setBounds(labelX, debugY + 64, labelW, 28);
}
