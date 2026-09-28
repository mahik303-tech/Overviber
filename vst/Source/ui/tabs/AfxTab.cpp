#include "AfxTab.h"

#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
#include "../../PluginProcessor.h"
#endif
#include <algorithm>

// ------------------------------------------------------------------------------
// AfxKeyboardZoneComponent Implementation
// ------------------------------------------------------------------------------
AfxTab::AfxKeyboardZoneComponent::AfxKeyboardZoneComponent(SynthEngine& eng) : engine(eng) {}

void AfxTab::AfxKeyboardZoneComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<ModernLookAndFeel*>(&getLookAndFeel());
    auto theme = lnf ? lnf->getTheme() : ModernTheme::getPresetThemes()[0];

    g.setColour(theme.cardBg);
    g.fillRect(bounds);
    g.setColour(theme.cardBorder);
    g.drawRect(bounds, 1.0f);

    int totalNotes = 128;
    float keyW = (bounds.getWidth() - 2.0f) / (float)totalNotes;

    static const juce::uint32 slotColours[16] = {
        0xff18b5c9, 0xff00e5ff, 0xff00b0ff, 0xff2979ff,
        0xff651fff, 0xff7c4dff, 0xffe040fb, 0xffff4081,
        0xffff5252, 0xffff6e40, 0xffffab00, 0xffffd740,
        0xff76ff03, 0xff00e676, 0xff1de9b6, 0xff26a69a
    };

    for (int note = 0; note < totalNotes; ++note) {
        uint8_t slot = engine.getAfxKit().getSlotForNote((uint8_t)note);
        juce::Colour c = juce::Colour(slotColours[slot % 16]);

        float kx = bounds.getX() + 1.0f + (float)note * keyW;
        auto keyRect = juce::Rectangle<float>(kx, bounds.getY() + 1.0f, std::max(1.0f, keyW), bounds.getHeight() - 2.0f);
        g.setColour(c.withAlpha(0.75f));
        g.fillRect(keyRect);

        if (note % 12 == 0) {
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.drawVerticalLine((int)kx, bounds.getY(), bounds.getBottom());
            g.setFont(ModernFontManager::createFont("D-DIN", 8.0f, juce::Font::bold));
            int oct = (note / 12) - 1;
            g.drawText("C" + juce::String(oct), (int)kx + 1, (int)bounds.getY() + 2, 18, 10, juce::Justification::left, false);
        }
    }
}

void AfxTab::AfxKeyboardZoneComponent::mouseDown(const juce::MouseEvent& e) {
    mouseDrag(e);
}

void AfxTab::AfxKeyboardZoneComponent::mouseDrag(const juce::MouseEvent& e) {
    if (getWidth() <= 0) return;
    int note = std::clamp((int)((float)e.x / (float)getWidth() * 128.0f), 0, 127);
    if (onNoteClicked) onNoteClicked((uint8_t)note);
    repaint();
}

