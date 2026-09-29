#pragma once

#include "OvercyclerTypes.h"
#include "wtosc.h"
#include "adsr.h"
#include "Ssi2144Filter.h"
#include "SstLadderFilter.h"
#include "SemFilter.h"
#include "audible/ShelvesFilter.h"
#include "audible/ElementsOsc.h"
#include "Lm13700Vca.h"
#include <memory>
#include <array>
#ifdef OVERVIBER_DIAGNOSTICS
#include "SignalDiagnostics.h"
#endif

// The Elements controls of a voice (0..1), as on the Elements panel: the
// exciter's contour, levels, flow / mallet and timbres, the resonator and
// the pitch as MIDI note.
struct ElementsControls {
    uint8_t model = 0;
    float geometry = 0, brightness = 0, damping = 0, position = 0, space = 0;
    float contour = 0, bow = 0, blow = 0, strike = 0;
    float flow = 0, mallet = 0, bowTimbre = 0, blowTimbre = 0, strikeTimbre = 0;
    float pitchMidiNote = 0;
};

class Voice {
public:
    Voice();
    ~Voice();
    void init(int8_t voiceIndex);
    void setSampleRate(float sr);

    void setOscSampleData(const uint16_t* aMain, const uint16_t* aXovr, const uint16_t* bMain, const uint16_t* bXovr);
    void gateOn(uint8_t note, uint16_t velocity, uint8_t flags);
    void gateOff();
    void reset();

    bool isActive() const;
    bool isGated() const { return gated; }       // key held (between gate on and off)
    uint8_t getNote() const { return currentNote; }

    // Filter model & mode
    void setFilterModelAndMode(uint8_t model, uint8_t mode, uint8_t semVariant = 0);
    void setShelvesEQParams(float lsFreq, float lsGain,
                            float p1Freq, float p1Gain, float p1Q,
                            float p2Freq, float p2Gain, float p2Q,
                            float hsFreq, float hsGain) {
        filterEQ.setEQParams(lsFreq, lsGain, p1Freq, p1Gain, p1Q, p2Freq, p2Gain, p2Q, hsFreq, hsGain);
    }

    // Oscillator engine configuration (Wavetable, Elements Modal, Hybrid)
    void setOscEngine(uint8_t engine) { oscEngine = engine; }
    uint8_t getOscEngine() const { return oscEngine; }

    void updateElementsParams(const ElementsControls& c);

    class ElementsOsc* getElementsOsc() { return oscElements.get(); }
    const class ElementsOsc* getElementsOsc() const { return oscElements.get(); }

    // Envelopes update (called at ~4000 Hz or control rate)
    void updateEnvelopes();

    // Set dynamic voice parameters
    void updateVoiceCVs(uint16_t pitchA, uint16_t pitchB,
                        oscWModTarget_t wmodTypeA, uint16_t wmodA,
                        oscWModTarget_t wmodTypeB, uint16_t wmodB,
                        uint16_t cutoffCV, uint16_t resonanceCV,
                        uint16_t ampCV,
                        float oscAGain, float oscBGain, float noiseGain,
                        bool hardSyncEnabled);

    // Audio sample generation
    float processSample(uint32_t tickStep);
    // Renders up to `count` samples while the voice is active and returns the
    // number rendered. Within a control-rate segment a voice that stops cannot
    // start again, because only note and clock events restart it.
    int process(float* out, int count, uint32_t tickStep);
    // Nonlinear filter cores see the mix 12 dB lower; the gain is restored
    // after the filter together with the measured per-model correction.
    static constexpr float kFilterInputPad = 0.25f;
    static constexpr float kFilterMakeup = 4.0f;
    std::array<float, 4> filterGains{1, 1, 1, 1};              // per filter model
    std::array<float, SemFilter::VariantCount> semGains{1, 1, 1, 1, 1};
#ifdef OVERVIBER_DIAGNOSTICS
    VoiceDiagnostics* diagnostics = nullptr; // Set only by the offline renderer.
#endif

    // ADSR references
    AdsrEnv& getFilEnv() { return filEnv; }
    const AdsrEnv& getFilEnv() const { return filEnv; }
    AdsrEnv& getAmpEnv() { return ampEnv; }
    const AdsrEnv& getAmpEnv() const { return ampEnv; }
    AdsrEnv& getWmodEnv() { return wmodEnv; }
    const AdsrEnv& getWmodEnv() const { return wmodEnv; }
    Lm13700Vca& getVca() { return vca; }
    const Lm13700Vca& getVca() const { return vca; }

    int8_t getIndex() const { return voiceIndex; }

private:
    void commitFilter();
    void updateFilterCV();
    uint8_t requestedFilter = 0, requestedMode = 0, requestedVariant = 0;
    uint16_t lastCutoff = 65535, lastResonance = 0;
    float filterFade = 1.0f, filterFadeStep = 1.0f / 480.0f;
    bool filterFadingOut = false;
    int8_t voiceIndex;
    uint8_t currentNote;
    bool active;
    bool gated = false;
    bool syncEnabled;
    int16_t syncPosition;
    uint8_t oscEngine = 0;

    WtOsc oscA;
    WtOsc oscB;
    std::unique_ptr<class ElementsOsc> oscElements;
    AdsrEnv filEnv;
    AdsrEnv ampEnv;
    AdsrEnv wmodEnv;

    Ssi2144Filter filterSSI;
    SemFilter filterSem;
    ShelvesFilter filterEQ;
    SstLadderFilter filterSST;
    uint8_t filterModel = 0;
    uint8_t filterMode = 0;
    uint8_t filterVariant = 0;

    // Coupling capacitor between filter and VCA: a one-pole highpass at
    // kDcBlockHz. Wavetables with an offset, Width mod and sync give the
    // oscillators a DC offset; blocked before the VCA it neither reaches the
    // console bus nor turns into envelope-shaped low-frequency bumps.
    static constexpr float kDcBlockHz = 10.0f;
    float dcBlockCoeff = 0.99869f;   // exp(-2 pi kDcBlockHz / sr), set in setSampleRate
    float dcBlockIn = 0.0f, dcBlockOut = 0.0f;

    Lm13700Vca vca;

    float gainA;
    float gainB;
    float gainNoise;

    uint32_t noiseLfsr;
};
