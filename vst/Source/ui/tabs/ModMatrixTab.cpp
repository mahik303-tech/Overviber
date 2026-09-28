#include "ModMatrixTab.h"

#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
#include "../../PluginProcessor.h"
#endif

ModMatrixTab::ModMatrixTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void ModMatrixTab::setup() {
    addAndMakeVisible(modMatrixCard);
    modMatrixCard.toBack();

    const char* colHeaders[] = { "SLOT", "SOURCE CONTROLLER", "VIA (SCALER / MOD)", "DESTINATION PARAMETER", "DEPTH" };
    for (int i = 0; i < 5; ++i) {
        matrixColLabels[i] = std::make_unique<juce::Label>("", colHeaders[i]);
        matrixColLabels[i]->setFont(modernLnf.getCustomFont(9.5f, juce::Font::bold));
        matrixColLabels[i]->setColour(juce::Label::textColourId, modernLnf.getTheme().accent);
        matrixColLabels[i]->setJustificationType(i == 4 ? juce::Justification::centred : juce::Justification::centredLeft);
        addAndMakeVisible(*matrixColLabels[i]);
    }

    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        matrixEnToggles[s] = createToggle("SLOT " + juce::String(s + 1));
        matrixEnToggles[s]->setToggleState(true, juce::dontSendNotification);
        matrixEnToggles[s]->onClick = [this, s]() {
            bool en = matrixEnToggles[s]->getToggleState();
            engine.getCurrentPreset().modMatrix[s].enabled = en;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
            if (processor) {
                juce::String paramId = "matrixSlot" + juce::String(s) + "_en";
                if (auto* p = processor->getAPVTS().getParameter(paramId)) {
                    p->setValueNotifyingHost(p->convertTo0to1(en ? 1.0f : 0.0f));
                }
            }
#endif
        };
        addAndMakeVisible(*matrixEnToggles[s]);

        for (int i = 0; i < modSrcCount; ++i) {
            matrixSrcCombos[s].addItem(PresetManager::getModSourceDisplayName((modSource_t)i), i + 1);
            matrixViaCombos[s].addItem(PresetManager::getModSourceDisplayName((modSource_t)i), i + 1);
        }
        for (int i = 0; i < modDestCount; ++i) {
            matrixDestCombos[s].addItem(PresetManager::getModDestDisplayName((modDest_t)i), i + 1);
        }

        matrixSrcCombos[s].setSelectedId(1, juce::dontSendNotification);
        matrixSrcCombos[s].onChange = [this, s]() {
            int src = matrixSrcCombos[s].getSelectedId() - 1;
            if (src >= 0 && src < modSrcCount) {
                engine.getCurrentPreset().modMatrix[s].source = (uint8_t)src;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
                if (processor) {
                    juce::String paramId = "matrixSlot" + juce::String(s) + "_src";
                    if (auto* p = processor->getAPVTS().getParameter(paramId)) {
                        p->setValueNotifyingHost(p->convertTo0to1((float)src));
                    }
                }
#endif
            }
        };
        addAndMakeVisible(matrixSrcCombos[s]);

        matrixViaCombos[s].setSelectedId(1, juce::dontSendNotification);
        matrixViaCombos[s].onChange = [this, s]() {
            int via = matrixViaCombos[s].getSelectedId() - 1;
            if (via >= 0 && via < modSrcCount) {
                engine.getCurrentPreset().modMatrix[s].viaSource = (uint8_t)via;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
                if (processor) {
                    juce::String paramId = "matrixSlot" + juce::String(s) + "_via";
                    if (auto* p = processor->getAPVTS().getParameter(paramId)) {
                        p->setValueNotifyingHost(p->convertTo0to1((float)via));
                    }
                }
#endif
            }
        };
        addAndMakeVisible(matrixViaCombos[s]);

        matrixDestCombos[s].setSelectedId(1, juce::dontSendNotification);
        matrixDestCombos[s].onChange = [this, s]() {
            int dest = matrixDestCombos[s].getSelectedId() - 1;
            if (dest >= 0 && dest < modDestCount) {
                engine.getCurrentPreset().modMatrix[s].dest = (uint8_t)dest;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
                if (processor) {
                    juce::String paramId = "matrixSlot" + juce::String(s) + "_dest";
                    if (auto* p = processor->getAPVTS().getParameter(paramId)) {
                        p->setValueNotifyingHost(p->convertTo0to1((float)dest));
                    }
                }
#endif
            }
        };
        addAndMakeVisible(matrixDestCombos[s]);

        matrixDepthKnobs[s] = createKnob("MatDepth" + juce::String(s), -100, 100, 0, KnobMode::BipolarPercent);
        matrixDepthKnobs[s]->textFromValueFunction = [](double val) -> juce::String {
            int v = (int)std::round(val);
            return (v > 0 ? "+" : "") + juce::String(v) + " %";
        };
        matrixDepthKnobs[s]->valueFromTextFunction = [](const juce::String& text) -> double {
            return std::clamp(text.replace("%", "").replace("+", "").trim().getDoubleValue(), -100.0, 100.0);
        };
        matrixDepthKnobs[s]->onValueChange = [this, s]() {
            int depth = (int)matrixDepthKnobs[s]->getValue();
            engine.getCurrentPreset().modMatrix[s].depth = (int16_t)depth;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
            if (processor) {
                juce::String paramId = "matrixSlot" + juce::String(s) + "_depth";
                if (auto* p = processor->getAPVTS().getParameter(paramId)) {
                    p->setValueNotifyingHost(p->convertTo0to1((float)depth));
                }
            }
#endif
        };
        addAndMakeVisible(*matrixDepthKnobs[s]);
    }

    // Row 2: Performance Controllers (Pitch Bend, Modwheel, Aftertouch)
    addAndMakeVisible(benderCard);
    addAndMakeVisible(modwheelCard);
    addAndMakeVisible(pressureCard);
    benderCard.toBack();
    modwheelCard.toBack();
    pressureCard.toBack();

    // Pitch Bend Range
    const char* benderRangeNames[3] = { "3 Semi (m3)", "5 Semi (4th)", "12 Semi (1 Oct)" };
    for (int i = 0; i < 3; ++i) {
        benderRangeToggles[i] = createToggle(benderRangeNames[i]);
        benderRangeToggles[i]->setRadioGroupId(1101);
        benderRangeToggles[i]->onClick = [this, i]() {
            setSteppedParam(spBenderRange, (uint8_t)i);
        };
        addAndMakeVisible(*benderRangeToggles[i]);
    }

    // Pitch Bend Target
    const char* benderTargetNames[5] = { "Off", "Pitch", "Cutoff", "Volume", "WaveMod" };
    for (int i = 0; i < 5; ++i) {
        benderTargetToggles[i] = createToggle(benderTargetNames[i]);
        benderTargetToggles[i]->setRadioGroupId(1102);
        benderTargetToggles[i]->onClick = [this, i]() {
            setSteppedParam(spBenderTarget, (uint8_t)i);
        };
        addAndMakeVisible(*benderTargetToggles[i]);
    }

    // Modwheel Range
    const char* modRangeNames[4] = { "Min", "Low", "High", "Max" };
    for (int i = 0; i < 4; ++i) {
        modwheelRangeToggles[i] = createToggle(modRangeNames[i]);
        modwheelRangeToggles[i]->setRadioGroupId(1103);
        modwheelRangeToggles[i]->onClick = [this, i]() {
            setSteppedParam(spModwheelRange, (uint8_t)i);
        };
        addAndMakeVisible(*modwheelRangeToggles[i]);
    }

    // Modwheel Target
    const char* modTargetNames[2] = { "LFO 1 Depth", "LFO 2 Depth" };
    for (int i = 0; i < 2; ++i) {
        modwheelTargetToggles[i] = createToggle(modTargetNames[i]);
        modwheelTargetToggles[i]->setRadioGroupId(1104);
        modwheelTargetToggles[i]->onClick = [this, i]() {
            setSteppedParam(spModwheelTarget, (uint8_t)i);
        };
        addAndMakeVisible(*modwheelTargetToggles[i]);
    }

    // Pressure Range
    const char* pressRangeNames[4] = { "Min", "Low", "High", "Max" };
    for (int i = 0; i < 4; ++i) {
        pressureRangeToggles[i] = createToggle(pressRangeNames[i]);
        pressureRangeToggles[i]->setRadioGroupId(1105);
        pressureRangeToggles[i]->onClick = [this, i]() {
            setSteppedParam(spPressureRange, (uint8_t)i);
        };
        addAndMakeVisible(*pressureRangeToggles[i]);
    }

    // Pressure Target
    const char* pressTargetNames[7] = { "Off", "Pitch", "Cutoff", "Volume", "WaveMod", "LFO 1", "LFO 2" };
    for (int i = 0; i < 7; ++i) {
        pressureTargetToggles[i] = createToggle(pressTargetNames[i]);
        pressureTargetToggles[i]->setRadioGroupId(1106);
        pressureTargetToggles[i]->onClick = [this, i]() {
            setSteppedParam(spPressureTarget, (uint8_t)i);
        };
        addAndMakeVisible(*pressureTargetToggles[i]);
    }

    assignComponentIDs();
}