AfxTab::AfxTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void AfxTab::setup() {
    addAndMakeVisible(voiceAllocCard);
    addAndMakeVisible(afxKitCard);
    voiceAllocCard.toBack();
    afxKitCard.toBack();

    // Voice Allocation & Priority controls
    voiceCountSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxBelow);
    voiceCountSlider->setName("VoiceCount");
    voiceCountSlider->setRange(1, 6, 1.0);
    voiceCountSlider->setNumDecimalPlacesToDisplay(0);
    voiceCountSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 14);
    voiceCountSlider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffb0bec5));
    voiceCountSlider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    voiceCountSlider->textFromValueFunction = [](double val) -> juce::String {
        int v = (int)std::round(val);
        if (v == 1) return "1 Voice";
        if (v == 6) return "6 Poly";
        return juce::String(v) + " Voices";
    };
    voiceCountSlider->valueFromTextFunction = [](const juce::String& text) -> double {
        return std::clamp(text.getIntValue(), 1, 6);
    };
    voiceCountSlider->setValue(6, juce::dontSendNotification);
    voiceCountSlider->updateText();
    voiceCountSlider->onValueChange = [this]() {
        setSteppedParam(spVoiceCount, (uint8_t)(std::clamp((int)std::round(voiceCountSlider->getValue()) - 1, 0, 5)));
    };

    const char* assignerPrioNames[3] = { "Last Note", "Lowest Note", "Highest Note" };
    for (int i = 0; i < 3; ++i) {
        assignerPrioToggles[i] = createToggle(assignerPrioNames[i]);
        assignerPrioToggles[i]->setRadioGroupId(1204);
        assignerPrioToggles[i]->onClick = [this, i]() {
            setSteppedParam(spAssignerPriority, (uint8_t)i);
        };
    }

    if (voiceCountSlider) addAndMakeVisible(*voiceCountSlider);
    for (int i = 0; i < 3; ++i) {
        if (assignerPrioToggles[i]) addAndMakeVisible(*assignerPrioToggles[i]);
    }

    // 16 AFX Sound Slots Buttons
    for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
        afxSlotButtons[i] = std::make_unique<juce::TextButton>("Slot " + juce::String(i + 1));
        afxSlotButtons[i]->setClickingTogglesState(false);
        afxSlotButtons[i]->onClick = [this, i]() {
            selectAfxSlot(i);
        };
        addAndMakeVisible(*afxSlotButtons[i]);
    }

    // Quick Mapping Mode Buttons
    octaveMapBtn = std::make_unique<juce::TextButton>("OCTAVE ZONES");
    octaveMapBtn->onClick = [this]() {
        engine.getAfxKit().mapOctaveZones();
        if (afxKeyboardZone) afxKeyboardZone->repaint();
    };
    addAndMakeVisible(*octaveMapBtn);

    chromaticMapBtn = std::make_unique<juce::TextButton>("CHROMATIC 16");
    chromaticMapBtn->onClick = [this]() {
        engine.getAfxKit().mapChromatic16();
        if (afxKeyboardZone) afxKeyboardZone->repaint();
    };
    addAndMakeVisible(*chromaticMapBtn);

    allToSlotBtn = std::make_unique<juce::TextButton>("ALL TO SELECTED");
    allToSlotBtn->onClick = [this]() {
        engine.getAfxKit().mapAllToSlot((uint8_t)selectedAfxSlot);
        if (afxKeyboardZone) afxKeyboardZone->repaint();
    };
    addAndMakeVisible(*allToSlotBtn);

    assignCurrentPresetBtn = std::make_unique<juce::TextButton>("COPY CURRENT PRESET TO SLOT");
    assignCurrentPresetBtn->onClick = [this]() {
        engine.getAfxKit().getSlot(selectedAfxSlot).preset = engine.getCurrentPreset();
        prepareSelectedPartWaves();
        updateAfxSlotButtons();
    };
    addAndMakeVisible(*assignCurrentPresetBtn);

    slotPresetCombo = createCombo();
    auto& pm = engine.getPresetManager();
    for (int p = 0; p < pm.getPresetCount(); ++p) {
        slotPresetCombo->addItem(pm.getPresetName(p), p + 1);
    }
    slotPresetCombo->onChange = [this]() {
        int id = slotPresetCombo->getSelectedId();
        if (id >= 1 && id <= engine.getPresetManager().getPresetCount()) {
            PresetData pData;
            if (engine.getPresetManager().loadPreset(id - 1, pData)) {
                engine.getAfxKit().getSlot(selectedAfxSlot).preset = pData;
                prepareSelectedPartWaves();
                engine.getAfxKit().getSlot(selectedAfxSlot).name = engine.getPresetManager().getPresetName(id - 1);
                updateAfxSlotButtons();
            }
        }
    };
    addAndMakeVisible(*slotPresetCombo);

    afxSlotDetailLabel = createLabel("AFX SOUND SLOT DETAILS", *this);
    afxVoiceLiveStatusLabel = createLabel("ACTIVE VOICE ALLOCATION", *this);

    afxKeyboardZone = std::make_unique<AfxKeyboardZoneComponent>(engine);
    afxKeyboardZone->onNoteClicked = [this](uint8_t note) {
        engine.getAfxKit().setNoteMapping(note, (uint8_t)selectedAfxSlot);
        if (afxKeyboardZone) afxKeyboardZone->repaint();
    };
    addAndMakeVisible(*afxKeyboardZone);

    selectAfxSlot(0);
    setupRoutingControls();

    assignComponentIDs();
}

