#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ClassicParamSchema.h"
#include <string>
#include <vector>
#include <array>

class LcdDisplay : public juce::Component, private juce::Timer {
public:
    enum class DisplayMode {
        Normal,
        PotEdit,
        WavePreview,
        ButtonEdit,
        Help,
        NumericInput
    };

    LcdDisplay();
    ~LcdDisplay() override;

    void setPage(ClassicUI::PageId page);
    ClassicUI::PageId getPage() const { return currentPage; }

    // Parameter updates
    void setPotParam(int index, const std::string& name, const std::string& valueStr, float rawNormalized);
    void setButtonParam(int index, const std::string& name, const std::string& valueStr);
    void setPresetInfo(const std::string& name, int number, bool isModified);
    void setVoiceActivity(int voiceIdx, bool active, float level);

    // Fullscreen interactive overlays
    void showPotEdit(int knobIndex, const std::string& longName, const std::string& valueStr,
                     float currentVal, float minVal, float maxVal, bool showLeftArrow = false, bool showRightArrow = false);

    void showWaveformPreview(abx_t osc, const std::string& bankName, const std::string& waveName,
                             const uint16_t* waveSamples, int sampleCount);

    void showButtonEdit(int buttonIndex, const std::string& longName, const std::string& activeValue,
                        const std::vector<std::string>& allOptions, int selectedOptionIndex);

    void showNumericInput(const std::string& prompt, const std::string& enteredDigits);
    void showHelpScreen();
    void resetToNormalScreen();

    void setContrast(float contrast01);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    ClassicUI::PageId currentPage{ ClassicUI::PageId::Osc };
    DisplayMode currentMode{ DisplayMode::Normal };

    // Preset & Voice State
    std::string presetName{ "Init Patch" };
    int presetNumber{ 0 };
    bool presetModified{ false };
    float contrast{ 0.75f };

    bool voiceActive[SYNTH_VOICE_COUNT]{ false };
    float voiceLevel[SYNTH_VOICE_COUNT]{ 0.0f };

    // Normal Screen Data
    struct PotDisplayData {
        std::string name{ "----" };
        std::string value{ "   0" };
        float normalized{ 0.0f };
    };
    std::array<PotDisplayData, 10> potData;

    struct ButtonDisplayData {
        std::string name{ "----" };
        std::string value{ " Off" };
    };
    std::array<ButtonDisplayData, 4> buttonData;

    // Fullscreen Overlay State
    struct PotEditState {
        int knobIndex{ 0 };
        std::string longName;
        std::string valueStr;
        float currentVal{ 0.0f };
        float minVal{ 0.0f };
        float maxVal{ 999.0f };
        bool leftArrow{ false };
        bool rightArrow{ false };
    } potEditState;

    struct WavePreviewState {
        abx_t osc{ abxAMain };
        std::string bankName;
        std::string waveName;
        std::vector<uint16_t> samples;
    } wavePreviewState;

    struct ButtonEditState {
        int buttonIndex{ 0 };
        std::string longName;
        std::string activeValue;
        std::vector<std::string> options;
        int selectedIndex{ 0 };
    } buttonEditState;

    struct NumericInputState {
        std::string prompt;
        std::string digits;
    } numericInputState;

    int overlayHoldTicks{ 0 };

    // Rendering Helpers
    void renderMatrixText(juce::Graphics& g, const juce::Rectangle<float>& screenArea);
    void renderPotEditScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea);
    void renderWavePreviewScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea);
    void renderButtonEditScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea);
    void renderHelpScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea);
    void renderNumericInputScreen(juce::Graphics& g, const juce::Rectangle<float>& screenArea);
    void renderVoiceMonitors(juce::Graphics& g, float x, float y, float w, float h);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LcdDisplay)
};
