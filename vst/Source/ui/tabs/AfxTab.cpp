#include "AfxTab.h"

#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
#include "../../PluginProcessor.h"
#endif
#include <algorithm>

namespace {

bool isBlackKey(int note) {
    const int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

// White keys below `note` (C-1 is white key 0).
int whiteKeysBefore(int note) {
    static const int whiteBefore[12] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
    return (note / 12) * 7 + whiteBefore[note % 12];
}

constexpr int kWhiteKeys = 75;   // of the 128 MIDI notes

} // namespace

juce::Colour AfxTab::padColour(int pad) {
    static const juce::uint32 colours[16] = {
        0xff18b5c9, 0xff00e5ff, 0xff00b0ff, 0xff2979ff,
        0xff651fff, 0xff7c4dff, 0xffe040fb, 0xffff4081,
        0xffff5252, 0xffff6e40, 0xffffab00, 0xffffd740,
        0xff76ff03, 0xff00e676, 0xff1de9b6, 0xff26a69a
    };
    return juce::Colour(colours[pad & 15]);
}

juce::String AfxTab::noteName(int note) {
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[note % 12]) + juce::String(note / 12 - 1);
}

juce::String AfxTab::keysText(const AfxKit& kit, int pad) {
    juce::Array<int> keys;
    for (int n = 0; n < 128; ++n)
        if (kit.getSlotForNote(static_cast<uint8_t>(n)) == pad) keys.add(n);
    if (keys.isEmpty()) return "no keys";
    if (keys.size() == 1) return noteName(keys[0]);
    const bool oneRange = keys.getLast() - keys.getFirst() + 1 == keys.size();
    if (oneRange) return noteName(keys.getFirst()) + " - " + noteName(keys.getLast()) + "  (" + juce::String(keys.size()) + " keys)";
    if (keys.size() <= 4) {
        juce::StringArray names;
        for (int n : keys) names.add(noteName(n));
        return names.joinIntoString(", ");
    }
    return juce::String(keys.size()) + " keys, " + noteName(keys.getFirst()) + " to " + noteName(keys.getLast());
}

// ------------------------------------------------------------------------------
// Pads
// ------------------------------------------------------------------------------
AfxTab::PadGrid::PadGrid(SynthModel& m, ModernLookAndFeel& l) : model(m), lnf(l) {
    setWantsKeyboardFocus(true);
}

juce::Rectangle<int> AfxTab::PadGrid::padBounds(int pad) const {
    constexpr int gap = 6;
    const int w = (getWidth() - 3 * gap) / 4, h = (getHeight() - 3 * gap) / 4;
    return { (pad % 4) * (w + gap), (pad / 4) * (h + gap), w, h };
}

void AfxTab::PadGrid::setSounding(uint16_t parts) {
    // A pad counts as sounding for one more tick: an editor tick between two
    // audio reports does not dim it.
    const uint16_t held = static_cast<uint16_t>(parts | previousParts);
    previousParts = parts;
    bool changed = false;
    for (size_t pad = 0; pad < glow.size(); ++pad) {
        const float before = glow[pad];
        glow[pad] = (held & (1u << pad)) ? 1.0f : (glow[pad] < 0.05f ? 0.0f : glow[pad] * 0.8f);
        changed |= std::abs(glow[pad] - before) > 0.001f;
    }
    if (changed) repaint();
}

