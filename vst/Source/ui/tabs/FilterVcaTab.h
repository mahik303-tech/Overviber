#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include "../components/ModernTelemetryComponents.h"
#include "../components/FilterCurveComponent.h"
#include <array>
#include <vector>

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
    enum class EqThirdControl { None, Q };   // the shelves have no Q
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
    // The UI groups the engine's filters in four families (LADDER, RIPPLES,
    // SEM, SHELVES); each entry stands for a model / SEM variant / mode.
    struct FilterChoice { uint8_t model, variant, mode; const char* label; };
    static constexpr int kFilterFamilyCount = 4;
    static const std::vector<FilterChoice>& filterChoices(int family);
    static int filterFamilyOf(int model, int semVariant);
    static int filterEntryOf(int family, int model, int mode);
    int getSelectedFilterFamily() const noexcept { return selectedFilterFamily; }
    int getSelectedFilterEntry() const noexcept { return selectedFilterEntry; }
    FilterModeOptions getFilterModeOptions() const;
    bool isShelvesEqActive() const noexcept { return selectedFilterModel == fmEQ && selectedFilterMode == 0; }
    EqBandBinding getActiveEqBandBinding() const;
    static const std::array<EqBandBinding, 4>& eqBands();

    // Settings page: preselect the same filter (true) or the family's last
    // choice (false) when switching filter families.
    void setMatchFilterOnFamilySwitch(bool match) noexcept { matchFilterOnFamilySwitch = match; }

private:
    void assignComponentIDs();
    void createFilterControls();
    void createFilterCurve();
    void createAmplifierControls();
    void createMixerControls();
    void bindPotKnob(juce::Slider& knob, continuousParameter_t cp, int offset, bool repaintCurve);
    void syncPotKnob(juce::Slider& knob, continuousParameter_t cp, int offset);
    static void formatEqFrequency(juce::Slider& s);
    static void formatEqGain(juce::Slider& s);
    static void formatEqQ(juce::Slider& s);
    static void formatFilterCutoff(juce::Slider& s);
    static void formatPercent(juce::Slider& s);
    static void formatBipolarPercent(juce::Slider& s);
    void selectEQBand(int band);
    void applyEQBandSelection(int band);
    void updateEQKnobsForCurrentBand();
    void updateFilterModeToggles();
    void selectFilterChoice(int family, int entry);
    void updateFilterUIState();

    ModernSectionCard filterCard{"FILTER", "VCF"};
    ModernSectionCard vcaCard{"AMPLIFIER", "AMP"};
    ModernSectionCard mixerCard{"TUNING & VOICES", "GLOBAL"};

    // Filter
    std::unique_ptr<juce::ToggleButton> filterModelToggles[4];
    std::unique_ptr<juce::ToggleButton> filterModeToggles[4];
    juce::TextButton eqBandButtons[4];
    std::unique_ptr<juce::Slider> cutoffKnob, resoKnob, filKbdKnob, filEnvAmtKnob;
    // Shelves EQ: Q of the selected mid band (third knob); KEY TRACK then
    // sits above ENV DEPTH, as it tracks the mid low band for all bands.
    std::unique_ptr<juce::Slider> eqQKnob;
    std::unique_ptr<juce::Label> eqQLabel;
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

    // Voices: how many play (1 .. 6) and which note a full voice set keeps.
    std::unique_ptr<juce::Slider> voiceCountSlider;
    std::unique_ptr<juce::ToggleButton> assignerPrioToggles[3];

    // Master mixer & tuning
    std::unique_ptr<juce::Slider> noiseVolKnob, masterTuneKnob, unisonDetuneKnob;
    std::unique_ptr<juce::ToggleButton> chromaticPitchToggles[3];
    std::unique_ptr<juce::Label> noiseVolLabel, masterTuneLabel, unisonDetuneLabel;

    // Visualisers. The curve references cutoffKnob/resoKnob, so it is
    // declared after them and destroyed first.
    std::unique_ptr<FilterCurveComponent> filterCurve;
    std::unique_ptr<ModernVoiceMeterPanel> voiceMeterPanel;

    int currentEQBand = 1;
    int selectedFilterModel = 0;    // engine model / mode of the preset
    int selectedFilterMode = 0;
    int selectedFilterFamily = 0;   // UI family / entry
    int selectedFilterEntry = 0;
    // Last entry chosen in each family (the editor's state, not saved).
    // Switching families preselects either the same filter as the current
    // one (by label, else the same type; for comparing) or the family's last
    // choice, as set in the settings page; a family not visited yet always
    // tries the same filter first, else takes its first entry.
    std::array<int, kFilterFamilyCount> familyEntryMemory{};
    std::array<bool, kFilterFamilyCount> familyVisited{};
    int entryForFamily(int family) const;
    bool matchFilterOnFamilySwitch = true;
    int laidOutFilterModel = -1; // model/mode the card layout was last built for
    int laidOutFilterMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterVcaTab)
};
