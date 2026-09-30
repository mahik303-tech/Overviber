#include "TestSynth.h"
#include "elements/dsp/multistage_envelope.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <chrono>
#include <cstring>
#include <new>

int main() {
    // Preserve the measured values in redirected CI output if an assertion fails.
    std::cout << std::unitbuf;
    std::cout << "==============================================================================\n";
    std::cout << "Overviber - Elements Modal Synthesis Voice Integration Test\n";
    std::cout << "==============================================================================\n\n";

    TestSynth engine;
    engine.prepare(48000.0f);
    // This suite edits the main preset: Multi-Channel routes MIDI channel 1
    // (used by every note below) to part 0, the edited preset.
    engine.setSteppedParam(spEngineMode, emMultiChannel);

    // The level references below were taken on the left channel with the
    // former linear pan law; the voices sit at pans from -0.7 to 0.7.
    // asFormerPan() scales a left-channel peak of one voice back to that law
    // (voice 1 at -0.7: left side 0.85 of the voice before, 0.69 now).
    auto voiceOf = [&](uint8_t note) {
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v)
            if (engine.getVoice(v).isGated() && engine.getVoice(v).getNote() == note) return v;
        return 0;
    };
    auto asFormerPan = [&](float peak, int voice) {
        const float pan = engine.getVoicePan(voice);
        return peak * 0.5f * (1.0f - pan) / (0.70710678f * std::cos((pan + 1.0f) * 0.25f * 3.14159265f));
    };

    const int blockSize = 64;
    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);

    // --------------------------------------------------------------------------
    // TEST 1: Elements Model 0 (Modal 64-band Resonator)
    // --------------------------------------------------------------------------
    {
        std::cout << "[TEST 1] Elements Modal Resonator (Model 0: Plates/Bars/Bells)...\n";
        engine.setSteppedParam(spOscEngine, oeElements);
        engine.setSteppedParam(spElementsModel, emModal);
        engine.setContinuousParam(cpElementsStrike, scan_potTo16bits(800)); // 80% strike
        engine.setContinuousParam(cpElementsGeometry, scan_potTo16bits(400));
        engine.setContinuousParam(cpElementsBrightness, scan_potTo16bits(600));
        engine.setContinuousParam(cpElementsDamping, scan_potTo16bits(350));
        engine.setContinuousParam(cpAmpRel, scan_potTo16bits(600)); // Long release for resonator ring

        engine.noteOn(60, 60000, 1);
        const int voice = voiceOf(60); // Middle C

        float peak = 0.0f;
        int nanCount = 0;
        int infCount = 0;

        for (int b = 0; b < 100; ++b) { // 100 blocks = 6400 samples (~133 ms)
            if (b == 50) engine.noteOff(60, 0, 1);

            engine.renderBlock(left.data(), right.data(), blockSize);
            for (int i = 0; i < blockSize; ++i) {
                float v = std::abs(left[i]);
                if (std::isnan(v)) nanCount++;
                if (std::isinf(v)) infCount++;
                if (v > peak) peak = v;
            }
        }

        std::cout << "  Peak: " << peak << ", NaNs: " << nanCount << ", Infs: " << infCount << "\n";
        assert(nanCount == 0 && infCount == 0);
        assert(asFormerPan(peak, voice) > 0.24f && asFormerPan(peak, voice) < 0.32f); // Modal reference at concert pitch (C4): ~0.279.
        std::cout << "  -> PASSED!\n\n";
    }

    // --------------------------------------------------------------------------
    // TEST 2: Elements Model 1 (Non-linear String)
    // --------------------------------------------------------------------------
    {
        std::cout << "[TEST 2] Elements Non-linear String (Model 1: Karplus-Strong)...\n";
        engine.setSteppedParam(spOscEngine, oeElements);
        engine.setSteppedParam(spElementsModel, emString);
        engine.setContinuousParam(cpElementsStrike, scan_potTo16bits(900));
        engine.setContinuousParam(cpElementsDamping, scan_potTo16bits(200));

        engine.noteOn(57, 55000, 1);
        const int voice = voiceOf(57); // Note A3

        float peak = 0.0f;
        int nanCount = 0;
        int infCount = 0;

        for (int b = 0; b < 100; ++b) {
            if (b == 60) engine.noteOff(57, 0, 1);
            engine.renderBlock(left.data(), right.data(), blockSize);
            for (int i = 0; i < blockSize; ++i) {
                float v = std::abs(left[i]);
                if (std::isnan(v)) nanCount++;
                if (std::isinf(v)) infCount++;
                if (v > peak) peak = v;
            }
        }

        std::cout << "  Peak: " << peak << ", NaNs: " << nanCount << ", Infs: " << infCount << "\n";
        assert(nanCount == 0 && infCount == 0);
        // String excitation is naturally quieter than modal; retain its dynamics.
        // Reference at concert pitch ~0.047 (0.043 at the former +32 semitones).
        assert(asFormerPan(peak, voice) > 0.035f && asFormerPan(peak, voice) < 0.055f);
        std::cout << "  -> PASSED!\n\n";
    }

    // --------------------------------------------------------------------------
    // TEST 3: Elements Model 2 (Chords Resonator)
    // --------------------------------------------------------------------------
    {
        std::cout << "[TEST 3] Elements Chords Resonator (Model 2)...\n";
        engine.setSteppedParam(spOscEngine, oeElements);
        engine.setSteppedParam(spElementsModel, emChords);
        engine.setContinuousParam(cpElementsGeometry, scan_potTo16bits(500)); // Chord select

        engine.noteOn(64, 58000, 1);
        const int voice = voiceOf(64); // Note E4

        float peak = 0.0f;
        int nanCount = 0;
        int infCount = 0;

        for (int b = 0; b < 100; ++b) {
            if (b == 50) engine.noteOff(64, 0, 1);
            engine.renderBlock(left.data(), right.data(), blockSize);
            for (int i = 0; i < blockSize; ++i) {
                float v = std::abs(left[i]);
                if (std::isnan(v)) nanCount++;
                if (std::isinf(v)) infCount++;
                if (v > peak) peak = v;
            }
        }

        std::cout << "  Peak: " << peak << ", NaNs: " << nanCount << ", Infs: " << infCount << "\n";
        assert(nanCount == 0 && infCount == 0);
        assert(asFormerPan(peak, voice) > 0.05f);
        std::cout << "  -> PASSED!\n\n";
    }

    // --------------------------------------------------------------------------
    // TEST 4: Elements Model 3 (Ominous Voice)
    // --------------------------------------------------------------------------
    {
        std::cout << "[TEST 4] Elements Ominous Voice (Model 3: Granular Formants)...\n";
        engine.setSteppedParam(spOscEngine, oeElements);
        engine.setSteppedParam(spElementsModel, emOminousVoice);
        engine.setContinuousParam(cpElementsStrike, scan_potTo16bits(800));
        engine.setContinuousParam(cpElementsBlow, scan_potTo16bits(500));

        engine.noteOn(48, 62000, 1);
        const int voice = voiceOf(48); // Note C3

        float peak = 0.0f;
        int nanCount = 0;
        int infCount = 0;

        for (int b = 0; b < 100; ++b) {
            if (b == 50) engine.noteOff(48, 0, 1);
            engine.renderBlock(left.data(), right.data(), blockSize);
            for (int i = 0; i < blockSize; ++i) {
                float v = std::abs(left[i]);
                if (std::isnan(v)) nanCount++;
                if (std::isinf(v)) infCount++;
                if (v > peak) peak = v;
            }
        }

        std::cout << "  Peak: " << peak << ", NaNs: " << nanCount << ", Infs: " << infCount << "\n";
        assert(nanCount == 0 && infCount == 0);
        // This short, low-note Ominous excitation is quiet by design; do not
        // normalize every model to the same peak. Reference at concert pitch ~0.0024.
        assert(asFormerPan(peak, voice) > 0.0019f && asFormerPan(peak, voice) < 0.0031f);
        std::cout << "  -> PASSED!\n\n";
    }

    // --------------------------------------------------------------------------
    // TEST 5: 6-Voice Full Polyphony Modal Synthesis
    // --------------------------------------------------------------------------
    {
        std::cout << "[TEST 5] 6-Voice Polyphonic Modal Synthesizer Performance...\n";
        engine.setSteppedParam(spOscEngine, oeElements);
        engine.setSteppedParam(spElementsModel, emModal);

        uint8_t chordNotes[6] = { 48, 55, 60, 64, 67, 71 }; // C maj9 chord
        for (int i = 0; i < 6; ++i) {
            engine.noteOn(chordNotes[i], 50000 + i * 2000, 1);
        }

        auto start = std::chrono::high_resolution_clock::now();
        const int numBlocks = 750; // 750 * 64 = 48000 samples = exactly 1.0 second
        float peak = 0.0f;

        for (int b = 0; b < numBlocks; ++b) {
            if (b == 500) {
                for (int i = 0; i < 6; ++i) engine.noteOff(chordNotes[i], 0, 1);
            }
            engine.renderBlock(left.data(), right.data(), blockSize);
            for (int i = 0; i < blockSize; ++i) {
                float v = std::abs(left[i]);
                if (v > peak) peak = v;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        double elapsedSec = std::chrono::duration<double>(end - start).count();
        double speedup = 1.0 / elapsedSec;

        std::cout << "  6-Voice Polyphony: 1.0s rendered in " << elapsedSec * 1000.0 << " ms ("
                  << speedup << "x Real-Time, CPU Load: " << (elapsedSec * 100.0) << "%)\n";
        std::cout << "  Combined Peak Level: " << peak << "\n";
        // Six voices over all pans: the left sum changes by only 6 %.
        assert(peak > 0.1f);
        assert(speedup > 1.0); // Must run faster than real time!
        std::cout << "  -> PASSED!\n\n";
    }

    // --------------------------------------------------------------------------
    // TEST 6: Modulation Matrix Routing to Elements Parameters
    // --------------------------------------------------------------------------
    {
        std::cout << "[TEST 6] Modulation Matrix Targeting Elements Parameters...\n";
        engine.setSteppedParam(spOscEngine, oeElements);
        engine.setSteppedParam(spElementsModel, emModal);

        // ModWheel -> modDestElementsBrightness (+50%)
        engine.setMatrixSlot(0, modSrcModWheel, modDestElementsBrightness, modSrcNone, 50, true);
        // Aftertouch -> modDestElementsGeometry (+40%)
        engine.setMatrixSlot(1, modSrcAftertouch, modDestElementsGeometry, modSrcNone, 40, true);

        engine.noteOn(60, 50000, 1);
        engine.modWheel(32768, 1); // 50% modwheel
        engine.channelPressure(40000, 1); // pressure

        for (int b = 0; b < 20; ++b) {
            engine.renderBlock(left.data(), right.data(), blockSize);
        }
        engine.allNotesOff();
        std::cout << "  -> PASSED!\n\n";
    }

    // --------------------------------------------------------------------------
    // TEST 7: Hybrid Mode (Dual Wavetable + Elements Modal Resonator)
    // --------------------------------------------------------------------------
    {
        std::cout << "[TEST 7] Hybrid Mode (Osc A Wavetable excites Elements Resonator)...\n";
        engine.setSteppedParam(spOscEngine, oeHybrid);
        engine.setSteppedParam(spElementsModel, emString);

        engine.noteOn(60, 50000, 1);
        const int voice = voiceOf(60);
        float peak = 0.0f;
        for (int b = 0; b < 50; ++b) {
            engine.renderBlock(left.data(), right.data(), blockSize);
            for (int i = 0; i < blockSize; ++i) {
                float v = std::abs(left[i]);
                if (v > peak) peak = v;
            }
        }
        engine.noteOff(60, 0, 1);
        for (int b = 0; b < 20; ++b) {
            engine.renderBlock(left.data(), right.data(), blockSize);
        }
        std::cout << "  Hybrid Peak: " << peak << "\n";
        assert(asFormerPan(peak, voice) > 0.05f);
        std::cout << "  -> PASSED!\n\n";
    }

    // --------------------------------------------------------------------------
    // TEST 8: Idle exciter envelope on uninitialised (heap) storage
    // --------------------------------------------------------------------------
    // The Elements firmware keeps its voices in zero-initialised static storage,
    // Overviber embeds them in heap-allocated engines. A finished envelope still
    // evaluates the segment after its last one, so its output must not depend
    // on whatever bytes the allocation happened to contain.
    {
        std::cout << "[TEST 8] Idle exciter envelope on uninitialised storage...\n";
        alignas(elements::MultistageEnvelope) unsigned char storage[sizeof(elements::MultistageEnvelope)];
        std::memset(storage, 0xFF, sizeof(storage));
        auto* envelope = new (storage) elements::MultistageEnvelope();
        envelope->Init();
        float idle = 0.0f;
        for (int i = 0; i < 64; ++i) idle = envelope->Process(0);
        std::cout << "  Idle envelope value: " << idle << "\n";
        assert(idle == 0.0f);
        envelope->~MultistageEnvelope();
        std::cout << "  -> PASSED!\n\n";
    }

    // --------------------------------------------------------------------------
    // The same for the bow and blow exciters (granular player phase, particle
    // range) and the string: an oscillator built on 0x00 and on 0xFF bytes
    // (NaN as float) must render bit-identically.
    {
        std::cout << "[TEST 9] Bow and blow exciters on uninitialised storage...\n";
        auto render = [](unsigned char fill) {
            const std::align_val_t alignment{alignof(ElementsOsc)};
            void* memory = ::operator new(sizeof(ElementsOsc), alignment);
            std::memset(memory, fill, sizeof(ElementsOsc));
            auto* osc = new (memory) ElementsOsc();
            osc->setRandomSeed(1234);
            osc->setSampleRate(48000.0f);
            osc->reset();
            osc->setModel(ElementsOsc::ModelString);
            osc->setPitch(48.0f);
            osc->setBowLevel(0.6f);
            osc->setBlowLevel(0.7f);
            osc->setStrikeLevel(0.3f);
            osc->gateOn(0.8f);
            std::vector<float> out(4096);
            for (auto& sample : out) sample = osc->processSample();
            osc->~ElementsOsc();
            ::operator delete(memory, alignment);
            return out;
        };
        const auto zeroed = render(0x00);
        const auto filled = render(0xFF);
        const bool identical = std::memcmp(zeroed.data(), filled.data(), zeroed.size() * sizeof(float)) == 0;
        std::cout << "  Renders identical: " << (identical ? "yes" : "no") << "\n";
        assert(identical);
        std::cout << "  -> PASSED!\n\n";
    }

    std::cout << "==============================================================================\n";
    std::cout << "ALL ELEMENTS VOICE INTEGRATION TESTS PASSED WITH DISTINCTION!\n";
    std::cout << "==============================================================================\n";
    return 0;
}
