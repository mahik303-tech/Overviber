#include "ModMatrixTab.h"
#include "../components/ModulationTargets.h"

#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
#include "../../PluginProcessor.h"
#endif

// Common routings, written into the selected slot.
const ModMatrixTab::QuickAssign ModMatrixTab::kQuickAssigns[6] = {
    { "MOD WHEEL > CUTOFF", modSrcModWheel, modDestCutoff, 50 },
    { "VELOCITY > LEVEL", modSrcVelocity, modDestAmpLevel, 50 },
    { "AFTERTOUCH > CUTOFF", modSrcAftertouch, modDestCutoff, 40 },
    { "LFO 1 > PITCH (VIBRATO)", modSrcLFO1, modDestPitchAll, 10 },
    { "LFO 2 > WAVEMOD", modSrcLFO2, modDestWaveModAll, 50 },
    { "KEY TRACK > CUTOFF", modSrcKeyTrack, modDestCutoff, 50 },
};

ModMatrixTab::ModMatrixTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void ModMatrixTab::setup() {
    addAndMakeVisible(controllersCard);
    controllersCard.toBack();
    addAndMakeVisible(modMatrixCard);
    modMatrixCard.toBack();

    setupControllers();

    routingView = std::make_unique<RoutingView>(model, modernLnf);
    routingView->onSelect = [this](int slot) { selectSlot(slot); };
    routingView->onSwitch = [this](int slot, bool enabled) {
        auto value = model.getCurrentPreset().modMatrix[slot];
        value.enabled = enabled;
        setSlot(slot, value);
    };
    routingView->onDepth = [this](int slot, int depth) {
        auto value = model.getCurrentPreset().modMatrix[slot];
        value.depth = (int16_t)depth;
        setSlot(slot, value);
    };
    addAndMakeVisible(*routingView);

    setupSlotEditor();
    assignComponentIDs();
    showSelectedSlot();
}

void ModMatrixTab::setupControllers() {
    auto radioGroup = [this](std::unique_ptr<juce::ToggleButton>* toggles, const char* const* names, int count,
                             int group, steppedParameter_t sp) {
        for (int i = 0; i < count; ++i) {
            toggles[i] = createToggle(names[i]);
            toggles[i]->setRadioGroupId(group);
            toggles[i]->onClick = [this, sp, i]() { setSteppedParam(sp, (uint8_t)i); };
            addAndMakeVisible(*toggles[i]);
        }
    };
    const char* benderRangeNames[3] = { "4 Semi (3rd)", "7 Semi (5th)", "12 Semi (1 Oct)" };
    const char* benderTargetNames[5] = { "Off", "Pitch", "Cutoff", "Volume", "WaveMod" };
    const char* modRangeNames[4] = { "Min", "Low", "High", "Max" };
    const char* modTargetNames[2] = { "LFO 1 Depth", "LFO 2 Depth" };
    const char* pressRangeNames[4] = { "Min", "Low", "High", "Max" };
    const char* pressTargetNames[7] = { "Off", "Pitch", "Cutoff", "Volume", "WaveMod", "LFO 1", "LFO 2" };
    radioGroup(benderRangeToggles, benderRangeNames, 3, 1101, spBenderRange);
    radioGroup(benderTargetToggles, benderTargetNames, 5, 1102, spBenderTarget);
    radioGroup(modwheelRangeToggles, modRangeNames, 4, 1103, spModwheelRange);
    radioGroup(modwheelTargetToggles, modTargetNames, 2, 1104, spModwheelTarget);
    radioGroup(pressureRangeToggles, pressRangeNames, 4, 1105, spPressureRange);
    radioGroup(pressureTargetToggles, pressTargetNames, 7, 1106, spPressureTarget);
}