void AfxTab::PadGrid::paint(juce::Graphics& g) {
    const auto& theme = lnf.getTheme();
    const auto& kit = model.getAfxKit();
    for (int pad = 0; pad < AFX_SLOT_COUNT; ++pad) {
        const auto r = padBounds(pad).toFloat();
        const auto colour = padColour(pad);
        const bool isSelected = pad == selected;
        const float lit = glow[static_cast<size_t>(pad)];

        g.setColour(theme.buttonBg.interpolatedWith(colour, 0.08f + 0.40f * lit));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(colour);
        g.fillRoundedRectangle(r.withWidth(5.0f), 2.0f);
        g.setColour(isSelected ? theme.accent : theme.buttonBorder);
        g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, isSelected ? 2.0f : 1.0f);

        const auto text = r.reduced(14.0f, 6.0f).withTrimmedLeft(0.0f);
        g.setFont(lnf.getCustomFont(18.0f, juce::Font::bold));
        g.setColour(isSelected ? colour : colour.withAlpha(0.75f));
        g.drawText(juce::String(pad + 1), text.withHeight(22.0f), juce::Justification::topLeft, false);
        g.setFont(lnf.getCustomFont(11.0f, juce::Font::bold));
        g.setColour(isSelected || lit > 0.1f ? theme.textTitle : theme.textBody);
        g.drawFittedText(kit.getSlot(pad).name, text.withTrimmedTop(22.0f).withHeight(16.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.8f);
        g.setFont(lnf.getCustomFont(9.5f, juce::Font::plain));
        g.setColour(theme.textMuted);
        g.drawFittedText(keysText(kit, pad), text.withTrimmedTop(38.0f).toNearestInt(),
                         juce::Justification::topLeft, 1, 0.8f);
    }
}

void AfxTab::PadGrid::mouseDown(const juce::MouseEvent& e) {
    for (int pad = 0; pad < AFX_SLOT_COUNT; ++pad)
        if (padBounds(pad).contains(e.getPosition())) {
            if (onSelect) onSelect(pad);
            return;
        }
}

bool AfxTab::PadGrid::keyPressed(const juce::KeyPress& key) {
    int pad = selected;
    if (key.isKeyCode(juce::KeyPress::leftKey)) pad -= 1;
    else if (key.isKeyCode(juce::KeyPress::rightKey)) pad += 1;
    else if (key.isKeyCode(juce::KeyPress::upKey)) pad -= 4;
    else if (key.isKeyCode(juce::KeyPress::downKey)) pad += 4;
    else return false;
    if (onSelect) onSelect(std::clamp(pad, 0, AFX_SLOT_COUNT - 1));
    return true;
}

void AfxTab::PadTitle::set(int pad, const juce::String& name) {
    text = juce::String(pad + 1) + "   " + name;
    colour = padColour(pad);
    repaint();
}

void AfxTab::PadTitle::paint(juce::Graphics& g) {
    g.setFont(lnf.getCustomFont(16.0f, juce::Font::bold));
    g.setColour(colour);
    g.drawFittedText(text, getLocalBounds(), juce::Justification::centredLeft, 1, 0.8f);
}

// ------------------------------------------------------------------------------
// Sound list
// ------------------------------------------------------------------------------
AfxTab::SoundList::SoundList(PresetManager& p, ModernLookAndFeel& l) : juce::ListBox("AfxSounds"), presets(p), lnf(l) {
    setModel(this);
    setRowHeight(20);
    setWantsKeyboardFocus(true);
}

int AfxTab::SoundList::getNumRows() { return presets.getPresetCount(); }

void AfxTab::SoundList::show(const juce::String& name) {
    updateContent();   // the preset folder may have changed
    int row = -1;
    for (int i = 0; i < presets.getPresetCount() && row < 0; ++i)
        if (presets.getPresetName(i) == name) row = i;
    if (row == getSelectedRow()) return;
    const juce::ScopedValueSetter<bool> quiet(showing, true);
    if (row < 0) deselectAllRows();
    else selectRow(row);
}

void AfxTab::SoundList::selectedRowsChanged(int lastRow) {
    if (!showing && lastRow >= 0 && onChoose) onChoose(lastRow);
}

void AfxTab::SoundList::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected) {
    const auto& theme = lnf.getTheme();
    if (rowIsSelected) {
        g.setColour(theme.accent.withAlpha(0.2f));
        g.fillRect(0, 0, width, height);
        g.setColour(theme.accent);
        g.fillRect(0, 0, 3, height);
    }
    g.setFont(lnf.getCustomFont(10.0f, juce::Font::bold));
    g.setColour(rowIsSelected ? theme.accent : theme.textMuted);
    g.drawText(juce::String(row).paddedLeft('0', 3), 10, 0, 30, height, juce::Justification::centredLeft);
    g.setFont(lnf.getCustomFont(11.5f, rowIsSelected ? juce::Font::bold : juce::Font::plain));
    g.setColour(rowIsSelected ? theme.textTitle : theme.textBody);
    g.drawText(presets.getPresetName(row), 42, 0, width - 48, height, juce::Justification::centredLeft, true);
}

