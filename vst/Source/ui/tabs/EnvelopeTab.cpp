#include "EnvelopeTab.h"
#include "../../data/ParamLabels.h"

namespace {
const char* const kStageIds[5] = { "Att", "Dec", "Sus", "Rel", "Vel" };
const char* const kStageKnobNames[5] = { "Atk", "Dec", "Sus", "Rel", "Vel" };
const char* const kStageLabels[5] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "SENSITIVITY" };
}

const std::array<EnvelopeTab::EnvelopeDescriptor, 3> EnvelopeTab::kEnvelopes{ {
    { "fil", "F", "Filter ADSR Curve", voiceconfig::kFilterEnvelope, { 0, 500, 500, 500 }, 0 },
    { "amp", "A", "Amplifier / VCA ADSR Curve", voiceconfig::kAmpEnvelope, { 0, 0, 999, 500 }, 1301 },
    { "wmod", "W", "WaveMod ADSR Curve", voiceconfig::kWaveModEnvelope, { 0, 500, 500, 500 }, 1302 },
} };

EnvelopeTab::EnvelopeTab(ModernTabContext& context)
    : ModernTabModule(context) {}

void EnvelopeTab::setup() {
    addAndMakeVisible(filEnvCard);
    addAndMakeVisible(ampEnvCard);
    addAndMakeVisible(wmodEnvCard);
    filEnvCard.toBack();
    ampEnvCard.toBack();
    wmodEnvCard.toBack();

    for (size_t e = 0; e < kEnvelopes.size(); ++e) createControls(sections[e], kEnvelopes[e]);
    // Interactive ADSR curve visualizers
    for (size_t e = 0; e < kEnvelopes.size(); ++e) createCurve(sections[e], kEnvelopes[e]);

    assignComponentIDs();
}

void EnvelopeTab::createControls(EnvelopeSection& section, const EnvelopeDescriptor& d) {
    const auto& p = d.params;
    const continuousParameter_t params[5] = { p.attack, p.decay, p.sustain, p.release, p.velocity };
    for (int k = 0; k < 5; ++k) {
        const bool percent = k == 2 || k == EnvelopeSection::kVelocity;
        const int init = k == EnvelopeSection::kVelocity ? 0 : d.defaults[(size_t)k];
        auto& knob = section.knobs[(size_t)k];
        knob = createKnob(juce::String(d.knobPrefix) + kStageKnobNames[k], 0, 999, init,
                          percent ? KnobMode::Percent : KnobMode::TimeMs);
        auto* raw = knob.get();
        const auto cp = params[k];
        knob->onValueChange = [this, raw, cp]() { setContinuousParam(cp, (float)raw->getValue()); };
        addAndMakeVisible(*knob);
        section.labels[(size_t)k] = createLabel(kStageLabels[k], *this);
    }

    if (d.typeRadioGroup != 0) {
        for (int i = 0; i < 4; ++i) {
            auto& toggle = section.typeToggles[(size_t)i];
            toggle = createToggle(paramlabels::kEnvelopeTypes[i]);
            toggle->setRadioGroupId(d.typeRadioGroup);
            toggle->onClick = [this, &section, p, i]() {
                setSteppedParam(p.slow, (i & 1) ? 1 : 0);
                setSteppedParam(p.linear, (i & 2) ? 1 : 0);
                for (int k : { 0, 1, 3 }) section.knobs[(size_t)k]->updateText();
            };
            addAndMakeVisible(*toggle);
        }
        section.loopToggle = std::make_unique<juce::ToggleButton>("LOOP ENVELOPE");
        auto* loop = section.loopToggle.get();
        section.loopToggle->onClick = [this, loop, p]() { setSteppedParam(p.loop, loop->getToggleState() ? 1 : 0); };
        addAndMakeVisible(*section.loopToggle);
    }

    // Time knobs show the real stage duration, including the x4 slow range.
    for (int k : { 0, 1, 3 }) {
        auto* knob = section.knobs[(size_t)k].get();
        const auto slow = p.slow;
        knob->textFromValueFunction = [this, slow](double value) {
            return formatEnvelopeTime(value, model.getCurrentPreset().steppedParams[slow] != 0);
        };
        knob->valueFromTextFunction = [this, slow](const juce::String& text) {
            return parseEnvelopeTime(text, model.getCurrentPreset().steppedParams[slow] != 0);
        };
        knob->updateText();
    }
}

void EnvelopeTab::createCurve(EnvelopeSection& section, const EnvelopeDescriptor& d) {
    auto& k = section.knobs;
    section.curve = std::make_unique<AdsrCurveComponent>(model, *k[0], *k[1], *k[2], *k[3], d.curveTitle);
    addAndMakeVisible(*section.curve);
}

void EnvelopeTab::assignComponentIDs() {
    filEnvCard.setComponentID("filEnvCard");
    ampEnvCard.setComponentID("ampEnvCard");
    wmodEnvCard.setComponentID("wmodEnvCard");
    for (size_t e = 0; e < kEnvelopes.size(); ++e) assignComponentIDs(sections[e], kEnvelopes[e]);
}

