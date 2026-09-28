#include "TestData.h"
#include "dsp/SynthEngine.h"
#include "dsp/OvercyclerTypes.h"
#include "dsp/ConsoleXProcessor.h"
#include "dsp/AfxKit.h"
#include <algorithm>
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
    std::cout << " Overviber - ConsoleX Master Summer & AFX Mode Test Harness\n";
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
    // [SCENARIO 1] Airwindows ConsoleX Golden Ratio Encode & Decode
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 1] ConsoleX Golden Ratio Processing & Bus Summing\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        ConsoleXProcessor proc;
        proc.setSampleRate(48000.0f);
        proc.setParameters(0.1f, 1.0f, 0.0f); // 0 dB drive, full outPad, 0 discontinuity

        // Test 1: Single sample encode & decode unity transparency
        float inL = 0.35f, inR = -0.42f;
        float encL = 0.0f, encR = 0.0f;
        proc.encodeVoice(inL, inR, encL, encR);

        // Process a short run so ultrasonic biquad settles to steady state
        float decL = 0.0f, decR = 0.0f;
        for (int i = 0; i < 32; ++i) {
            proc.decodeMaster(encL, encR, decL, decR);
        }

        // A single voice should decode with near bit-level mathematical transparency
        float diffL = std::abs(decL - inL);
        float diffR = std::abs(decR - inR);
        check("ConsoleX: Single voice decode transparency (< 0.01)", diffL < 0.01f && diffR < 0.01f);

        // Test 2: Multi-voice summing non-linearity (analog bus compression & glue)
        // Linear summing of 6 voices of 0.25 = 1.50 (clipping)
        // ConsoleX encoding and decoding should soft-saturate and tame the peaks
        float sumEncL = 0.0f, sumEncR = 0.0f;
        for (int v = 0; v < 6; ++v) {
            float vEncL = 0.0f, vEncR = 0.0f;
            proc.encodeVoice(0.25f, 0.25f, vEncL, vEncR);
            sumEncL += vEncL;
            sumEncR += vEncR;
        }
        float sumDecL = 0.0f, sumDecR = 0.0f;
        for (int i = 0; i < 32; ++i) {
            proc.decodeMaster(sumEncL, sumEncR, sumDecL, sumDecR);
        }
        check("ConsoleX: Multi-voice bus compression tames peaks (< 1.50)", sumDecL < 1.45f && sumDecL > 0.8f);

        // Test 3: Discontinuity (Air acoustic steepening) at high SPL
        proc.setParameters(0.1f, 1.0f, 0.9f); // high discontinuity
        float discDecL = 0.0f, discDecR = 0.0f;
        for (int i = 0; i < 32; ++i) {
            proc.decodeMaster(sumEncL, sumEncR, discDecL, discDecR);
        }
        check("ConsoleX: Discontinuity modifies high amplitude peaks", std::abs(discDecL - sumDecL) > 0.001f);
    }

    // -----------------------------------------------------------------
    // [SCENARIO 2] ConsoleX master bus with the Mackity parallel send
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 2] ConsoleX Master Bus & Mackity Parallel Send\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        // Two identical engines rendering the same chord; only the send differs.
        auto render = [&](int sendPot, std::vector<float>& out) {
            SynthEngine e;
            e.prepare(48000.0f);
            if (!initializeTestData(e, argc, argv)) return false;
            if (e.getPresetManager().getPresetCount() > 0) e.loadPreset(0);
            e.setContinuousParam(cpConsoleDiscontinuity, scan_potTo16bits(600));
            e.setContinuousParam(cpMackityDrive, scan_potTo16bits(500));
            e.setContinuousParam(cpMackitySend, scan_potTo16bits(sendPot));
            for (int n : {48, 60, 64, 67}) e.noteOn((uint8_t)n, 60000, 1);
            out.assign(48000, 0.0f);
            std::vector<float> r(512);
            for (size_t pos = 0; pos < out.size(); pos += 512)
                e.renderBlock(out.data() + pos, r.data(), (int)std::min<size_t>(512, out.size() - pos));
            return true;
        };
        auto stats = [](const std::vector<float>& v, float& peak, bool& finite) {
            peak = 0.0f; finite = true;
            for (float s : v) { finite &= std::isfinite(s); peak = std::max(peak, std::abs(s)); }
        };

        std::vector<float> dryA, dryB, wet;
        const bool rendered = render(0, dryA) && render(0, dryB) && render(999, wet);
        check("Mackity Send: test engines initialised", rendered);
        if (rendered) {
            float dryPeak, wetPeak; bool dryFinite, wetFinite;
            stats(dryA, dryPeak, dryFinite);
            stats(wet, wetPeak, wetFinite);
            double diff = 0.0;
            for (size_t i = 0; i < wet.size(); ++i) diff = std::max(diff, (double)std::abs(wet[i] - dryA[i]));

            check("ConsoleX: renders audio without NaNs/Infs", dryFinite && dryPeak > 0.01f);
            check("Mackity Send 0: output is deterministic (no send path)", dryA == dryB);
            check("Mackity Send 100%: audibly enriches the bus", diff > 0.01);
            check("Mackity Send 100%: finite and below the output ceiling", wetFinite && wetPeak <= 1.0f);
        }
    }

    // -----------------------------------------------------------------
    // [SCENARIO 3] Multitimbral AFX Mode (Sound-per-Key Bass Station / Waldorf)
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 3] Multitimbral AFX Mode (Sound per Key)\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        // 1. Enable AFX Mode
        engine.setSteppedParam(spEngineMode, emAFX);

        // Test slot mapping
        uint8_t slotNote36 = engine.getAfxKit().getSlotForNote(36); // C2
        uint8_t slotNote60 = engine.getAfxKit().getSlotForNote(60); // C4
        check("AFX Mode: Note 36 and Note 60 map to valid sound slots", slotNote36 < AFX_SLOT_COUNT && slotNote60 < AFX_SLOT_COUNT);

        // Trigger note 36 (Bass slot)
        engine.noteOn(36, 50000, 1);
        engine.renderBlock(leftOut.data(), rightOut.data(), 256);
        check("AFX Mode: Note 36 renders audio using Slot preset", leftOut[100] != 0.0f || rightOut[100] != 0.0f);
        engine.noteOff(36, 0, 1);

        // Trigger note 60 (Lead/Pluck slot)
        engine.noteOn(60, 50000, 1);
        engine.renderBlock(leftOut.data(), rightOut.data(), 256);
        check("AFX Mode: Note 60 renders audio independently", leftOut[100] != 0.0f || rightOut[100] != 0.0f);
        engine.noteOff(60, 0, 1);

        // Switch back to Multi-Channel (the default)
        engine.setSteppedParam(spEngineMode, emMultiChannel);
        engine.noteOn(60, 50000, 1);
        engine.renderBlock(leftOut.data(), rightOut.data(), 256);
        check("AFX Mode: Switching back to Multi-Channel restores master preset", leftOut[100] != 0.0f || rightOut[100] != 0.0f);
        engine.noteOff(60, 0, 1);
    }

    // -----------------------------------------------------------------
    // [SCENARIO 4] Native MIDI Output (Internal Arp to Ableton Live)
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 4] Native MIDI Output (Internal Arp Routing)\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        // Turn on internal arpeggiator (Up mode, 1/16th note, host sync off, fast BPM)
        engine.setSteppedParam(spArpMode, amUp);
        engine.setSteppedParam(spArpOctaves, 0); // 1 Octave
        engine.setSteppedParam(spArpRate, 3);    // 1/16th
        engine.setSteppedParam(spArpSync, 0);    // Internal Free BPM
        engine.setInternalBpm(160.0f);

        // Clear any previous MIDI out
        std::vector<MidiOutEvent> events;
        engine.pullPendingMidiOut(events);

        // Play chord
        engine.noteOn(60, 60000, 1); // C4
        engine.noteOn(64, 60000, 1); // E4
        engine.noteOn(67, 60000, 1); // G4

        // Render audio blocks which triggers internal clock ticker and arp steps
        for (int b = 0; b < 20; ++b) {
            engine.renderBlock(leftOut.data(), rightOut.data(), 512);
        }

        // Pull queued MIDI output events
        engine.pullPendingMidiOut(events);

        bool receivedNoteOn = false;
        bool receivedNoteOff = false;
        bool validChannelAndVel = true;

        for (const auto& ev : events) {
            if (ev.isNoteOn) receivedNoteOn = true;
            if (!ev.isNoteOn) receivedNoteOff = true;
            if (ev.channel < 1 || ev.channel > 16 || ev.velocity > 127) {
                validChannelAndVel = false;
            }
        }

        check("MIDI Out: Emitted MIDI events during arpeggiation (> 0)", events.size() > 0);
        check("MIDI Out: Emitted both Note-On and Note-Off events", receivedNoteOn && receivedNoteOff);
        check("MIDI Out: Valid MIDI channel (1..16) and 7-bit velocity", validChannelAndVel);

        // Turn off arp and release keys
        engine.setSteppedParam(spArpMode, amOff);
        engine.noteOff(60, 0, 1);
        engine.noteOff(64, 0, 1);
        engine.noteOff(67, 0, 1);
        engine.allNotesOff();
    }

    std::cout << "\n=================================================================\n";
    std::cout << " TEST SUMMARY: " << totalPassed << " PASSED, " << totalFailed << " FAILED\n";
    std::cout << "=================================================================\n";

    return (totalFailed == 0) ? 0 : 1;
}
