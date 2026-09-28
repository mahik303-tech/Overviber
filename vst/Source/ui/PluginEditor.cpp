#include "PluginEditor.h"
#include "../PluginProcessor.h"
#include <iomanip>
#include <sstream>

OvercyclerAudioProcessorEditor::VersionBadgeComponent::VersionBadgeComponent() {
    setComponentID("versionBadgeLabel");
}

void OvercyclerAudioProcessorEditor::VersionBadgeComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(accent.withAlpha(0.25f));
    g.fillRect(bounds);
    g.setColour(accent);
    g.drawRect(bounds, 1.0f);
    g.setFont(ModernFontManager::createFont("D-DIN", 10.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawText(OVERVIBER_VERSION_STRING, bounds, juce::Justification::centred, false);
}

OvercyclerAudioProcessorEditor::TopBarSubtitleComponent::TopBarSubtitleComponent() {
    setComponentID("subtitleLabel");
}

void OvercyclerAudioProcessorEditor::TopBarSubtitleComponent::paint(juce::Graphics& g) {
    g.setFont(ModernFontManager::createFont("D-DIN", 10.5f, juce::Font::bold));
    g.setColour(bodyCol);
    g.drawText("Vibe-coded 6-Voice-Synthesizer", 0, 1, getWidth(), 14, juce::Justification::left, false);

    g.setFont(ModernFontManager::createFont("D-DIN", 9.5f, juce::Font::plain));
    g.setColour(accentCol.withAlpha(0.95f));
    g.drawText("Hybrid Wavetable / Analog Architecture", 0, 15, getWidth(), 14, juce::Justification::left, false);
}

OvercyclerAudioProcessorEditor::OvercyclerAudioProcessorEditor(OvercyclerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), activePage(ClassicUI::PageId::Osc) {

    // Classic controls (hidden initially in modern mode)
    lcdDisplay.setComponentID("lcdDisplay");
    addChildComponent(lcdDisplay);

    for (int i = 0; i < 10; ++i) {
        knobs[i] = std::make_unique<OvercyclerKnob>("Knob" + std::to_string(i));
        knobs[i]->setComponentID("classicKnob" + std::to_string(i));
        knobs[i]->onValueChange = [this, i]() {
            onKnobChanged(i, (float)knobs[i]->getValue());
        };
        addChildComponent(knobs[i].get());
    }

    keypad.setComponentID("keypad");
    keypad.setOnKeyPress([this](char key) {
        handleKeyPress(key);
    });
    addChildComponent(keypad);

    // Modern View (visible by default)
    modernView = std::make_unique<ModernEditorView>(processor.getModel(), &processor);
    modernView->onSkinModeChanged = [this](bool modern) {
        setGuiMode(modern);
    };
    modernView->onWindowScaleChanged = [this](float scale) {
        int baseW = 1100;
        int baseH = 700;
        int targetW = (int)std::round(baseW * scale);
        int targetH = (int)std::round(baseH * scale);
        setSize(targetW, targetH);
        if (modernView) modernView->setSavedWindowScale(scale);
    };
    modernView->onThemeChanged = [this](const ModernTheme& theme) {
        brandLogoLabel.setColour(juce::Label::textColourId, isModernMode ? theme.textTitle : juce::Colour(0xffeaebee));
        versionBadgeLabel.setAccentColour(isModernMode ? theme.accent : juce::Colour(0xff18b5c9));
        subtitleLabel.setColours(isModernMode ? theme.textBody : juce::Colour(0xffe2e5eb),
                                 isModernMode ? theme.accent : juce::Colour(0xff18b5c9));
        if (modernPresetBar) modernPresetBar->setAccentColour(theme.accent);
        if (presetBrowserOverlay) presetBrowserOverlay->setAccentColour(theme.accent);
        if (saveAsModal) saveAsModal->setAccentColour(theme.accent);
        repaint();
    };
    modernView->onOpenWaveBrowser = [this](abx_t osc) {
        if (presetBrowserOverlay) {
            presetBrowserOverlay->setMode(ModernPresetBrowserOverlay::BrowserMode::Waveforms, osc);
            presetBrowserOverlay->setVisible(true);
            presetBrowserOverlay->toFront(true);
        }
    };
    addAndMakeVisible(modernView.get());
    modernView->updateFromEngine();

    auto initTheme = modernView->getTheme();

    // Top-Bar Branding Labels (inspectable in debug mode)
    brandLogoLabel.setComponentID("brandLogoLabel");
    brandLogoLabel.setText("OVERVIBER", juce::dontSendNotification);
    brandLogoLabel.setFont(ModernFontManager::createFont("D-DIN", 21.0f, juce::Font::bold | juce::Font::italic));
    brandLogoLabel.setJustificationType(juce::Justification::centredLeft);
    brandLogoLabel.setColour(juce::Label::textColourId, initTheme.textTitle);
    addAndMakeVisible(brandLogoLabel);

    versionBadgeLabel.setAccentColour(initTheme.accent);
    addAndMakeVisible(versionBadgeLabel);

    subtitleLabel.setColours(initTheme.textBody, initTheme.accent);
    addAndMakeVisible(subtitleLabel);

    // Modern Preset Manager
    modernPresetBar = std::make_unique<ModernPresetBar>(processor.getModel(), &processor);
    modernPresetBar->setAccentColour(modernView->getTheme().accent);
    modernPresetBar->onToggleBrowser = [this]() {
        if (presetBrowserOverlay) {
            bool show = !presetBrowserOverlay->isVisible();
            presetBrowserOverlay->setVisible(show);
            if (show) presetBrowserOverlay->toFront(true);
        }
    };
    modernPresetBar->onOpenSaveAs = [this]() {
        if (saveAsModal) {
            auto& pm = processor.getModel().getPresetManager();
            juce::String curName = pm.getPresetName(processor.getCurrentProgram());
            saveAsModal->setAccentColour(modernView->getTheme().accent);
            saveAsModal->show(curName, [this](const juce::String& newName) {
                auto& pm = processor.getModel().getPresetManager();
                auto currentPreset = processor.getModel().getCurrentPreset();
                int curIdx = processor.getCurrentProgram();
                juce::String curName = pm.getPresetName(curIdx);

                if (newName.trim() == curName.trim()) {
                    currentPreset.presetName = newName.toStdString();
                    if (pm.savePreset(curIdx, currentPreset)) {
                        if (modernPresetBar) {
                            modernPresetBar->updateDisplay();
                            modernPresetBar->setSaveFlashText("SAVED!");
                        }
                        if (presetBrowserOverlay) presetBrowserOverlay->refreshPresetList();
                    }
                } else {
                    if (pm.saveNewPreset(newName.toStdString(), currentPreset)) {
                        int newIdx = pm.getPresetCount() - 1;
                        processor.setCurrentProgram(newIdx);
                        if (modernPresetBar) {
                            modernPresetBar->updateDisplay();
                            modernPresetBar->setSaveFlashText("SAVED!");
                        }
                        if (presetBrowserOverlay) presetBrowserOverlay->refreshPresetList();
                        if (modernView) modernView->updateFromEngine();
                    }
                }
            });
            saveAsModal->toFront(true);
        }
    };
    modernPresetBar->onPresetChanged = [this](int /*newIdx*/) {
        updateKnobMappings();
        if (modernView) modernView->updateFromEngine();
        if (presetBrowserOverlay) presetBrowserOverlay->repaint();
    };
    addAndMakeVisible(modernPresetBar.get());

    presetBrowserOverlay = std::make_unique<ModernPresetBrowserOverlay>(processor.getModel(), &processor);
    presetBrowserOverlay->setAccentColour(modernView->getTheme().accent);
    presetBrowserOverlay->onPresetSelected = [this](int /*idx*/) {
        updateKnobMappings();
        if (modernView) modernView->updateFromEngine();
        if (modernPresetBar) modernPresetBar->updateDisplay();
    };
    presetBrowserOverlay->onWaveSelected = [this](abx_t /*osc*/, const juce::String& /*bank*/, const juce::String& /*wave*/) {
        if (modernView) modernView->updateFromEngine();
    };
    addChildComponent(presetBrowserOverlay.get());

    saveAsModal = std::make_unique<ModernSaveAsModal>();
    addChildComponent(saveAsModal.get());

    // Classic UI Switch Button (shown only when in Classic mode)
    guiModeButton.setComponentID("guiModeButton");
    guiModeButton.setButtonText("SWITCH TO MODERN SKIN");
    guiModeButton.setClickingTogglesState(false);
    guiModeButton.onClick = [this]() {
        setGuiMode(true);
    };
    addChildComponent(guiModeButton);

    // Classic Preset selector & navigation (shown only when in Classic mode)
    presetSelector.setComponentID("presetSelector");
    addChildComponent(presetSelector);
    auto& pm = processor.getModel().getPresetManager();
    for (int i = 0; i < pm.getPresetCount(); ++i) {
        presetSelector.addItem(pm.getPresetName(i), i + 1);
    }
    presetSelector.setSelectedId(processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetSelector.onChange = [this]() {
        int idx = presetSelector.getSelectedId() - 1;
        if (idx >= 0) {
            processor.setCurrentProgram(idx);
            updateKnobMappings();
            if (modernView != nullptr) modernView->updateFromEngine();
            if (modernPresetBar != nullptr) modernPresetBar->updateDisplay();
        }
    };

    prevPresetBtn.setComponentID("prevPresetBtn");
    prevPresetBtn.onClick = [this]() {
        int idx = processor.getCurrentProgram() - 1;
        if (idx >= 0) {
            presetSelector.setSelectedId(idx + 1, juce::sendNotification);
        }
    };
    addChildComponent(prevPresetBtn);

    nextPresetBtn.setComponentID("nextPresetBtn");
    nextPresetBtn.onClick = [this]() {
        int idx = processor.getCurrentProgram() + 1;
        if (idx < processor.getNumPrograms()) {
            presetSelector.setSelectedId(idx + 1, juce::sendNotification);
        }
    };
    addChildComponent(nextPresetBtn);

    switchPage(ClassicUI::PageId::Osc);

    setResizable(true, true);
    setResizeLimits(960, 610, 1920, 1220);
    getConstrainer()->setFixedAspectRatio(1100.0 / 700.0);

    float initScale = modernView ? modernView->getSavedWindowScale() : 1.0f;
    if (initScale >= 0.5f && initScale <= 2.5f) {
        int targetW = (int)std::round(1100.0f * initScale);
        int targetH = (int)std::round(700.0f * initScale);
        setSize(targetW, targetH);
    } else {
        setSize(1100, 700);
    }
    startTimerHz(30);
}

OvercyclerAudioProcessorEditor::~OvercyclerAudioProcessorEditor() {
    stopTimer();
}

void OvercyclerAudioProcessorEditor::setGuiMode(bool modern) {
    isModernMode = modern;

    auto theme = modernView ? modernView->getTheme() : ModernTheme::getPresetThemes()[0];
    brandLogoLabel.setColour(juce::Label::textColourId, isModernMode ? theme.textTitle : juce::Colour(0xffeaebee));
    versionBadgeLabel.setAccentColour(isModernMode ? theme.accent : juce::Colour(0xff18b5c9));
    subtitleLabel.setColours(isModernMode ? theme.textBody : juce::Colour(0xffe2e5eb),
                             isModernMode ? theme.accent : juce::Colour(0xff18b5c9));

    lcdDisplay.setVisible(!isModernMode);
    keypad.setVisible(!isModernMode);
    for (int i = 0; i < 10; ++i) {
        knobs[i]->setVisible(!isModernMode);
    }

    presetSelector.setVisible(!isModernMode);
    prevPresetBtn.setVisible(!isModernMode);
    nextPresetBtn.setVisible(!isModernMode);
    guiModeButton.setVisible(!isModernMode);

    if (modernView != nullptr) {
        modernView->setVisible(isModernMode);
        if (isModernMode) {
            modernView->updateFromEngine();
        }
    }

    if (modernPresetBar != nullptr) {
        modernPresetBar->setVisible(isModernMode);
        if (isModernMode) {
            modernPresetBar->updateDisplay();
        }
    }

    if (!isModernMode) {
        if (presetBrowserOverlay != nullptr) presetBrowserOverlay->setVisible(false);
        if (saveAsModal != nullptr) saveAsModal->setVisible(false);
        updateKnobMappings();
    }

    resized();
    repaint();
}

void OvercyclerAudioProcessorEditor::switchPage(ClassicUI::PageId newPage) {
    activePage = newPage;
    lcdDisplay.setPage(newPage);

    char k = '1';
    switch (newPage) {
    case ClassicUI::PageId::Osc: k = '1'; break;
    case ClassicUI::PageId::WMod: k = '2'; break;
    case ClassicUI::PageId::Fil: k = '3'; break;
    case ClassicUI::PageId::Amp: k = '4'; break;
    case ClassicUI::PageId::Lfo1: k = '5'; break;
    case ClassicUI::PageId::Lfo2: k = '6'; break;
    case ClassicUI::PageId::Arp: k = '7'; break;
    case ClassicUI::PageId::Seq: k = '8'; break;
    case ClassicUI::PageId::Misc: k = '9'; break;
    case ClassicUI::PageId::Presets: k = '0'; break;
    case ClassicUI::PageId::Help: k = '*'; break;
    }
    keypad.setActiveKey(k);

    const auto& pageDef = ClassicUI::SchemaRegistry::getPage(activePage);
    keypad.setActionSublabels(pageDef.buttons[0].shortName, pageDef.buttons[1].shortName,
                              pageDef.buttons[2].shortName, pageDef.buttons[3].shortName);

    isNumericInputActive = false;
    numericBuffer.clear();

    for (int i = 0; i < 10; ++i) {
        knobAcquired[i] = true;
        knobs[i]->setPendingPickup(false, false);
    }

    updateKnobMappings();
}

void OvercyclerAudioProcessorEditor::handleKeyPress(char key) {
    // 1. Numeric Keypad Entry mode ('*')
    if (key == '*') {
        if (isNumericInputActive) {
            // Cancel input
            isNumericInputActive = false;
            numericBuffer.clear();
            lcdDisplay.resetToNormalScreen();
        } else {
            isNumericInputActive = true;
            numericBuffer.clear();
            std::string prompt = (activePage == ClassicUI::PageId::Presets) ? "PRESET NUMBER (0-999)" :
                                 (lastEditedKnobIndex >= 0) ? ClassicUI::SchemaRegistry::getPage(activePage).pots[lastEditedKnobIndex].longName : "ACTIVE PARAMETER";
            lcdDisplay.showNumericInput(prompt, "___");
        }
        return;
    }

    if (isNumericInputActive) {
        if (key >= '0' && key <= '9') {
            handleNumericInput(key);
            return;
        } else {
            isNumericInputActive = false;
            numericBuffer.clear();
        }
    }

    // 2. Direct Page Selection
    switch (key) {
    case '1': switchPage(ClassicUI::PageId::Osc); return;
    case '2': switchPage(ClassicUI::PageId::WMod); return;
    case '3': switchPage(ClassicUI::PageId::Fil); return;
    case '4': switchPage(ClassicUI::PageId::Amp); return;
    case '5': switchPage(ClassicUI::PageId::Lfo1); return;
    case '6': switchPage(ClassicUI::PageId::Lfo2); return;
    case '7': switchPage(ClassicUI::PageId::Arp); return;
    case '8': switchPage(ClassicUI::PageId::Seq); return;
    case '9': switchPage(ClassicUI::PageId::Misc); return;
    case '0': switchPage(ClassicUI::PageId::Presets); return;
    case 'A':
    case 'B':
    case 'C':
    case 'D':
        handleActionKey(key);
        return;
    case '#': {
        // Transpose Toggle / Reset
        transposeOffset = (transposeOffset == 0) ? 12 : (transposeOffset == 12) ? -12 : 0;
        processor.getModel().getArpeggiator().setTranspose((int8_t)transposeOffset);
        auto& preset = processor.getModel().getCurrentPreset();
        int curTune = 500 + transposeOffset;
        preset.continuousParams[cpMasterTune] = (uint16_t)scan_potTo16bits(curTune);
        processor.getModel().applyPreset();
        updateKnobMappings();
        lcdDisplay.showPotEdit(9, "KEYBOARD TRANSPOSE", (transposeOffset >= 0 ? "+" : "") + std::to_string(transposeOffset) + " ST", (float)transposeOffset, -24, 24);
        return;
    }
    default:
        break;
    }
}

void OvercyclerAudioProcessorEditor::handleNumericInput(char digit) {
    numericBuffer.push_back(digit);
    std::string disp = numericBuffer;
    while (disp.size() < 3) disp.push_back('_');

    std::string prompt = (activePage == ClassicUI::PageId::Presets) ? "PRESET NUMBER (0-999)" :
                         (lastEditedKnobIndex >= 0) ? ClassicUI::SchemaRegistry::getPage(activePage).pots[lastEditedKnobIndex].longName : "ACTIVE PARAMETER";
    lcdDisplay.showNumericInput(prompt, disp);

    if (numericBuffer.size() == 3) {
        int val = std::stoi(numericBuffer);
        isNumericInputActive = false;
        numericBuffer.clear();

        if (activePage == ClassicUI::PageId::Presets) {
            if (val >= 0 && val < processor.getNumPrograms()) {
                processor.setCurrentProgram(val);
                presetSelector.setSelectedId(val + 1, juce::dontSendNotification);
            }
        } else if (lastEditedKnobIndex >= 0) {
            onKnobChanged(lastEditedKnobIndex, (float)val);
        }
        updateKnobMappings();
    }
}

void OvercyclerAudioProcessorEditor::handleActionKey(char key) {
    int bIdx = (key == 'A') ? 0 : (key == 'B') ? 1 : (key == 'C') ? 2 : 3;
    const auto& pageDef = ClassicUI::SchemaRegistry::getPage(activePage);
    const auto& bDef = pageDef.buttons[bIdx];
    auto& preset = processor.getModel().getCurrentPreset();

    if (bDef.kind == ClassicUI::ParamKind::Stepped) {
        int curVal = preset.steppedParams[bDef.sp];
        int numOpts = (int)bDef.options.size();
        int nextVal = (curVal + 1) % std::max(1, numOpts);
        processor.setSteppedParamFromUI(bDef.sp, (uint8_t)nextVal);
        lcdDisplay.showButtonEdit(bIdx, bDef.longName, bDef.options[nextVal], bDef.options, nextVal);
        updateKnobMappings();
        return;
    }

    if (bDef.kind == ClassicUI::ParamKind::Custom) {
        switch (bDef.customId) {
        case ClassicUI::CustomActionId::AXoSwap: {
            std::swap(preset.oscBank[abxAMain], preset.oscBank[abxACrossover]);
            std::swap(preset.oscWave[abxAMain], preset.oscWave[abxACrossover]);
            processor.getModel().getWaveManager().loadWave(abxAMain, preset.oscBank[abxAMain], preset.oscWave[abxAMain]);
            processor.getModel().applyPreset();
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, "SWAPPED", { "Normal", "Swapped" }, 1);
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::BXoSwap: {
            std::swap(preset.oscBank[abxBMain], preset.oscBank[abxBCrossover]);
            std::swap(preset.oscWave[abxBMain], preset.oscWave[abxBCrossover]);
            processor.getModel().getWaveManager().loadWave(abxBMain, preset.oscBank[abxBMain], preset.oscWave[abxBMain]);
            processor.getModel().applyPreset();
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, "SWAPPED", { "Normal", "Swapped" }, 1);
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::WModEnvType: {
            int cur = preset.steppedParams[spWModEnvLin] * 2 + preset.steppedParams[spWModEnvSlow];
            int nxt = (cur + 1) % 4;
            processor.setSteppedParamFromUI(spWModEnvLin, (uint8_t)((nxt >> 1) & 1));
            processor.setSteppedParamFromUI(spWModEnvSlow, (uint8_t)(nxt & 1));
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, bDef.options[nxt], bDef.options, nxt);
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::FilEnvType: {
            int cur = preset.steppedParams[spFilEnvLin] * 2 + preset.steppedParams[spFilEnvSlow];
            int nxt = (cur + 1) % 4;
            processor.setSteppedParamFromUI(spFilEnvLin, (uint8_t)((nxt >> 1) & 1));
            processor.setSteppedParamFromUI(spFilEnvSlow, (uint8_t)(nxt & 1));
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, bDef.options[nxt], bDef.options, nxt);
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::AmpEnvType: {
            int cur = preset.steppedParams[spAmpEnvLin] * 2 + preset.steppedParams[spAmpEnvSlow];
            int nxt = (cur + 1) % 4;
            processor.setSteppedParamFromUI(spAmpEnvLin, (uint8_t)((nxt >> 1) & 1));
            processor.setSteppedParamFromUI(spAmpEnvSlow, (uint8_t)(nxt & 1));
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, bDef.options[nxt], bDef.options, nxt);
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::ArpMode: {
            int cur = preset.steppedParams[spArpMode];
            int nxt = (cur + 1) % (int)bDef.options.size();
            processor.setSteppedParamFromUI(spArpMode, (uint8_t)nxt);
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, bDef.options[nxt], bDef.options, nxt);
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::ArpHold: {
            int nxt = !preset.steppedParams[spArpHold];
            processor.setSteppedParamFromUI(spArpHold, (uint8_t)nxt);
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, nxt ? "On  " : "Off ", { "Off ", "On  " }, nxt);
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::LoadBasic: {
            preset.setDefaults();
            processor.getModel().applyPreset();
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, "BASIC PATCH LOADED", { "Init" }, 0);
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::MidiPanic: {
            processor.getModel().panic();
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, "ALL VOICES OFF", { "Panic" }, 0);
            break;
        }
        case ClassicUI::CustomActionId::ShowHelp: {
            switchPage(ClassicUI::PageId::Help);
            break;
        }
        case ClassicUI::CustomActionId::TuneFilters: {
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, "FILTER TUNING COMPLETE", { "Done" }, 0);
            break;
        }
        case ClassicUI::CustomActionId::PresetLoad: {
            processor.setCurrentProgram(processor.getCurrentProgram());
            updateKnobMappings();
            break;
        }
        case ClassicUI::CustomActionId::PresetSave: {
            auto& pm = processor.getModel().getPresetManager();
            pm.savePreset(processor.getCurrentProgram(), preset);
            lcdDisplay.showButtonEdit(bIdx, bDef.longName, "PRESET SAVED", { "Saved" }, 0);
            break;
        }
        case ClassicUI::CustomActionId::PresetPrev: {
            prevPresetBtn.triggerClick();
            break;
        }
        case ClassicUI::CustomActionId::PresetNext: {
            nextPresetBtn.triggerClick();
            break;
        }
        default:
            break;
        }
    }
}