void ModMatrixTab::assignComponentIDs() {
    benderCard.setComponentID("benderCard");
    modwheelCard.setComponentID("modwheelCard");
    pressureCard.setComponentID("pressureCard");
    modMatrixCard.setComponentID("modMatrixCard");

    for (int i = 0; i < 3; ++i) {
        if (benderRangeToggles[i]) benderRangeToggles[i]->setComponentID("benderRangeToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 5; ++i) {
        if (benderTargetToggles[i]) benderTargetToggles[i]->setComponentID("benderTargetToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 4; ++i) {
        if (modwheelRangeToggles[i]) modwheelRangeToggles[i]->setComponentID("modwheelRangeToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 2; ++i) {
        if (modwheelTargetToggles[i]) modwheelTargetToggles[i]->setComponentID("modwheelTargetToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 4; ++i) {
        if (pressureRangeToggles[i]) pressureRangeToggles[i]->setComponentID("pressureRangeToggle[" + juce::String(i) + "]");
    }
    for (int i = 0; i < 7; ++i) {
        if (pressureTargetToggles[i]) pressureTargetToggles[i]->setComponentID("pressureTargetToggle[" + juce::String(i) + "]");
    }
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        if (matrixEnToggles[s]) matrixEnToggles[s]->setComponentID("matrixEnToggle[" + juce::String(s) + "]");
        matrixSrcCombos[s].setComponentID("matrixSrcCombo[" + juce::String(s) + "]");
        matrixViaCombos[s].setComponentID("matrixViaCombo[" + juce::String(s) + "]");
        matrixDestCombos[s].setComponentID("matrixDestCombo[" + juce::String(s) + "]");
        if (matrixDepthKnobs[s]) matrixDepthKnobs[s]->setComponentID("matrixDepthKnob[" + juce::String(s) + "]");
        if (matrixSlotLabels[s]) matrixSlotLabels[s]->setComponentID("matrixSlotLabel[" + juce::String(s) + "]");
    }
}

void ModMatrixTab::updateFromEngine() {
    const auto& preset = engine.getCurrentPreset();

    int bRange = preset.steppedParams[spBenderRange];
    for (int i = 0; i < 3; ++i) {
        if (benderRangeToggles[i]) benderRangeToggles[i]->setToggleState(i == bRange, juce::dontSendNotification);
    }
    int bTarget = preset.steppedParams[spBenderTarget];
    for (int i = 0; i < 5; ++i) {
        if (benderTargetToggles[i]) benderTargetToggles[i]->setToggleState(i == bTarget, juce::dontSendNotification);
    }
    int mRange = preset.steppedParams[spModwheelRange];
    for (int i = 0; i < 4; ++i) {
        if (modwheelRangeToggles[i]) modwheelRangeToggles[i]->setToggleState(i == mRange, juce::dontSendNotification);
    }
    int mTarget = preset.steppedParams[spModwheelTarget];
    for (int i = 0; i < 2; ++i) {
        if (modwheelTargetToggles[i]) modwheelTargetToggles[i]->setToggleState(i == mTarget, juce::dontSendNotification);
    }
    int pRange = preset.steppedParams[spPressureRange];
    for (int i = 0; i < 4; ++i) {
        if (pressureRangeToggles[i]) pressureRangeToggles[i]->setToggleState(i == pRange, juce::dontSendNotification);
    }
    int pTarget = preset.steppedParams[spPressureTarget];
    for (int i = 0; i < 7; ++i) {
        if (pressureTargetToggles[i]) pressureTargetToggles[i]->setToggleState(i == pTarget, juce::dontSendNotification);
    }

    // Modulation Matrix (8 Slots)
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        if (matrixEnToggles[s]) matrixEnToggles[s]->setToggleState(preset.modMatrix[s].enabled, juce::dontSendNotification);
        safeSetCombo(matrixSrcCombos[s], preset.modMatrix[s].source + 1);
        safeSetCombo(matrixViaCombos[s], preset.modMatrix[s].viaSource + 1);
        safeSetCombo(matrixDestCombos[s], preset.modMatrix[s].dest + 1);
        safeSetKnob(matrixDepthKnobs[s].get(), preset.modMatrix[s].depth);
    }
}

void ModMatrixTab::resized() {
    const auto tabBounds = getLocalBounds();

    int cardGap = 5;
    int totalW = tabBounds.getWidth();
    int totalH = tabBounds.getHeight();

    int knobSz = getStandardKnobSize(); // 55px XL standard hardware knob
    int depthKnobSz = knobSz;
    int depthW = 68;

    int totalRows = MOD_MATRIX_SLOT_COUNT; // 8
    int colLabelH = 16;
    int contentTop = 22;
    int marginY = 6;

    int minRow2H = 110;
    int maxRow1H = std::max(200, totalH - minRow2H - cardGap);

    int desiredRowH = std::max(56, depthKnobSz + 2); // ~57-58px comfortably fits 55px XL knobs
    int desiredRow1H = contentTop + marginY * 2 + colLabelH + 7 + totalRows * desiredRowH;

    int row1H = std::min(desiredRow1H, maxRow1H);
    int rowH = std::max(depthKnobSz, (row1H - contentTop - marginY * 2 - colLabelH - 7) / totalRows);
    int actualRowsH = totalRows * rowH;
    int remainingH = (row1H - contentTop) - (colLabelH + 7 + actualRowsH);
    marginY = std::max(4, remainingH / 2);
    row1H = contentTop + marginY * 2 + colLabelH + 7 + actualRowsH;

    int colGap = 5;
    int colW = (totalW - colGap * 2) / 3;
    int col1X = 0;
    int col2X = col1X + colW + colGap;
    int col3X = col2X + colW + colGap;

    modMatrixCard.setBounds(0, 0, totalW, row1H);
    modMatrixCard.clearDividers();
    modMatrixCard.addVerticalDivider(col2X - 3, 28, row1H - 6);
    modMatrixCard.addVerticalDivider(col3X - 3, 28, row1H - 6);

    int headerY = contentTop + marginY;
    int divY = headerY + colLabelH + 3;
    int rowStartY = divY + 4;

    // Column 1: Aligned flush with benderCard (col1X to col1X + colW)
    int enW = 74;
    int enX = col1X + 12;
    int srcX = enX + enW + 6;
    int srcW = (col1X + colW - 8) - srcX;

    // Column 2: Aligned flush with modwheelCard (col2X to col2X + colW)
    int viaX = col2X + 8;
    int viaW = colW - 16;

    // Column 3: Aligned flush with pressureCard (col3X to col3X + colW)
    int depthX = col3X + colW - 12 - depthW;
    int destX = col3X + 8;
    int destW = (depthX - 6) - destX;

    if (matrixColLabels[0]) matrixColLabels[0]->setBounds(enX, headerY, enW, colLabelH);
    if (matrixColLabels[1]) matrixColLabels[1]->setBounds(srcX, headerY, srcW, colLabelH);
    if (matrixColLabels[2]) matrixColLabels[2]->setBounds(viaX, headerY, viaW, colLabelH);
    if (matrixColLabels[3]) matrixColLabels[3]->setBounds(destX, headerY, destW, colLabelH);
    if (matrixColLabels[4]) matrixColLabels[4]->setBounds(depthX, headerY, depthW, colLabelH);

    modMatrixCard.addDivider(divY, "");

    int comboH = std::clamp(rowH - 26, 24, 28);
    int togH = 22;

    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        int ry = rowStartY + s * rowH;
        int cy = ry + (rowH - comboH) / 2;
        int ty = ry + (rowH - togH) / 2;
        int ky = ry + (rowH - depthKnobSz) / 2;

        if (matrixEnToggles[s]) matrixEnToggles[s]->setBounds(enX, ty, enW, togH);
        matrixSrcCombos[s].setBounds(srcX, cy, srcW, comboH);
        matrixViaCombos[s].setBounds(viaX, cy, viaW, comboH);
        matrixDestCombos[s].setBounds(destX, cy, destW, comboH);
        if (matrixDepthKnobs[s]) matrixDepthKnobs[s]->setBounds(depthX + (depthW - depthKnobSz) / 2, ky, depthKnobSz, depthKnobSz);

        if (s < MOD_MATRIX_SLOT_COUNT - 1) {
            modMatrixCard.addDivider(ry + rowH - 1, "");
        }
    }

    // Row 2: Performance Controllers (Pitch Bend, Modwheel, Aftertouch)
    int row2Y = row1H + cardGap;
    int row2H = totalH - row2Y;
    int midX = colW / 2;
    int subColW = midX - 10;
    int toggleH = std::clamp((row2H - 38) / 4 - 2, 16, 20);
    int toggleStep = toggleH + 2;
    int ctrlStartY = row2Y + 34;

    // Pitch Bend (col1X)
    benderCard.setBounds(col1X, row2Y, colW, row2H);
    benderCard.clearDividers();
    benderCard.addDivider(6, 26, subColW, "RANGE");
    benderCard.addDivider(midX + 4, 26, subColW, "DESTINATION");
    benderCard.addVerticalDivider(midX, 22, row2H - 6);

    for (int i = 0; i < 3; ++i) {
        if (benderRangeToggles[i])
            benderRangeToggles[i]->setBounds(col1X + 8, ctrlStartY + i * toggleStep, subColW - 6, toggleH);
    }
    int halfSubColW = (subColW - 6) / 2;
    for (int i = 0; i < 5; ++i) {
        if (benderTargetToggles[i]) {
            int col = i / 3;
            int row = i % 3;
            int tx = col1X + midX + 6 + col * (halfSubColW + 4);
            benderTargetToggles[i]->setBounds(tx, ctrlStartY + row * toggleStep, halfSubColW, toggleH);
        }
    }

    // Modwheel (col2X)
    modwheelCard.setBounds(col2X, row2Y, colW, row2H);
    modwheelCard.clearDividers();
    modwheelCard.addDivider(6, 26, subColW, "INTENSITY");
    modwheelCard.addDivider(midX + 4, 26, subColW, "DESTINATION");
    modwheelCard.addVerticalDivider(midX, 22, row2H - 6);

    for (int i = 0; i < 4; ++i) {
        if (modwheelRangeToggles[i])
            modwheelRangeToggles[i]->setBounds(col2X + 8, ctrlStartY + i * toggleStep, subColW - 6, toggleH);
    }
    for (int i = 0; i < 2; ++i) {
        if (modwheelTargetToggles[i])
            modwheelTargetToggles[i]->setBounds(col2X + midX + 6, ctrlStartY + i * toggleStep, subColW - 6, toggleH);
    }

    // Pressure (col3X)
    pressureCard.setBounds(col3X, row2Y, colW, row2H);
    pressureCard.clearDividers();
    pressureCard.addDivider(6, 26, subColW, "SENSITIVITY");
    pressureCard.addDivider(midX + 4, 26, subColW, "DESTINATION");
    pressureCard.addVerticalDivider(midX, 22, row2H - 6);

    for (int i = 0; i < 4; ++i) {
        if (pressureRangeToggles[i])
            pressureRangeToggles[i]->setBounds(col3X + 8, ctrlStartY + i * toggleStep, subColW - 6, toggleH);
    }
    for (int i = 0; i < 7; ++i) {
        if (pressureTargetToggles[i]) {
            int col = i / 4;
            int row = i % 4;
            int tx = col3X + midX + 6 + col * (halfSubColW + 4);
            pressureTargetToggles[i]->setBounds(tx, ctrlStartY + row * toggleStep, halfSubColW, toggleH);
        }
    }
}