void EnvelopeTab::assignComponentIDs(EnvelopeSection& section, const EnvelopeDescriptor& d) {
    const juce::String prefix(d.idPrefix);
    for (int k = 0; k < 5; ++k) {
        if (section.knobs[(size_t)k]) section.knobs[(size_t)k]->setComponentID(prefix + kStageIds[k] + "Knob");
        if (section.labels[(size_t)k]) section.labels[(size_t)k]->setComponentID(prefix + kStageIds[k] + "Label");
    }
    for (int i = 0; i < 4; ++i)
        if (section.typeToggles[(size_t)i])
            section.typeToggles[(size_t)i]->setComponentID(prefix + "EnvTypeToggle[" + juce::String(i) + "]");
    if (section.loopToggle) section.loopToggle->setComponentID(prefix + "EnvLoopToggle");
    if (section.curve) section.curve->setComponentID(prefix + "AdsrCurve");
}

void EnvelopeTab::updateFromEngine() {
    for (size_t e = 0; e < kEnvelopes.size(); ++e) updateSection(sections[e], kEnvelopes[e]);
}

void EnvelopeTab::updateSection(EnvelopeSection& section, const EnvelopeDescriptor& d) {
    const auto& preset = model.getCurrentPreset();
    const auto& p = d.params;
    const continuousParameter_t params[5] = { p.attack, p.decay, p.sustain, p.release, p.velocity };
    for (int k = 0; k < 5; ++k)
        safeSetKnob(section.knobs[(size_t)k].get(), scan_potFrom16bits(preset.continuousParams[params[k]]));

    if (section.loopToggle) safeSetToggle(section.loopToggle.get(), preset.steppedParams[p.loop] != 0);
    const int typeId = (preset.steppedParams[p.linear] ? 2 : 0) + (preset.steppedParams[p.slow] ? 1 : 0);
    for (int i = 0; i < 4; ++i)
        if (section.typeToggles[(size_t)i])
            section.typeToggles[(size_t)i]->setToggleState(i == typeId, juce::dontSendNotification);

    // The slow range may have changed without a knob value changing.
    for (int k : { 0, 1, 3 })
        if (section.knobs[(size_t)k]) section.knobs[(size_t)k]->updateText();
    if (section.curve) section.curve->repaint();
}

void EnvelopeTab::resized() {
    const auto tabBounds = getLocalBounds();

    int colGap = 5;
    int colW = (tabBounds.getWidth() - colGap * 2) / 3;
    const int colX[3] = { 0, colW + colGap, 2 * (colW + colGap) };
    int cardTopH = 114;

    int velColW = 74;
    int adsrAreaW = colW - velColW - 14;
    int sepX = 6 + adsrAreaW + 2;

    ModernSectionCard* cards[3] = { &filEnvCard, &ampEnvCard, &wmodEnvCard };
    for (int e = 0; e < 3; ++e) {
        auto& card = *cards[e];
        card.setBounds(colX[e], 0, colW, cardTopH);
        card.clearDividers();
        card.addDivider(6, 34, adsrAreaW, "ADSR");
        card.addDivider(sepX + 4, 34, velColW - 8, "VELOCITY");
        card.addVerticalDivider(sepX, 28, cardTopH - 6);
        layoutKnobs(sections[(size_t)e], colX[e], adsrAreaW, sepX, velColW);
    }

    // Interactive ADSR curves and the curve type / loop toggles below them
    int envCurveGap = 5;
    int envCurveY = cardTopH + envCurveGap;
    int totalBottomH = tabBounds.getHeight() - envCurveY;
    int togAreaH = 46; // 2 rows of toggles
    int curveH = std::max(40, totalBottomH - togAreaH - 6);
    int togY = envCurveY + curveH + 6;
    for (int e = 0; e < 3; ++e) {
        auto& section = sections[(size_t)e];
        if (section.curve) section.curve->setBounds(colX[e], envCurveY, colW, curveH);
        layoutToggles(section, colX[e], colW, togY);
    }
}

// ADSR knobs in four columns, the velocity knob right of the divider.
void EnvelopeTab::layoutKnobs(EnvelopeSection& section, int startX, int adsrAreaW, int sepX, int velColW) {
    int knob4W = adsrAreaW / 4;
    int knobSz = getStandardKnobSize();
    int knobY = 40;
    for (int k = 0; k < 4; ++k)
        layoutKnob(section.knobs[(size_t)k].get(), section.labels[(size_t)k],
                   startX + 6 + knob4W * k + (knob4W - knobSz) / 2, knobY, knobSz);
    const int velX = startX + sepX + (velColW - knobSz) / 2;
    layoutKnob(section.knobs[EnvelopeSection::kVelocity].get(), section.labels[EnvelopeSection::kVelocity],
               velX, knobY, knobSz);
}

// Two rows: exponential types (fast, slow) left, linear types in the middle,
// loop on the right of the first row.
void EnvelopeTab::layoutToggles(EnvelopeSection& section, int startX, int cardW, int startY) {
    auto& typeToggles = section.typeToggles;
    int margin = 8;
    int availW = cardW - 2 * margin;
    int colGap = 6;
    int colW = (availW - colGap * 2) / 3;
    int c1X = startX + margin;
    int c2X = c1X + colW + colGap;
    int c3X = startX + cardW - margin - (colW + 4);
    int row1Y = startY;
    int row2Y = startY + 22;
    int togH = 20;

    if (typeToggles[0]) typeToggles[0]->setBounds(c1X, row1Y, colW, togH);
    if (typeToggles[2]) typeToggles[2]->setBounds(c2X, row1Y, colW, togH);
    if (section.loopToggle) section.loopToggle->setBounds(c3X, row1Y, colW + 4, togH);

    if (typeToggles[1]) typeToggles[1]->setBounds(c1X, row2Y, colW, togH);
    if (typeToggles[3]) typeToggles[3]->setBounds(c2X, row2Y, colW, togH);
}