void OvercyclerAudioProcessorEditor::updateKnobMappings() {
    const auto& pageDef = ClassicUI::SchemaRegistry::getPage(activePage);
    auto& preset = processor.getModel().getCurrentPreset();
    auto& waveMgr = processor.getModel().getWaveManager();

    // 1. Update 10 Potentiometers
    for (int i = 0; i < 10; ++i) {
        const auto& pDef = pageDef.pots[i];
        if (pDef.kind == ClassicUI::ParamKind::None) {
            knobs[i]->setEnabled(false);
            lcdDisplay.setPotParam(i, "----", "    ", 0.0f);
            continue;
        }

        knobs[i]->setEnabled(true);

        if (pDef.kind == ClassicUI::ParamKind::Continuous) {
            knobs[i]->setRange(pDef.minVal, pDef.maxVal, pDef.step);
            int rawVal = scan_potFrom16bits(preset.continuousParams[pDef.cp]);
            float potVal = pDef.zeroCentered ? (float)(rawVal - 500) : (float)rawVal;

            knobs[i]->setValue(potVal, juce::dontSendNotification);

            // Formatted string for LCD
            std::string dispStr;
            if (activePage == ClassicUI::PageId::Osc && (pDef.cp == cpAFreq || pDef.cp == cpBFreq)) {
                int mode = preset.steppedParams[spChromaticPitch];
                if (mode == 1 || mode == 2) { // Semi or Oct
                    int note = (int)(potVal / 1000.0f * 128.0f);
                    const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                    dispStr = std::string(notes[note % 12]) + std::to_string(note / 12);
                } else {
                    dispStr = std::to_string((int)potVal);
                }
            } else if (pDef.zeroCentered) {
                dispStr = (potVal >= 0 ? "+" : "") + std::to_string((int)potVal);
            } else {
                dispStr = std::to_string((int)potVal);
            }

            float norm = (potVal - pDef.minVal) / std::max(1.0f, pDef.maxVal - pDef.minVal);
            lcdDisplay.setPotParam(i, pDef.shortName, dispStr, norm);
        } else if (pDef.kind == ClassicUI::ParamKind::Stepped) {
            knobs[i]->setRange(pDef.minVal, pDef.maxVal, pDef.step);
            int stepVal = preset.steppedParams[pDef.sp];
            if (pDef.sp == spVoiceCount) stepVal += 1;

            knobs[i]->setValue((float)stepVal, juce::dontSendNotification);

            std::string dispStr = (stepVal >= 0 && stepVal < (int)pDef.options.size()) ?
                                  pDef.options[stepVal] : std::to_string(stepVal);
            float norm = (float)(stepVal - pDef.minVal) / std::max(1.0f, pDef.maxVal - pDef.minVal);
            lcdDisplay.setPotParam(i, pDef.shortName, dispStr, norm);
        } else if (pDef.kind == ClassicUI::ParamKind::Custom) {
            switch (pDef.customId) {
            case ClassicUI::CustomActionId::OscABank: {
                const auto& banks = waveMgr.getBankNames();
                knobs[i]->setRange(0, std::max(0, (int)banks.size() - 1), 1);
                std::string curBank = waveMgr.getCurrentBank(abxAMain);
                auto it = std::find(banks.begin(), banks.end(), curBank);
                int bIdx = (it != banks.end()) ? (int)(it - banks.begin()) : 0;
                knobs[i]->setValue((float)bIdx, juce::dontSendNotification);
                lcdDisplay.setPotParam(i, "ABnk", curBank.substr(0, 4), (float)bIdx / std::max(1, (int)banks.size() - 1));
                break;
            }
            case ClassicUI::CustomActionId::OscAWave: {
                std::string curBank = waveMgr.getCurrentBank(abxAMain);
                const auto& waves = waveMgr.getWaveNames(curBank);
                knobs[i]->setRange(0, std::max(0, (int)waves.size() - 1), 1);
                std::string curWave = waveMgr.getCurrentWave(abxAMain);
                auto it = std::find(waves.begin(), waves.end(), curWave);
                int wIdx = (it != waves.end()) ? (int)(it - waves.begin()) : 0;
                knobs[i]->setValue((float)wIdx, juce::dontSendNotification);
                lcdDisplay.setPotParam(i, "AWav", curWave.substr(0, 4), (float)wIdx / std::max(1, (int)waves.size() - 1));
                break;
            }
            case ClassicUI::CustomActionId::OscBBank: {
                const auto& banks = waveMgr.getBankNames();
                knobs[i]->setRange(0, std::max(0, (int)banks.size() - 1), 1);
                std::string curBank = waveMgr.getCurrentBank(abxBMain);
                auto it = std::find(banks.begin(), banks.end(), curBank);
                int bIdx = (it != banks.end()) ? (int)(it - banks.begin()) : 0;
                knobs[i]->setValue((float)bIdx, juce::dontSendNotification);
                lcdDisplay.setPotParam(i, "BBnk", curBank.substr(0, 4), (float)bIdx / std::max(1, (int)banks.size() - 1));
                break;
            }
            case ClassicUI::CustomActionId::OscBWave: {
                std::string curBank = waveMgr.getCurrentBank(abxBMain);
                const auto& waves = waveMgr.getWaveNames(curBank);
                knobs[i]->setRange(0, std::max(0, (int)waves.size() - 1), 1);
                std::string curWave = waveMgr.getCurrentWave(abxBMain);
                auto it = std::find(waves.begin(), waves.end(), curWave);
                int wIdx = (it != waves.end()) ? (int)(it - waves.begin()) : 0;
                knobs[i]->setValue((float)wIdx, juce::dontSendNotification);
                lcdDisplay.setPotParam(i, "BWav", curWave.substr(0, 4), (float)wIdx / std::max(1, (int)waves.size() - 1));
                break;
            }
            case ClassicUI::CustomActionId::TransposeValue: {
                knobs[i]->setRange(-24, 24, 1);
                knobs[i]->setValue((float)transposeOffset, juce::dontSendNotification);
                lcdDisplay.setPotParam(i, "Trsp", (transposeOffset >= 0 ? "+" : "") + std::to_string(transposeOffset), (transposeOffset + 24.0f) / 48.0f);
                break;
            }
            case ClassicUI::CustomActionId::MidiChannel: {
                const int channel = processor.getMidiInputChannel();
                knobs[i]->setRange(0, 16, 1);
                knobs[i]->setValue((float)channel, juce::dontSendNotification);
                const std::string display = channel == 0 ? "Omni" : "Ch" + std::to_string(channel);
                lcdDisplay.setPotParam(i, "MidC", display, channel / 16.0f);
                break;
            }
            default:
                break;
            }
        }
    }

    // 2. Update 4 Buttons (A, B, C, D)
    for (int b = 0; b < 4; ++b) {
        const auto& bDef = pageDef.buttons[b];
        if (bDef.kind == ClassicUI::ParamKind::None) {
            lcdDisplay.setButtonParam(b, "----", "");
            continue;
        }

        if (bDef.kind == ClassicUI::ParamKind::Stepped) {
            int stepVal = preset.steppedParams[bDef.sp];
            std::string dispStr = (stepVal >= 0 && stepVal < (int)bDef.options.size()) ?
                                  bDef.options[stepVal] : std::to_string(stepVal);
            lcdDisplay.setButtonParam(b, bDef.shortName, dispStr);
        } else if (bDef.kind == ClassicUI::ParamKind::Custom) {
            switch (bDef.customId) {
            case ClassicUI::CustomActionId::AXoSwap:
            case ClassicUI::CustomActionId::BXoSwap:
                lcdDisplay.setButtonParam(b, bDef.shortName, "0000");
                break;
            case ClassicUI::CustomActionId::WModEnvType: {
                int cur = preset.steppedParams[spWModEnvLin] * 2 + preset.steppedParams[spWModEnvSlow];
                lcdDisplay.setButtonParam(b, "WEnT", bDef.options[cur % 4]);
                break;
            }
            case ClassicUI::CustomActionId::FilEnvType: {
                int cur = preset.steppedParams[spFilEnvLin] * 2 + preset.steppedParams[spFilEnvSlow];
                lcdDisplay.setButtonParam(b, "FEnT", bDef.options[cur % 4]);
                break;
            }
            case ClassicUI::CustomActionId::AmpEnvType: {
                int cur = preset.steppedParams[spAmpEnvLin] * 2 + preset.steppedParams[spAmpEnvSlow];
                lcdDisplay.setButtonParam(b, "AEnT", bDef.options[cur % 4]);
                break;
            }
            case ClassicUI::CustomActionId::ArpMode: {
                int m = preset.steppedParams[spArpMode];
                lcdDisplay.setButtonParam(b, "AMod", (m >= 0 && m < (int)bDef.options.size()) ? bDef.options[m] : "Off ");
                break;
            }
            case ClassicUI::CustomActionId::ArpHold: {
                int h = preset.steppedParams[spArpHold];
                lcdDisplay.setButtonParam(b, "AHld", h ? "On  " : "Off ");
                break;
            }
            default:
                lcdDisplay.setButtonParam(b, bDef.shortName, "");
                break;
            }
        }
    }
}

