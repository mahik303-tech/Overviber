#pragma once

#include "../dsp/OvercyclerTypes.h"
#include "PresetData.h"
#include <string>
#include <vector>
#include <map>

class PresetManager {
public:
    PresetManager();
    ~PresetManager() = default;

    void setBaseDirectory(const std::string& path);
    const std::string& getBaseDirectory() const { return basePath; }

    void scanPresets();
    int getPresetCount() const { return (int)presetFiles.size(); }
    std::string getPresetName(int index) const;

    bool loadPreset(int index, PresetData& outPreset);
    bool loadPresetFile(const std::string& filePath, PresetData& outPreset);
    bool parsePresetString(const std::string& content, PresetData& outPreset);

    bool savePresetFile(const std::string& filePath, const PresetData& preset);
    std::string serializePresetToString(const PresetData& preset);

    bool savePreset(int index, const PresetData& preset);
    bool saveNewPreset(const std::string& name, PresetData preset);
    bool deletePreset(int index);
    int getPresetNumber(int index) const;
    const std::string& getPresetFilePath(int index) const;

    static const char* getContinuousParamName(continuousParameter_t cp);
    static const char* getContinuousParamDisplayName(continuousParameter_t cp);
    static bool isContinuousParamZeroCentered(continuousParameter_t cp);
    static const char* getSteppedParamName(steppedParameter_t sp);
    static const char* getSteppedParamDisplayName(steppedParameter_t sp);

    static const char* getModSourceName(modSource_t src);
    static const char* getModSourceDisplayName(modSource_t src);
    static const char* getModDestName(modDest_t dest);
    static const char* getModDestDisplayName(modDest_t dest);

private:
    std::string basePath;
    std::vector<std::pair<int, std::string>> presetFiles; // number, full path
    std::vector<std::string> presetNames;
};
