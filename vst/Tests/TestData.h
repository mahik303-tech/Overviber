#pragma once

#include "dsp/SynthEngine.h"
#include <filesystem>
#include <iostream>

// Each executable receives this checkout's data path from CMake. An explicit
// argument can override it for isolated fixtures and missing-data checks.
inline bool initializeTestData(SynthEngine& engine, int argc, char* argv[]) {
    const std::filesystem::path root = argc > 1 ? argv[1] : OVERVIBER_TEST_DATA_DIR;
    if (!std::filesystem::is_directory(root / "WAVEDATA") ||
        !std::filesystem::is_directory(root / "PRESETS")) {
        std::cerr << "Missing test data: " << root << '\n';
        return false;
    }
    engine.getWaveManager().setBaseDirectory((root / "WAVEDATA").string());
    engine.getPresetManager().setBaseDirectory((root / "PRESETS").string());
    if (engine.getPresetManager().getPresetCount() == 0) {
        std::cerr << "No presets in test data: " << root << '\n';
        return false;
    }
    std::cout << "[INIT] Test data: " << root << '\n';
    return true;
}