void OvercyclerAudioProcessorEditor::onKnobChanged(int knobIndex, float value) {
    const auto& pageDef = ClassicUI::SchemaRegistry::getPage(activePage);
    const auto& pDef = pageDef.pots[knobIndex];
    lastEditedKnobIndex = knobIndex;

    auto& preset = processor.getModel().getCurrentPreset();
    auto& waveMgr = processor.getModel().getWaveManager();

    if (pDef.kind == ClassicUI::ParamKind::Continuous) {
        float potVal = pDef.zeroCentered ? (value + 500.0f) : value;
        processor.setContinuousParamFromUI(pDef.cp, potVal);

        std::string valStr = (pDef.zeroCentered && value >= 0 ? "+" : "") + std::to_string((int)value);
        lcdDisplay.showPotEdit(knobIndex, pDef.longName, valStr, value, pDef.minVal, pDef.maxVal);
    } else if (pDef.kind == ClassicUI::ParamKind::Stepped) {
        uint8_t stepVal = (pDef.sp == spVoiceCount) ? (uint8_t)(value - 1) : (uint8_t)value;
        processor.setSteppedParamFromUI(pDef.sp, stepVal);

        std::string valStr = ((int)value >= 0 && (int)value < (int)pDef.options.size()) ?
                             pDef.options[(int)value] : std::to_string((int)value);
        lcdDisplay.showPotEdit(knobIndex, pDef.longName, valStr, value, pDef.minVal, pDef.maxVal);
    } else if (pDef.kind == ClassicUI::ParamKind::Custom) {
        switch (pDef.customId) {
        case ClassicUI::CustomActionId::OscABank: {
            const auto& banks = waveMgr.getBankNames();
            int idx = juce::jlimit(0, (int)banks.size() - 1, (int)value);
            std::string newBank = banks[idx];
            preset.oscBank[abxAMain] = newBank;
            const auto& waves = waveMgr.getWaveNames(newBank);
            if (!waves.empty()) {
                preset.oscWave[abxAMain] = waves[0];
                waveMgr.loadWave(abxAMain, newBank, waves[0]);
            }
            lcdDisplay.showPotEdit(knobIndex, "OSC A BANK", newBank, value, 0, (float)banks.size() - 1);
            break;
        }
        case ClassicUI::CustomActionId::OscAWave: {
            std::string curBank = waveMgr.getCurrentBank(abxAMain);
            const auto& waves = waveMgr.getWaveNames(curBank);
            int idx = juce::jlimit(0, (int)waves.size() - 1, (int)value);
            std::string newWave = waves[idx];
            preset.oscWave[abxAMain] = newWave;
            waveMgr.loadWave(abxAMain, curBank, newWave);
            lcdDisplay.showWaveformPreview(abxAMain, curBank, newWave, waveMgr.getWaveData(abxAMain), WTOSC_SAMPLE_COUNT);
            break;
        }
        case ClassicUI::CustomActionId::OscBBank: {
            const auto& banks = waveMgr.getBankNames();
            int idx = juce::jlimit(0, (int)banks.size() - 1, (int)value);
            std::string newBank = banks[idx];
            preset.oscBank[abxBMain] = newBank;
            const auto& waves = waveMgr.getWaveNames(newBank);
            if (!waves.empty()) {
                preset.oscWave[abxBMain] = waves[0];
                waveMgr.loadWave(abxBMain, newBank, waves[0]);
            }
            lcdDisplay.showPotEdit(knobIndex, "OSC B BANK", newBank, value, 0, (float)banks.size() - 1);
            break;
        }
        case ClassicUI::CustomActionId::OscBWave: {
            std::string curBank = waveMgr.getCurrentBank(abxBMain);
            const auto& waves = waveMgr.getWaveNames(curBank);
            int idx = juce::jlimit(0, (int)waves.size() - 1, (int)value);
            std::string newWave = waves[idx];
            preset.oscWave[abxBMain] = newWave;
            waveMgr.loadWave(abxBMain, curBank, newWave);
            lcdDisplay.showWaveformPreview(abxBMain, curBank, newWave, waveMgr.getWaveData(abxBMain), WTOSC_SAMPLE_COUNT);
            break;
        }
        case ClassicUI::CustomActionId::TransposeValue: {
            transposeOffset = (int)value;
            processor.getModel().getArpeggiator().setTranspose((int8_t)transposeOffset);
            lcdDisplay.showPotEdit(knobIndex, "KEYBOARD TRANSPOSE", (transposeOffset >= 0 ? "+" : "") + std::to_string(transposeOffset) + " ST", value, -24, 24);
            break;
        }
        case ClassicUI::CustomActionId::MidiChannel: {
            const int channel = juce::jlimit(0, 16, (int)value);
            processor.setMidiInputChannel(channel);
            const std::string display = channel == 0 ? "Omni" : "Ch " + std::to_string(channel);
            lcdDisplay.showPotEdit(knobIndex, "MIDI IN CHANNEL", display, (float)channel, 0, 16);
            break;
        }
        default:
            break;
        }
    }

    updateKnobMappings();
}

