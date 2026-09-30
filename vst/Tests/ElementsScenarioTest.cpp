#include "dsp/audible/ElementsOsc.h"
#include "NoDenormals.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <chrono>
#include <vector>
#include <cmath>
#include <cassert>

int main() {
    flushDenormalsToZero();
    std::cout << "=================================================================\n";
    std::cout << "   Overviber / Elements Modal Oscillator Integration Test\n";
    std::cout << "=================================================================\n";

    auto osc = std::make_unique<ElementsOsc>();
    const float testSampleRate = 48000.0f;
    osc->setSampleRate(testSampleRate);

    const char* modelNames[4] = {
        "Modal Resonator (Plates/Bells)",
        "Non-linear String (Karplus-Strong)",
        "Chords Resonator",
        "Ominous Voice (Formant Choir)"
    };

    bool allTestsPassed = true;

    for (int m = 0; m < 4; ++m) {
        std::cout << "\nTesting Model " << m << ": " << modelNames[m] << "..." << std::endl;
        std::cout << "  - Initializing model..." << std::endl;
        osc->reset();
        osc->setModel(static_cast<uint8_t>(m));
        osc->setPitch(60.0f); // Middle C
        osc->setGeometry(0.35f);
        osc->setBrightness(0.6f);
        osc->setDamping(0.4f);
        osc->setPosition(0.5f);
        osc->setStrikeLevel(0.85f);
        osc->setStrikeMeta(0.5f);
        osc->setStrikeTimbre(0.5f);
        osc->setSpace(0.3f);

        std::cout << "  - Gating on..." << std::endl;
        osc->gateOn(0.9f);

        const int numSamples = 48000; // 1 full second of audio
        std::vector<float> audioOut(numSamples, 0.0f);

        uint64_t nanCount = 0;
        uint64_t infCount = 0;
        float peakAbs = 0.0f;
        double energySum = 0.0;

        auto startTime = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < numSamples; ++i) {
            if (i == 24000) {
                // Gate OFF at half second
                osc->gateOff();
            }
            float s = osc->processSample();
            audioOut[i] = s;

            if (std::isnan(s)) nanCount++;
            if (std::isinf(s)) infCount++;
            float a = std::abs(s);
            if (a > peakAbs) peakAbs = a;
            energySum += a;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        double realTimeMultiple = (1000.0 / elapsedMs);

        std::cout << "  - Rendered " << numSamples << " samples in " << std::fixed << std::setprecision(2) << elapsedMs << " ms" << std::endl;
        std::cout << "  - Throughput: " << std::setprecision(1) << realTimeMultiple << "x Real-Time" << std::endl;
        std::cout << "  - Peak Absolute Amplitude: " << std::setprecision(4) << peakAbs << std::endl;
        std::cout << "  - Average Energy: " << std::setprecision(6) << (energySum / numSamples) << std::endl;
        std::cout << "  - NaNs: " << nanCount << ", Infs: " << infCount << std::endl;

        if (nanCount > 0 || infCount > 0) {
            std::cerr << "  FAILED: Detected NaNs or Infs!" << std::endl;
            allTestsPassed = false;
        } else if (peakAbs < 0.001f) {
            std::cerr << "  FAILED: Output silent!" << std::endl;
            allTestsPassed = false;
        } else if (peakAbs > 2.5f) {
            std::cerr << "  FAILED: Output clipped or unstable (peak: " << peakAbs << ")!" << std::endl;
            allTestsPassed = false;
        } else {
            std::cout << "  PASSED!" << std::endl;
        }
    }

    // Benchmark 6-Voice Polyphony Simulation (1 Second)
    std::cout << "\n-----------------------------------------------------------------" << std::endl;
    std::cout << "Simulating 6-Voice Full Polyphony (All voices sounding simultaneously)..." << std::endl;
    std::vector<std::unique_ptr<ElementsOsc>> voices;
    for (int v = 0; v < 6; ++v) {
        auto voice = std::make_unique<ElementsOsc>();
        voice->setSampleRate(testSampleRate);
        voice->setModel(0); // Modal Resonator (highest filter density: 64 SVFs each)
        voice->setPitch(48.0f + v * 4.0f);
        voice->gateOn(0.8f);
        voices.push_back(std::move(voice));
    }

    const int polyBlockSize = 48000;
    auto polyStart = std::chrono::high_resolution_clock::now();
    float polyAccum = 0.0f;
    for (int i = 0; i < polyBlockSize; ++i) {
        float mix = 0.0f;
        for (int v = 0; v < 6; ++v) {
            mix += voices[v]->processSample();
        }
        polyAccum += std::abs(mix);
    }
    auto polyEnd = std::chrono::high_resolution_clock::now();
    double polyMs = std::chrono::duration<double, std::milli>(polyEnd - polyStart).count();
    double polyRealTime = (1000.0 / polyMs);
    double cpuPercent = (polyMs / 1000.0) * 100.0;

    std::cout << "  - 6 Voices x 48,000 samples rendered in: " << std::setprecision(2) << polyMs << " ms\n";
    std::cout << "  - 6-Voice Real-Time Multiple: " << std::setprecision(1) << polyRealTime << "x Real-Time\n";
    std::cout << "  - Estimated 6-Voice CPU Load: " << std::setprecision(2) << cpuPercent << "%\n";

    if (polyRealTime < 5.0) {
        std::cerr << "  WARNING: Performance below 5x real-time threshold!\n";
        allTestsPassed = false;
    } else {
        std::cout << "  PERFORMANCE PASSED! (>5x real-time comfortably achieved)\n";
    }

    std::cout << "\n=================================================================\n";
    if (allTestsPassed) {
        std::cout << "  >>> ALL ELEMENTS MODAL TESTS PASSED SUCCESSFULLY! <<<\n";
    } else {
        std::cout << "  >>> SOME TESTS FAILED! <<<\n";
    }
    std::cout << "=================================================================\n";

    return allTestsPassed ? 0 : 1;
}
