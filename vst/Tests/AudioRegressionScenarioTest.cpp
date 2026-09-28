#include "dsp/SynthEngine.h"
#include "dsp/FilterCalibration.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <memory>

int main() {
    std::cout << std::unitbuf;
    {
        auto chorus = std::make_unique<SynthEngine>();
        chorus->getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
        chorus->getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
        chorus->prepare(44100);
        int preset10 = -1;
        for (int i = 0; i < chorus->getPresetManager().getPresetCount(); ++i)
            if (chorus->getPresetManager().getPresetNumber(i) == 10) preset10 = i;
        if (preset10 < 0) return 1;
        chorus->loadPreset(preset10);
        for (int note : {19, 31, 34, 38}) chorus->noteOn(note, 36122, 1);
        float left[512], right[512];
        float maximumVoicePeak = 0.0f;
        for (int b = 0; b < 172; ++b) {
            chorus->beginVoiceMeterBlock();
            chorus->renderBlock(left, right, 512);
            for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
                maximumVoicePeak = std::max(maximumVoicePeak, chorus->getVoicePeakLevel(v) / 65535.0f);
        }
        std::cout << "Preset 10 / supplied MIDI chord: maximum metered voice peak " << maximumVoicePeak << '\n';
        if (maximumVoicePeak >= 1.0f) {
            std::cerr << "Preset 10 has a genuinely clipping post-fader voice\n";
            return 1;
        }
    }
    {
        auto choir = std::make_unique<SynthEngine>();
        choir->getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
        choir->getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
        choir->prepare(44100);
        int preset12 = -1;
        for (int i = 0; i < choir->getPresetManager().getPresetCount(); ++i)
            if (choir->getPresetManager().getPresetNumber(i) == 12) preset12 = i;
        if (preset12 < 0) return 1;
        choir->loadPreset(preset12);
        RenderDiagnostics diagnostics; choir->setDiagnostics(&diagnostics);
        // First chord from "test poly long notes.mid". Preset 12 maps every
        // note to all six voices, so the final event exercises full unison.
        for (int note : {19, 31, 34, 38}) choir->noteOn(note, 36122, 1);
        float left[512], right[512];
        for (int b = 0; b < 172; ++b) choir->renderBlock(left, right, 512);
        choir->setDiagnostics(nullptr);
        const float choirBusPeak = std::max(diagnostics.busLeft.peak, diagnostics.busRight.peak);
        const float choirOutputPeak = std::max(diagnostics.outputLeft.peak, diagnostics.outputRight.peak);
        std::cout << "Preset 12 / supplied MIDI chord: bus peak " << choirBusPeak
                  << "; output peak " << choirOutputPeak << '\n';
        if (choirBusPeak > 0.90f || choirOutputPeak > 0.90f) {
            std::cerr << "Preset 12 exceeds calibrated unison headroom\n";
            return 1;
        }
    }
    // Filter level spread through the ConsoleX bus, without and with the
    // Mackity send. The spread check uses the dry bus, as before.
    double minimumRms = 1, maximumRms = 0;
    for (int send : {0, 500}) for (int model = 0; model < 4; ++model) {
        auto engine = std::make_unique<SynthEngine>();
        engine->prepare(44100);
        engine->setSteppedParam(spEngineMode, emMultiChannel);
        engine->setSteppedParam(spFilterModel, static_cast<uint8_t>(model));
        engine->setContinuousParam(cpMackitySend, static_cast<uint16_t>(scan_potTo16bits(send)));
        engine->setContinuousParam(cpAVol, 16000);
        for (int n : {48, 55, 60, 64, 67, 72}) engine->noteOn(n, 60000, 1);
        float l[512], r[512]; double energy = 0; float peak = 0;
        const auto begin = std::chrono::steady_clock::now();
        for (int b = 0; b < 192; ++b) {
            engine->renderBlock(l, r, 512);
            for (int i = 0; i < 512; ++i) {
                if (!std::isfinite(l[i]) || !std::isfinite(r[i])) return 1;
                if (b >= 32) { energy += l[i]*l[i] + r[i]*r[i]; peak = std::max(peak, std::max(std::abs(l[i]), std::abs(r[i]))); }
            }
        }
        const double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        const double rms = std::sqrt(energy/(160*1024));
        if (send == 0) { minimumRms = std::min(minimumRms, rms); maximumRms = std::max(maximumRms, rms); }
        std::cout << "Mackity send " << send << " filter " << model << " six voices / 98304 samples, 44100 Hz, block 512: " << ms << " ms; RMS " << rms << "; peak " << peak << '\n';
    }
    if (maximumRms / minimumRms > 1.25) { std::cerr << "Nominal filter level spread exceeds 1.94 dB\n"; return 1; }
    ConsoleXProcessor console; console.setCalibratedGain(true); console.setParameters(0.1f, 1, 0);
    double maxError = 0;
    for (int i = -50000; i <= 50000; ++i) {
        const float input = static_cast<float>(i) / 10000.0f;
        double x = std::abs(static_cast<double>(input) * ConsoleXProcessor::INV_PHI);
        if (x > 0.75) x = 0.75 + 0.249999 * std::tanh((x - 0.75) / 0.249999);
        const double reference = std::copysign(-std::expm1(std::log1p(-x) * ConsoleXProcessor::PHI), input);
        float l, r; console.encodeVoice(input, input, l, r);
        maxError = std::max(maxError, std::abs(reference - l));
    }
    std::cout << "Console encoder maximum error: " << maxError << '\n';
    if (maxError > 0.000002) return 1;
    float maxSwitchDelta = 0;
    for (float rate : {44100.f, 48000.f, 96000.f}) for (int from = 0; from < 4; ++from) for (int to = 0; to < 4; ++to) {
        auto engine = std::make_unique<SynthEngine>(); engine->prepare(rate);
        engine->setSteppedParam(spEngineMode, emMultiChannel);
        engine->setContinuousParam(cpAVol, 16000);
        auto* wave = engine->getWaveManager().getMutableWaveData(abxAMain);
        for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) wave[i] = static_cast<uint16_t>(32768 + 32000 * std::sin(6.283185307179586*i/WTOSC_SAMPLE_COUNT));
        engine->setSteppedParam(spFilterModel, static_cast<uint8_t>(from)); engine->noteOn(48, 60000, 1);
        float l[512], r[512]; for (int b = 0; b < 20; ++b) engine->renderBlock(l,r,512);
        float naturalStep = 0;
        for (int i=1;i<512;++i) naturalStep=std::max(naturalStep,std::abs(l[i]-l[i-1]));
        float previous = l[511];
        float localStep = 0;
        engine->setSteppedParam(spFilterModel, static_cast<uint8_t>(to));
        for (int b = 0; b < 8; ++b) {
            engine->renderBlock(l,r,512);
            for (int i = 0; i < 512; ++i) { localStep = std::max(localStep,std::abs(l[i]-previous)); previous=l[i]; if (!std::isfinite(l[i])) return 1; }
        }
        // Compare with the same waveform's measured slope, not an arbitrary
        // absolute jump threshold (the no-change control already reaches .011).
        if (localStep > naturalStep * 1.10f + 0.0002f) {
            std::cerr << "Excess transition step " << rate << ' ' << from << " -> " << to << ": " << localStep << " vs " << naturalStep << '\n';
            return 1;
        }
        maxSwitchDelta = std::max(maxSwitchDelta,localStep);
    }
    std::cout << "Maximum sine sample step during filter changes: " << maxSwitchDelta << '\n';
    return 0;
}