void OvercyclerAudioProcessorEditor::timerCallback() {
    auto& model = processor.getModel();
    if (modernPresetBar) modernPresetBar->updateDisplay();

    if (!isModernMode) {
        lcdDisplay.setPresetInfo(model.getCurrentPreset().presetName, processor.getCurrentProgram(), false);

        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            int32_t lvl = model.getVoiceAmpLevel(v);
            lcdDisplay.setVoiceActivity(v, lvl > 100, (float)lvl / 65535.0f);
        }

        if (processor.checkAndResetHostParamsChanged()) {
            updateKnobMappings();
        }
    }
}

void OvercyclerAudioProcessorEditor::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto theme = modernView ? modernView->getTheme() : ModernTheme::getPresetThemes()[0];

    if (isModernMode) {
        // Top 50px header background matches active modern palette theme
        juce::ColourGradient headerGrad(theme.cardHeader.brighter(0.04f), 0, 0,
                                         theme.windowBg, 0, 50.0f, false);
        g.setGradientFill(headerGrad);
        g.fillRect(0.0f, 0.0f, bounds.getWidth(), 50.0f);

        // Header bottom separators
        g.setColour(theme.cardBorder);
        g.drawHorizontalLine(49, 0.0f, bounds.getWidth());
        g.setColour(theme.accent.withAlpha(0.6f));
        g.drawHorizontalLine(50, 0.0f, bounds.getWidth());

        // Body background
        g.setColour(theme.windowBg);
        g.fillRect(0.0f, 50.0f, bounds.getWidth(), bounds.getHeight() - 50.0f);
    } else {
        // Dark metallic chassis background (authentic Hammond PT-10 texture)
        juce::ColourGradient chassisGrad(juce::Colour(0xff222327), 0, 0,
                                         juce::Colour(0xff131416), 0, bounds.getHeight(), false);
        g.setGradientFill(chassisGrad);
        g.fillRect(bounds);

        // Subtle textured noise grain simulation
        g.setColour(juce::Colour(0x06ffffff));
        for (float y = 50.0f; y < bounds.getBottom(); y += 4.0f) {
            g.drawHorizontalLine((int)y, 0.0f, bounds.getWidth());
        }
    }

    // Bevel outer frame (sharp industrial corners)
    g.setColour(isModernMode ? theme.cardBorder : juce::Colour(0xff33353b));
    g.drawRect(bounds.reduced(0.5f), 1.5f);

    // Allen hex bolts around chassis
    g.setColour(isModernMode ? theme.cardBorder.brighter(0.2f) : juce::Colour(0xff9ea0a8));
    float boltRadius = 4.0f;
    float boltOffsets[4][2] = {
        {18.0f, 18.0f},
        {bounds.getRight() - 18.0f, 18.0f},
        {18.0f, bounds.getBottom() - 18.0f},
        {bounds.getRight() - 18.0f, bounds.getBottom() - 18.0f}
    };
    for (auto& b : boltOffsets) {
        g.fillEllipse(b[0] - boltRadius, b[1] - boltRadius, boltRadius * 2.0f, boltRadius * 2.0f);
        g.setColour(isModernMode ? theme.windowBg : juce::Colour(0xff1a1b1d));
        g.drawEllipse(b[0] - boltRadius, b[1] - boltRadius, boltRadius * 2.0f, boltRadius * 2.0f, 1.0f);
        g.setColour(isModernMode ? theme.cardBorder.brighter(0.2f) : juce::Colour(0xff9ea0a8));
    }

    if (!isModernMode) {
        // Silkscreen white connection lines from LCD display down to the knobs
        g.setColour(juce::Colour(0x50ffffff));
        auto lcdArea = lcdDisplay.getBounds().toFloat().reduced(8.0f);
        float colW = (lcdArea.getWidth() - 130.0f) / 5.0f;

        for (int i = 0; i < 5; ++i) {
            float lx = lcdArea.getX() + i * colW + colW * 0.5f + 6.0f;
            auto topKnobCenter = knobs[i]->getBounds().getCentre().toFloat();
            auto bottomKnobCenter = knobs[i + 5]->getBounds().getCentre().toFloat();

            // Line from LCD to top knob
            g.drawLine(lx, (float)lcdDisplay.getBottom(), lx, topKnobCenter.y - 32.0f, 1.5f);

            // Vertical line linking top knob to bottom knob
            g.drawLine(topKnobCenter.x, topKnobCenter.y + 32.0f, bottomKnobCenter.x, bottomKnobCenter.y - 32.0f, 1.5f);
        }

        // Silkscreen Panel Legend: Active Page Title
        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::bold));
        g.setColour(juce::Colour(0x90ffffff));
        const auto& pageDef = ClassicUI::SchemaRegistry::getPage(activePage);
        std::string pageTitle = "ACTIVE PAGE: " + std::string(pageDef.name);
        g.drawText(pageTitle, lcdDisplay.getX(), lcdDisplay.getY() - 20, 300, 16, juce::Justification::left, false);
    }
}

