#pragma once

#include "../dsp/OvercyclerTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

class WaveManager {
public:
    enum class StandardShape {
        Sine,
        Triangle,
        Sawtooth,
        Square,
        Pulse25,
        Pulse10,
        HalfSine,
        Harmonics1to8,
        WhiteNoise,
        Parabolic
    };

    WaveManager();
    ~WaveManager() = default;

    void setBaseDirectory(const std::string& path);
    const std::string& getBaseDirectory() const { return basePath; }

    void scanDirectory();

    const std::vector<std::string>& getBankNames() const { return bankNames; }
    std::vector<std::string> getWaveNames(const std::string& bank) const;

    bool loadWave(abx_t abx, const std::string& bank, const std::string& waveName, int frameIdx = 0);
    bool loadWaveFromFile(abx_t abx, const std::string& absoluteFilePath, int frameIdx = 0);
    bool saveWavFile(const std::string& filePath, const uint16_t* samples, int sampleCount = WTOSC_SAMPLE_COUNT);
    bool saveUserWave(abx_t abx, const std::string& waveName);

    bool readWavFile(const std::string& filePath, std::vector<uint16_t>& outSamples, int frameIdx = 0, int* outTotalFrames = nullptr);

    const uint16_t* getWaveData(abx_t abx) const;
    uint16_t* getMutableWaveData(abx_t abx) {
        if (abx < 0 || abx >= abxCount) return nullptr;
        return sampleData[abx];
    }

    const std::string& getCurrentBank(abx_t abx) const { return currentBank[abx]; }
    const std::string& getCurrentWave(abx_t abx) const { return currentWave[abx]; }
    int getCurrentFrame(abx_t abx) const { return currentFrame[abx]; }
    int getTotalFrames(abx_t abx) const { return totalFrames[abx]; }
    void setCurrentFrame(abx_t abx, int frameIdx);

    void generateStandardShape(abx_t abx, StandardShape shape);

private:
    void generateFallbackWaves();

    std::string basePath;
    std::vector<std::string> bankNames;
    std::map<std::string, std::vector<std::string>> bankWaves;
    std::map<std::string, std::string> bankFullPaths;

    std::string currentBank[abxCount];
    std::string currentWave[abxCount];
    int currentFrame[abxCount] = { 0, 0, 0, 0 };
    int totalFrames[abxCount] = { 1, 1, 1, 1 };

    // 2400 samples per slot
    uint16_t sampleData[abxCount][WTOSC_SAMPLE_COUNT];

    // Fallback basic shapes
    std::vector<uint16_t> fallbackSaw;
    std::vector<uint16_t> fallbackSin;
    std::vector<uint16_t> fallbackSqu;
    std::vector<uint16_t> fallbackTri;
};
