#include "ModernTabContext.h"
#include "../../dsp/lfo.h"
#include "../../dsp/adsr.h"
#include "../../dsp/ControlTimes.h"
#include <algorithm>

ModernTabModule::ModernTabModule(ModernTabContext& ctx)
    : context(ctx), model(ctx.model), processor(ctx.processor), modernLnf(ctx.lookAndFeel) {}

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
            // The base pitch is the 16-bit value / 1024 semitones (cpAFreq >> 2).
            const float st = (float)scan_potTo16bits((int)std::round(val)) / 1024.0f;
            return "+" + juce::String(st, 1) + " st";
        }
        case KnobMode::TuneCents: {
            // Firmware master tune: (value >> 7) - 256 in 1/256 semitone.
            const int tune = (scan_potTo16bits((int)std::round(val) + 500) >> 7) - 256;
            const int ct = (int)std::round(tune * 100.0f / 256.0f);
            return (ct > 0 ? "+" : "") + juce::String(ct) + " ct";
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
        case KnobMode::LfoSpeedHz:
            return formatLfoSpeed(val, 0);   // the LFO tab adds the speed range
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
            return std::clamp((double)scan_potFrom16bits((int)std::clamp(st * 1024.0f, 0.0f, 65535.0f)), min, max);
        }
        if (mode == KnobMode::TuneCents) {
            float ct = t.replace("ct", "").replace("+", "").trim().getFloatValue();
            return std::clamp((ct / 100.0f) * 499.0f, (float)min, (float)max);
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

// Envelope time knobs (0..999) show the real stage duration of the envelope
// (attack, decay, release; see adsrStageMilliseconds). Set by EnvelopeTab.
juce::String ModernTabModule::formatEnvelopeTime(double potValue, bool slow) {
    const float ms = adsrStageMilliseconds(static_cast<uint16_t>(scan_potTo16bits((int)std::round(potValue))), slow);
    if (ms < 0.05f) return "0 ms";
    if (ms < 1.0f) return juce::String(ms, 1) + " ms";
    if (ms >= 1000.0f) return juce::String(ms / 1000.0f, 2) + " s";
    return juce::String((int)std::round(ms)) + " ms";
}

double ModernTabModule::parseEnvelopeTime(const juce::String& text, bool slow) {
    juce::String t = text.trim();
    float ms = t.getFloatValue();
    if (t.endsWithIgnoreCase("ms")) ms = t.dropLastCharacters(2).trim().getFloatValue();
    else if (t.endsWithIgnoreCase("s")) ms = t.dropLastCharacters(1).trim().getFloatValue() * 1000.0f;
    return scan_potFrom16bits(adsrCVForMilliseconds(ms, slow));
}

static juce::String formatMilliseconds(float ms) {
    if (ms >= 1000.0f) return juce::String(ms / 1000.0f, 2) + " s";
    return juce::String((int)std::round(ms)) + " ms";
}

juce::String ModernTabModule::formatGlideTime(double potValue) {
    const float ms = controltimes::glideOctaveMilliseconds(static_cast<uint16_t>(scan_potTo16bits((int)std::round(potValue))));
    return ms <= 0.0f ? juce::String("Off") : formatMilliseconds(ms) + "/oct";
}

juce::String ModernTabModule::formatModDelayTime(double potValue) {
    const float ms = controltimes::modDelayMilliseconds(static_cast<uint16_t>(scan_potTo16bits((int)std::round(potValue))));
    return ms <= 0.0f ? juce::String("Off") : formatMilliseconds(ms);
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

juce::String ModernTabModule::formatLfoSpeed(double potValue, int speedRange) {
    const int pot = scan_potFrom16bits(scan_potTo16bits((int)std::round(potValue)));
    const float hz = lfoCycleHz(pot, (int8_t)speedRange);
    if (hz <= 0.0f) return "0 Hz";
    if (hz < 0.1f) return juce::String(hz, 3) + " Hz";
    if (hz < 10.0f) return juce::String(hz, 2) + " Hz";
    return juce::String(hz, 1) + " Hz";
}