void ModMatrixTab::setupSlotEditor() {
    auto withSelected = [this](auto change) {
        auto value = model.getCurrentPreset().modMatrix[selectedSlot];
        change(value);
        setSlot(selectedSlot, value);
    };

    slotEnableToggle = createToggle("Slot On");
    slotEnableToggle->onClick = [this, withSelected]() {
        const bool on = slotEnableToggle->getToggleState();
        withSelected([on](ModMatrixSlot& v) { v.enabled = on; });
    };
    addAndMakeVisible(*slotEnableToggle);

    // Short names on the buttons, the full ones as tooltips (enum order:
    // controllers, envelopes, LFOs; destinations of the synth, then Elements).
    static const char* const kSourceShort[modSrcCount] = {
        "NONE", "MOD WHEEL", "PITCH BEND", "AFTERTOUCH", "SLIDE CC 74", "VELOCITY", "LIFT VEL", "KEY TRACK",
        "BREATH", "EXPRESSION", "FILTER ENV", "AMP ENV", "WMOD ENV", "LFO 1", "LFO 1 +", "LFO 2", "LFO 2 +", "CONSTANT" };
    static const char* const kDestShort[modDestCount] = {
        "NONE", "PITCH", "PITCH A", "PITCH B", "DETUNE", "WAVEMOD", "WAVEMOD A",
        "WAVEMOD B", "LEVEL A", "LEVEL B", "NOISE", "CUTOFF", "RESONANCE", "AMP (VCA)",
        "GEOMETRY", "BRIGHTNESS", "DAMPING", "POSITION", "SPACE", "BOW", "BLOW",
        "STRIKE", "CONTOUR", "FLOW", "MALLET", "BOW TIMBRE", "BLOW TIMBRE", "STRIKE TIMBRE" };
    juce::StringArray sourceLabels, sourceTips, destLabels, destTips;
    for (int i = 0; i < modSrcCount; ++i) {
        sourceLabels.add(kSourceShort[i]);
        sourceTips.add(PresetManager::getModSourceDisplayName((modSource_t)i));
    }
    for (int i = 0; i < modDestCount; ++i) {
        destLabels.add(kDestShort[i]);
        destTips.add(PresetManager::getModDestDisplayName((modDest_t)i));
    }
    sourceChoice = std::make_unique<ModernChoiceButtons>(sourceLabels, 9);
    viaChoice = std::make_unique<ModernChoiceButtons>(sourceLabels, 9);
    destChoice = std::make_unique<ModernChoiceButtons>(destLabels, 7);
    sourceChoice->setTooltips(sourceTips);
    viaChoice->setTooltips(sourceTips);
    destChoice->setTooltips(destTips);
    sourceChoice->onSelect = [withSelected](int id) { withSelected([id](ModMatrixSlot& v) { v.source = (uint8_t)id; }); };
    viaChoice->onSelect = [withSelected](int id) { withSelected([id](ModMatrixSlot& v) { v.viaSource = (uint8_t)id; }); };
    destChoice->onSelect = [withSelected](int id) { withSelected([id](ModMatrixSlot& v) { v.dest = (uint8_t)id; }); };
    for (auto* choice : { sourceChoice.get(), viaChoice.get(), destChoice.get() }) {
        choice->setGap(3);
        addAndMakeVisible(*choice);
    }
    sourceLabel = createLabel("SOURCE", *this);
    viaLabel = createLabel("VIA", *this);
    destLabel = createLabel("DESTINATION", *this);
    viaLabel->setTooltip("Via: a second source that scales the depth (NONE: full depth)");
    for (auto* label : { sourceLabel.get(), viaLabel.get(), destLabel.get() })
        label->setJustificationType(juce::Justification::centredLeft);

    // The depth as a bipolar bar in the editor row (the routing overview's
    // bars set it too).
    depthKnob = std::make_unique<juce::Slider>("MatDepth");
    depthKnob->setSliderStyle(juce::Slider::LinearBar);
    depthKnob->setRange(-100.0, 100.0, 1.0);
    depthKnob->setDoubleClickReturnValue(true, 0.0);
    depthKnob->textFromValueFunction = [](double val) -> juce::String {
        int v = (int)std::round(val);
        return "DEPTH  " + juce::String(v > 0 ? "+" : "") + juce::String(v) + " %";
    };
    depthKnob->valueFromTextFunction = [](const juce::String& text) -> double {
        return std::clamp(text.replace("%", "").replace("+", "").trim().getDoubleValue(), -100.0, 100.0);
    };
    depthKnob->onValueChange = [this, withSelected]() {
        const int depth = (int)depthKnob->getValue();
        withSelected([depth](ModMatrixSlot& v) { v.depth = (int16_t)depth; });
    };
    addAndMakeVisible(*depthKnob);
    depthLabel = createLabel("DEPTH", *this);
    depthLabel->setVisible(false);   // the bar names itself

    clearSlotButton.onClick = [this]() { setSlot(selectedSlot, ModMatrixSlot{}); };
    addAndMakeVisible(clearSlotButton);

    for (int i = 0; i < 6; ++i) {
        quickButtons[i] = std::make_unique<juce::TextButton>(kQuickAssigns[i].text);
        quickButtons[i]->onClick = [this, i]() {
            ModMatrixSlot value;
            value.source = (uint8_t)kQuickAssigns[i].source;
            value.dest = (uint8_t)kQuickAssigns[i].dest;
            value.depth = (int16_t)kQuickAssigns[i].depth;
            setSlot(selectedSlot, value);
        };
        addAndMakeVisible(*quickButtons[i]);
    }

    hintLabel.setText("Right-click a knob on any tab (cutoff, resonance, pitch, levels, WaveMod, Elements) to "
                      "modulate it. A modulated knob shows a white arc: the summed depth of its slots.",
                      juce::dontSendNotification);
    hintLabel.setFont(modernLnf.getCustomFont(10.0f, juce::Font::plain));
    hintLabel.setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
    hintLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(hintLabel);
}

