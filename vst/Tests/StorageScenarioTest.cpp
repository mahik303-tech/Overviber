#include "data/OverviberPaths.h"
#include <iostream>
#include <iomanip>
#include <string>

static bool runTest(const std::string& testName, bool condition) {
    std::cout << "  " << std::left << std::setw(55) << testName << ": "
              << (condition ? "[PASS]" : "[FAIL]") << "\n";
    return condition;
}

int main() {
    std::cout << "=================================================================\n";
    std::cout << " Overviber - Cross-Platform Storage & Directory Test Harness\n";
    std::cout << "=================================================================\n\n";

    int totalPassed = 0;
    int totalFailed = 0;

    auto check = [&](const std::string& name, bool cond) {
        if (runTest(name, cond)) totalPassed++;
        else totalFailed++;
    };

    // 1. Path Resolution Tests
    std::cout << "--- 1. Path Resolution Checks ---\n";
    auto configDir = OverviberPaths::getAppConfigDirectory();
    std::cout << "    App Config Dir: " << configDir.getFullPathName() << "\n";
    check("App Config Directory is not empty", configDir.getFullPathName().isNotEmpty());
    check("App Config Directory ends with 'Overviber'", configDir.getFileName() == "Overviber");

    auto docDir = OverviberPaths::getDocumentsDirectory();
    std::cout << "    Documents Dir:  " << docDir.getFullPathName() << "\n";
    check("Documents Directory is not empty", docDir.getFullPathName().isNotEmpty());
    check("Documents Directory ends with 'Overviber'", docDir.getFileName() == "Overviber");

    auto presetsDir = OverviberPaths::getPresetsDirectory();
    std::cout << "    Presets Dir:    " << presetsDir.getFullPathName() << "\n";
    check("Presets Directory is under Documents", presetsDir.getParentDirectory() == docDir);
    check("Presets Directory name is 'PRESETS'", presetsDir.getFileName() == "PRESETS");

    auto waveDir = OverviberPaths::getWaveDataDirectory();
    std::cout << "    WaveData Dir:   " << waveDir.getFullPathName() << "\n";
    check("WaveData Directory is under Documents", waveDir.getParentDirectory() == docDir);
    check("WaveData Directory name is 'WAVEDATA'", waveDir.getFileName() == "WAVEDATA");

    auto userWaveDir = OverviberPaths::getUserWaveDirectory();
    std::cout << "    User Wave Dir:  " << userWaveDir.getFullPathName() << "\n";
    check("User Wave Directory is under WAVEDATA", userWaveDir.getParentDirectory() == waveDir);
    check("User Wave Directory name is 'User'", userWaveDir.getFileName() == "User");

    // 2. Factory Disk Probing
    std::cout << "\n--- 2. Factory Disk Probing ---\n";
    auto factoryDisk = juce::File(OVERVIBER_TEST_DATA_DIR);
    std::cout << "    Factory Disk:   " << factoryDisk.getFullPathName() << "\n";
    check("Factory Disk Directory discovered", factoryDisk.exists());
    if (factoryDisk.exists()) {
        check("Factory Disk has PRESETS directory", factoryDisk.getChildFile("PRESETS").isDirectory());
        check("Factory Disk has WAVEDATA directory", factoryDisk.getChildFile("WAVEDATA").isDirectory());
    }

    // 3. Storage Initialization and Auto-Seeding
    std::cout << "\n--- 3. Storage Initialization & Auto-Seeding ---\n";
    // All writes stay in a unique child of CTest's build-local working directory.
    const auto sandbox = juce::File::getCurrentWorkingDirectory()
        .getChildFile("storage-" + juce::Uuid().toString());
    struct Cleanup {
        juce::File directory;
        ~Cleanup() { directory.deleteRecursively(); }
    } cleanup{sandbox};
    configDir = sandbox.getChildFile("config");
    docDir = sandbox.getChildFile("documents").getChildFile("Overviber");
    presetsDir = docDir.getChildFile("PRESETS");
    waveDir = docDir.getChildFile("WAVEDATA");
    userWaveDir = waveDir.getChildFile("User");
    OverviberPaths::initializeStorage(configDir, docDir, factoryDisk);

    check("Config Directory created on disk", configDir.isDirectory());
    check("Documents Overviber Directory created on disk", docDir.isDirectory());
    check("Presets Directory created on disk", presetsDir.isDirectory());
    check("WaveData Directory created on disk", waveDir.isDirectory());
    check("User Wave Directory created on disk", userWaveDir.isDirectory());

    auto presets = presetsDir.findChildFiles(juce::File::findFiles, false, "*.conf");
    std::cout << "    Presets found in Documents: " << presets.size() << "\n";
    check("Presets seeded into Documents/Overviber/PRESETS", presets.size() > 0);

    auto waveSubDirs = waveDir.findChildFiles(juce::File::findDirectories, false);
    std::cout << "    Wave subdirectories in Documents: " << waveSubDirs.size() << "\n";
    check("Wavetable banks seeded into Documents/Overviber/WAVEDATA", waveSubDirs.size() > 1);

    // 4. Repeat Initialization Idempotency
    std::cout << "\n--- 4. Idempotency Check ---\n";
    OverviberPaths::initializeStorage(configDir, docDir, factoryDisk);
    auto presetsAfterRepeat = presetsDir.findChildFiles(juce::File::findFiles, false, "*.conf");
    check("Idempotent initialization preserves preset count", presetsAfterRepeat.size() == presets.size());

    // Summary
    std::cout << "\n=================================================================\n";
    std::cout << " Test Summary: " << totalPassed << " passed, " << totalFailed << " failed.\n";
    std::cout << "=================================================================\n";

    return (totalFailed == 0) ? 0 : 1;
}
