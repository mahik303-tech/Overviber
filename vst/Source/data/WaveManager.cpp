#include "WaveManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstdlib>

namespace fs = std::filesystem;

WaveManager::WaveManager() {
    generateFallbackWaves();

    for (int i = 0; i < abxCount; ++i) {
        currentBank[i] = "_basic";
        currentFrame[i] = 0;
        totalFrames[i] = 1;
        if (i == abxAMain || i == abxBMain) {
            currentWave[i] = "saw.wav";
            std::memcpy(sampleData[i], fallbackSaw.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
        } else {
            currentWave[i] = "sin.wav";
            std::memcpy(sampleData[i], fallbackSin.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
        }
    }
}

void WaveManager::generateFallbackWaves() {
    fallbackSaw.resize(WTOSC_SAMPLE_COUNT);
    fallbackSin.resize(WTOSC_SAMPLE_COUNT);
    fallbackSqu.resize(WTOSC_SAMPLE_COUNT);
    fallbackTri.resize(WTOSC_SAMPLE_COUNT);

    const float guard = (float)WTOSC_SAMPLES_GUARD_BAND;
    const float range = (float)(UINT16_MAX - 2 * WTOSC_SAMPLES_GUARD_BAND);

    for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
        float phase = (float)i / (float)WTOSC_SAMPLE_COUNT;

        // Saw: 1.0 down to -1.0
        float saw = 1.0f - 2.0f * phase;
        fallbackSaw[i] = (uint16_t)(guard + (saw * 0.5f + 0.5f) * range);

        // Sin
        float sinVal = std::sin(2.0f * 3.14159265358979323846f * phase);
        fallbackSin[i] = (uint16_t)(guard + (sinVal * 0.5f + 0.5f) * range);

        // Square
        float squ = (phase < 0.5f) ? 1.0f : -1.0f;
        fallbackSqu[i] = (uint16_t)(guard + (squ * 0.5f + 0.5f) * range);

        // Triangle
        float tri = (phase < 0.25f) ? (4.0f * phase) :
                    (phase < 0.75f) ? (2.0f - 4.0f * phase) :
                    (-4.0f + 4.0f * phase);
        fallbackTri[i] = (uint16_t)(guard + (tri * 0.5f + 0.5f) * range);
    }
}

void WaveManager::generateStandardShape(abx_t abx, StandardShape shape) {
    if (abx < 0 || abx >= abxCount) return;

    const float guard = (float)WTOSC_SAMPLES_GUARD_BAND;
    const float range = (float)(UINT16_MAX - 2 * WTOSC_SAMPLES_GUARD_BAND);
    const float twoPi = 2.0f * 3.14159265358979323846f;

    std::string name = "Sine";
    switch (shape) {
        case StandardShape::Sine: name = "Sine"; break;
        case StandardShape::Triangle: name = "Triangle"; break;
        case StandardShape::Sawtooth: name = "Sawtooth"; break;
        case StandardShape::Square: name = "Square"; break;
        case StandardShape::Pulse25: name = "Pulse 25%"; break;
        case StandardShape::Pulse10: name = "Pulse 10%"; break;
        case StandardShape::HalfSine: name = "Half Sine"; break;
        case StandardShape::Harmonics1to8: name = "Harmonics 1-8"; break;
        case StandardShape::WhiteNoise: name = "Noise"; break;
        case StandardShape::Parabolic: name = "Parabolic"; break;
    }

    currentBank[abx] = "_shapes";
    currentWave[abx] = name;
    currentFrame[abx] = 0;
    totalFrames[abx] = 1;

    for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
        float phase = (float)i / (float)WTOSC_SAMPLE_COUNT;
        float v = 0.0f;

        switch (shape) {
            case StandardShape::Sine:
                v = std::sin(twoPi * phase);
                break;
            case StandardShape::Triangle:
                v = (phase < 0.25f) ? (4.0f * phase) :
                    (phase < 0.75f) ? (2.0f - 4.0f * phase) :
                    (-4.0f + 4.0f * phase);
                break;
            case StandardShape::Sawtooth:
                v = 1.0f - 2.0f * phase;
                break;
            case StandardShape::Square:
                v = (phase < 0.5f) ? 1.0f : -1.0f;
                break;
            case StandardShape::Pulse25:
                v = (phase < 0.25f) ? 1.0f : -1.0f;
                break;
            case StandardShape::Pulse10:
                v = (phase < 0.10f) ? 1.0f : -1.0f;
                break;
            case StandardShape::HalfSine:
                v = (phase < 0.5f) ? std::sin(twoPi * phase) : 0.0f;
                break;
            case StandardShape::Harmonics1to8: {
                float sum = 0.0f;
                for (int k = 1; k <= 8; ++k) {
                    sum += (1.0f / (float)k) * std::sin(twoPi * (float)k * phase);
                }
                v = sum * 0.55f;
                break;
            }
            case StandardShape::WhiteNoise:
                v = ((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
                break;
            case StandardShape::Parabolic: {
                float p = phase * 2.0f - 1.0f;
                v = 1.0f - 2.0f * (p * p);
                break;
            }
        }

        v = std::clamp(v, -1.0f, 1.0f);
        sampleData[abx][i] = (uint16_t)(guard + (v * 0.5f + 0.5f) * range);
    }
}

void WaveManager::setBaseDirectory(const std::string& path) {
    basePath = path;
    scanDirectory();
}

void WaveManager::scanDirectory() {
    bankNames.clear();
    bankWaves.clear();
    bankFullPaths.clear();

    if (basePath.empty() || !fs::exists(basePath)) {
        return;
    }

    try {
        // Ensure User folder exists
        fs::path userDir = fs::path(basePath) / "User";
        if (!fs::exists(userDir)) {
            fs::create_directories(userDir);
        }

        for (const auto& entry : fs::directory_iterator(basePath)) {
            if (entry.is_directory()) {
                std::string bName = entry.path().filename().string();
                bankNames.push_back(bName);
                bankFullPaths[bName] = entry.path().string();

                std::vector<std::string> waves;
                for (const auto& file : fs::directory_iterator(entry.path())) {
                    if (file.is_regular_file()) {
                        std::string ext = file.path().extension().string();
                        for (auto& c : ext) c = (char)std::tolower(c);
                        if (ext == ".wav") {
                            waves.push_back(file.path().filename().string());
                        }
                    }
                }
                std::sort(waves.begin(), waves.end());
                bankWaves[bName] = waves;
            }
        }
        std::sort(bankNames.begin(), bankNames.end());
    } catch (...) {}
}

std::vector<std::string> WaveManager::getWaveNames(const std::string& bank) const {
    auto it = bankWaves.find(bank);
    if (it != bankWaves.end()) return it->second;
    return {};
}

bool WaveManager::readWavFile(const std::string& filePath, std::vector<uint16_t>& outSamples, int frameIdx, int* outTotalFrames) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    char header[12];
    if (!file.read(header, 12)) return false;

    if (std::strncmp(header, "RIFF", 4) != 0 || std::strncmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = 44100;
    uint16_t bitsPerSample = 16;
    uint32_t dataBytes = 0;
    std::streampos dataPos = 0;

    while (file.good()) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (!file.read(chunkId, 4)) break;
        if (!file.read(reinterpret_cast<char*>(&chunkSize), 4)) break;

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            file.read(reinterpret_cast<char*>(&numChannels), 2);
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            uint32_t byteRate = 0;
            file.read(reinterpret_cast<char*>(&byteRate), 4);
            uint16_t blockAlign = 0;
            file.read(reinterpret_cast<char*>(&blockAlign), 2);
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);

            if (chunkSize > 16) {
                file.seekg((int)chunkSize - 16, std::ios::cur);
            }
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            dataBytes = chunkSize;
            dataPos = file.tellg();
            break;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (dataBytes <= 0 || dataPos == std::streampos(0) || numChannels == 0 || bitsPerSample == 0) {
        return false;
    }

    int bytesPerSample = bitsPerSample / 8;
    int totalSamples = (int)(dataBytes / (numChannels * bytesPerSample));
    if (totalSamples <= 0) return false;

    file.seekg(dataPos);
    std::vector<float> allFloats(totalSamples);

    for (int i = 0; i < totalSamples; ++i) {
        float sampleVal = 0.0f;
        if (audioFormat == 3) { // 32-bit IEEE float format
            float f = 0.0f;
            file.read(reinterpret_cast<char*>(&f), sizeof(float));
            sampleVal = f;
            if (numChannels > 1) file.seekg((numChannels - 1) * sizeof(float), std::ios::cur);
        } else if (bitsPerSample == 16) {
            int16_t s = 0;
            file.read(reinterpret_cast<char*>(&s), 2);
            sampleVal = (float)s / 32768.0f;
            if (numChannels > 1) file.seekg((numChannels - 1) * 2, std::ios::cur);
        } else if (bitsPerSample == 24) {
            uint8_t b[3];
            file.read(reinterpret_cast<char*>(b), 3);
            int32_t s24 = (int32_t)((b[0]) | (b[1] << 8) | (b[2] << 16));
            if (s24 & 0x800000) s24 |= ~0xFFFFFF; // Sign extend
            sampleVal = (float)s24 / 8388608.0f;
            if (numChannels > 1) file.seekg((numChannels - 1) * 3, std::ios::cur);
        } else if (bitsPerSample == 32) {
            int32_t s32 = 0;
            file.read(reinterpret_cast<char*>(&s32), 4);
            sampleVal = (float)s32 / 2147483648.0f;
            if (numChannels > 1) file.seekg((numChannels - 1) * 4, std::ios::cur);
        } else if (bitsPerSample == 8) {
            uint8_t s8 = 0;
            file.read(reinterpret_cast<char*>(&s8), 1);
            sampleVal = ((float)s8 - 128.0f) / 128.0f;
            if (numChannels > 1) file.seekg((numChannels - 1), std::ios::cur);
        }
        allFloats[i] = sampleVal;
    }

    // Determine Wavetable Frame Structure
    int frameSize = 2400;
    if (totalSamples == WTOSC_SAMPLE_COUNT) {
        frameSize = WTOSC_SAMPLE_COUNT;
    } else if (totalSamples >= 2048 && (totalSamples % 2048 == 0)) {
        frameSize = 2048; // Standard 2048-sample frame size
    } else if (totalSamples >= 2400 && (totalSamples % 2400 == 0)) {
        frameSize = 2400;
    } else if (totalSamples >= 1024 && (totalSamples % 1024 == 0)) {
        frameSize = 1024;
    } else if (totalSamples >= 4096 && (totalSamples % 4096 == 0)) {
        frameSize = 4096;
    } else if (totalSamples >= 600 && (totalSamples % 600 == 0)) {
        frameSize = 600; // AKWF frame size
    } else if (totalSamples > 2048) {
        frameSize = 2048;
    } else {
        frameSize = totalSamples;
    }

    int numFrames = std::max(1, totalSamples / frameSize);
    if (outTotalFrames != nullptr) {
        *outTotalFrames = numFrames;
    }

    int targetFrame = std::clamp(frameIdx, 0, numFrames - 1);
    int startOffset = targetFrame * frameSize;
    int actualFrameSize = std::min(frameSize, totalSamples - startOffset);

    // Convert extracted frame to Overcycler guarded unsigned 16-bit format
    std::vector<uint16_t> frameSamples(actualFrameSize);
    for (int i = 0; i < actualFrameSize; ++i) {
        float s = std::clamp(allFloats[startOffset + i], -1.0f, 1.0f);
        int32_t d = (int32_t)(s * (INT16_MAX - WTOSC_SAMPLES_GUARD_BAND));
        d -= INT16_MIN;
        frameSamples[i] = (uint16_t)__USAT(d, 16);
    }

    // Resample to 2400 samples
    outSamples.resize(WTOSC_SAMPLE_COUNT);
    resampleWave(frameSamples.data(), outSamples.data(), (uint16_t)actualFrameSize, WTOSC_SAMPLE_COUNT);

    return true;
}

bool WaveManager::loadWave(abx_t abx, const std::string& bank, const std::string& waveName, int frameIdx) {
    if (abx < 0 || abx >= abxCount) return false;

    currentBank[abx] = bank;
    currentWave[abx] = waveName;

    // Check full path map or basePath
    std::string bankPath;
    auto it = bankFullPaths.find(bank);
    if (it != bankFullPaths.end()) {
        bankPath = it->second;
    } else {
        bankPath = (fs::path(basePath) / bank).string();
    }

    fs::path p = fs::path(bankPath) / waveName;
    std::vector<uint16_t> loaded;
    int totFrames = 1;
    if (fs::exists(p) && readWavFile(p.string(), loaded, frameIdx, &totFrames)) {
        std::memcpy(sampleData[abx], loaded.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
        currentFrame[abx] = frameIdx;
        totalFrames[abx] = totFrames;
        return true;
    }

    // Fallback: check waveName keywords
    totalFrames[abx] = 1;
    currentFrame[abx] = 0;
    if (waveName.find("saw") != std::string::npos) {
        std::memcpy(sampleData[abx], fallbackSaw.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
    } else if (waveName.find("sin") != std::string::npos) {
        std::memcpy(sampleData[abx], fallbackSin.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
    } else if (waveName.find("squ") != std::string::npos) {
        std::memcpy(sampleData[abx], fallbackSqu.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
    } else if (waveName.find("tri") != std::string::npos) {
        std::memcpy(sampleData[abx], fallbackTri.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
    } else {
        std::memcpy(sampleData[abx], fallbackSaw.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
    }

    return true;
}

bool WaveManager::loadWaveFromFile(abx_t abx, const std::string& absoluteFilePath, int frameIdx) {
    if (abx < 0 || abx >= abxCount) return false;

    std::vector<uint16_t> loaded;
    int totFrames = 1;
    if (readWavFile(absoluteFilePath, loaded, frameIdx, &totFrames)) {
        std::memcpy(sampleData[abx], loaded.data(), WTOSC_SAMPLE_COUNT * sizeof(uint16_t));
        currentBank[abx] = "Custom";
        currentWave[abx] = fs::path(absoluteFilePath).filename().string();
        currentFrame[abx] = frameIdx;
        totalFrames[abx] = totFrames;
        return true;
    }
    return false;
}

void WaveManager::setCurrentFrame(abx_t abx, int frameIdx) {
    if (abx < 0 || abx >= abxCount) return;
    loadWave(abx, currentBank[abx], currentWave[abx], frameIdx);
}

bool WaveManager::saveWavFile(const std::string& filePath, const uint16_t* samples, int sampleCount) {
    if (!samples || sampleCount <= 0) return false;

    try {
        fs::path p(filePath);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
    } catch (...) {}

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    uint32_t sampleRate = 48000;
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    uint32_t dataBytes = (uint32_t)(sampleCount * blockAlign);
    uint32_t chunkSize = 36 + dataBytes;

    // RIFF header
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);

    // fmt chunk
    file.write("fmt ", 4);
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataBytes), 4);

    for (int i = 0; i < sampleCount; ++i) {
        float norm = ((float)samples[i] - 32768.0f) / 28167.0f;
        norm = std::clamp(norm, -1.0f, 1.0f);
        int16_t s16 = (int16_t)std::clamp((int)(norm * 32767.0f), -32768, 32767);
        file.write(reinterpret_cast<const char*>(&s16), sizeof(int16_t));
    }

    return true;
}

bool WaveManager::saveUserWave(abx_t abx, const std::string& waveName) {
    if (abx < 0 || abx >= abxCount) return false;
    if (basePath.empty()) return false;

    std::string safeName = waveName;
    if (safeName.size() < 4 || safeName.substr(safeName.size() - 4) != ".wav") {
        safeName += ".wav";
    }

    fs::path userDir = fs::path(basePath) / "User";
    fs::path outPath = userDir / safeName;

    if (saveWavFile(outPath.string(), sampleData[abx], WTOSC_SAMPLE_COUNT)) {
        scanDirectory();
        currentBank[abx] = "User";
        currentWave[abx] = safeName;
        currentFrame[abx] = 0;
        totalFrames[abx] = 1;
        return true;
    }
    return false;
}

const uint16_t* WaveManager::getWaveData(abx_t abx) const {
    if (abx < 0 || abx >= abxCount) return nullptr;
    return sampleData[abx];
}
