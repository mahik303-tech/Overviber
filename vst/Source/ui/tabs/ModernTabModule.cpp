#include "ModernTabContext.h"
#include <algorithm>

ModernTabModule::ModernTabModule(ModernTabContext& ctx)
    : context(ctx), engine(ctx.engine), processor(ctx.processor), modernLnf(ctx.lookAndFeel) {}

void ModernTabModule::setContinuousParam(continuousParameter_t cp, float potVal) {
    if (context.setContinuousParam) context.setContinuousParam(cp, potVal);
}

void ModernTabModule::setSteppedParam(steppedParameter_t sp, uint8_t stepVal) {
    if (context.setSteppedParam) context.setSteppedParam(sp, stepVal);
}

std::unique_ptr<juce::Slider> ModernTabModule::createKnob(const juce::String& name, double min, double max, double init,
                                                           KnobMode mode, const juce::String& suffix) {
    auto slider = std::make_unique<juce::Slider>(name);
    slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    slider->setRange(min, max, 1.0);
    slider->setTextValueSuffix(suffix);
    slider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffb0bec5));
    slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    // Smart Physical Unit Value Formatter
    slider->textFromValueFunction = [mode](double val) -> juce::String {
        switch (mode) {
        case KnobMode::Percent:
            return juce::String((int)std::round((val / 999.0) * 100.0)) + " %";
        case KnobMode::BipolarPercent: {
            int pct = (int)std::round((val / 499.0) * 100.0);
            return (pct > 0 ? "+" : "") + juce::String(pct) + " %";
        }
        case KnobMode::PitchSemitones: {
            float st = ((float)val / 499.0f) * 12.0f;
            int roundSt = (int)std::round(st);
            return (roundSt > 0 ? "+" : "") + juce::String(roundSt) + " st";
        }
        case KnobMode::FineDetuneCents: {
            float ct = ((float)val / 499.0f) * 50.0f;
            int roundCt = (int)std::round(ct);
            return (roundCt > 0 ? "+" : "") + juce::String(roundCt) + " ct";
        }
        case KnobMode::CutoffHz: {
            float hz = 20.0f * std::pow(10.0f, ((float)val / 999.0f) * 3.0f);
            if (hz >= 1000.0f)
                return juce::String(hz / 1000.0f, 2) + " kHz";
            return juce::String((int)std::round(hz)) + " Hz";
        }
        case KnobMode::TimeMs: {
            float frac = (float)val / 999.0f;
            float ms = frac * frac * 8000.0f;
            if (ms < 1.0f) return "0 ms";
            if (ms >= 1000.0f) return juce::String(ms / 1000.0f, 2) + " s";
            return juce::String((int)std::round(ms)) + " ms";
        }
        case KnobMode::LfoSpeedHz: {
            float hz = 0.05f * std::pow(1000.0f, (float)val / 999.0f);
            if (hz >= 10.0f) return juce::String(hz, 1) + " Hz";
            return juce::String(hz, 2) + " Hz";
        }
        case KnobMode::Raw:
        default:
            return juce::String((int)std::round(val));
        }
    };

    slider->valueFromTextFunction = [mode, min, max](const juce::String& text) -> double {
        juce::String t = text.trim();
        if (mode == KnobMode::CutoffHz) {
            if (t.endsWithIgnoreCase("khz")) {
                float khz = t.dropLastCharacters(3).trim().getFloatValue();
                return std::clamp((std::log10(std::max(20.0f, khz * 1000.0f) / 20.0f) / 3.0f) * 999.0f, (float)min, (float)max);
            }
            if (t.endsWithIgnoreCase("hz")) {
                float hz = t.dropLastCharacters(2).trim().getFloatValue();
                return std::clamp((std::log10(std::max(20.0f, hz) / 20.0f) / 3.0f) * 999.0f, (float)min, (float)max);
            }
        }
        if (mode == KnobMode::Percent) {
            float p = t.replace("%", "").trim().getFloatValue();
            return std::clamp((p / 100.0f) * 999.0f, (float)min, (float)max);
        }
        if (mode == KnobMode::BipolarPercent) {
            float p = t.replace("%", "").replace("+", "").trim().getFloatValue();
            return std::clamp((p / 100.0f) * 499.0f, (float)min, (float)max);
        }
        if (mode == KnobMode::PitchSemitones) {
            float st = t.replace("st", "").replace("+", "").trim().getFloatValue();
            return std::clamp((st / 12.0f) * 499.0f, (float)min, (float)max);
        }
        if (mode == KnobMode::FineDetuneCents) {
            float ct = t.replace("ct", "").replace("+", "").trim().getFloatValue();
            return std::clamp((ct / 50.0f) * 499.0f, (float)min, (float)max);
        }
        return std::clamp(t.getDoubleValue(), min, max);
    };

    slider->setValue(init, juce::dontSendNotification);
    slider->updateText();

    return slider;
}

std::unique_ptr<juce::Label> ModernTabModule::createLabel(const juce::String& text, juce::Component& parent) {
    auto label = std::make_unique<juce::Label>("", text);
    label->setFont(modernLnf.getCustomFont(9.0f, juce::Font::bold));
    label->setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
    label->setJustificationType(juce::Justification::centred);
    parent.addAndMakeVisible(*label);
    return label;
}

std::unique_ptr<juce::ComboBox> ModernTabModule::createCombo() {
    return std::make_unique<juce::ComboBox>();
}

std::unique_ptr<juce::ToggleButton> ModernTabModule::createToggle(const juce::String& text) {
    return std::make_unique<juce::ToggleButton>(text);
}

void ModernTabModule::safeSetKnob(juce::Slider* s, double val) {
    if (s && !s->isMouseButtonDown()) {
        s->setValue(val, juce::dontSendNotification);
        s->updateText();
    }
}

void ModernTabModule::safeSetToggle(juce::Button* b, bool state) {
    if (b && !b->isMouseButtonDown()) {
        b->setToggleState(state, juce::dontSendNotification);
    }
}

void ModernTabModule::safeSetCombo(juce::ComboBox& c, int id) {
    if (!c.isPopupActive()) {
        c.setSelectedId(id, juce::dontSendNotification);
    }
}