void AfxTab::SoundList::paint(juce::Graphics& g) { g.fillAll(lnf.getTheme().buttonBg); }

void AfxTab::SoundList::paintOverChildren(juce::Graphics& g) {
    g.setColour(hasKeyboardFocus(true) ? lnf.getTheme().textMuted : lnf.getTheme().buttonBorder);
    g.drawRect(getLocalBounds(), 1);
}

// ------------------------------------------------------------------------------
// Keyboard
// ------------------------------------------------------------------------------
AfxTab::KeyMap::KeyMap(SynthModel& m, ModernLookAndFeel& l) : model(m), lnf(l) {}

juce::Rectangle<float> AfxTab::KeyMap::keyBounds(int note) const {
    const float keyW = (float)getWidth() / (float)kWhiteKeys;
    const float h = (float)getHeight();
    if (!isBlackKey(note)) return { (float)whiteKeysBefore(note) * keyW, 0.0f, keyW, h };
    const float blackW = keyW * 0.62f;
    return { (float)whiteKeysBefore(note) * keyW - blackW * 0.5f, 0.0f, blackW, h * 0.60f };
}

int AfxTab::KeyMap::noteAt(juce::Point<float> p) const {
    if (!getLocalBounds().toFloat().contains(p)) return -1;
    for (int note = 0; note < 128; ++note)
        if (isBlackKey(note) && keyBounds(note).contains(p)) return note;
    for (int note = 0; note < 128; ++note)
        if (!isBlackKey(note) && keyBounds(note).contains(p)) return note;
    return -1;
}

void AfxTab::KeyMap::paint(juce::Graphics& g) {
    const auto& theme = lnf.getTheme();
    const auto& kit = model.getAfxKit();
    const juce::Colour ivory(0xffdfe4e8), ebony(0xff15181c);
    g.setColour(theme.cardBorder);
    g.fillRect(getLocalBounds());

    for (const bool black : { false, true }) {
        for (int note = 0; note < 128; ++note) {
            if (isBlackKey(note) != black) continue;
            const int pad = kit.getSlotForNote(static_cast<uint8_t>(note));
            const bool mine = pad == selected;
            const auto colour = padColour(pad);
            auto r = keyBounds(note);
            if (!black) r = r.withTrimmedRight(1.0f);
            g.setColour(black ? ebony.interpolatedWith(colour, mine ? 0.85f : 0.35f)
                              : ivory.interpolatedWith(colour, mine ? 0.90f : 0.30f));
            g.fillRect(r);
            if (mine) {   // a marker at the bottom of the selected pad's keys
                g.setColour(black ? theme.textTitle : ebony);
                g.fillRect(r.getCentreX() - 1.5f, r.getBottom() - 8.0f, 3.0f, 3.0f);
            }
            if (note == hovered) {
                g.setColour(theme.accent);
                g.drawRect(r, 2.0f);
            }
            if (!black && note % 12 == 0 && r.getWidth() >= 9.0f) {
                g.setFont(lnf.getCustomFont(8.0f, juce::Font::bold));
                g.setColour(ebony.withAlpha(0.8f));
                g.drawText("C" + juce::String(note / 12 - 1), r.withTrimmedBottom(10.0f).removeFromBottom(12.0f),
                           juce::Justification::centred, false);
            }
        }
    }
}

void AfxTab::KeyMap::mouseDown(const juce::MouseEvent& e) { mouseDrag(e); }

void AfxTab::KeyMap::mouseDrag(const juce::MouseEvent& e) {
    const int note = noteAt(e.position);
    hover(note);
    if (note >= 0 && onPaint) onPaint(note);
}

void AfxTab::KeyMap::mouseMove(const juce::MouseEvent& e) { hover(noteAt(e.position)); }