void AfxTab::assignComponentIDs() {
    voiceAllocCard.setComponentID("voiceAllocCard");
    afxKitCard.setComponentID("afxKitCard");

    if (voiceCountSlider) voiceCountSlider->setComponentID("voiceCountSlider");
    for (int i = 0; i < 3; ++i) {
        if (assignerPrioToggles[i]) assignerPrioToggles[i]->setComponentID("assignerPrioToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
        if (afxSlotButtons[i]) afxSlotButtons[i]->setComponentID("afxSlotButton[" + juce::String(i) + "]");
    }
    if (octaveMapBtn) octaveMapBtn->setComponentID("octaveMapBtn");
    if (chromaticMapBtn) chromaticMapBtn->setComponentID("chromaticMapBtn");
    if (allToSlotBtn) allToSlotBtn->setComponentID("allToSlotBtn");
    if (assignCurrentPresetBtn) assignCurrentPresetBtn->setComponentID("assignCurrentPresetBtn");
    if (slotPresetCombo) slotPresetCombo->setComponentID("slotPresetCombo");
    if (afxSlotDetailLabel) afxSlotDetailLabel->setComponentID("afxSlotDetailLabel");
    if (afxVoiceLiveStatusLabel) afxVoiceLiveStatusLabel->setComponentID("afxVoiceLiveStatusLabel");
    if (afxKeyboardZone) afxKeyboardZone->setComponentID("afxKeyboardZone");
}

void AfxTab::selectAfxSlot(int slotIndex) {
    selectedAfxSlot = std::clamp(slotIndex, 0, AFX_SLOT_COUNT - 1);
    setSteppedParam(spAFXSelectedSlot, (uint8_t)selectedAfxSlot);
    updateAfxSlotButtons();
    const auto& route = engine.getPartRoute(selectedAfxSlot);
    routeEnabled.setToggleState(route.enabled != 0, juce::dontSendNotification);
    routeChannel.setSelectedId(route.channel + 1, juce::dontSendNotification);
    routeLow.setValue(route.low, juce::dontSendNotification);
    routeHigh.setValue(route.high, juce::dontSendNotification);
}

void AfxTab::prepareSelectedPartWaves() {
    auto& slot = engine.getAfxKit().getSlot(selectedAfxSlot);
    if (selectedAfxSlot != 0) slot.waveManager.setBaseDirectory(engine.getWaveManager().getBaseDirectory());
    for (int w = 0; w < abxCount; ++w)
        slot.waveManager.loadWave(static_cast<abx_t>(w), slot.preset.oscBank[w], slot.preset.oscWave[w]);
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (processor && selectedAfxSlot == 0) processor->updateAPVTSFromEngine();
#endif
}

void AfxTab::setupRoutingControls() {
    for (auto* component : std::initializer_list<juce::Component*>{&customRouteToggle, &routeEnabled, &routeChannel,
         &routeLow, &routeHigh, &saveSetupButton, &loadSetupButton}) addAndMakeVisible(component);
    routeChannel.addItem("ANY MIDI CHANNEL", 1);
    for (int ch = 1; ch <= 16; ++ch) routeChannel.addItem("MIDI CHANNEL " + juce::String(ch), ch + 1);
    for (auto* slider : {&routeLow, &routeHigh}) {
        slider->setRange(0, 127, 1); slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 22);
    }
    routeLow.setTooltip("Lowest MIDI note in this part's zone"); routeHigh.setTooltip("Highest MIDI note in this part's zone");
    routeLow.setTextValueSuffix(" LOW"); routeHigh.setTextValueSuffix(" HIGH");
    auto update = [this] {
        auto& route = engine.getPartRoute(selectedAfxSlot);
        route.enabled = routeEnabled.getToggleState(); route.channel = static_cast<uint8_t>(std::max(0, routeChannel.getSelectedId() - 1));
        route.low = static_cast<uint8_t>(std::min(routeLow.getValue(), routeHigh.getValue()));
        route.high = static_cast<uint8_t>(std::max(routeLow.getValue(), routeHigh.getValue()));
    };
    routeEnabled.onClick = update; routeChannel.onChange = update; routeLow.onValueChange = update; routeHigh.onValueChange = update;
    customRouteToggle.setToggleState(engine.usesCustomRouting(), juce::dontSendNotification);
    customRouteToggle.onClick = [this] { engine.setCustomRouting(customRouteToggle.getToggleState()); };
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    auto choose = [this](bool save) {
        setupChooser = std::make_unique<juce::FileChooser>(save ? "Save complete setup" : "Load complete setup",
            juce::File{}, "*.ovm");
        juce::Component::SafePointer<AfxTab> safe(this);
        setupChooser->launchAsync((save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
            : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles,
            [safe, save](const juce::FileChooser& chooser) {
                if (!safe || !safe->processor || chooser.getResult() == juce::File{}) return;
                const auto file = save ? chooser.getResult().withFileExtension("ovm") : chooser.getResult();
                const bool ok = save ? safe->processor->saveSetup(file) : safe->processor->loadSetup(file);
                if (!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Setup", "The setup could not be saved or loaded.");
                safe->customRouteToggle.setToggleState(safe->engine.usesCustomRouting(), juce::dontSendNotification);
                safe->selectAfxSlot(0);
                if (safe->context.refreshFromEngine) safe->context.refreshFromEngine();
            });
    };
    saveSetupButton.onClick = [choose] { choose(true); }; loadSetupButton.onClick = [choose] { choose(false); };
#else
    saveSetupButton.setEnabled(false); loadSetupButton.setEnabled(false);
#endif
    selectAfxSlot(selectedAfxSlot);
}

void AfxTab::updateAfxSlotButtons() {
    for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
        if (afxSlotButtons[i]) {
            const auto& slot = engine.getAfxKit().getSlot(i);
            afxSlotButtons[i]->setButtonText(juce::String(i + 1) + ": " + slot.name);
            afxSlotButtons[i]->setToggleState(i == selectedAfxSlot, juce::dontSendNotification);
        }
    }
    if (afxSlotDetailLabel) {
        const auto& slot = engine.getAfxKit().getSlot(selectedAfxSlot);
        const char* fModels[4] = { "SSI2144 24dB Ladder", "Liquid Ripples OTA", "Shelves 4-Band EQ/SVF", "SST Vintage Moog Ladder" };
        int m = std::clamp((int)slot.preset.steppedParams[spFilterModel], 0, 3);
        int cut = (int)scan_potFrom16bits(slot.preset.continuousParams[cpCutoff]);
        int res = (int)scan_potFrom16bits(slot.preset.continuousParams[cpResonance]);
        afxSlotDetailLabel->setText("SLOT " + juce::String(selectedAfxSlot + 1) + " [" + slot.name + "]  |  " +
                                    fModels[m] + "  |  Cutoff: " + juce::String(cut) + "  |  Reso: " + juce::String(res),
                                    juce::dontSendNotification);
    }
    if (afxKeyboardZone) afxKeyboardZone->repaint();
}

void AfxTab::updateFromEngine() {
    const auto& preset = engine.getCurrentPreset();

    safeSetKnob(voiceCountSlider.get(), juce::jlimit(1, SYNTH_VOICE_COUNT, preset.steppedParams[spVoiceCount] + 1));
    uint8_t aPrio = preset.steppedParams[spAssignerPriority];
    for (int i = 0; i < 3; ++i) {
        if (assignerPrioToggles[i])
            assignerPrioToggles[i]->setToggleState(i == aPrio, juce::dontSendNotification);
    }
    updateAfxSlotButtons();
}

void AfxTab::resized() {
    const auto tabBounds = getLocalBounds();

    int colGap = 5;
    int totalW = tabBounds.getWidth();
    int totalH = tabBounds.getHeight();

    int col1W = 270;
    int col2W = totalW - col1W - colGap;
    int col1X = 0;
    int col2X = col1X + col1W + colGap;

    // Card 1: VOICE ALLOCATION & PRIORITY
    voiceAllocCard.setBounds(col1X, 0, col1W, totalH);
    voiceAllocCard.clearDividers();
    voiceAllocCard.addDivider(26, "ACTIVE VOICE COUNT (1 - 6)");
    if (voiceCountSlider) {
        voiceCountSlider->setBounds(col1X + 16, 44, col1W - 32, 34);
    }

    voiceAllocCard.addDivider(106, "NOTE ASSIGNMENT PRIORITY");
    int prioStartY = 126;
    int prioStep = 28;
    for (int i = 0; i < 3; ++i) {
        if (assignerPrioToggles[i]) {
            assignerPrioToggles[i]->setBounds(col1X + 16, prioStartY + i * prioStep, col1W - 32, 22);
        }
    }

    voiceAllocCard.addDivider(228, "MULTI-SOUND AFX ROUTING");
    if (afxVoiceLiveStatusLabel) {
        afxVoiceLiveStatusLabel->setBounds(col1X + 16, 246, col1W - 32, 38);
        afxVoiceLiveStatusLabel->setFont(modernLnf.getCustomFont(10.0f, juce::Font::plain));
        afxVoiceLiveStatusLabel->setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
        afxVoiceLiveStatusLabel->setText(
            "16 parts share six voices. Custom routing uses the selected part's MIDI channel and note zone.",
            juce::dontSendNotification
        );
    }

    // Card 2: AFX SOUND KIT & KEYBOARD ZONE MAPPING
    customRouteToggle.setBounds(16, 288, 238, 22);
    routeEnabled.setBounds(16, 312, 238, 22);
    routeChannel.setBounds(16, 338, 238, 24);
    routeLow.setBounds(16, 366, 238, 24); routeHigh.setBounds(16, 394, 238, 24);
    saveSetupButton.setBounds(16, 426, 115, 24); loadSetupButton.setBounds(139, 426, 115, 24);
    afxKitCard.setBounds(col2X, 0, col2W, totalH);
    afxKitCard.clearDividers();
    afxKitCard.addDivider(26, "AFX SOUND SLOTS (16 SOUND PRESET PROFILES)");

    int gridW = col2W - 32;
    int slotBtnW = (gridW - 3 * 6) / 4;
    int slotBtnH = 24;
    int slotStartY = 42;
    for (int s = 0; s < AFX_SLOT_COUNT; ++s) {
        int col = s % 4;
        int row = s / 4;
        int bx = col2X + 16 + col * (slotBtnW + 6);
        int by = slotStartY + row * (slotBtnH + 5);
        if (afxSlotButtons[s]) {
            afxSlotButtons[s]->setBounds(bx, by, slotBtnW, slotBtnH);
        }
    }

    afxKitCard.addDivider(164, "PRESET ASSIGNMENT & QUICK MAP ACTIONS");
    int row1Y = 180;
    int comboW = 210;
    if (slotPresetCombo) slotPresetCombo->setBounds(col2X + 16, row1Y, comboW, 26);
    if (assignCurrentPresetBtn) assignCurrentPresetBtn->setBounds(col2X + 16 + comboW + 8, row1Y, 220, 26);

    int row2Y = 214;
    int mapBtnW = (gridW - 2 * 6) / 3;
    if (octaveMapBtn) octaveMapBtn->setBounds(col2X + 16, row2Y, mapBtnW, 24);
    if (chromaticMapBtn) chromaticMapBtn->setBounds(col2X + 16 + (mapBtnW + 6), row2Y, mapBtnW, 24);
    if (allToSlotBtn) allToSlotBtn->setBounds(col2X + 16 + (mapBtnW + 6) * 2, row2Y, mapBtnW, 24);

    afxKitCard.addDivider(252, "SELECTED SLOT PARAMETERS");
    if (afxSlotDetailLabel) {
        afxSlotDetailLabel->setBounds(col2X + 16, 266, gridW, 22);
        afxSlotDetailLabel->setFont(modernLnf.getCustomFont(10.5f, juce::Font::bold));
        afxSlotDetailLabel->setColour(juce::Label::textColourId, modernLnf.getTheme().accent);
    }

    afxKitCard.addDivider(298, "KEYBOARD NOTE ZONE MAPPING (CLICK NOTE TO ASSIGN TO SELECTED SLOT)");
    int kbdY = 316;
    int kbdH = std::max(60, totalH - kbdY - 14);
    if (afxKeyboardZone) {
        afxKeyboardZone->setBounds(col2X + 16, kbdY, gridW, kbdH);
    }
}