void OvercyclerAudioProcessorEditor::resized() {
    // Top-bar branding labels are positioned at the top left
    brandLogoLabel.setBounds(28, 12, 120, 26);
    versionBadgeLabel.setBounds(152, 16, 40, 18);
    subtitleLabel.setBounds(200, 10, 230, 30);

    if (isModernMode) {
        guiModeButton.setVisible(false);
        presetSelector.setVisible(false);
        prevPresetBtn.setVisible(false);
        nextPresetBtn.setVisible(false);

        if (modernPresetBar != nullptr) {
            modernPresetBar->setVisible(true);
            int barW = std::min(630, getWidth() - 460);
            modernPresetBar->setBounds(getWidth() - barW - 16, 12, barW, 26);
        }

        if (modernView != nullptr) {
            modernView->setBounds(0, 50, getWidth(), getHeight() - 50);
        }

        if (presetBrowserOverlay != nullptr) {
            presetBrowserOverlay->setBounds(16, 48, getWidth() - 32, getHeight() - 60);
        }

        if (saveAsModal != nullptr) {
            saveAsModal->setBounds(0, 0, getWidth(), getHeight());
        }
    } else {
        if (modernPresetBar != nullptr) modernPresetBar->setVisible(false);
        if (presetBrowserOverlay != nullptr) presetBrowserOverlay->setVisible(false);
        if (saveAsModal != nullptr) saveAsModal->setVisible(false);

        int headerY = 12;
        guiModeButton.setVisible(true);
        guiModeButton.setButtonText("SWITCH TO MODERN SKIN");
        guiModeButton.setBounds(getWidth() - 210, headerY, 194, 26);

        nextPresetBtn.setVisible(true);
        nextPresetBtn.setBounds(getWidth() - 245, headerY, 26, 26);

        presetSelector.setVisible(true);
        presetSelector.setBounds(getWidth() - 425, headerY, 175, 26);

        prevPresetBtn.setVisible(true);
        prevPresetBtn.setBounds(getWidth() - 455, headerY, 26, 26);

        // Center classic panel if window is enlarged
        int offsetX = std::max(0, (getWidth() - 960) / 2);
        int offsetY = std::max(0, (getHeight() - 620) / 2);

        // LCD screen (580x155)
        lcdDisplay.setBounds(offsetX + 45, offsetY + 72, 580, 155);

        // Keypad on the right
        keypad.setBounds(offsetX + 670, offsetY + 160, 230, 280);

        // Knobs in 2 rows of 5 aligned with LCD columns
        auto lcdArea = lcdDisplay.getBounds().toFloat().reduced(8.0f);
        float colW = (lcdArea.getWidth() - 130.0f) / 5.0f;
        int knobSize = 58;
        int knobRow1Y = offsetY + 285;
        int knobRow2Y = offsetY + 440;

        for (int i = 0; i < 5; ++i) {
            float lx = lcdArea.getX() + i * colW + colW * 0.5f + 6.0f;
            int kx = (int)(lx - knobSize * 0.5f);
            knobs[i]->setBounds(kx, knobRow1Y, knobSize, knobSize);
            knobs[i + 5]->setBounds(kx, knobRow2Y, knobSize, knobSize);
        }
    }
}
