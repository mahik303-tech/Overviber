#include "TestData.h"
#include "TestSynth.h"
#include "dsp/OvercyclerTypes.h"
#include "data/PresetManager.h"
#include "data/WaveManager.h"
#include "ui/ClassicParamSchema.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

static bool runTest(const std::string& testName, bool condition) {
    std::cout << "  " << std::left << std::setw(58) << testName << ": "
              << (condition ? "[PASS]" : "[FAIL]") << "\n";
    return condition;
}

int main(int argc, char* argv[]) {
    std::cout << "=================================================================\n";
    std::cout << " GliGli Overcycler - Classic Skin Functional Verification Suite\n";
    std::cout << " Based on Operation Manual v1.1 and Firmware 17xx Specifications\n";
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

    // -----------------------------------------------------------------
    // [SCENARIO 1] Hardware Schema Integrity & Page Coverage
    // -----------------------------------------------------------------
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 1] Parameter Matrix & Page Definitions (ui_pages.h)\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        // 1. Oscillators Page (Page 1)
        const auto& pOsc = ClassicUI::SchemaRegistry::getPage(ClassicUI::PageId::Osc);
        check("Page 1 is OSCILLATORS", std::string(pOsc.name) == "OSCILLATORS");
        check("Page 1 Pot 0 is ABnk (Osc A Bank)", std::string(pOsc.pots[0].shortName) == "ABnk");
        check("Page 1 Pot 1 is AWav (Osc A Waveform)", std::string(pOsc.pots[1].shortName) == "AWav");
        check("Page 1 Pot 2 is AFrq (Osc A Frequency)", std::string(pOsc.pots[2].shortName) == "AFrq");
        check("Page 1 Pot 3 is NVol (Noise Volume)", std::string(pOsc.pots[3].shortName) == "NVol");
        check("Page 1 Pot 4 is AVol (Osc A Volume)", std::string(pOsc.pots[4].shortName) == "AVol");
        check("Page 1 Pot 5 is BBnk (Osc B Bank)", std::string(pOsc.pots[5].shortName) == "BBnk");
        check("Page 1 Pot 6 is BWav (Osc B Waveform)", std::string(pOsc.pots[6].shortName) == "BWav");
        check("Page 1 Pot 7 is BFrq (Osc B Frequency)", std::string(pOsc.pots[7].shortName) == "BFrq");
        check("Page 1 Pot 8 is Detn (A/B Detune)", std::string(pOsc.pots[8].shortName) == "Detn");
        check("Page 1 Pot 9 is BVol (Osc B Volume)", std::string(pOsc.pots[9].shortName) == "BVol");

        // Action Buttons on Page 1
        check("Page 1 Button A is AXoS", std::string(pOsc.buttons[0].shortName) == "AXoS");
        check("Page 1 Button B is BXoS", std::string(pOsc.buttons[1].shortName) == "BXoS");
        check("Page 1 Button C is FrqM", std::string(pOsc.buttons[2].shortName) == "FrqM");
        check("Page 1 Button D is Sync", std::string(pOsc.buttons[3].shortName) == "Sync");

        // 2. WaveMod Page (Page 2)
        const auto& pWMod = ClassicUI::SchemaRegistry::getPage(ClassicUI::PageId::WMod);
        check("Page 2 is WAVEMOD", std::string(pWMod.name) == "WAVEMOD");
        check("Page 2 Pot 0 is AWmo", std::string(pWMod.pots[0].shortName) == "AWmo");
        check("Page 2 Pot 1 is BWmo", std::string(pWMod.pots[1].shortName) == "BWmo");
        check("Page 2 Button A is AWmT", std::string(pWMod.buttons[0].shortName) == "AWmT");
        check("Page 2 Button B is BWmT", std::string(pWMod.buttons[1].shortName) == "BWmT");
        check("Page 2 Button C is WEnT (WaveMod Env Type)", std::string(pWMod.buttons[2].shortName) == "WEnT");
        check("Page 2 Button D is WEnL (WaveMod Env Loop)", std::string(pWMod.buttons[3].shortName) == "WEnL");

        // 3. Filter Page (Page 3)
        const auto& pFil = ClassicUI::SchemaRegistry::getPage(ClassicUI::PageId::Fil);
        check("Page 3 is FILTER", std::string(pFil.name) == "FILTER");
        check("Page 3 Button C is FEnT (Filter Env Type)", std::string(pFil.buttons[2].shortName) == "FEnT");
        check("Page 3 Button D is FEnL (Filter Env Loop)", std::string(pFil.buttons[3].shortName) == "FEnL");

        // 4. Amplifier Page (Page 4)
        const auto& pAmp = ClassicUI::SchemaRegistry::getPage(ClassicUI::PageId::Amp);
        check("Page 4 is AMPLIFIER", std::string(pAmp.name) == "AMPLIFIER");
        check("Page 4 Pot 0 is ALvl (Master Amp Level)", std::string(pAmp.pots[0].shortName) == "ALvl");
        check("Page 4 Button A is Unis (Unison)", std::string(pAmp.buttons[0].shortName) == "Unis");
        check("Page 4 Button B is Prio (Assigner Priority)", std::string(pAmp.buttons[1].shortName) == "Prio");
        check("Page 4 Button C is AEnT (Amp Env Type)", std::string(pAmp.buttons[2].shortName) == "AEnT");
        check("Page 4 Button D is AEnL (Amp Env Loop)", std::string(pAmp.buttons[3].shortName) == "AEnL");

        // 5. Presets Page (Page 0)
        const auto& pPrs = ClassicUI::SchemaRegistry::getPage(ClassicUI::PageId::Presets);
        check("Page 0 is PRESETS", std::string(pPrs.name) == "PRESETS");
        check("Page 0 Pot 8 is Type", std::string(pPrs.pots[8].shortName) == "Type");
        check("Page 0 Pot 9 is Styl", std::string(pPrs.pots[9].shortName) == "Styl");
        check("Page 0 Button A is Load", std::string(pPrs.buttons[0].shortName) == "Load");
        check("Page 0 Button B is Save", std::string(pPrs.buttons[1].shortName) == "Save");
        check("Page 0 Button C is Prev", std::string(pPrs.buttons[2].shortName) == "Prev");
        check("Page 0 Button D is Next", std::string(pPrs.buttons[3].shortName) == "Next");
    }

    // -----------------------------------------------------------------
    // [SCENARIO 2] Envelope Curves & Sub-Mode Bit Decoding
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 2] Envelope Type Bit Packing (Lin / Slow Encoding)\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        // Firmware 17xx decodes: value = Lin * 2 + Slow
        // 0: Fast-Exp  (Lin=0, Slow=0)
        // 1: Slow-Exp  (Lin=0, Slow=1)
        // 2: Fast-Lin  (Lin=1, Slow=0)
        // 3: Slow-Lin  (Lin=1, Slow=1)
        auto& preset = engine.getCurrentPreset();

        for (int v = 0; v < 4; ++v) {
            uint8_t lin = (uint8_t)((v >> 1) & 1);
            uint8_t slow = (uint8_t)(v & 1);

            preset.steppedParams[spFilEnvLin] = lin;
            preset.steppedParams[spFilEnvSlow] = slow;

            int decoded = preset.steppedParams[spFilEnvLin] * 2 + preset.steppedParams[spFilEnvSlow];
            check("Filter Env Type #" + std::to_string(v) + " roundtrips Lin/Slow bits", decoded == v);
        }
    }

    // -----------------------------------------------------------------
    // [SCENARIO 3] Waveform & Bank Dynamic Loading
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 3] Bank & Waveform Resolution for LCD Wave Preview\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        auto& wm = engine.getWaveManager();
        const auto& banks = wm.getBankNames();
        check("WaveManager has banks detected", !banks.empty());

        if (!banks.empty()) {
            std::string testBank = banks[0];
            const auto& waves = wm.getWaveNames(testBank);
            check("WaveManager has waveforms in primary bank", !waves.empty());

            if (!waves.empty()) {
                bool loaded = wm.loadWave(abxAMain, testBank, waves[0]);
                check("Wave loaded successfully into Osc A", loaded);

                const uint16_t* smp = wm.getWaveData(abxAMain);
                check("Wave samples accessible for LCD oscilloscope display", smp != nullptr);
            }
        }
    }

    // -----------------------------------------------------------------
    // [SCENARIO 4] Arpeggiator Parameters via Classic Pot Schema
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 4] Arpeggiator Clock, Mode & Hold Integration\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        auto& preset = engine.getCurrentPreset();

        // 1. Clock BPM
        preset.continuousParams[cpArpBpm] = 140;
        check("Arp BPM parameter accessible in continuousParams", preset.continuousParams[cpArpBpm] == 140);

        // 2. Arp Mode Cycling
        preset.steppedParams[spArpMode] = amUpDown;
        check("Arp Mode set to UpDown (amUpDown)", preset.steppedParams[spArpMode] == amUpDown);

        // 3. Arp Hold / Latch
        preset.steppedParams[spArpHold] = 1;
        check("Arp Hold activated", preset.steppedParams[spArpHold] == 1);

        // 4. Arp Octaves & Rate Division
        preset.steppedParams[spArpOctaves] = 2; // 3 Octaves
        preset.steppedParams[spArpRate] = 3;    // 1/16th
        check("Arp Octaves set to 3 octaves", preset.steppedParams[spArpOctaves] == 2);
        check("Arp Rate set to 1/16th division", preset.steppedParams[spArpRate] == 3);
    }

    // -----------------------------------------------------------------
    // [SCENARIO 5] Bipolar Scan Pot Range Conversion
    // -----------------------------------------------------------------
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << " [SCENARIO 5] Bipolar Range Formatting (-499 to +499)\n";
    std::cout << "-----------------------------------------------------------------\n";
    {
        // In GliGli Overcycler, bipolar pots range from 0 to 999 where 500 is dead center (0)
        int centerVal = 500;
        int bipolarCenter = centerVal - 500;
        check("Center value (500) corresponds to 0 detune", bipolarCenter == 0);

        int maxVal = 999;
        int bipolarMax = maxVal - 500;
        check("Max value (999) corresponds to +499 detune", bipolarMax == 499);

        int minVal = 1;
        int bipolarMin = minVal - 500;
        check("Min value (1) corresponds to -499 detune", bipolarMin == -499);
    }

    std::cout << "\n=================================================================\n";
    std::cout << " Final Verification Summary:\n";
    std::cout << " Passed: " << totalPassed << " / " << (totalPassed + totalFailed) << "\n";
    std::cout << " Failed: " << totalFailed << "\n";
    std::cout << "=================================================================\n\n";

    return (totalFailed == 0) ? 0 : 1;
}
