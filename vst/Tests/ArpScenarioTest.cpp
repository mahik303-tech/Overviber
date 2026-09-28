#include "TestData.h"
// ==============================================================================
// Overviber - Arpeggiator Audio, MIDI & Visualizer Test Harness
//
// Tests original GliGli firmware arp modes as well as Arpligner-inspired
// chord degree mapping (amDegree) and strumming (amStrum) by Yves Parès.
//
// License: GNU General Public License v3.0 (GPL-3.0)
// ==============================================================================

#include "dsp/SynthEngine.h"
#include "dsp/OvercyclerTypes.h"
#include "dsp/arp.h"
#include "dsp/assigner.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <array>
#include <set>
#include <memory>

namespace fs = std::filesystem;

struct AudioStats {
    uint64_t totalSamples = 0;
    uint64_t nanCount = 0;
    uint64_t infCount = 0;
    float peakAbs = 0.0f;
    double sumSq = 0.0;

    void update(const float* left, const float* right, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            float l = left[i];
            float r = right[i];

            if (std::isnan(l) || std::isnan(r)) nanCount++;
            if (std::isinf(l) || std::isinf(r)) infCount++;

            float al = std::fabs(l);
            float ar = std::fabs(r);
            if (al > peakAbs) peakAbs = al;
            if (ar > peakAbs) peakAbs = ar;

            sumSq += (double)(l * l + r * r) * 0.5;
            totalSamples++;
        }
    }

    float getRMS() const {
        if (totalSamples == 0) return 0.0f;
        return (float)std::sqrt(sumSq / (double)totalSamples);
    }

    bool isSane(float maxPeakAllowed = 4.0f) const {
        return (nanCount == 0) && (infCount == 0) && (peakAbs <= maxPeakAllowed);
    }
};

static std::string midiNoteName(uint8_t note) {
    if (note == ASSIGNER_NO_NOTE) return "-";
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int oct = ((int)note / 12) - 1;
    return std::string(names[note % 12]) + std::to_string(oct);
}

