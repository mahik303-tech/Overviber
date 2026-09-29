#pragma once

#include "OvercyclerTypes.h"
#include "ConsoleXProcessor.h"
#include "MackityProcessor.h"
#include "../data/PresetManager.h"
#include <cmath>
#ifdef OVERVIBER_DIAGNOSTICS
#include "SignalDiagnostics.h"
#endif

// ==============================================================================
// Master bus: stereo sum of the voices through the console, the Mackity
// parallel send, the output ceiling and the crossfade after a preset change.
//
// Per output sample the engine calls addVoice() for every sounding voice and
// then process(). Settings come from the main preset (engine-wide).
// ==============================================================================
class MasterBus {
public:
    // Voices enter the console 6 dB down, so the factory presets played
    // densely sit in its warm range (-6 .. 0 dB on the meters) instead of at
    // its ceiling; the fader pushes a voice into the saturation. The level
    // after the decoder makes that up: clean signals keep their level.
    static constexpr float kConsoleInputGain = 0.5f;
    static constexpr float kBusHeadroom = 0.9f;
    // Mackity send return at full send: -6 dB, or -12 dB with the pad.
    static constexpr float kMackityReturnGain = 0.5f;
    static constexpr float kMackityReturnPadGain = 0.25f;
    static constexpr float kSendSmoothingSeconds = 0.01f;
    // Output ceiling: linear up to the threshold, then a tanh knee that
    // approaches threshold + range (0.98).
    static constexpr float kCeilingThreshold = 0.9f;
    static constexpr float kCeilingRange = 0.08f;
    // Master meter reference: the ceiling's threshold reads +2 dB (red).
    static constexpr float kOutputMeterReference = 0.9f / 1.2589254f;
    static constexpr float kPresetCrossfadeSeconds = 0.003f;

    MasterBus() { updateSmoothing(48000.0f); }

    void prepare(float sampleRate) {
        mackity.setSampleRate(sampleRate);
        console.setSampleRate(sampleRate);
        updateSmoothing(sampleRate);
    }

    void reset() {
        mackity.reset();
        sendLevel = 0.0f;
        console.reset();
        lastLeft = lastRight = 0.0f;
        transitionLeft = transitionRight = 0.0f;
        transitionSamples = transitionRemaining = 0;
    }

    void setParameters(const PresetData& main) {
        auto pot = [&](continuousParameter_t cp) {
            return (float)scan_potFrom16bits(main.continuousParams[cp]);
        };
        console.setParameters(pot(cpConsoleDrive) / 999.0f, pot(cpConsolePad) / 999.0f,
                              pot(cpConsoleDiscontinuity) / 999.0f);
        // The send return carries the level; the Mackity output pad stays at 0 dB.
        mackity.setParameters(pot(cpMackityDrive), 999.0f);
        const float returnGain = main.steppedParams[spMackityReturnPad] != 0
            ? kMackityReturnPadGain : kMackityReturnGain;
        sendTarget = returnGain * pot(cpMackitySend) / 999.0f;
    }

    // Fades from the last output sample to the new sound after a preset change.
    void startPresetTransition(float fromLeft, float fromRight, float sampleRate) {
        transitionLeft = fromLeft;
        transitionRight = fromRight;
        transitionSamples = std::max(1, static_cast<int>(sampleRate * kPresetCrossfadeSeconds));
        transitionRemaining = transitionSamples;
    }
    float getLastLeft() const { return lastLeft; }
    float getLastRight() const { return lastRight; }

    // Console bus load where the console starts to saturate (bus knee): the
    // meters show the load in dB relative to it.
    static constexpr float kConsoleKnee = 0.65f;

    // Adds one voice sample with its pan gains, encoded by the console.
    // Returns the voice's encoded level, its share of the bus load.
    float addVoice(float sample, float panLeft, float panRight) {
        float encL = 0.0f, encR = 0.0f;
        const float trimmed = sample * kConsoleInputGain;
        console.encodeVoice(trimmed * panLeft, trimmed * panRight, encL, encR);
        sumLeft += encL;
        sumRight += encR;
        return std::max(std::abs(encL), std::abs(encR));
    }


    // Finishes one output sample and clears the sum for the next one.
    void process(float& outL, float& outR) {
        outL = 0.0f;
        outR = 0.0f;
        console.decodeMaster(sumLeft, sumRight, outL, outR);
#ifdef OVERVIBER_DIAGNOSTICS
        if (diagnostics) {
            diagnostics->consoleLeft.add(outL);
            diagnostics->consoleRight.add(outR);
        }
#endif
        outL *= kBusHeadroom;
        outR *= kBusHeadroom;

        // Saturated copy of the bus added on top. sendLevel includes the return
        // gain, so the pad toggle is smoothed together with the send knob.
        sendLevel += (sendTarget - sendLevel) * sendSmoothing;
        if (sendLevel > 1.0e-5f) {
            float wetL = outL, wetR = outR;
            mackity.processSample(wetL, wetR);
            outL += wetL * sendLevel;
            outR += wetR * sendLevel;
        } else {
            sendLevel = 0.0f;
        }

        outL = ceiling(outL);
        outR = ceiling(outR);
        if (transitionRemaining > 0) {
            const float oldWeight = static_cast<float>(transitionRemaining)
                / static_cast<float>(transitionSamples);
            outL = transitionLeft * oldWeight + outL * (1.0f - oldWeight);
            outR = transitionRight * oldWeight + outR * (1.0f - oldWeight);
            --transitionRemaining;
        }
        lastLeft = outL;
        lastRight = outR;
#ifdef OVERVIBER_DIAGNOSTICS
        if (diagnostics) {
            diagnostics->busLeft.add(sumLeft);
            diagnostics->busRight.add(sumRight);
            diagnostics->outputLeft.add(outL);
            diagnostics->outputRight.add(outR);
        }
#endif
        sumLeft = sumRight = 0.0f;
    }

    ConsoleXProcessor& getConsole() { return console; }
    const ConsoleXProcessor& getConsole() const { return console; }
    MackityProcessor& getMackity() { return mackity; }
#ifdef OVERVIBER_DIAGNOSTICS
    RenderDiagnostics* diagnostics = nullptr;
#endif

private:
    static float ceiling(float x) {
        return std::abs(x) <= kCeilingThreshold ? x
            : std::copysign(kCeilingThreshold + kCeilingRange * std::tanh((std::abs(x) - kCeilingThreshold) / kCeilingRange), x);
    }
    void updateSmoothing(float sampleRate) {
        sendSmoothing = 1.0f - std::exp(-1.0f / (kSendSmoothingSeconds * sampleRate));
    }

    ConsoleXProcessor console;
    MackityProcessor mackity;
    float sumLeft = 0.0f, sumRight = 0.0f;
    float sendTarget = 0.0f;
    float sendLevel = 0.0f;               // smoothed send amount including return gain
    float sendSmoothing = 0.0f;           // one-pole coefficient, ~10 ms
    float lastLeft = 0.0f, lastRight = 0.0f;
    float transitionLeft = 0.0f, transitionRight = 0.0f;
    int transitionSamples = 0, transitionRemaining = 0;
};