void ModMatrixTab::assignComponentIDs() {
    controllersCard.setComponentID("controllersCard");
    modMatrixCard.setComponentID("modMatrixCard");

    for (int i = 0; i < 3; ++i) benderRangeToggles[i]->setComponentID("benderRangeToggle[" + juce::String(i) + "]");
    for (int i = 0; i < 5; ++i) benderTargetToggles[i]->setComponentID("benderTargetToggle[" + juce::String(i) + "]");
    for (int i = 0; i < 4; ++i) modwheelRangeToggles[i]->setComponentID("modwheelRangeToggle[" + juce::String(i) + "]");
    for (int i = 0; i < 2; ++i) modwheelTargetToggles[i]->setComponentID("modwheelTargetToggle[" + juce::String(i) + "]");
    for (int i = 0; i < 4; ++i) pressureRangeToggles[i]->setComponentID("pressureRangeToggle[" + juce::String(i) + "]");
    for (int i = 0; i < 7; ++i) pressureTargetToggles[i]->setComponentID("pressureTargetToggle[" + juce::String(i) + "]");

    routingView->setComponentID("matrixRoutingView");
    slotEnableToggle->setComponentID("matrixEnToggle");
    sourceChoice->setIdPrefix("matrixSrcButton");
    viaChoice->setIdPrefix("matrixViaButton");
    destChoice->setIdPrefix("matrixDestButton");
    depthKnob->setComponentID("matrixDepthKnob");
    sourceLabel->setComponentID("matrixSrcLabel");
    viaLabel->setComponentID("matrixViaLabel");
    destLabel->setComponentID("matrixDestLabel");
    depthLabel->setComponentID("matrixDepthLabel");
    clearSlotButton.setComponentID("matrixClearButton");
    for (int i = 0; i < 6; ++i) quickButtons[i]->setComponentID("matrixQuickButton[" + juce::String(i) + "]");
    hintLabel.setComponentID("matrixHintLabel");
}

void ModMatrixTab::selectSlot(int slot) {
    selectedSlot = std::clamp(slot, 0, MOD_MATRIX_SLOT_COUNT - 1);
    if (routingView) routingView->setSelectedSlot(selectedSlot);
    showSelectedSlot();
    resized();   // the editor's section title names the slot
}

