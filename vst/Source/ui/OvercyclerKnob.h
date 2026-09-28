#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class OvercyclerKnob : public juce::Slider {
public:
    explicit OvercyclerKnob(const juce::String& name = "") {
        setSliderStyle(juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setName(name);
        setRotaryParameters(-2.4f, 2.4f, true); // ~275 degrees total rotation
    }

    ~OvercyclerKnob() override = default;

    void setPendingPickup(bool pending, bool needTurnRight) {
        if (pickupPending != pending || pickupNeedRight != needTurnRight) {
            pickupPending = pending;
            pickupNeedRight = needTurnRight;
            repaint();
        }
    }

    bool isPickupPending() const { return pickupPending; }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto center = bounds.getCentre();

        // 1. Soft Drop Shadow
        g.setColour(juce::Colour(0x66000000));
        g.fillEllipse(center.x - radius + 1.5f, center.y - radius + 2.5f, radius * 2.0f, radius * 2.0f);

        // 2. Outer Fluted Bezel (WH148 Potentiometer Grip)
        juce::ColourGradient rimGrad(juce::Colour(0xff3a3c42), center.x - radius, center.y - radius,
                                     juce::Colour(0xff18191c), center.x + radius, center.y + radius, false);
        g.setGradientFill(rimGrad);
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        // Fine radial fluting / knurling notches (18 teeth)
        g.setColour(juce::Colour(0x28000000));
        for (int i = 0; i < 18; ++i) {
            float a = i * (juce::MathConstants<float>::twoPi / 18.0f);
            float x1 = center.x + std::cos(a) * (radius * 0.88f);
            float y1 = center.y + std::sin(a) * (radius * 0.88f);
            float x2 = center.x + std::cos(a) * radius;
            float y2 = center.y + std::sin(a) * radius;
            g.drawLine(x1, y1, x2, y2, 1.2f);
        }

        // 3. Inner Brushed Aluminum / Bakelite Cap
        float innerRadius = radius * 0.80f;
        juce::ColourGradient capGrad(juce::Colour(0xff25272b), center.x, center.y - innerRadius,
                                     juce::Colour(0xff121315), center.x, center.y + innerRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

        // Inner rim bevel highlight
        g.setColour(juce::Colour(0x35ffffff));
        g.drawEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

        // Center machined recess
        float centerRecessRadius = innerRadius * 0.35f;
        g.setColour(juce::Colour(0xff0d0e10));
        g.fillEllipse(center.x - centerRecessRadius, center.y - centerRecessRadius,
                      centerRecessRadius * 2.0f, centerRecessRadius * 2.0f);

        // 4. White Indicator Pointer Line
        float angle = (float)valueToProportionOfLength(getValue());
        float rotaryAngle = -2.4f + angle * 4.8f; // -137 deg to +137 deg

        float ptrStartX = center.x + std::sin(rotaryAngle) * (innerRadius * 0.38f);
        float ptrStartY = center.y - std::cos(rotaryAngle) * (innerRadius * 0.38f);
        float ptrEndX = center.x + std::sin(rotaryAngle) * (innerRadius * 0.94f);
        float ptrEndY = center.y - std::cos(rotaryAngle) * (innerRadius * 0.94f);

        if (pickupPending) {
            // Soft-takeover pending: amber indicator
            g.setColour(juce::Colour(0xffffb74d));
            g.drawLine(ptrStartX, ptrStartY, ptrEndX, ptrEndY, 2.0f);

            // Small pickup direction dot on outer edge
            float dotA = rotaryAngle + (pickupNeedRight ? 0.3f : -0.3f);
            float dotX = center.x + std::sin(dotA) * (radius * 0.92f);
            float dotY = center.y - std::cos(dotA) * (radius * 0.92f);
            g.fillEllipse(dotX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
        } else {
            g.setColour(juce::Colours::white);
            g.drawLine(ptrStartX, ptrStartY, ptrEndX, ptrEndY, 2.0f);
        }
    }

private:
    bool pickupPending{ false };
    bool pickupNeedRight{ false };
};
