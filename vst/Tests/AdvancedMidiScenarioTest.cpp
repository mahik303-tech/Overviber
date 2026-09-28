#include "TestData.h"
#include "dsp/SynthEngine.h"
#include "dsp/OvercyclerTypes.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

static bool runTest(const std::string& testName, bool condition) {
    std::cout << "  " << std::left << std::setw(50) << testName << ": "
              << (condition ? "[PASS]" : "[FAIL]") << "\n";
    return condition;
}

int main(int argc, char* argv[]) {
    std::cout << "=================================================================\n";
    std::cout << " Overviber - Advanced MIDI, MPE & Poly-AT Test Harness\n";
    std::cout << "=================================================================\n\n";

    SynthEngine engine;
    engine.prepare(48000.0f);

    if (!initializeTestData(engine, argc, argv)) return 1;

    if (engine.getPresetManager().getPresetCount() > 0) {
        engine.loadPreset(0);
    }

    int totalPassed = 0;
    int totalFailed = 0;

    auto check = [&](const std::string& name, bool cond) {
        if (runTest(name, cond)) totalPassed++;
        else totalFailed++;
    };

    std::vector<float> leftOut(1024, 0.0f);
    std::vector<float> rightOut(1024, 0.0f);

    // -----------------------------------------------------------------
    // [SCENARIO 1] Polyphonic Aftertouch (0xA0 / Per-Note Pressure)
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 1] Polyphonic Aftertouch & Per-Voice Target Routing\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        engine.setSteppedParam(spMPEMode, 0); // Standard MIDI with Poly-AT
        engine.setSteppedParam(spPressureTarget, modFilter); // Modulate filter cutoff
        engine.setSteppedParam(spPressureRange, 3);          // Max sensitivity (shift = 0)

        // Play Note 60 on Voice 0, Note 64 on Voice 1
        engine.noteOn(60, 65535, 1);
        engine.noteOn(64, 65535, 1);

        // Render to settle initial CVs
        engine.renderBlock(leftOut.data(), rightOut.data(), 256);

        // Apply high Poly-AT exclusively to Note 60 (0xA0 Key Pressure)
        engine.polyAftertouch(60, 60000);
        engine.polyAftertouch(64, 0);

        // Render audio to let 1-pole smoother ramp up
        for (int b = 0; b < 10; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 128);
        }

        // The two note allocations must retain independent pressure values.
        const int voice60 = engine.findVoiceByNote(60);
        const int voice64 = engine.findVoiceByNote(64);
        const auto* expression60 = engine.getVoiceExpressionState(voice60);
        const auto* expression64 = engine.getVoiceExpressionState(voice64);
        check("Poly-AT: Independent Voice Pressure Application",
              expression60 && expression64 && expression60->pressure == 60000
                  && expression64->pressure == 0 && expression60->hasPerVoicePressure);

        // Switch Pressure Target to WaveMod Depth
        engine.setSteppedParam(spPressureTarget, modWaveMod);
        engine.polyAftertouch(64, 55000);
        for (int b = 0; b < 10; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 128);
        }
        expression64 = engine.getVoiceExpressionState(voice64);
        check("Poly-AT: Dynamic Target Switch to WaveMod",
              engine.getCurrentPreset().steppedParams[spPressureTarget] == modWaveMod
                  && expression64 && expression64->pressure == 55000
                  && expression64->hasPerVoicePressure);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [SCENARIO 2] MPE Multi-Channel Expression (Ch 2-7 Member Channels)
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 2] MPE Multi-Channel Pitch Bend & Slide Expression\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        engine.setSteppedParam(spMPEMode, 1);             // MPE 6-Voice Zone
        engine.setSteppedParam(spMPEPitchBendRange, 2);   // +/- 24 Semitones (MPE Default)
        engine.setSteppedParam(spTimbreTarget, modWaveMod); // CC74 modulates WaveMod

        // Finger 1: Note 60 on Channel 2
        engine.noteOn(60, 65535, 2);
        // Finger 2: Note 64 on Channel 3
        engine.noteOn(64, 65535, 3);

        // Independent microtonal Pitch Bends per finger
        engine.pitchBend(8191, 2);   // +24 semitones on Ch 2
        engine.pitchBend(-8192, 3);  // -24 semitones on Ch 3

        // Independent Timbre / CC74 per finger
        engine.timbreSlide(65535, 2); // Max slide on Ch 2
        engine.timbreSlide(1000, 3);  // Min slide on Ch 3

        // Per-Channel Pressure
        engine.channelPressure(50000, 2);
        engine.channelPressure(5000, 3);

        // Render audio frames with active MPE streams
        for (int b = 0; b < 15; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 128);
        }

        const int voiceCh2 = engine.findVoiceByChannel(2);
        const int voiceCh3 = engine.findVoiceByChannel(3);
        const auto* expressionCh2 = engine.getVoiceExpressionState(voiceCh2);
        const auto* expressionCh3 = engine.getVoiceExpressionState(voiceCh3);
        check("MPE: Multi-Channel Simultaneous Note Allocation", voiceCh2 >= 0 && voiceCh3 >= 0 && voiceCh2 != voiceCh3);
        check("MPE: Independent Per-Finger Pitch Bending (+/-24 st)",
              expressionCh2 && expressionCh3 && expressionCh2->pitchBendOffset > 0
                  && expressionCh3->pitchBendOffset < 0 && expressionCh2->hasPerVoiceBend
                  && expressionCh3->hasPerVoiceBend);
        check("MPE: Per-Channel CC74 Timbre / Slide Routing",
              expressionCh2 && expressionCh3 && expressionCh2->timbre == 65535
                  && expressionCh3->timbre == 1000 && expressionCh2->hasPerVoiceTimbre
                  && expressionCh3->hasPerVoiceTimbre);
        check("MPE: Per-Channel Polyphonic Channel Pressure",
              expressionCh2 && expressionCh3 && expressionCh2->pressure == 50000
                  && expressionCh3->pressure == 5000 && expressionCh2->hasPerVoicePressure
                  && expressionCh3->hasPerVoicePressure);

        // Lower-zone mode must not treat channels above 7 as per-note MPE channels.
        engine.pitchBend(4096, 8);
        check("MPE Lower Zone: Channel 8 Uses Global Expression", engine.getGlobalPitchBend() != 0);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [SCENARIO 3] Global Channel Pressure & Master Channel Controls
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 3] Global Channel Pressure & MPE Master Channel\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        engine.setSteppedParam(spMPEMode, 0); // Standard MIDI mode
        engine.setSteppedParam(spPressureTarget, modFilter);

        // Play a 4-note chord on Channel 1
        engine.noteOn(48, 50000, 1);
        engine.noteOn(52, 50000, 1);
        engine.noteOn(55, 50000, 1);
        engine.noteOn(59, 50000, 1);

        // Send Global Channel Pressure (0xD0) on Channel 1
        engine.channelPressure(45000, 1);

        for (int b = 0; b < 10; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 128);
        }
        check("Standard MIDI: Global Channel Pressure Uniform Modulation", engine.getGlobalPressure() == 45000);

        // Mod wheel on Channel 1
        engine.modWheel(65535, 1);
        for (int b = 0; b < 10; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 128);
        }
        check("Master Channel: Mod Wheel Performance Modulation", engine.getGlobalModWheel() == 65535);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [SCENARIO 4] High-Resolution 16-Bit Velocity & Lift Dynamics
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 4] High-Resolution 16-Bit Velocity & Lift Dynamics\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        engine.setSteppedParam(spReleaseVelocityAmt, 2); // Medium release velocity sensitivity

        // Strike Note with high-res 16-bit velocity
        engine.noteOn(60, 65535, 1);
        for (int b = 0; b < 10; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 128);
        }

        // Release with fast lift velocity (Note-Off Velocity)
        engine.noteOff(60, 60000, 1);
        for (int b = 0; b < 10; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 128);
        }
        const int velocityVoice = engine.findVoiceByNote(60);
        const auto* velocityExpression = engine.getVoiceExpressionState(velocityVoice);
        check("High-Res Velocity: 16-bit Note-On Dynamic Scaling",
              velocityExpression && velocityExpression->noteOnVelocity == 65535);
        check("High-Res Velocity: 16-bit Note-Off (Lift) Release Scaling",
              velocityExpression && velocityExpression->noteOffVelocity == 60000);

        engine.allNotesOff();
    }

    // -----------------------------------------------------------------
    // [REGRESSION] Channel-qualified note expression and advertised bend ranges
    // -----------------------------------------------------------------
    {
        engine.reset();
        engine.setSteppedParam(spMPEMode, 0);
        engine.noteOn(60, 40000, 2);
        engine.noteOn(60, 50000, 3);
        const int samePitchCh2 = engine.findVoiceByChannel(2);
        const int samePitchCh3 = engine.findVoiceByChannel(3);
        engine.polyAftertouch(60, 47000, 3);
        const auto* ch2Expression = engine.getVoiceExpressionState(samePitchCh2);
        const auto* ch3Expression = engine.getVoiceExpressionState(samePitchCh3);
        check("Poly-AT: Same Pitch Remains Channel-Qualified",
              ch2Expression && ch3Expression && ch2Expression->pressure == 0
                  && ch3Expression->pressure == 47000);

        engine.noteOff(60, 33000, 3);
        check("Release Velocity: Same Pitch Remains Channel-Qualified",
              ch2Expression && ch3Expression && ch2Expression->noteOffVelocity == 0
                  && ch3Expression->noteOffVelocity == 33000);
        engine.allNotesOff();

        const int expectedRanges[] = {3, 5, 12};
        bool bendRangesPass = true;
        for (uint8_t setting = 0; setting < 3; ++setting) {
            engine.setSteppedParam(spBenderRange, setting);
            engine.pitchBend(8191, 1);
            const int expected = (8191 * expectedRanges[setting] * WTOSC_CV_SEMITONE) / 8192;
            bendRangesPass &= engine.getGlobalPitchBend() == expected;
        }
        check("Pitch Bend: UI Ranges 3/5/12 Semitones", bendRangesPass);
    }

    // -----------------------------------------------------------------
    // [SCENARIO 5] Anti-Zipper Filter Stability & Smoothing Benchmark
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 5] Anti-Zipper Smoothing Filter & Audio Stability\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        engine.reset();
        engine.setSteppedParam(spMPEMode, 1);
        engine.setSteppedParam(spPressureTarget, modFilter);
        engine.noteOn(60, 65535, 2);

        bool audioClean = true;
        // Stress-test with extreme alternating Poly-AT steps at high frequency
        for (int step = 0; step < 100; ++step) {
            uint16_t altPress = (step % 2 == 0) ? 65535 : 0;
            engine.polyAftertouch(60, altPress);
            engine.renderBlock(leftOut.data(), rightOut.data(), 64);

            for (int i = 0; i < 64; ++i) {
                if (std::isnan(leftOut[i]) || std::isinf(leftOut[i]) ||
                    std::isnan(rightOut[i]) || std::isinf(rightOut[i])) {
                    audioClean = false;
                    break;
                }
            }
        }
        check("Anti-Zipper: 1-Pole Low-Pass Filter Stability (No NaN/Inf)", audioClean);

        engine.allNotesOff();
    }

    std::cout << "\n=================================================================\n";
    std::cout << " GRAND SUMMARY: " << totalPassed << " Passed, " << totalFailed << " Failed\n";
    std::cout << " Pass Rate: " << std::fixed << std::setprecision(2)
              << ((float)totalPassed / (float)(totalPassed + totalFailed) * 100.0f) << " %\n";
    std::cout << "=================================================================\n\n";

    return totalFailed > 0 ? 1 : 0;
}