void ModMatrixTab::showSelectedSlot() {
    if (!depthKnob) return;
    const auto& slot = model.getCurrentPreset().modMatrix[selectedSlot];
    safeSetToggle(slotEnableToggle.get(), slot.enabled);
    sourceChoice->setSelected(slot.source);
    viaChoice->setSelected(slot.viaSource);
    destChoice->setSelected(slot.dest);
    safeSetKnob(depthKnob.get(), slot.depth);
}

void ModMatrixTab::setSlot(int slot, const ModMatrixSlot& value) {
    if (slot < 0 || slot >= MOD_MATRIX_SLOT_COUNT) return;
    auto& target = model.getCurrentPreset().modMatrix[slot];
    const uint8_t curve = target.curve;
    target = value;
    target.curve = curve;
    notifyHost(slot, target);
    if (routingView) routingView->repaint();
    if (slot == selectedSlot) showSelectedSlot();
    if (context.modulationChanged) context.modulationChanged();
}

int ModMatrixTab::addModulation(modSource_t source, modDest_t dest) {
    const int slot = modtargets::firstFreeSlot(model.getCurrentPreset());
    if (slot < 0) return -1;
    ModMatrixSlot value;
    value.source = (uint8_t)source;
    value.dest = (uint8_t)dest;
    value.depth = 50;
    setSlot(slot, value);
    selectSlot(slot);
    return slot;
}

