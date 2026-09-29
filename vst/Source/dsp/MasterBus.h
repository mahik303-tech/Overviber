#pragma once

#include "OvercyclerTypes.h"
#include "ConsoleXProcessor.h"
#include "MackityProcessor.h"
#include "Halfband2x.h"
#include "../data/PresetManager.h"
#include <cmath>
#ifdef OVERVIBER_DIAGNOSTICS
#include "SignalDiagnostics.h"
#endif

// ==============================================================================
// Master bus: stereo sum of the voices through the console, the Mackity
// parallel send, the output ceiling and the crossfade after a preset change.
//
// The bus runs at the voices' rate (oversampling 1 or 2 x the output rate):
// per voice-rate sample the engine calls addVoice() for every sounding voice
// and then endSubsample(); console and Mackity send work there, so their
// harmonics stay above the audio band. process() then decimates to one
// output sample (Halfband2x.h) and applies the output ceiling, the preset
// crossfade and the mute.
// Settings come from the main preset (engine-wide).
// ==============================================================================
class MasterBus {
public:
    // Voices enter the console 6 dB down, so the factory presets played
    // densely sit in its warm range (-6 .. 0 dB on the meters) instead of at
    // its ceiling; the fader pushes a voice into the saturation. The level
    // after the decoder makes that up, 0.5 dB short of the former 0.9: the
    // 2x signal path keeps the treble the old one lost, which raised the
    // peaks of dense low chords by that much (FactoryPresetHeadroom).
    static constexpr float kConsoleInputGain = 0.5f;
    static constexpr float kBusHeadroom = 0.85f;
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
    static constexpr float kMuteFadeSeconds = 0.005f;

    // Master mute (a mixer state, not part of the preset), faded over 5 ms.
    void setMuted(bool muted) { muteTarget = muted ? 0.0f : 1.0f; }

    MasterBus() { updateSmoothing(48000.0f, 48000.0f); }

    void prepare(float sampleRate, int newOversampling = 1) {
        oversampling = std::clamp(newOversampling, 1, 2);
        const float busRate = sampleRate * static_cast<float>(oversampling);
        mackity.setSampleRate(busRate);
        console.setSampleRate(busRate);
        updateSmoothing(sampleRate, busRate);
        decimatorLeft.reset();
        decimatorRight.reset();
        subsamples = 0;
    }

    void reset() {
        mackity.reset();
        sendLevel = 0.0f;
        console.reset();
        lastLeft = lastRight = 0.0f;
        transitionLeft = transitionRight = 0.0f;
        transitionSamples = transitionRemaining = 0;
        decimatorLeft.reset();
        decimatorRight.reset();
        subsamples = 0;
        sumLeft = sumRight = 0.0f;
    }

    void setParameters(const PresetData& main) {
        auto pot = [&](continuousParameter_t cp) {
            return (float)scan_potFrom16bits(main.continuousParams[cp]);
        };
        // The console's output pad (cpConsolePad, the former master fader) stays
        // at 1.0: level and saturation come from the amp level, the console
        // drive and the voice faders.
        console.setParameters(pot(cpConsoleDrive) / 999.0f, 1.0f, pot(cpConsoleDiscontinuity) / 999.0f);
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


    // Finishes one voice-rate sample of the bus and clears the sum.
    void endSubsample() {
        float outL = 0.0f, outR = 0.0f;
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

        if (subsamples < 2) {
            subLeft[subsamples] = outL;
            subRight[subsamples] = outR;
            ++subsamples;
        }
#ifdef OVERVIBER_DIAGNOSTICS
        if (diagnostics) {
            diagnostics->busLeft.add(sumLeft);
            diagnostics->busRight.add(sumRight);
        }
#endif
        sumLeft = sumRight = 0.0f;
    }

    // Finishes one output sample from the bus samples since the last call.
    void process(float& outL, float& outR) {
        if (oversampling == 2) {
            outL = decimatorLeft.process(subLeft[0], subLeft[1]);
            outR = decimatorRight.process(subRight[0], subRight[1]);
        } else {
            outL = subLeft[0];
            outR = subRight[0];
        }
        subsamples = 0;
        // After the decimator, whose ringing could otherwise pass the limit.
        outL = ceiling(outL);
        outR = ceiling(outR);
        if (transitionRemaining > 0) {
            const float oldWeight = static_cast<float>(transitionRemaining)
                / static_cast<float>(transitionSamples);
            outL = transitionLeft * oldWeight + outL * (1.0f - oldWeight);
            outR = transitionRight * oldWeight + outR * (1.0f - oldWeight);
            --transitionRemaining;
        }
        if (muteGain != muteTarget)
            muteGain = muteTarget > muteGain ? std::min(muteTarget, muteGain + muteStep)
                                             : std::max(muteTarget, muteGain - muteStep);
        outL *= muteGain;
        outR *= muteGain;
        lastLeft = outL;
        lastRight = outR;
#ifdef OVERVIBER_DIAGNOSTICS
        if (diagnostics) {
            diagnostics->outputLeft.add(outL);
            diagnostics->outputRight.add(outR);
        }
#endif
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
    void updateSmoothing(float sampleRate, float busRate) {
        muteStep = 1.0f / std::max(1.0f, sampleRate * kMuteFadeSeconds);
        sendSmoothing = 1.0f - std::exp(-1.0f / (kSendSmoothingSeconds * busRate));
    }

    ConsoleXProcessor console;
    MackityProcessor mackity;
    float sumLeft = 0.0f, sumRight = 0.0f;
    float sendTarget = 0.0f;
    float muteGain = 1.0f, muteTarget = 1.0f, muteStep = 1.0f / 240.0f;
    float sendLevel = 0.0f;               // smoothed send amount including return gain
    float sendSmoothing = 0.0f;           // one-pole coefficient, ~10 ms
    float lastLeft = 0.0f, lastRight = 0.0f;
    float transitionLeft = 0.0f, transitionRight = 0.0f;
    int transitionSamples = 0, transitionRemaining = 0;
    int oversampling = 1;
    int subsamples = 0;
    float subLeft[2]{}, subRight[2]{};
    halfband::Downsampler decimatorLeft, decimatorRight;
};
