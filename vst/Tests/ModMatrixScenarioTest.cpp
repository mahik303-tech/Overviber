#include "TestData.h"
#include "TestSynth.h"
#include "dsp/OvercyclerTypes.h"
#include "data/PresetManager.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

static bool runTest(const std::string& testName, bool condition) {
    std::cout << "  " << std::left << std::setw(55) << testName << ": "
              << (condition ? "[PASS]" : "[FAIL]") << "\n";
    return condition;
}

int main(int argc, char* argv[]) {
    std::cout << "=================================================================\n";
    std::cout << " Overviber - Polyphonic MIDI Modulation Matrix Test Suite\n";
    std::cout << "=================================================================\n\n";

    TestSynth engine;
    engine.prepare(48000.0f);

    if (!initializeTestData(engine, argc, argv)) return 1;

    int totalPassed = 0;
    int totalFailed = 0;

    auto check = [&](const std::string& name, bool cond) {
        if (runTest(name, cond)) totalPassed++;
        else totalFailed++;
    };

    std::vector<float> leftOut(512, 0.0f);
    std::vector<float> rightOut(512, 0.0f);
    // Channel controllers glide to a new value within a few milliseconds
    // (MidiInput::smoothControllers); the matrix reads the smoothed value.
    auto settle = [&] { for (int b = 0; b < 4; ++b) engine.renderBlock(leftOut.data(), rightOut.data(), 512); };

    // -----------------------------------------------------------------
    // [SCENARIO 1] Basic Matrix Slot Routing & Dynamic Depth
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 1] Matrix Routing: ModWheel -> Cutoff & LFO1 -> Pitch\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        // Slot 0: ModWheel -> Cutoff (+50%)
        engine.setMatrixSlot(0, modSrcModWheel, modDestCutoff, modSrcNone, 50, true);
        // Slot 1: LFO 1 -> Pitch All (+25%)
        engine.setMatrixSlot(1, modSrcLFO1, modDestPitchAll, modSrcNone, 25, true);

        engine.noteOn(60, 60000, 1);
        engine.renderBlock(leftOut.data(), rightOut.data(), 128);

        // Verify Mod Wheel modulation source value
        engine.modWheel(32768, 1);
        settle();
        float mwVal = engine.evaluateModSource(0, modSrcModWheel);
        check("ModWheel Source Normalization (0.50)", std::abs(mwVal - 0.5f) < 0.01f);

        // Verify LFO 1 Source
        float lfoVal = engine.evaluateModSource(0, modSrcLFO1);
        check("LFO 1 Bipolar Normalization Valid Range", lfoVal >= -1.0f && lfoVal <= 1.0f);

        // Disable slot 0 and verify
        engine.setMatrixSlot(0, modSrcModWheel, modDestCutoff, modSrcNone, 50, false);
        check("Matrix Slot Bypass / Enable Control", !engine.getCurrentPreset().modMatrix[0].enabled);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [SCENARIO 2] Polyphonic Per-Voice Aftertouch Modulation
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 2] Polyphonic Per-Voice Aftertouch Matrix Routing\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        // Slot 0: Aftertouch -> WaveMod All (+80%)
        engine.setMatrixSlot(0, modSrcAftertouch, modDestWaveModAll, modSrcNone, 80, true);

        // Play Note 60 (Voice 0) and Note 64 (Voice 1)
        engine.noteOn(60, 60000, 1);
        engine.noteOn(64, 60000, 1);
        engine.renderBlock(leftOut.data(), rightOut.data(), 128);

        // Apply Polyphonic Aftertouch exclusively to Note 60
        engine.polyAftertouch(60, 50000);
        engine.polyAftertouch(64, 0);

        // Render to let 1-pole smoother update
        for (int b = 0; b < 8; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 128);
        }

        float atVoice0 = engine.evaluateModSource(0, modSrcAftertouch);
        float atVoice1 = engine.evaluateModSource(1, modSrcAftertouch);

        check("Poly-AT: Voice 0 Active Pressure (~0.76)", atVoice0 > 0.65f);
        check("Poly-AT: Voice 1 Zero Isolation (0.00)", atVoice1 < 0.05f);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [SCENARIO 3] Secondary Modulator ("Via") Scaler Routing
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 3] Secondary Modulator ('Via') Scaling Matrix Routing\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        // Slot 0: LFO 1 -> PitchAll *scaled via* ModWheel (+50%)
        engine.setMatrixSlot(0, modSrcLFO1, modDestPitchAll, modSrcModWheel, 50, true);

        engine.noteOn(60, 60000, 1);
        engine.renderBlock(leftOut.data(), rightOut.data(), 128);

        // ModWheel at 0 -> LFO scaling should be 0
        engine.modWheel(0, 1);
        settle();
        float viaVal0 = engine.evaluateModSource(0, engine.getCurrentPreset().modMatrix[0].viaSource);
        check("Via Modulator Zero Depth when Controller is at Min", viaVal0 < 0.01f);

        // ModWheel at 100% -> LFO scaling should be full
        engine.modWheel(65535, 1);
        settle();
        float viaVal1 = engine.evaluateModSource(0, engine.getCurrentPreset().modMatrix[0].viaSource);
        check("Via Modulator Full Depth when Controller is at Max", std::abs(viaVal1 - 1.0f) < 0.01f);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [SCENARIO 4] Note Velocity & Release Velocity (Lift)
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 4] Note-On Velocity & Note-Off Lift Velocity Routing\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        // Slot 0: Velocity -> Filter Cutoff (+60%)
        engine.setMatrixSlot(0, modSrcVelocity, modDestCutoff, modSrcNone, 60, true);
        // Slot 1: Release Velocity -> Amp Level (+40%)
        engine.setMatrixSlot(1, modSrcReleaseVelocity, modDestAmpLevel, modSrcNone, 40, true);

        engine.noteOn(60, 52000, 1);
        engine.renderBlock(leftOut.data(), rightOut.data(), 128);

        float velVal = engine.evaluateModSource(0, modSrcVelocity);
        check("Note-On Velocity Extraction (~0.79)", std::abs(velVal - (52000.0f / 65535.0f)) < 0.01f);

        engine.noteOff(60, 48000, 1);
        float relVelVal = engine.evaluateModSource(0, modSrcReleaseVelocity);
        check("Note-Off Lift Velocity Extraction (~0.73)", std::abs(relVelVal - (48000.0f / 65535.0f)) < 0.01f);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [SCENARIO 5] Key Tracking & MPE Timbre / Slide (CC 74)
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 5] Key Tracking & MPE Timbre (CC 74) Routing\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        // Slot 0: Key Track -> Filter Resonance (+30%)
        engine.setMatrixSlot(0, modSrcKeyTrack, modDestResonance, modSrcNone, 30, true);
        // Slot 1: Timbre Slide -> WaveMod Depth (+50%)
        engine.setMatrixSlot(1, modSrcTimbreSlide, modDestWaveModAll, modSrcNone, 50, true);

        // Note 60 (Middle C) -> Key Track = 0.0
        engine.noteOn(60, 60000, 1);
        float keyC = engine.evaluateModSource(0, modSrcKeyTrack);
        check("Key Tracking at Middle C (Note 60 = 0.0)", std::abs(keyC) < 0.01f);

        // Note 72 (1 Octave up) -> Key Track > 0
        engine.noteOn(72, 60000, 1);
        float keyHigh = engine.evaluateModSource(1, modSrcKeyTrack);
        check("Key Tracking Higher Note (Note 72 > 0.0)", keyHigh > 0.15f);

        // CC 74 Timbre Slide
        engine.timbreSlide(49152, 1);
        settle();
        float timbreVal = engine.evaluateModSource(0, modSrcTimbreSlide);
        check("Timbre Slide CC 74 Normalization (~0.75)", std::abs(timbreVal - 0.75f) < 0.01f);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [SCENARIO 6] Preset Serialization & Backward Compatibility
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 6] Preset Serialization & Legacy Patch Migration\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        PresetData p1;
        p1.setDefaults();
        p1.presetName = "Matrix Test Patch";
        p1.modMatrix[0] = { modSrcModWheel, modDestCutoff, modSrcNone, 75, 0, true };
        p1.modMatrix[1] = { modSrcAftertouch, modDestWaveModAll, modSrcExpression, -40, 0, true };
        p1.modMatrix[2] = { modSrcLFO1, modDestPitchAll, modSrcBreath, 20, 0, false };

        PresetManager pm;
        std::string serialized = pm.serializePresetToString(p1);
        check("Preset String Contains Matrix Block", serialized.find("matrixSlot0_src") != std::string::npos);

        PresetData p2;
        bool parseOk = pm.parsePresetString(serialized, p2);
        check("Preset Deserialization Succeeded", parseOk);
        check("Slot 0 Source Restored (ModWheel)", p2.modMatrix[0].source == modSrcModWheel);
        check("Slot 0 Dest Restored (Cutoff)", p2.modMatrix[0].dest == modDestCutoff);
        check("Slot 0 Depth Restored (+75%)", p2.modMatrix[0].depth == 75);
        check("Slot 1 Via Restored (Expression)", p2.modMatrix[1].viaSource == modSrcExpression);
        check("Slot 1 Depth Restored (-40%)", p2.modMatrix[1].depth == -40);
        check("Slot 2 Bypass Restored (Disabled)", !p2.modMatrix[2].enabled);

        // A preset without matrixSlot keys (such as the hardware's) has an
        // empty matrix; its performance targets act directly in the engine.
        std::string hardwarePreset = "presetName = Hardware Patch\n"
                                     "spModwheelTarget = 1\n"  // LFO 2
                                     "spPressureTarget = 4\n"  // modWaveMod
                                     "spTimbreTarget = 3\n";   // modVolume
        PresetData pHardware;
        pm.parsePresetString(hardwarePreset, pHardware);
        bool empty = true;
        for (const auto& slot : pHardware.modMatrix) empty &= slot.source == modSrcNone && slot.dest == modDestNone;
        check("Preset Without Matrix: No Matrix Slots", empty);
        check("Preset Without Matrix: Targets Kept", pHardware.steppedParams[spModwheelTarget] == 1
              && pHardware.steppedParams[spPressureTarget] == modWaveMod);
    }

    // -----------------------------------------------------------------
    // [SCENARIO 7] Real-Time Audio Signal Flow & Stability
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 7] Real-Time Audio Signal Stability & Zero-NaN Check\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        // Complex matrix routing across 4 slots simultaneously
        engine.setMatrixSlot(0, modSrcLFO1, modDestPitchOscA, modSrcNone, 30, true);
        engine.setMatrixSlot(1, modSrcLFO2, modDestCutoff, modSrcModWheel, 60, true);
        engine.setMatrixSlot(2, modSrcAftertouch, modDestResonance, modSrcNone, 40, true);
        engine.setMatrixSlot(3, modSrcFilterEnv, modDestWaveModAll, modSrcNone, 70, true);

        engine.noteOn(60, 60000, 1);
        engine.noteOn(64, 55000, 1);
        engine.noteOn(67, 50000, 1);
        engine.modWheel(40000, 1);
        engine.polyAftertouch(60, 30000);

        bool audioClean = true;
        for (int block = 0; block < 30; ++block) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 256);
            for (int i = 0; i < 256; ++i) {
                if (std::isnan(leftOut[i]) || std::isnan(rightOut[i]) ||
                    std::isinf(leftOut[i]) || std::isinf(rightOut[i])) {
                    audioClean = false;
                    break;
                }
            }
        }
        check("Audio Buffer: Clean Output (No NaNs, No Infs)", audioClean);

        engine.allNotesOff();
    }

    std::cout << "\n=================================================================\n";
    std::cout << " Results: " << totalPassed << " Passed, " << totalFailed << " Failed\n";
    std::cout << "=================================================================\n";

    return (totalFailed == 0) ? 0 : 1;
}