// The processor forwards the host parameters to the audio engine.
void ModMatrixTab::notifyHost(int slot, const ModMatrixSlot& value) {
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    if (!processor) return;
    const juce::String prefix = "matrixSlot" + juce::String(slot);
    auto set = [this](const juce::String& id, float v) {
        if (auto* p = processor->getAPVTS().getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    set(prefix + "_src", (float)value.source);
    set(prefix + "_via", (float)value.viaSource);
    set(prefix + "_dest", (float)value.dest);
    set(prefix + "_depth", (float)value.depth);
    set(prefix + "_en", value.enabled ? 1.0f : 0.0f);
#else
    juce::ignoreUnused(slot, value);
#endif
}

void ModMatrixTab::updateFromEngine() {
    const auto& preset = model.getCurrentPreset();
    auto showRadio = [](std::unique_ptr<juce::ToggleButton>* toggles, int count, int selected) {
        for (int i = 0; i < count; ++i)
            if (toggles[i]) toggles[i]->setToggleState(i == selected, juce::dontSendNotification);
    };
    showRadio(benderRangeToggles, 3, preset.steppedParams[spBenderRange]);
    showRadio(benderTargetToggles, 5, preset.steppedParams[spBenderTarget]);
    showRadio(modwheelRangeToggles, 4, preset.steppedParams[spModwheelRange]);
    showRadio(modwheelTargetToggles, 2, preset.steppedParams[spModwheelTarget]);
    showRadio(pressureRangeToggles, 4, preset.steppedParams[spPressureRange]);
    showRadio(pressureTargetToggles, 7, preset.steppedParams[spPressureTarget]);

    showSelectedSlot();
    if (routingView) routingView->repaint();
}

void ModMatrixTab::resized() {
    const int totalW = getWidth();
    const int totalH = getHeight();
    constexpr int col1W = 270, colGap = 5;
    const int col2X = col1W + colGap;
    const int col2W = totalW - col2X;

    // Card 1: performance controllers, each with its range and destination.
    controllersCard.setBounds(0, 0, col1W, totalH);
    controllersCard.clearDividers();
    constexpr int toggleH = 20, toggleStep = 22, rangeX = 16, rangeW = 118, destX = 142, destW = 112;
    auto section = [&](int y, const char* title, const char* rangeTitle,
                       std::unique_ptr<juce::ToggleButton>* ranges, int rangeCount,
                       std::unique_ptr<juce::ToggleButton>* targets, int targetCount) {
        controllersCard.addDivider(y, title);
        controllersCard.addDivider(rangeX, y + 16, rangeW - 6, rangeTitle);
        controllersCard.addDivider(destX, y + 16, destW, "DESTINATION");
        const int top = y + 32;
        for (int i = 0; i < rangeCount; ++i) ranges[i]->setBounds(rangeX, top + i * toggleStep, rangeW - 6, toggleH);
        for (int i = 0; i < targetCount; ++i) targets[i]->setBounds(destX, top + i * toggleStep, destW, toggleH);
        const int rows = std::max(rangeCount, targetCount);
        controllersCard.addVerticalDivider(destX - 8, top, top + rows * toggleStep - 2);
        return top + rows * toggleStep + 10;
    };
    // Sections start below the card header (dividers above y 30 are not drawn).
    int y = section(36, "PITCH BEND", "RANGE", benderRangeToggles, 3, benderTargetToggles, 5);
    y = section(y, "MODULATION WHEEL (CC 1)", "INTENSITY", modwheelRangeToggles, 4, modwheelTargetToggles, 2);
    section(y, "AFTERTOUCH", "SENSITIVITY", pressureRangeToggles, 4, pressureTargetToggles, 7);

    // Card 2: the matrix.
    modMatrixCard.setBounds(col2X, 0, col2W, totalH);
    modMatrixCard.clearDividers();
    const int left = col2X + 16, gridW = col2W - 32;
    modMatrixCard.addDivider(36, "ROUTING  (CLICK A SLOT TO EDIT, DRAG ITS BAR FOR THE DEPTH)");
    constexpr int routingTop = 48, routingRowH = 22;
    if (routingView) routingView->setBounds(left, routingTop, gridW, MOD_MATRIX_SLOT_COUNT * routingRowH);

    // The selected slot: switch, clear and depth in one row, then source,
    // via and destination as button grids with their captions on the left.
    const int editorY = routingTop + MOD_MATRIX_SLOT_COUNT * routingRowH + 10;
    modMatrixCard.addDivider(editorY, "SELECTED SLOT " + juce::String(selectedSlot + 1));
    const int rowY = editorY + 14;
    constexpr int captionW = 84, clearW = 100, gap = 8;
    if (slotEnableToggle) slotEnableToggle->setBounds(left, rowY, captionW, 22);
    clearSlotButton.setBounds(left + captionW, rowY, clearW, 22);
    depthKnob->setBounds(left + captionW + clearW + gap, rowY, gridW - captionW - clearW - gap, 22);

    const int gridX = left + captionW, gridsW = gridW - captionW;
    int gridY = rowY + 32;
    auto grid = [&](juce::Label& caption, ModernChoiceButtons& choice, int rows) {
        caption.setBounds(left, gridY, captionW - 8, 20);
        const int h = rows * 20 + (rows - 1) * 3;
        choice.setBounds(gridX, gridY, gridsW, h);
        gridY += h + 8;
    };
    grid(*sourceLabel, *sourceChoice, 2);
    grid(*viaLabel, *viaChoice, 2);
    grid(*destLabel, *destChoice, 4);

    const int quickY = gridY + 2;
    modMatrixCard.addDivider(quickY, "QUICK ASSIGN  (INTO THE SELECTED SLOT)");
    const int quickW = (gridW - 5 * 6) / 6;
    for (int i = 0; i < 6; ++i)
        quickButtons[i]->setBounds(left + i * (quickW + 6), quickY + 14, quickW, 22);

    const int hintY = quickY + 44;
    modMatrixCard.addDivider(hintY, "MODULATION FROM ANY TAB");
    hintLabel.setBounds(left, hintY + 8, gridW, 30);
}

// ------------------------------------------------------------------------------
// Routing overview
// ------------------------------------------------------------------------------
ModMatrixTab::RoutingView::RoutingView(SynthModel& m, ModernLookAndFeel& laf) : model(m), lnf(laf) {
    setWantsKeyboardFocus(true);
}

juce::Rectangle<int> ModMatrixTab::RoutingView::rowBounds(int slot) const {
    const int rowH = getHeight() / MOD_MATRIX_SLOT_COUNT;
    return { 0, slot * rowH, getWidth(), rowH };
}

juce::Rectangle<int> ModMatrixTab::RoutingView::switchBounds(int slot) const {
    const auto row = rowBounds(slot);
    return { row.getX() + 6, row.getCentreY() - 7, 14, 14 };
}

juce::Rectangle<int> ModMatrixTab::RoutingView::barBounds(int slot) const {
    const auto row = rowBounds(slot);
    const int barW = getWidth() * 28 / 100;
    return { row.getRight() - 52 - barW, row.getCentreY() - 6, barW, 12 };
}

int ModMatrixTab::RoutingView::depthAt(int slot, int x) const {
    const auto bar = barBounds(slot);
    const double t = (double)(x - bar.getX()) / std::max(1, bar.getWidth());
    return (int)std::lround(std::clamp(t * 200.0 - 100.0, -100.0, 100.0));
}

int ModMatrixTab::RoutingView::slotAt(juce::Point<int> p) const {
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s)
        if (rowBounds(s).contains(p)) return s;
    return -1;
}

void ModMatrixTab::RoutingView::paint(juce::Graphics& g) {
    const auto& theme = lnf.getTheme();
    const auto& matrix = model.getCurrentPreset().modMatrix;
    const int barW = getWidth() * 28 / 100;
    const int textX = 44, textW = getWidth() - textX - barW - 64;
    const int sourceW = textW * 36 / 100, viaW = textW * 24 / 100, arrowW = 22, destW = textW - sourceW - viaW - arrowW;

    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s) {
        const auto& slot = matrix[s];
        const auto row = rowBounds(s);
        const bool used = !modtargets::isFree(slot);
        const bool live = used && slot.enabled;

        g.setColour(s % 2 == 0 ? theme.cardBg.brighter(0.04f) : theme.cardBg);
        g.fillRect(row);
        if (s == selected) {
            g.setColour(theme.accentDark.withAlpha(0.35f));
            g.fillRect(row);
            g.setColour(theme.accent);
            g.drawRect(row, 1);
        }

        // Switch: an LED like the toggles'
        const auto sw = switchBounds(s).toFloat();
        g.setColour(theme.knobBodyTop);
        g.fillRect(sw);
        g.setColour(theme.cardBorder);
        g.drawRect(sw, 1.0f);
        if (slot.enabled) {
            g.setColour(used ? theme.accent : theme.textMuted);
            g.fillRect(sw.reduced(3.0f));
        }

        g.setFont(lnf.getCustomFont(10.0f, juce::Font::bold));
        g.setColour(live ? theme.textTitle : theme.textMuted);
        g.drawText(juce::String(s + 1), row.getX() + 24, row.getY(), 16, row.getHeight(), juce::Justification::centred);

        if (!used) {
            g.setFont(lnf.getCustomFont(10.0f, juce::Font::plain));
            g.setColour(theme.textMuted);
            g.drawText("empty", textX, row.getY(), textW, row.getHeight(), juce::Justification::centredLeft);
            continue;
        }
        g.setFont(lnf.getCustomFont(10.5f, juce::Font::bold));
        g.setColour(live ? theme.textBody : theme.textMuted);
        g.drawText(PresetManager::getModSourceDisplayName((modSource_t)slot.source), textX, row.getY(), sourceW,
                   row.getHeight(), juce::Justification::centredLeft, true);
        if (slot.viaSource != modSrcNone) {
            g.setFont(lnf.getCustomFont(9.5f, juce::Font::plain));
            g.setColour(theme.textMuted);
            g.drawText("x " + juce::String(PresetManager::getModSourceDisplayName((modSource_t)slot.viaSource)),
                       textX + sourceW, row.getY(), viaW, row.getHeight(), juce::Justification::centredLeft, true);
        }
        // Arrow
        const float ax = (float)(textX + sourceW + viaW + 4), ay = (float)row.getCentreY();
        g.setColour(live ? theme.accent : theme.textMuted);
        g.drawLine(ax, ay, ax + 12.0f, ay, 1.5f);
        juce::Path head;
        head.addTriangle(ax + 14.0f, ay, ax + 9.0f, ay - 3.5f, ax + 9.0f, ay + 3.5f);
        g.fillPath(head);
        g.setFont(lnf.getCustomFont(10.5f, juce::Font::bold));
        g.setColour(live ? theme.textBody : theme.textMuted);
        g.drawText(PresetManager::getModDestDisplayName((modDest_t)slot.dest), textX + sourceW + viaW + arrowW,
                   row.getY(), destW, row.getHeight(), juce::Justification::centredLeft, true);

        // Bipolar depth bar
        const auto bar = barBounds(s).toFloat();
        g.setColour(theme.knobTrack);
        g.fillRect(bar);
        const float centre = bar.getCentreX();
        const float end = centre + bar.getWidth() * 0.5f * (float)slot.depth / 100.0f;
        g.setColour(live ? theme.accent : theme.textMuted);
        g.fillRect(juce::Rectangle<float>(std::min(centre, end), bar.getY() + 2.0f, std::abs(end - centre), bar.getHeight() - 4.0f));
        g.setColour(theme.cardBorder);
        g.drawRect(bar, 1.0f);
        g.drawVerticalLine((int)centre, bar.getY(), bar.getBottom());
        g.setFont(lnf.getCustomFont(10.0f, juce::Font::bold));
        g.setColour(live ? theme.textTitle : theme.textMuted);
        g.drawText((slot.depth > 0 ? "+" : "") + juce::String(slot.depth) + " %", (int)bar.getRight() + 4, row.getY(),
                   46, row.getHeight(), juce::Justification::centredRight);
    }
    if (lnf.showsKeyboardFocus(*this)) {
        g.setColour(theme.accent);
        g.drawRect(getLocalBounds(), 1);
    }
}