int main(int argc, char* argv[]) {
    std::cout << "=================================================================\n";
    std::cout << " Overviber - Arpeggiator Audio, MIDI & Visualizer Test Harness\n";
    std::cout << "=================================================================\n\n";

    SynthEngine engine;
    engine.prepare(48000.0f);

    // Setup base directories
    if (!initializeTestData(engine, argc, argv)) return 1;

    int presetCount = engine.getPresetManager().getPresetCount();
    std::cout << "[INIT] Scanned " << presetCount << " presets from disk.\n\n";

    if (presetCount == 0) {
        std::cerr << "[ERROR] No presets found!\n";
        return 1;
    }

    const int blockSize = 128;
    std::vector<float> leftBuf(blockSize, 0.0f);
    std::vector<float> rightBuf(blockSize, 0.0f);

    uint64_t totalAllTests = 0;
    uint64_t totalAllPass = 0;
    uint64_t totalAllFail = 0;

    std::ofstream reportFile("test_arp_results.md");
    reportFile << "# Comprehensive Arpeggiator Test Report (Audio, MIDI & Visualizer)\n\n";
    reportFile << "Generated: Overviber Automated Test Suite\n\n";

    // -----------------------------------------------------------------
    // [SCENARIO 1] Full Preset Matrix Arp Audio Test (51 Presets)
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 1] Full Preset Matrix Arp Audio Test (" << presetCount << " Presets)\n";
    std::cout << "-----------------------------------------------------------------\n";

    reportFile << "## Scenario 1: Full Preset Matrix Arp Audio Test\n\n";
    reportFile << "| # | Preset Name | Peak | RMS | Status |\n";
    reportFile << "|---|-------------|------|-----|--------|\n";

    int scen1Pass = 0;
    int scen1Fail = 0;

    for (int p = 0; p < presetCount; ++p) {
        engine.reset();
        engine.loadPreset(p);

        // Configure Arpeggiator: Up mode, 16th notes
        engine.getArpeggiator().setMode(amUp, 0);

        // Play chord: C3 (48), E3 (52), G3 (55), B3 (59)
        constexpr uint16_t testVelocity = static_cast<uint16_t>(100U * 65535U / 127U);
        engine.noteOn(48, testVelocity);
        engine.noteOn(52, testVelocity);
        engine.noteOn(55, testVelocity);
        engine.noteOn(59, testVelocity);

        AudioStats stats;
        // Render 100 audio blocks (~0.27 seconds of arpeggio playback, ~6 arpeggio steps)
        for (int b = 0; b < 100; ++b) {
            std::fill(leftBuf.begin(), leftBuf.end(), 0.0f);
            std::fill(rightBuf.begin(), rightBuf.end(), 0.0f);
            engine.renderBlock(leftBuf.data(), rightBuf.data(), blockSize);
            stats.update(leftBuf.data(), rightBuf.data(), blockSize);
        }

        // Release chord
        engine.noteOff(48, 0);
        engine.noteOff(52, 0);
        engine.noteOff(55, 0);
        engine.noteOff(59, 0);

        // Render 50 release blocks
        for (int b = 0; b < 50; ++b) {
            std::fill(leftBuf.begin(), leftBuf.end(), 0.0f);
            std::fill(rightBuf.begin(), rightBuf.end(), 0.0f);
            engine.renderBlock(leftBuf.data(), rightBuf.data(), blockSize);
            stats.update(leftBuf.data(), rightBuf.data(), blockSize);
        }

        bool pass = stats.isSane(5.0f) && (stats.peakAbs > 0.0001f);
        if (pass) scen1Pass++;
        else scen1Fail++;

        totalAllTests++;
        if (pass) totalAllPass++; else totalAllFail++;

        std::string pName = engine.getPresetManager().getPresetName(p);
        if (pName.empty()) pName = "Preset " + std::to_string(p);

        reportFile << "| " << p << " | " << pName << " | "
                   << std::fixed << std::setprecision(3) << stats.peakAbs << " | "
                   << std::fixed << std::setprecision(3) << stats.getRMS() << " | "
                   << (pass ? "PASS" : "**FAIL**") << " |\n";

        if ((p + 1) % 10 == 0 || p == presetCount - 1) {
            std::cout << "  Tested " << (p + 1) << "/" << presetCount << " presets... Pass: "
                      << scen1Pass << ", Fail: " << scen1Fail << "\n";
        }
    }

    std::cout << "Scenario 1 Complete: " << scen1Pass << " Passed, " << scen1Fail << " Failed.\n\n";

    // -----------------------------------------------------------------
    // [SCENARIO 2] Arp Modes & Note Sequence Verification (MIDI & Pattern)
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 2] Arp Modes & Note Sequence Verification (MIDI & Pattern)\n";
    std::cout << "-----------------------------------------------------------------\n";

    reportFile << "\n## Scenario 2: Arp Modes & Note Sequence Verification\n\n";

    int scen2Pass = 0;
    int scen2Fail = 0;

    auto testModeSequence = [&](const std::string& testName, arpMode_t mode,
                                const std::vector<uint8_t>& inputChord,
                                const std::vector<uint8_t>& expectedCycle,
                                bool testCollisionCheck = false) -> bool {
        Arpeggiator testArp;
        testArp.setMode(mode, 0);

        std::vector<uint8_t> capturedNotes;
        std::vector<int8_t> capturedGates;
        testArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) {
                capturedNotes.push_back(note);
            }
            capturedGates.push_back(gate);
        });

        for (uint8_t n : inputChord) {
            testArp.assignNote(n, 1);
        }

        // Run sufficient clock ticks to cycle through the pattern multiple times
        int numTicks = (int)expectedCycle.size() * 3;
        for (int t = 0; t < numTicks; ++t) {
            testArp.clockTick();
        }

        bool match = true;
        if (capturedNotes.size() < expectedCycle.size()) {
            match = false;
        } else {
            for (size_t i = 0; i < capturedNotes.size(); ++i) {
                uint8_t expected = expectedCycle[i % expectedCycle.size()];
                if (capturedNotes[i] != expected) {
                    match = false;
                    break;
                }
            }
        }

        totalAllTests++;
        if (match) { scen2Pass++; totalAllPass++; } else { scen2Fail++; totalAllFail++; }

        std::cout << "  " << std::left << std::setw(32) << testName << " : "
                  << (match ? "PASS" : "FAIL") << "\n";

        reportFile << "- **" << testName << "**: " << (match ? "PASS" : "**FAIL**") << "\n";
        reportFile << "  - Expected cycle: ";
        for (auto n : expectedCycle) reportFile << midiNoteName(n) << " (" << (int)n << ") ";
        reportFile << "\n  - Captured first " << std::min((int)capturedNotes.size(), (int)expectedCycle.size() * 2) << " notes: ";
        for (size_t i = 0; i < std::min(capturedNotes.size(), (size_t)(expectedCycle.size() * 2)); ++i) {
            reportFile << midiNoteName(capturedNotes[i]) << " ";
        }
        reportFile << "\n";

        return match;
    };

    // 2.1 Mode Up: Ascending triad C3, E3, G3
    testModeSequence("Mode Up (Triad C3-E3-G3)", amUp, {48, 52, 55}, {48, 52, 55});

    // 2.2 Mode Up: 4-note 7th chord C3, E3, G3, B3
    testModeSequence("Mode Up (7th C3-E3-G3-B3)", amUp, {48, 52, 55, 59}, {48, 52, 55, 59});

    // 2.3 Mode Down: Descending chord
    testModeSequence("Mode Down (Triad C3-E3-G3)", amDown, {48, 52, 55}, {55, 52, 48});

    // 2.4 Mode Down: 4-note chord
    testModeSequence("Mode Down (7th C3-E3-G3-B3)", amDown, {48, 52, 55, 59}, {59, 55, 52, 48});

    // 2.5 Mode Up/Down: Triad (no duplicate boundary notes)
    testModeSequence("Mode Up/Down (Triad C3-E3-G3)", amUpDown, {48, 52, 55}, {48, 52, 55, 52});

    // 2.6 Mode Up/Down: 4-note chord
    testModeSequence("Mode Up/Down (7th C3-E3-G3-B3)", amUpDown, {48, 52, 55, 59}, {48, 52, 55, 59, 55, 52});

    // 2.7 CRITICAL COLLISION TEST: Chord C4 (60) + G4 (67). Notice 60 + 67 = 127!
    // In legacy buggy firmware this collision destroyed note 60!
    testModeSequence("Mode Up/Down Note Sum 127 Collision (C4-G4)", amUpDown, {60, 67}, {60, 67});

    // 2.8 Mode Up/Down with 3 notes across middle octave (C4=60, E4=64, G4=67)
    testModeSequence("Mode Up/Down Middle Octave (C4-E4-G4)", amUpDown, {60, 64, 67}, {60, 64, 67, 64});

    // 2.9 Mode As-Played (amAssign): Played out of pitch order
    testModeSequence("Mode As-Played (G3 -> C3 -> B3 -> E3)", amAssign, {55, 48, 59, 52}, {55, 48, 59, 52});

    // 2.10 Mode Random: verify all generated notes belong to the chord set
    {
        Arpeggiator randArp;
        randArp.setMode(amRandom, 0);
        std::set<uint8_t> validChord = {48, 52, 55, 60};
        for (uint8_t n : validChord) randArp.assignNote(n, 1);

        std::vector<uint8_t> randNotes;
        randArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) randNotes.push_back(note);
        });

        for (int i = 0; i < 64; ++i) randArp.clockTick();

        bool randValid = (randNotes.size() == 64);
        std::set<uint8_t> pickedNotes;
        for (uint8_t n : randNotes) {
            if (validChord.find(n) == validChord.end()) {
                randValid = false;
                break;
            }
            pickedNotes.insert(n);
        }
        // At least 3 different notes should be chosen in 64 random ticks
        if (pickedNotes.size() < 3) randValid = false;

        totalAllTests++;
        if (randValid) { scen2Pass++; totalAllPass++; } else { scen2Fail++; totalAllFail++; }

        std::cout << "  " << std::left << std::setw(32) << "Mode Random Note Set Integrity" << " : "
                  << (randValid ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Mode Random Note Set Integrity**: " << (randValid ? "PASS" : "**FAIL**")
                   << " (64 notes generated, all within chord set, distinct notes picked: " << pickedNotes.size() << ")\n";
    }

    std::cout << "Scenario 2 Complete: " << scen2Pass << " Passed, " << scen2Fail << " Failed.\n\n";

    // -----------------------------------------------------------------
    // [SCENARIO 3] Latch / Hold Logic Stress Test
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 3] Latch / Hold Logic Stress Test\n";
    std::cout << "-----------------------------------------------------------------\n";

    reportFile << "\n## Scenario 3: Latch / Hold Logic Stress Test\n\n";

    int scen3Pass = 0;
    int scen3Fail = 0;

    {
        Arpeggiator holdArp;
        holdArp.setMode(amUp, 1); // Hold enabled

        std::vector<uint8_t> playedNotes;
        holdArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) playedNotes.push_back(note);
        });

        // 3.1 Press keys while hold is enabled
        holdArp.assignNote(48, 1);
        holdArp.assignNote(52, 1);
        holdArp.assignNote(55, 1);

        for (int i = 0; i < 6; ++i) holdArp.clockTick();
        bool step1Pass = (playedNotes.size() == 6) && (playedNotes[0] == 48 && playedNotes[1] == 52 && playedNotes[2] == 55);

        totalAllTests++;
        if (step1Pass) { scen3Pass++; totalAllPass++; } else { scen3Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Hold Mode Note Insertion" << " : "
                  << (step1Pass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Hold Mode Note Insertion**: " << (step1Pass ? "PASS" : "**FAIL**") << "\n";

        // 3.2 Release all physical keys - arpeggiator must continue playing latched notes
        playedNotes.clear();
        holdArp.assignNote(48, 0);
        holdArp.assignNote(52, 0);
        holdArp.assignNote(55, 0);

        for (int i = 0; i < 6; ++i) holdArp.clockTick();
        bool step2Pass = (playedNotes.size() == 6) && (playedNotes[0] == 48 && playedNotes[1] == 52 && playedNotes[2] == 55);

        totalAllTests++;
        if (step2Pass) { scen3Pass++; totalAllPass++; } else { scen3Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Hold Latch Key-Release Playback" << " : "
                  << (step2Pass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Hold Latch Key-Release Playback**: " << (step2Pass ? "PASS" : "**FAIL**") << "\n";

        // 3.3 Press new keys - previous latched notes should be automatically replaced
        playedNotes.clear();
        holdArp.assignNote(60, 1);
        holdArp.assignNote(64, 1);

        for (int i = 0; i < 4; ++i) holdArp.clockTick();
        bool step3Pass = (playedNotes.size() == 4) && (playedNotes[0] == 60 && playedNotes[1] == 64);

        totalAllTests++;
        if (step3Pass) { scen3Pass++; totalAllPass++; } else { scen3Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Hold Replacement by New Chord" << " : "
                  << (step3Pass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Hold Replacement by New Chord**: " << (step3Pass ? "PASS" : "**FAIL**") << "\n";

        // 3.4 Disabling hold - releases latched notes when physical keys are up
        holdArp.assignNote(60, 0);
        holdArp.assignNote(64, 0);
        holdArp.setMode(amUp, 0); // Hold toggled OFF

        playedNotes.clear();
        for (int i = 0; i < 4; ++i) holdArp.clockTick();
        bool step4Pass = playedNotes.empty() && !holdArp.hasActiveNotes();

        totalAllTests++;
        if (step4Pass) { scen3Pass++; totalAllPass++; } else { scen3Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Hold Release on Toggle Off" << " : "
                  << (step4Pass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Hold Release on Toggle Off**: " << (step4Pass ? "PASS" : "**FAIL**") << "\n";
    }

    std::cout << "Scenario 3 Complete: " << scen3Pass << " Passed, " << scen3Fail << " Failed.\n\n";

    // -----------------------------------------------------------------
    // [SCENARIO 4] Transposition & Gate Timing Integrity Test
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 4] Transposition & Gate Timing Integrity Test\n";
    std::cout << "-----------------------------------------------------------------\n";

    reportFile << "\n## Scenario 4: Transposition & Gate Timing Integrity Test\n\n";

    int scen4Pass = 0;
    int scen4Fail = 0;

    {
        Arpeggiator transArp;
        transArp.setMode(amUp, 0);
        transArp.assignNote(60, 1); // C4

        // 4.1 Octave up (+12 semitones)
        transArp.setTranspose(12);
        uint8_t notePlayed = 0;
        transArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) notePlayed = note;
        });
        transArp.clockTick();
        bool trans1Pass = (notePlayed == 72); // C5

        totalAllTests++;
        if (trans1Pass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Transpose +12 Semitones (Octave Up)" << " : "
                  << (trans1Pass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Transpose +12 Semitones (Octave Up)**: " << (trans1Pass ? "PASS" : "**FAIL**") << "\n";

        // 4.2 Octave down (-12 semitones)
        transArp.setTranspose(-12);
        transArp.clockTick();
        bool trans2Pass = (notePlayed == 48); // C3

        totalAllTests++;
        if (trans2Pass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Transpose -12 Semitones (Octave Down)" << " : "
                  << (trans2Pass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Transpose -12 Semitones (Octave Down)**: " << (trans2Pass ? "PASS" : "**FAIL**") << "\n";

        // 4.3 Gate state check: finishPreviousNote clears gateState
        bool gateBefore = transArp.isGateActive();
        transArp.finishPreviousNote();
        bool gateAfter = transArp.isGateActive();
        bool gatePass = gateBefore && !gateAfter;

        totalAllTests++;
        if (gatePass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Gate State Clear on finishPreviousNote" << " : "
                  << (gatePass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Gate State Clear on finishPreviousNote**: " << (gatePass ? "PASS" : "**FAIL**") << "\n";

        // 4.4 Even modes that preserve note memory must release the sounding gate.
        Arpeggiator switchArp;
        int switchNoteOffs = 0;
        switchArp.setMode(amRandom, 0);
        switchArp.assignNote(60, 1);
        switchArp.setNoteAssignCallback([&](uint8_t, int8_t gate, uint16_t) {
            if (!gate) ++switchNoteOffs;
        });
        switchArp.clockTick();
        switchArp.setMode(amAssign, 0);
        bool switchPass = switchNoteOffs == 1 && !switchArp.isGateActive() && switchArp.hasActiveNotes();
        totalAllTests++;
        if (switchPass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Mode Switch Releases Active Gate" << " : "
                  << (switchPass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Mode Switch Releases Active Gate**: " << (switchPass ? "PASS" : "**FAIL**") << "\n";

        // 4.5 MIDI note 127 must remain representable while latched.
        Arpeggiator topNoteArp;
        topNoteArp.setMode(amUp, 1);
        topNoteArp.assignNote(127, 1);
        topNoteArp.assignNote(127, 0);
        uint8_t topActive[1]{};
        const bool topNoteHeld = topNoteArp.getActiveNotes(topActive, 1) == 1 && topActive[0] == 127;
        topNoteArp.setMode(amUp, 0);
        const bool topNoteReleased = !topNoteArp.hasActiveNotes();
        const bool topNotePass = topNoteHeld && topNoteReleased;
        totalAllTests++;
        if (topNotePass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Hold Preserves MIDI Note 127" << " : "
                  << (topNotePass ? "PASS" : "FAIL") << "\n";

        // 4.6 A tie keeps the existing gate open until the following play step.
        Arpeggiator tieArp;
        std::vector<int> tieGates;
        tieArp.setMode(amUp, 0);
        tieArp.setStepPattern(0, 0);
        tieArp.setStepPattern(1, 2);
        tieArp.setStepPattern(2, 0);
        tieArp.assignNote(60, 1);
        tieArp.setNoteAssignCallback([&](uint8_t, int8_t gate, uint16_t) { tieGates.push_back(gate); });
        tieArp.clockTick();
        const size_t afterPlay = tieGates.size();
        tieArp.clockTick();
        const bool tieStayedOpen = tieArp.isGateActive() && tieGates.size() == afterPlay;
        tieArp.clockTick();
        const bool tiePass = tieStayedOpen && tieGates.size() == afterPlay + 2
            && tieGates[tieGates.size() - 2] == 0 && tieGates.back() == 1;
        totalAllTests++;
        if (tiePass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Tie Keeps Previous Gate Open" << " : "
                  << (tiePass ? "PASS" : "FAIL") << "\n";

        // 4.7 Transposition must never emit an invalid MIDI note.
        Arpeggiator clampArp;
        std::vector<uint8_t> clampedNotes;
        clampArp.setMode(amUp, 0);
        clampArp.setTranspose(-24);
        clampArp.assignNote(0, 1);
        clampArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t) {
            if (gate) clampedNotes.push_back(note);
        });
        clampArp.clockTick();
        clampArp.allNotesOff();
        clampArp.setMode(amUp, 0);
        clampArp.setTranspose(24);
        clampArp.assignNote(127, 1);
        clampArp.clockTick();
        const bool clampPass = clampedNotes.size() == 2 && clampedNotes[0] == 0 && clampedNotes[1] == 127;
        totalAllTests++;
        if (clampPass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Transpose Clamps MIDI Range" << " : "
                  << (clampPass ? "PASS" : "FAIL") << "\n";

        // 4.8 Assigner configuration changes must release gates before clearing allocations.
        VoiceAssigner configAssigner;
        int configNoteOffs = 0;
        configAssigner.setCallback([&](uint8_t, int8_t gate, int8_t, uint16_t, uint8_t) {
            if (!gate) ++configNoteOffs;
        });
        configAssigner.assignNote(60, 1, 50000, 1, 1);
        configAssigner.setVoiceMask(0x03);
        configAssigner.assignNote(62, 1, 50000, 1, 2);
        configAssigner.setPriority(apHigh);
        const bool configReleasePass = configNoteOffs == 2;
        totalAllTests++;
        if (configReleasePass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Config Changes Release Gates" << " : "
                  << (configReleasePass ? "PASS" : "FAIL") << "\n";

        // 4.9 Panic/all-notes-off clears both the generated gate and arp memory.
        auto panicEngine = std::make_unique<SynthEngine>();
        panicEngine->prepare(48000.0f);
        panicEngine->setSteppedParam(spArpMode, amUp);
        panicEngine->noteOn(60, 50000, 1);
        panicEngine->getArpeggiator().clockTick();
        panicEngine->allNotesOff();
        bool panicHasNoteOff = false;
        for (const auto& event : panicEngine->getPendingMidiOut()) panicHasNoteOff |= !event.isNoteOn;
        const bool panicPass = panicHasNoteOff && !panicEngine->getArpeggiator().hasActiveNotes()
            && !panicEngine->getArpeggiator().isGateActive();
        totalAllTests++;
        if (panicPass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "All Notes Off Clears Arp" << " : "
                  << (panicPass ? "PASS" : "FAIL") << "\n";

        // 4.10 Equal pitches on different channels retain independent channel/velocity metadata.
        Arpeggiator channelArp;
        struct CapturedArpEvent { uint8_t note, channel; uint16_t velocity; int8_t gate; };
        std::vector<CapturedArpEvent> channelEvents;
        channelArp.setMode(amAssign, 0);
        channelArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t velocity, uint8_t channel) {
            channelEvents.push_back({note, channel, velocity, gate});
        });
        channelArp.assignNote(60, 1, 12000, 2);
        channelArp.assignNote(60, 1, 52000, 3);
        channelArp.clockTick();
        channelArp.clockTick();
        channelArp.assignNote(60, 0, 0, 2);
        channelArp.clockTick();
        std::vector<CapturedArpEvent> channelOns;
        for (const auto& event : channelEvents) if (event.gate) channelOns.push_back(event);
        const bool channelMetadataPass = channelOns.size() == 3
            && channelOns[0].channel == 2 && channelOns[0].velocity == 12000
            && channelOns[1].channel == 3 && channelOns[1].velocity == 52000
            && channelOns[2].channel == 3 && channelOns[2].velocity == 52000;
        totalAllTests++;
        if (channelMetadataPass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Arp Preserves Channel/Velocity" << " : "
                  << (channelMetadataPass ? "PASS" : "FAIL") << "\n";

        // 4.11 Stopping a host-synced transport must close the current generated gate immediately.
        auto stoppedEngine = std::make_unique<SynthEngine>();
        stoppedEngine->prepare(48000.0f);
        stoppedEngine->setSteppedParam(spArpMode, amUp);
        stoppedEngine->noteOn(60, 50000, 4);
        stoppedEngine->getArpeggiator().clockTick();
        stoppedEngine->clearPendingMidiOut();
        stoppedEngine->setHostTransport(1.0, false);
        bool stopSentNoteOff = false;
        for (const auto& event : stoppedEngine->getPendingMidiOut())
            stopSentNoteOff |= !event.isNoteOn && event.channel == 4 && event.note == 60;
        const bool transportStopPass = stopSentNoteOff && !stoppedEngine->getArpeggiator().isGateActive();
        totalAllTests++;
        if (transportStopPass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Transport Stop Closes Arp Gate" << " : "
                  << (transportStopPass ? "PASS" : "FAIL") << "\n";

        // 4.12 A delayed swing step's gate is measured from its actual trigger tick.
        auto swingEngine = std::make_unique<SynthEngine>();
        swingEngine->prepare(48000.0f);
        swingEngine->setHostSyncEnabled(false);
        swingEngine->setInternalBpm(120.0f);
        swingEngine->setSteppedParam(spArpMode, amUp);
        swingEngine->getArpeggiator().setRate(5);
        swingEngine->getArpeggiator().setSwing(0.75f);
        swingEngine->getArpeggiator().setGateLength(0.10f);
        swingEngine->noteOn(60, 50000, 1);
        int absoluteSample = 0, noteOnCount = 0, secondOnSample = -1, secondOffSample = -1;
        std::array<float, 128> swingLeft{}, swingRight{};
        for (int block = 0; block < 100 && secondOffSample < 0; ++block) {
            swingEngine->clearPendingMidiOut();
            swingEngine->renderBlock(swingLeft.data(), swingRight.data(), 128);
            for (const auto& event : swingEngine->getPendingMidiOut()) {
                if (event.isNoteOn) {
                    if (++noteOnCount == 2) secondOnSample = absoluteSample + event.sampleOffset;
                } else if (secondOnSample >= 0) {
                    secondOffSample = absoluteSample + event.sampleOffset;
                }
            }
            absoluteSample += 128;
        }
        const int swungGateSamples = secondOffSample - secondOnSample;
        const bool swingGatePass = secondOnSample >= 0 && swungGateSamples >= 450 && swungGateSamples <= 550;
        totalAllTests++;
        if (swingGatePass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Swing Gate Uses Delayed Trigger" << " : "
                  << (swingGatePass ? "PASS" : "FAIL") << "\n";

        // 4.13 Hold at 100% gate retains one control tick for a real release.
        auto fullGateEngine = std::make_unique<SynthEngine>();
        fullGateEngine->prepare(48000.0f);
        fullGateEngine->setHostSyncEnabled(false);
        fullGateEngine->setInternalBpm(120.0f);
        fullGateEngine->setSteppedParam(spArpMode, amUp);
        fullGateEngine->setSteppedParam(spArpHold, 1);
        fullGateEngine->getArpeggiator().setRate(5);
        fullGateEngine->getArpeggiator().setGateLength(1.0f);
        fullGateEngine->noteOn(60, 50000, 1);
        int fullGateAbsolute = 0, firstOnSample = -1, firstOffSample = -1, nextOnSample = -1;
        std::array<float, 128> fullGateLeft{}, fullGateRight{};
        for (int block = 0; block < 100 && nextOnSample < 0; ++block) {
            fullGateEngine->clearPendingMidiOut();
            fullGateEngine->renderBlock(fullGateLeft.data(), fullGateRight.data(), 128);
            for (const auto& event : fullGateEngine->getPendingMidiOut()) {
                const int eventSample = fullGateAbsolute + event.sampleOffset;
                if (event.isNoteOn) {
                    if (firstOnSample < 0) firstOnSample = eventSample;
                    else nextOnSample = eventSample;
                } else if (firstOnSample >= 0 && firstOffSample < 0) {
                    firstOffSample = eventSample;
                }
            }
            fullGateAbsolute += 128;
        }
        const bool fullGateReleasePass = firstOnSample >= 0 && firstOffSample > firstOnSample
            && nextOnSample > firstOffSample;
        totalAllTests++;
        if (fullGateReleasePass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Hold 100% Gate Release Gap" << " : "
                  << (fullGateReleasePass ? "PASS" : "FAIL") << "\n";

        // 4.14 Voice-pattern offsets saturate at the MIDI ceiling instead of wrapping.
        VoiceAssigner rangeAssigner;
        uint8_t rangePattern[SYNTH_VOICE_COUNT] = {0, 12, ASSIGNER_NO_NOTE, ASSIGNER_NO_NOTE, ASSIGNER_NO_NOTE, ASSIGNER_NO_NOTE};
        std::vector<uint8_t> rangeNotes;
        rangeAssigner.setCallback([&](uint8_t note, int8_t gate, int8_t, uint16_t, uint8_t) {
            if (gate) rangeNotes.push_back(note);
        });
        rangeAssigner.setPattern(rangePattern, 0);
        rangeAssigner.assignNote(120, 1, 50000, 1, 1);
        const bool patternRangePass = rangeNotes.size() == 2 && rangeNotes[0] == 120 && rangeNotes[1] == 127;
        totalAllTests++;
        if (patternRangePass) { scen4Pass++; totalAllPass++; } else { scen4Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(32) << "Pattern Offset Clamps MIDI Range" << " : "
                  << (patternRangePass ? "PASS" : "FAIL") << "\n";
    }

    std::cout << "Scenario 4 Complete: " << scen4Pass << " Passed, " << scen4Fail << " Failed.\n\n";

    // -----------------------------------------------------------------
    // [SCENARIO 5] Visualizer vs Audio/MIDI Equivalence Verification
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 5] Visualizer vs Audio/MIDI Equivalence Verification\n";
    std::cout << "-----------------------------------------------------------------\n";

    reportFile << "\n## Scenario 5: Visualizer vs Audio/MIDI Equivalence Verification\n\n";

    int scen5Pass = 0;
    int scen5Fail = 0;

    auto testVisualizerEquivalence = [&](const std::string& name, arpMode_t mode,
                                        const std::vector<uint8_t>& chord) -> bool {
        Arpeggiator testArp;
        testArp.setMode(mode, 0);
        for (uint8_t n : chord) testArp.assignNote(n, 1);

        // 1. Get 16-step pattern predicted by the visualizer
        uint8_t visualPattern[16];
        int patternCount = testArp.getPattern(visualPattern, 16);

        // 2. Capture actual 16 notes generated by audio engine / clockTick
        std::vector<uint8_t> engineNotes;
        testArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) engineNotes.push_back(note);
        });

        for (int step = 0; step < 16; ++step) {
            testArp.clockTick();
        }

        bool match = (patternCount > 0) && (engineNotes.size() == 16);
        if (match) {
            for (int s = 0; s < 16; ++s) {
                if (visualPattern[s] != engineNotes[s]) {
                    match = false;
                    break;
                }
            }
        }

        // 3. Verify visualizer pitch-to-row monotonicity
        uint8_t minNote = 127, maxNote = 0;
        for (uint8_t n : chord) {
            if (n < minNote) minNote = n;
            if (n > maxNote) maxNote = n;
        }

        int patternRows[16];
        const int numRows = 8;
        for (int s = 0; s < 16; ++s) {
            uint8_t n = visualPattern[s];
            if (n == ASSIGNER_NO_NOTE || maxNote == minNote) {
                patternRows[s] = 3;
            } else {
                int r = (int)std::round(((float)(n - minNote) / (float)(maxNote - minNote)) * (numRows - 1));
                patternRows[s] = std::clamp(r, 0, numRows - 1);
            }
        }

        // Check that higher pitch notes are in strictly greater or equal rows
        bool rowMonotonic = true;
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 16; ++j) {
                if (visualPattern[i] > visualPattern[j] && patternRows[i] < patternRows[j]) {
                    rowMonotonic = false;
                    break;
                }
            }
        }

        bool overallPass = match && rowMonotonic;

        totalAllTests++;
        if (overallPass) { scen5Pass++; totalAllPass++; } else { scen5Fail++; totalAllFail++; }

        std::cout << "  " << std::left << std::setw(36) << name << " : "
                  << (overallPass ? "PASS" : "FAIL") << "\n";

        reportFile << "- **" << name << "**: " << (overallPass ? "PASS" : "**FAIL**") << "\n";
        reportFile << "  - Visualizer 16-step pattern : ";
        for (int s = 0; s < 16; ++s) reportFile << midiNoteName(visualPattern[s]) << " ";
        reportFile << "\n  - Audio engine playback      : ";
        for (int s = 0; s < 16; ++s) reportFile << midiNoteName(engineNotes[s]) << " ";
        reportFile << "\n  - Visualizer row assignments : ";
        for (int s = 0; s < 16; ++s) reportFile << patternRows[s] << " ";
        reportFile << "\n";

        return overallPass;
    };

    testVisualizerEquivalence("Up Mode (C3-E3-G3 Triad)", amUp, {48, 52, 55});
    testVisualizerEquivalence("Up Mode (C3-E3-G3-B3 7th Chord)", amUp, {48, 52, 55, 59});
    testVisualizerEquivalence("Down Mode (C3-E3-G3 Triad)", amDown, {48, 52, 55});
    testVisualizerEquivalence("Down Mode (C3-E3-G3-B3 7th Chord)", amDown, {48, 52, 55, 59});
    testVisualizerEquivalence("Up/Down Mode (C3-E3-G3 Triad)", amUpDown, {48, 52, 55});
    testVisualizerEquivalence("Up/Down Mode (C3-E3-G3-B3 7th Chord)", amUpDown, {48, 52, 55, 59});
    testVisualizerEquivalence("Up/Down Middle Octave (C4-E4-G4)", amUpDown, {60, 64, 67});
    testVisualizerEquivalence("As-Played Mode (Order of Entry)", amAssign, {55, 48, 59, 52});

    std::cout << "Scenario 5 Complete: " << scen5Pass << " Passed, " << scen5Fail << " Failed.\n\n";

    // -----------------------------------------------------------------
    // [SCENARIO 6] Extended Arp Features (Multi-Octaves, Modes, Patterns, Rates)
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 6] Extended Arp Features (Multi-Octaves, Chord/Converge, Sequencer)\n";
    std::cout << "-----------------------------------------------------------------\n";

    reportFile << "\n## Scenario 6: Extended Arp Features Verification\n\n";

    int scen6Pass = 0;
    int scen6Fail = 0;

    // 6.1 Multi-Octave Up Test (2 Octaves)
    {
        Arpeggiator multiArp;
        multiArp.setMode(amUp, 0);
        multiArp.setOctaves(2);
        multiArp.assignNote(48, 1); // C3
        multiArp.assignNote(52, 1); // E3
        multiArp.assignNote(55, 1); // G3

        std::vector<uint8_t> captured;
        multiArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) captured.push_back(note);
        });

        for (int i = 0; i < 12; ++i) multiArp.clockTick();

        std::vector<uint8_t> expected = { 48, 52, 55, 60, 64, 67 }; // C3, E3, G3, C4, E4, G4
        bool match = (captured.size() == 12);
        for (size_t i = 0; i < captured.size() && match; ++i) {
            if (captured[i] != expected[i % expected.size()]) match = false;
        }

        totalAllTests++;
        if (match) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Multi-Octave Up (2 Octaves)" << " : " << (match ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Multi-Octave Up (2 Octaves)**: " << (match ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.2 Multi-Octave Down Test (3 Octaves)
    {
        Arpeggiator multiArp;
        multiArp.setMode(amDown, 0);
        multiArp.setOctaves(3);
        multiArp.assignNote(48, 1); // C3
        multiArp.assignNote(55, 1); // G3

        std::vector<uint8_t> captured;
        multiArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) captured.push_back(note);
        });

        for (int i = 0; i < 12; ++i) multiArp.clockTick();

        std::vector<uint8_t> expected = { 79, 72, 67, 60, 55, 48 }; // G5, C5, G4, C4, G3, C3
        bool match = (captured.size() == 12);
        for (size_t i = 0; i < captured.size() && match; ++i) {
            if (captured[i] != expected[i % expected.size()]) match = false;
        }

        totalAllTests++;
        if (match) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Multi-Octave Down (3 Octaves)" << " : " << (match ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Multi-Octave Down (3 Octaves)**: " << (match ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.3 Four-octave visualizer/audio range
    {
        Arpeggiator fourOctaveArp;
        fourOctaveArp.setMode(amUp, 0);
        fourOctaveArp.setOctaves(4);
        fourOctaveArp.assignNote(48, 1); // C3 through C6

        uint8_t visualPattern[8]{};
        fourOctaveArp.getPattern(visualPattern, 8);
        std::vector<uint8_t> captured;
        fourOctaveArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t) {
            if (gate) captured.push_back(note);
        });
        for (int i = 0; i < 8; ++i) fourOctaveArp.clockTick();

        const std::array<uint8_t, 4> expected{48, 60, 72, 84};
        bool match = captured.size() == 8;
        for (int i = 0; i < 8 && match; ++i)
            match = captured[i] == expected[i % 4] && visualPattern[i] == expected[i % 4];

        totalAllTests++;
        if (match) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Four-Octave Visualizer/Audio Range" << " : " << (match ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Four-Octave Visualizer/Audio Range**: " << (match ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.4 Mode Chord Playback (Triggers all active chord notes in parallel per step)
    {
        Arpeggiator chordArp;
        chordArp.setMode(amChord, 0);
        chordArp.setOctaves(1);
        chordArp.assignNote(48, 1);
        chordArp.assignNote(52, 1);
        chordArp.assignNote(55, 1);

        std::vector<uint8_t> captured;
        chordArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) captured.push_back(note);
        });

        for (int i = 0; i < 4; ++i) chordArp.clockTick();

        // 4 ticks * 3 notes = 12 captured notes, repeating [48, 52, 55]
        bool match = (captured.size() == 12) &&
                     (captured[0] == 48 && captured[1] == 52 && captured[2] == 55) &&
                     (captured[3] == 48 && captured[4] == 52 && captured[5] == 55);
        totalAllTests++;
        if (match) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Mode Chord Playback (Parallel Poly)" << " : " << (match ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Mode Chord Playback (Parallel Poly)**: " << (match ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.4 Mode Converge Playback (Outside-In: Lowest, Highest, 2nd Lowest, 2nd Highest...)
    {
        Arpeggiator convArp;
        convArp.setMode(amConverge, 0);
        convArp.setOctaves(1);
        convArp.assignNote(48, 1); // C3
        convArp.assignNote(52, 1); // E3
        convArp.assignNote(55, 1); // G3
        convArp.assignNote(59, 1); // B3

        std::vector<uint8_t> captured;
        convArp.setNoteAssignCallback([&](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) captured.push_back(note);
        });

        for (int i = 0; i < 8; ++i) convArp.clockTick();

        std::vector<uint8_t> expected = { 48, 59, 52, 55 }; // Lowest (48), Highest (59), 2nd Lowest (52), 2nd Highest (55)
        bool match = (captured.size() == 8);
        for (size_t i = 0; i < captured.size() && match; ++i) {
            if (captured[i] != expected[i % expected.size()]) match = false;
        }

        totalAllTests++;
        if (match) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Mode Converge (Outside-In)" << " : " << (match ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Mode Converge (Outside-In)**: " << (match ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.5 Clock Division Rates Verification
    {
        Arpeggiator rateArp;
        rateArp.setRate(0); // 1/4 Note
        bool r0 = (rateArp.getStepDivisionTicks() == 48);
        rateArp.setRate(1); // 1/8 Note
        bool r1 = (rateArp.getStepDivisionTicks() == 24);
        rateArp.setRate(2); // 1/8 Triplet
        bool r2 = (rateArp.getStepDivisionTicks() == 16);
        rateArp.setRate(3); // 1/16 Note
        bool r3 = (rateArp.getStepDivisionTicks() == 12);
        rateArp.setRate(4); // 1/16 Triplet
        bool r4 = (rateArp.getStepDivisionTicks() == 8);
        rateArp.setRate(5); // 1/32 Note
        bool r5 = (rateArp.getStepDivisionTicks() == 6);

        bool ratePass = r0 && r1 && r2 && r3 && r4 && r5;
        totalAllTests++;
        if (ratePass) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Clock Division Rates (1/4 to 1/32)" << " : " << (ratePass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Clock Division Rates (1/4 to 1/32)**: " << (ratePass ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.6 16-Step Pattern Sequencer (Play, Accent, Tie, Mute)
    {
        Arpeggiator seqArp;
        seqArp.setMode(amUp, 0);
        seqArp.assignNote(48, 1);
        seqArp.assignNote(52, 1);
        seqArp.assignNote(55, 1);

        // Configure step pattern: Step 0=Play, Step 1=Accent, Step 2=Mute, Step 3=Tie
        seqArp.setStepPattern(0, 0); // Play
        seqArp.setStepPattern(1, 1); // Accent
        seqArp.setStepPattern(2, 3); // Mute
        seqArp.setStepPattern(3, 2); // Tie

        bool patCheck = (seqArp.getStepPattern(0) == 0) &&
                        (seqArp.getStepPattern(1) == 1) &&
                        (seqArp.getStepPattern(2) == 3) &&
                        (seqArp.getStepPattern(3) == 2);

        // Verify cycleStepPattern cycles 0 -> 1 -> 2 -> 3 -> 0
        seqArp.cycleStepPattern(0);
        bool cycleCheck = (seqArp.getStepPattern(0) == 1);

        bool seqPass = patCheck && cycleCheck;
        totalAllTests++;
        if (seqPass) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "16-Step Pattern Sequencer Logic" << " : " << (seqPass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **16-Step Pattern Sequencer Logic**: " << (seqPass ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.7 Gate Length and Swing Groove Bounds
    {
        Arpeggiator paramArp;
        paramArp.setGateLength(0.50f);
        bool gPass = std::abs(paramArp.getGateLength() - 0.50f) < 0.001f;
        paramArp.setSwing(0.66f);
        bool sPass = std::abs(paramArp.getSwing() - 0.66f) < 0.001f;

        bool boundsPass = gPass && sPass;
        totalAllTests++;
        if (boundsPass) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Gate Length & Swing Bounds" << " : " << (boundsPass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Gate Length & Swing Bounds**: " << (boundsPass ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.8 MIDI Tempo Sync Toggle & Free BPM Knob Engine Control
    {
        SynthEngine syncEngine;
        syncEngine.prepare(48000.0f);
        syncEngine.setHostBpm(128.0f);
        syncEngine.setHostSyncEnabled(true);
        bool syncActive = (std::abs(syncEngine.getEffectiveBpm() - 128.0f) < 0.01f);
        syncEngine.setHostTransport(4.25, false);
        const uint32_t stoppedTick = syncEngine.getCurrentTick();
        float syncLeft[1024]{}, syncRight[1024]{};
        syncEngine.renderBlock(syncLeft, syncRight, 1024);
        const bool transportStopped = stoppedTick == 204 && syncEngine.getCurrentTick() == stoppedTick;
        syncEngine.setHostTransport(4.25, true);
        syncEngine.renderBlock(syncLeft, syncRight, 1024);
        const bool transportAdvanced = syncEngine.getCurrentTick() > stoppedTick;

        syncEngine.setHostSyncEnabled(false);
        syncEngine.setInternalBpm(175.0f);
        bool freeActive = (std::abs(syncEngine.getEffectiveBpm() - 175.0f) < 0.01f);

        bool syncPass = syncActive && transportStopped && transportAdvanced && freeActive;
        totalAllTests++;
        if (syncPass) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "MIDI Tempo Sync & Free BPM" << " : " << (syncPass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **MIDI Tempo Sync & Free BPM**: " << (syncPass ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.9 Chord Degree Mode (Arpligner Concept) with Octave Wraparound
    {
        Arpeggiator degArp;
        degArp.setMode(amDegree, 0);
        degArp.assignNote(48, 1); // C3 (Root / Deg 0)
        degArp.assignNote(52, 1); // E3 (3rd / Deg 1)
        degArp.assignNote(55, 1); // G3 (5th / Deg 2)

        // Set sequence degrees: 0, 1, 2, 3 (Wraps to C4=60), 4 (Wraps to E4=64), 5 (Wraps to G4=67)
        degArp.setStepDegree(0, 0);
        degArp.setStepDegree(1, 1);
        degArp.setStepDegree(2, 2);
        degArp.setStepDegree(3, 3);
        degArp.setStepDegree(4, 4);
        degArp.setStepDegree(5, 5);

        uint8_t pat[16];
        degArp.getPattern(pat, 6);

        bool d0 = (pat[0] == 48); // Root
        bool d1 = (pat[1] == 52); // 2nd note (E3)
        bool d2 = (pat[2] == 55); // 3rd note (G3)
        bool d3 = (pat[3] == 60); // 4th note wraps to Root + 12
        bool d4 = (pat[4] == 64); // 5th note wraps to E3 + 12
        bool d5 = (pat[5] == 67); // 6th note wraps to G3 + 12

        std::vector<uint8_t> playedNotes;
        degArp.setNoteAssignCallback([&playedNotes](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) playedNotes.push_back(note);
        });

        for (int i = 0; i < 6; ++i) {
            degArp.clockTick();
        }

        bool tickCheck = (playedNotes.size() == 6) &&
                         (playedNotes[0] == 48) && (playedNotes[1] == 52) && (playedNotes[2] == 55) &&
                         (playedNotes[3] == 60) && (playedNotes[4] == 64) && (playedNotes[5] == 67);

        bool degPass = d0 && d1 && d2 && d3 && d4 && d5 && tickCheck;
        totalAllTests++;
        if (degPass) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Chord Degree & Octave Wrap (Arpligner)" << " : " << (degPass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Chord Degree & Octave Wrap (Arpligner)**: " << (degPass ? "PASS" : "**FAIL**") << "\n";
    }

    // 6.10 Polyphonic Strum Mode (Arpligner Multi-Note Triggering)
    {
        Arpeggiator strumArp;
        strumArp.setMode(amStrum, 0);
        strumArp.assignNote(48, 1);
        strumArp.assignNote(52, 1);
        strumArp.assignNote(55, 1);
        strumArp.setStepDegree(0, 0); // Triggers Deg 0 (48) + Deg 2 (55)

        std::vector<uint8_t> strummed;
        strumArp.setNoteAssignCallback([&strummed](uint8_t note, int8_t gate, uint16_t vel) {
            if (gate) strummed.push_back(note);
        });

        strumArp.clockTick();
        bool polyPass = (strummed.size() >= 2) && (strummed[0] == 48) && (strummed[1] == 55);

        totalAllTests++;
        if (polyPass) { scen6Pass++; totalAllPass++; } else { scen6Fail++; totalAllFail++; }
        std::cout << "  " << std::left << std::setw(36) << "Polyphonic Strum Mode" << " : " << (polyPass ? "PASS" : "FAIL") << "\n";
        reportFile << "- **Polyphonic Strum Mode**: " << (polyPass ? "PASS" : "**FAIL**") << "\n";
    }

    std::cout << "Scenario 6 Complete: " << scen6Pass << " Passed, " << scen6Fail << " Failed.\n\n";

    // -----------------------------------------------------------------
    // GRAND SUMMARY
    // -----------------------------------------------------------------
    double passRate = (totalAllTests > 0) ? ((double)totalAllPass / (double)totalAllTests) * 100.0 : 0.0;

    std::cout << "=================================================================\n";
    std::cout << " GRAND SUMMARY:\n";
    std::cout << " Total Test Executions: " << totalAllTests << "\n";
    std::cout << " Passed:                " << totalAllPass << "\n";
    std::cout << " Failed:                " << totalAllFail << "\n";
    std::cout << " Pass Rate:             " << std::fixed << std::setprecision(2) << passRate << " %\n";
    std::cout << "=================================================================\n\n";

    reportFile << "\n## Grand Summary\n\n";
    reportFile << "- **Total Executions**: " << totalAllTests << "\n";
    reportFile << "- **Passed**: " << totalAllPass << "\n";
    reportFile << "- **Failed**: " << totalAllFail << "\n";
    reportFile << "- **Pass Rate**: " << std::fixed << std::setprecision(2) << passRate << " %\n\n";
    reportFile << (totalAllFail == 0 ? "### RESULT: ALL TESTS PASSED (100%)\n" : "### RESULT: FAILURES DETECTED\n");
    reportFile.close();

    std::cout << "[REPORT] Detailed Markdown report written to: test_arp_results.md\n\n";

    return (totalAllFail == 0) ? 0 : 1;
}
