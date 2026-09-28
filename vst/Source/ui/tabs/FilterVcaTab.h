#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include "../components/ModernTelemetryComponents.h"
#include "../components/FilterCurveComponent.h"
#include <array>

// FILTER / VCA tab. Row 1: filter card and the interactive response curve
// (incl. the 4-band Shelves EQ) with the filter envelope routing. Row 2: the
// amplifier with console/saturation drive, the master mixer/tuning card and
// the voice console mixer (whose master strip carries the Mackity send).
class FilterVcaTab final : public ModernTabModule {
public:
    struct FilterModeOptions {
        std::array<juce::String, 4> labels;
        int visibleCount = 1;
        int selectedIndex = 0;
    };
    enum class EqThirdControl { Percent, Q };
    struct EqBandBinding {
        continuousParameter_t frequency;
        continuousParameter_t gain;
        continuousParameter_t third;
        EqThirdControl thirdControl;
        juce::String frequencyLabel, gainLabel, thirdLabel;
    };

    explicit FilterVcaTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

    ModernVoiceMeterPanel* getVoiceMeterPanel() noexcept { return voiceMeterPanel.get(); }

    // Filter UI state: which model/mode/EQ band the controls currently edit.
    int getCurrentEQBand() const noexcept { return currentEQBand; }
    void setCurrentEQBand(int band) noexcept { currentEQBand = juce::jlimit(0, 3, band); }
    int getSelectedFilterModel() const noexcept { return selectedFilterModel; }
    int getSelectedFilterMode() const noexcept { return selectedFilterMode; }
    void setSelectedFilterModel(int value) noexcept { selectedFilterModel = juce::jlimit(0, 3, value); }
    void setSelectedFilterMode(int value) noexcept { selectedFilterMode = juce::jlimit(0, 3, value); }
    FilterModeOptions getFilterModeOptions() const;
    bool isShelvesEqActive() const noexcept { return selectedFilterModel == 2 && selectedFilterMode == 0; }
    EqBandBinding getActiveEqBandBinding() const;

private:
    void assignComponentIDs();
    void selectEQBand(int band);
    void applyEQBandSelection(int band);
    void updateEQKnobsForCurrentBand();
    void updateFilterModeToggles(int model);
    void updateFilterUIState(int model, int mode);

    ModernSectionCard filterCard{"FILTER", "VCF"};
    ModernSectionCard vcaCard{"AMPLIFIER", "AMP"};
    ModernSectionCard mixerCard{"TUNING & UNISON", "GLOBAL"};

    // Filter
    std::unique_ptr<juce::ToggleButton> filterModelToggles[4];
    std::unique_ptr<juce::ToggleButton> filterModeToggles[4];
    juce::TextButton eqBandButtons[4];
    std::unique_ptr<juce::Slider> cutoffKnob, resoKnob, filKbdKnob, filEnvAmtKnob;
    std::unique_ptr<juce::ToggleButton> filEnvTypeToggles[4];
    std::unique_ptr<juce::ToggleButton> filEnvLoopToggle;
    std::unique_ptr<juce::Label> cutoffLabel, resoLabel, filKbdLabel, filEnvAmtLabel;

    // Amplifier
    std::unique_ptr<juce::Slider> ampLevelKnob, glideKnob;
    std::unique_ptr<juce::ToggleButton> unisonToggle;
    std::unique_ptr<juce::Label> ampLevelLabel, glideLabel;

    // Console & saturation: ConsoleX drive and air, Mackity send drive
    std::unique_ptr<juce::Slider> consoleDriveKnob, consoleDiscontinuityKnob, mackityDriveKnob;
    std::unique_ptr<juce::Label> consoleDriveLabel, consoleDiscontinuityLabel, mackityDriveLabel;

    // Multi-Channel is the default; AFX (sound per key) is an option.
    std::unique_ptr<juce::ToggleButton> afxModeToggle;

    // Master mixer & tuning
    std::unique_ptr<juce::Slider> noiseVolKnob, masterTuneKnob, unisonDetuneKnob;
    std::unique_ptr<juce::ToggleButton> chromaticPitchToggles[3];
    std::unique_ptr<juce::Label> noiseVolLabel, masterTuneLabel, unisonDetuneLabel;

    // Visualisers. The curve references cutoffKnob/resoKnob, so it is
    // declared after them and destroyed first.
    std::unique_ptr<FilterCurveComponent> filterCurve;
    std::unique_ptr<ModernVoiceMeterPanel> voiceMeterPanel;

    int currentEQBand = 1;
    int selectedFilterModel = 0;
    int selectedFilterMode = 0;
    int laidOutFilterModel = -1; // model/mode the card layout was last built for
    int laidOutFilterMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterVcaTab)
};