void AfxTab::KeyMap::mouseExit(const juce::MouseEvent&) { hover(-1); }

void AfxTab::KeyMap::hover(int note) {
    if (note == hovered) return;
    hovered = note;
    repaint();
    if (onHover) onHover(note);
}

// ------------------------------------------------------------------------------
// Tab
// ------------------------------------------------------------------------------
AfxTab::AfxTab(ModernTabContext& ctx) : ModernTabModule(ctx) {}

void AfxTab::setup() {
    for (auto* card : { &kitCard, &soundCard, &keyCard }) {
        addAndMakeVisible(*card);
        card->toBack();
    }
    setupKitControls();
    setupSoundControls();
    setupKeyControls();
    setupSetupFiles();
    assignComponentIDs();
    selectPad(model.getCurrentPreset().steppedParams[spAFXSelectedSlot]);
    updateFromEngine();
}

void AfxTab::setupKitControls() {
    afxModeButton.setClickingTogglesState(true);
    afxModeButton.setTooltip("On: every key plays the sound of its pad. Off: MIDI channel N plays pad N.");
    afxModeButton.onClick = [this] {
        const bool on = afxModeButton.getToggleState();
        // The kit's key map decides which sound a key plays; split / layer
        // routes of an older session would override it.
        if (on) model.setCustomRouting(false);
        setSteppedParam(spEngineMode, static_cast<uint8_t>(on ? emAFX : emMultiChannel));
        updateFromEngine();
    };
    addAndMakeVisible(afxModeButton);
    modeLabel.setFont(modernLnf.getCustomFont(10.0f, juce::Font::plain));
    modeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modeLabel);
    addAndMakeVisible(saveSetupButton);
    addAndMakeVisible(loadSetupButton);

    pads = std::make_unique<PadGrid>(model, modernLnf);
    pads->onSelect = [this](int pad) { selectPad(pad); };
    addAndMakeVisible(*pads);
}

void AfxTab::setupSoundControls() {
    addAndMakeVisible(padTitle);
    for (auto* label : { &padHintLabel, &keysLabel, &keysHintLabel }) {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
        addAndMakeVisible(*label);
    }
    keysLabel.setColour(juce::Label::textColourId, modernLnf.getTheme().textBody);
    keysHintLabel.setText("Paint keys on the keyboard below; ALL KEYS gives this pad every key.",
                          juce::dontSendNotification);

    soundList = std::make_unique<SoundList>(model.getPresetManager(), modernLnf);
    soundList->onChoose = [this](int preset) { loadSoundIntoPad(preset); };
    addAndMakeVisible(*soundList);

    levelKnob = createKnob("PadLevel", 0, 999, 999, KnobMode::Percent);
    levelKnob->onValueChange = [this] { setPadLevel((int)std::round(levelKnob->getValue())); };
    addAndMakeVisible(*levelKnob);
    levelLabel = createLabel("LEVEL", *this);

    copyEditButton.onClick = [this] { copyEditedSound(); };
    addAndMakeVisible(copyEditButton);
}

void AfxTab::setupKeyControls() {
    keyMap = std::make_unique<KeyMap>(model, modernLnf);
    keyMap->onPaint = [this](int note) {
        auto& kit = model.getAfxKit();
        if (kit.getSlotForNote(static_cast<uint8_t>(note)) == selectedPad) return;
        kit.setNoteMapping(static_cast<uint8_t>(note), static_cast<uint8_t>(selectedPad));
        keysChanged();
    };
    keyMap->onHover = [this](int note) { showHover(note); };
    addAndMakeVisible(*keyMap);

    auto& kit = model.getAfxKit();
    octaveMapButton.onClick = [this, &kit] { kit.mapOctaveZones(); keysChanged(); };
    chromaticMapButton.onClick = [this, &kit] { kit.mapChromatic16(); keysChanged(); };
    allKeysButton.onClick = [this, &kit] { kit.mapAllToSlot(static_cast<uint8_t>(selectedPad)); keysChanged(); };
    defaultMapButton.onClick = [this, &kit] { kit.mapDefault(); keysChanged(); };
    octaveMapButton.setTooltip("Pad 1 on octave -1, pad 2 on octave 0, ...");
    chromaticMapButton.setTooltip("The pads in turn on consecutive keys");
    allKeysButton.setTooltip("Every key plays the selected pad");
    defaultMapButton.setTooltip("The key map of the default kit");
    for (auto* button : { &octaveMapButton, &chromaticMapButton, &allKeysButton, &defaultMapButton })
        addAndMakeVisible(*button);

    hoverLabel.setFont(modernLnf.getCustomFont(10.0f, juce::Font::plain));
    hoverLabel.setJustificationType(juce::Justification::centredLeft);
    hoverLabel.setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
    addAndMakeVisible(hoverLabel);
    showHover(-1);
}