void ModMatrixTab::RoutingView::mouseDown(const juce::MouseEvent& e) {
    draggingSlot = -1;
    const int slot = slotAt(e.getPosition());
    if (slot < 0) return;
    if (switchBounds(slot).expanded(4).contains(e.getPosition())) {
        if (onSwitch) onSwitch(slot, !model.getCurrentPreset().modMatrix[slot].enabled);
        return;
    }
    if (onSelect) onSelect(slot);
    if (barBounds(slot).expanded(0, 6).contains(e.getPosition())
        && !modtargets::isFree(model.getCurrentPreset().modMatrix[slot])) {
        draggingSlot = slot;
        if (onDepth) onDepth(slot, depthAt(slot, e.x));
    }
}

void ModMatrixTab::RoutingView::mouseDrag(const juce::MouseEvent& e) {
    if (draggingSlot >= 0 && onDepth) onDepth(draggingSlot, depthAt(draggingSlot, e.x));
}

void ModMatrixTab::RoutingView::mouseDoubleClick(const juce::MouseEvent& e) {
    const int slot = slotAt(e.getPosition());
    if (slot >= 0 && barBounds(slot).expanded(0, 6).contains(e.getPosition()) && onDepth) onDepth(slot, 0);
}

// Up / down select a slot, left / right change its depth (shift: by 10),
// space switches it on or off.
bool ModMatrixTab::RoutingView::keyPressed(const juce::KeyPress& key) {
    const auto& slot = model.getCurrentPreset().modMatrix[selected];
    if (key.isKeyCode(juce::KeyPress::upKey) || key.isKeyCode(juce::KeyPress::downKey)) {
        const int step = key.isKeyCode(juce::KeyPress::upKey) ? -1 : 1;
        if (onSelect) onSelect(std::clamp(selected + step, 0, MOD_MATRIX_SLOT_COUNT - 1));
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::leftKey) || key.isKeyCode(juce::KeyPress::rightKey)) {
        const int step = (key.getModifiers().isShiftDown() ? 10 : 1) * (key.isKeyCode(juce::KeyPress::leftKey) ? -1 : 1);
        if (onDepth && !modtargets::isFree(slot)) onDepth(selected, std::clamp(slot.depth + step, -100, 100));
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::spaceKey)) {
        if (onSwitch) onSwitch(selected, !slot.enabled);
        return true;
    }
    return false;
}