void AfxTab::setupSetupFiles() {
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
    auto choose = [this](bool save) {
        setupChooser = std::make_unique<juce::FileChooser>(save ? "Save kit (complete setup)" : "Load kit (complete setup)",
            juce::File{}, "*.ovm");
        juce::Component::SafePointer<AfxTab> safe(this);
        setupChooser->launchAsync((save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
            : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles,
            [safe, save](const juce::FileChooser& chooser) {
                if (!safe || !safe->processor || chooser.getResult() == juce::File{}) return;
                const auto file = save ? chooser.getResult().withFileExtension("ovm") : chooser.getResult();
                const bool ok = save ? safe->processor->saveSetup(file) : safe->processor->loadSetup(file);
                if (!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Kit", "The kit could not be saved or loaded.");
                safe->selectPad(0);
                if (safe->context.refreshFromEngine) safe->context.refreshFromEngine();
            });
    };
    saveSetupButton.onClick = [choose] { choose(true); };
    loadSetupButton.onClick = [choose] { choose(false); };
    saveSetupButton.setTooltip("Saves all pads, the key map and the mixer (.ovm)");
    loadSetupButton.setTooltip("Loads a kit saved with SAVE KIT (.ovm)");
#else
    saveSetupButton.setEnabled(false);
    loadSetupButton.setEnabled(false);
#endif
}

void AfxTab::assignComponentIDs() {
    kitCard.setComponentID("afxKitCard");
    soundCard.setComponentID("afxSoundCard");
    keyCard.setComponentID("afxKeyCard");
    afxModeButton.setComponentID("engineModeToggle[1]");
    modeLabel.setComponentID("afxModeLabel");
    saveSetupButton.setComponentID("saveSetupButton");
    loadSetupButton.setComponentID("loadSetupButton");
    pads->setComponentID("afxPads");
    padTitle.setComponentID("afxPadTitle");
    keysHintLabel.setComponentID("afxKeysHintLabel");
    padHintLabel.setComponentID("afxPadHintLabel");
    keysLabel.setComponentID("afxKeysLabel");
    soundList->setComponentID("afxSoundList");
    levelKnob->setComponentID("afxLevelKnob");
    levelLabel->setComponentID("afxLevelLabel");
    copyEditButton.setComponentID("assignCurrentPresetBtn");
    keyMap->setComponentID("afxKeyboardZone");
    octaveMapButton.setComponentID("octaveMapBtn");
    chromaticMapButton.setComponentID("chromaticMapBtn");
    allKeysButton.setComponentID("allToSlotBtn");
    defaultMapButton.setComponentID("defaultMapBtn");
    hoverLabel.setComponentID("afxHoverLabel");
}

void AfxTab::selectPad(int pad) {
    selectedPad = std::clamp(pad, 0, AFX_SLOT_COUNT - 1);
    // Only a real change reaches the host: opening the editor re-selects the
    // stored pad, and an edit there is flagged by Bitwig (performEdit()
    // before the plug-in is initialised).
    if (model.getCurrentPreset().steppedParams[spAFXSelectedSlot] != (uint8_t)selectedPad)
        setSteppedParam(spAFXSelectedSlot, (uint8_t)selectedPad);
    if (pads) pads->setSelected(selectedPad);
    if (keyMap) keyMap->setSelected(selectedPad);
    showSelectedPad();
    resized();   // the card's badge names the pad
}

void AfxTab::showSelectedPad() {
    if (!soundList) return;
    const auto& slot = model.getAfxKit().getSlot(selectedPad);
    padTitle.set(selectedPad, slot.name);
    padHintLabel.setText(selectedPad == 0 ? "Pad 1 plays the sound you edit in the other tabs."
                                          : "Pick a preset, or copy the sound you edit in the other tabs.",
                         juce::dontSendNotification);
    keysLabel.setText(keysText(model.getAfxKit(), selectedPad), juce::dontSendNotification);

    soundList->show(slot.name);

    safeSetKnob(levelKnob.get(), scan_potFrom16bits(slot.preset.continuousParams[cpAmpLevel]));
    copyEditButton.setEnabled(selectedPad != 0);
    copyEditButton.setButtonText(selectedPad == 0 ? "PAD 1 IS THE EDITED SOUND" : "COPY EDITED SOUND TO THIS PAD");
}

void AfxTab::loadSoundIntoPad(int presetIndex) {
    auto& presets = model.getPresetManager();
    if (presetIndex < 0 || presetIndex >= presets.getPresetCount()) return;
    if (selectedPad == 0) {
        // Pad 1 is the edited preset: load it as the preset bar does.
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
        if (processor) {
            processor->setCurrentProgram(presetIndex);
            return;
        }
#endif
        model.loadPreset(presetIndex);
        showSelectedPad();
        return;
    }
    PresetData preset;
    if (!presets.loadPreset(presetIndex, preset)) return;
    auto& slot = model.getAfxKit().getSlot(selectedPad);
    slot.preset = preset;
    slot.name = presets.getPresetName(presetIndex);
    slot.waveManager.setBaseDirectory(model.getWaveManager().getBaseDirectory());
    for (int w = 0; w < abxCount; ++w)
        slot.waveManager.loadWave(static_cast<abx_t>(w), slot.preset.oscBank[w], slot.preset.oscWave[w]);
    showSelectedPad();
    if (pads) pads->repaint();
}

// Pad 1's sound, with its waves as they are now (also edited ones), into the
// selected pad.
void AfxTab::copyEditedSound() {
    if (selectedPad == 0) return;
    auto& kit = model.getAfxKit();
    auto& slot = kit.getSlot(selectedPad);
    slot.preset = model.getCurrentPreset();
    slot.name = kit.getSlot(0).name;
    for (int w = 0; w < abxCount; ++w) {
        const auto abx = static_cast<abx_t>(w);
        std::copy_n(model.getWaveManager().getWaveData(abx), WTOSC_SAMPLE_COUNT, slot.waveManager.getMutableWaveData(abx));
    }
    showSelectedPad();
    if (pads) pads->repaint();
}

void AfxTab::setPadLevel(int pot) {
    if (selectedPad == 0) {
        setContinuousParam(cpAmpLevel, (float)pot);   // pad 1: the host parameter
        return;
    }
    model.getAfxKit().getSlot(selectedPad).preset.continuousParams[cpAmpLevel] =
        static_cast<uint16_t>(scan_potTo16bits(pot));
}

void AfxTab::keysChanged() {
    if (keyMap) keyMap->repaint();
    if (pads) pads->repaint();
    keysLabel.setText(keysText(model.getAfxKit(), selectedPad), juce::dontSendNotification);
}

void AfxTab::showHover(int note) {
    if (note < 0) {
        hoverLabel.setText("Click or drag over keys to put them on the selected pad.", juce::dontSendNotification);
        return;
    }
    const int pad = model.getAfxKit().getSlotForNote(static_cast<uint8_t>(note));
    hoverLabel.setText(noteName(note) + " (note " + juce::String(note) + ") plays pad " + juce::String(pad + 1)
                       + ": " + model.getAfxKit().getSlot(pad).name, juce::dontSendNotification);
}

void AfxTab::advanceActivity() {
    const uint16_t sounding = model.takeSoundingParts();
    if (pads && isShowing()) pads->setSounding(sounding);
}

void AfxTab::updateFromEngine() {
    if (!pads) return;
    const bool on = model.getCurrentPreset().steppedParams[spEngineMode] == emAFX;
    safeSetToggle(&afxModeButton, on);
    afxModeButton.setButtonText(on ? "AFX MODE: ON" : "AFX MODE: OFF");
    modeLabel.setText(on ? "ON: every key plays the sound of its pad."
                         : "OFF: MIDI channel N plays pad N. Switch on to play the kit.",
                      juce::dontSendNotification);
    modeLabel.setColour(juce::Label::textColourId, on ? modernLnf.getTheme().accent : modernLnf.getTheme().textMuted);
    showSelectedPad();
    if (pads) pads->repaint();
    if (keyMap) keyMap->repaint();
}

void AfxTab::resized() {
    const int totalW = getWidth(), totalH = getHeight();
    if (totalW <= 0 || totalH <= 0) return;
    constexpr int gap = 5, keyCardH = 196;
    const int topH = totalH - keyCardH - gap;
    const int kitW = totalW * 60 / 100;
    const int soundX = kitW + gap, soundW = totalW - soundX;

    // Kit: the switch and the kit files, then the pads.
    kitCard.setBounds(0, 0, kitW, topH);
    kitCard.clearDividers();
    const int left = 16, innerW = kitW - 32;
    afxModeButton.setBounds(left, 38, 140, 28);
    loadSetupButton.setBounds(left + innerW - 96, 40, 96, 24);
    saveSetupButton.setBounds(left + innerW - 2 * 96 - 8, 40, 96, 24);
    modeLabel.setBounds(left + 150, 40, innerW - 150 - 2 * 96 - 16, 24);
    kitCard.addDivider(74, "PADS  (CLICK TO SELECT)");
    pads->setBounds(left, 90, innerW, topH - 90 - 12);

    // Selected pad
    soundCard.setBounds(soundX, 0, soundW, topH);
    soundCard.setHeader("SELECTED PAD", "PAD " + juce::String(selectedPad + 1));
    soundCard.clearDividers();
    const int sx = soundX + 16, sw = soundW - 32;
    padTitle.setBounds(sx, 36, sw, 24);
    padHintLabel.setBounds(sx, 60, sw, 18);
    // The preset list, the level beside it
    constexpr int levelW = 84;
    const int listW = sw - levelW - 12;
    soundCard.addDivider(10, 88, listW + 6, "SOUND");
    soundCard.addDivider(16 + listW + 12, 88, levelW - 6, "LEVEL");
    const int keysDivY = std::max(200, topH - 128);
    soundList->setBounds(sx, 100, listW, keysDivY - 12 - 100);
    const int knobSz = getStandardKnobSize();
    layoutKnob(levelKnob, levelLabel, sx + listW + 12 + (levelW - knobSz) / 2, 104, knobSz);
    soundCard.addDivider(keysDivY, "KEYS");
    keysLabel.setBounds(sx, keysDivY + 12, sw, 20);
    keysHintLabel.setBounds(sx, keysDivY + 32, sw, 18);
    copyEditButton.setBounds(sx, topH - 12 - 26, sw, 26);

    // Keyboard
    const int keyY = topH + gap;
    keyCard.setBounds(0, keyY, totalW, keyCardH);
    keyCard.clearDividers();
    keyCard.addDivider(36, "KEY MAP  (CLICK OR DRAG KEYS ONTO THE SELECTED PAD)");
    constexpr int mapW = 96;
    int bx = totalW - 16 - 4 * mapW - 3 * 6;
    for (auto* button : { &octaveMapButton, &chromaticMapButton, &allKeysButton, &defaultMapButton }) {
        button->setBounds(bx, keyY + 48, mapW, 24);
        bx += mapW + 6;
    }
    hoverLabel.setBounds(16, keyY + 48, totalW - 32 - 4 * mapW - 3 * 6 - 12, 24);
    keyMap->setBounds(16, keyY + 80, totalW - 32, keyCardH - 80 - 12);
}
