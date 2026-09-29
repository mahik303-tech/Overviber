#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"
#include "../ModernPresetManager.h"
#include "../theme/ModernFontManager.h"
#include <vector>

// SETTINGS tab: MPE / release-velocity settings, skin & palette editing with
// user palettes, typography, window scale, startup defaults
// (skin_config.conf, user_palettes.conf), and a debug card with the inspector
// switch and a copy of the current state for test scenarios.
class SettingsTab final : public ModernTabModule {
public:
    // Editor-level services driven by the settings page (implemented by
    // ModernEditorView): theme/typography of the whole editor, window
    // scale, skin switching, the debug overlay and the modal dialogs.
    class Host {
    public:
        virtual ~Host() = default;
        virtual void applyTheme(const ModernTheme& theme) = 0;
        virtual void applyFontFamily(const juce::String& familyName) = 0;
        virtual void applyFontScale(float scale) = 0;
        virtual void windowScaleChanged(float scale) = 0;
        virtual void switchToClassicSkin() = 0;
        virtual void debugModeChanged(bool enabled) = 0;
        // Filter family switch: preselect the same filter (true) or the
        // family's last choice (false).
        virtual void filterFamilySwitchChanged(bool matchSameFilter) = 0;
        virtual void showColourPicker(juce::Colour initialColour, const juce::String& roleTitle,
                                      std::function<void(juce::Colour)> onColourChanged,
                                      std::function<void(juce::Colour)> onApply) = 0;
        virtual void showPaletteSaveDialog(const juce::String& initialName,
                                           std::function<void(const juce::String& newName)> onSave) = 0;
    };

    SettingsTab(ModernTabContext& context, Host& host);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

    // Appearance persistence (skin_config.conf, user_palettes.conf).
    void loadSkinConfig();
    void saveSkinConfig();

    // Mirrors a theme applied to the editor into the palette controls.
    void themeApplied(const ModernTheme& theme);

    void setDebugMode(bool enabled);
    bool isDebugModeEnabled() const noexcept { return debugMode; }

    // The current state as text: the main preset in the preset file format
    // (all parameters, readable by PresetManager::parsePresetString) and the
    // mixer and routing, which are not part of a preset.
    static juce::String describeState(SynthModel& model);

    float getSavedWindowScale() const noexcept { return savedWindowScale; }
    void setSavedWindowScale(float scale) noexcept { savedWindowScale = scale; }

private:
    class ColorSwatchButton : public juce::Button {
    public:
        ColorSwatchButton();
        ~ColorSwatchButton() override;

        void setSwatchColour(juce::Colour c) { colour = c; repaint(); }
        juce::Colour getSwatchColour() const { return colour; }

        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        void clicked() override;

        std::function<void(juce::Colour)> onColourChanged;
        std::function<void()> onOpenColorPicker;

    private:
        juce::Colour colour = juce::Colour(0xff18b5c9);
    };

    class PaletteSwatchStrip : public juce::Component {
    public:
        std::function<void(int roleIndex)> onRoleSelected;

        void setTheme(const ModernTheme& t) {
            theme = t;
            repaint();
        }

        void setSelectedRole(int r) {
            selectedRole = r;
            repaint();
        }

        int getSelectedRole() const { return selectedRole; }

        void paint(juce::Graphics& g) override {
            auto b = getLocalBounds().toFloat();
            int count = ModernTheme::NumColorRoles;
            float gap = 4.0f;
            float w = (b.getWidth() - (float)(count - 1) * gap) / (float)count;

            const char* shortNames[] = { "ACCENT", "CHASSIS", "PANELS", "HEADER", "BORDERS", "DIALS", "TEXT" };

            for (int i = 0; i < count; ++i) {
                juce::Rectangle<float> rect(b.getX() + (float)i * (w + gap), b.getY(), w, b.getHeight());
                juce::Colour c = theme.getColorForRole(i);

                g.setColour(c);
                g.fillRect(rect);

                if (i == selectedRole) {
                    g.setColour(juce::Colours::white);
                    g.drawRect(rect, 2.0f);
                } else {
                    g.setColour(theme.cardBorder);
                    g.drawRect(rect, 1.0f);
                }

                g.setFont(ModernFontManager::createFont("D-DIN", 9.0f, juce::Font::bold));
                juce::Colour textC = (c.getBrightness() > 0.45f) ? juce::Colours::black : juce::Colours::white;
                g.setColour(textC);
                g.drawText(shortNames[i], rect, juce::Justification::centred, false);
            }
        }

        void mouseDown(const juce::MouseEvent& e) override {
            int count = ModernTheme::NumColorRoles;
            float gap = 4.0f;
            float w = (getWidth() - (float)(count - 1) * gap) / (float)count;

            for (int i = 0; i < count; ++i) {
                float rx = (float)i * (w + gap);
                if (e.x >= rx && e.x <= rx + w) {
                    selectedRole = i;
                    repaint();
                    if (onRoleSelected) onRoleSelected(i);
                    break;
                }
            }
        }

    private:
        ModernTheme theme = ModernTheme::getPresetThemes()[0];
        int selectedRole = 0;
    };

    void assignComponentIDs();
    void createControllerToggles();
    void createThemeControls();
    void createTypographyControls();
    void createWindowControls();
    void createDebugControls();
    void createBehaviourControls();
    void createStepToggles(std::unique_ptr<juce::ToggleButton>* toggles, const char* const* names, int count,
                           int radioGroup, steppedParameter_t sp);
    void setupInfoLabel(juce::Label& label, const juce::String& text);
    std::unique_ptr<juce::ToggleButton> createSettingToggle(const juce::String& text, bool state,
                                                            std::function<void(bool)> apply);
    std::unique_ptr<juce::Slider> createSettingKnob(const juce::String& name, float value, const juce::String& caption,
                                                    std::unique_ptr<juce::Label>& label, std::function<void(float)> apply);
    std::unique_ptr<juce::Slider> createColourKnob(const juce::String& name, bool hue, double init,
                                                   const juce::String& caption, std::unique_ptr<juce::Label>& label);
    void addPageKnob(juce::Slider& knob);
    void setColourKnobs(juce::Colour c);
    void loadUserPalettes();
    void saveUserPalettes();
    void refreshThemePresetCombo();
    void updateRoleColorInSliders();
    void applyColorToActiveRole();

    Host& host;

    // Invisible scroll frame holding all cards (vertical scroll bar only).
    juce::Component scrollContent;
    juce::Viewport viewport;
    std::vector<juce::Slider*> pageKnobs;  // wheel off while the page scrolls

    ModernSectionCard themeCard{"SKIN & PALETTE", "APPEARANCE"};
    ModernSectionCard debugCard{"DEVELOPER & DEBUG", "DEBUG"};
    ModernSectionCard behaviourCard{"EDITOR BEHAVIOUR", "EDITOR"};

    // MPE & release velocity
    std::unique_ptr<juce::ToggleButton> timbreTargetToggles[7];
    std::unique_ptr<juce::ToggleButton> mpeModeToggles[3];
    std::unique_ptr<juce::ToggleButton> mpeBendRangeToggles[5];
    std::unique_ptr<juce::ToggleButton> releaseVelocityToggles[4];

    // Skin & palette
    juce::ComboBox themePresetCombo;
    std::unique_ptr<juce::Slider> customHueKnob, customSatKnob, customBriKnob;
    std::unique_ptr<juce::Label> customHueLabel, customSatLabel, customBriLabel;
    juce::ComboBox fontSelectorCombo;
    juce::ComboBox fontScaleCombo;
    ModernHeaderButton savePaletteBtn{"savePalette", "Save Palette As..."};
    ModernHeaderButton saveDefaultBtn{"saveDefault", "Set as Default"};
    juce::Label defaultInfoLabel;

    ModernHeaderButton skinSwitchBtn{"skinSwitchBtn", "SWITCH TO CLASSIC SKIN"};
    juce::ComboBox windowScaleCombo;

    std::unique_ptr<juce::ToggleButton> debugModeToggle;
    juce::Label debugInfoLabel;
    ModernHeaderButton copyStateBtn{"copyStateBtn", "COPY STATE TO CLIPBOARD"};
    juce::Label copyStateInfoLabel;

    // Filter family switch: same filter or last choice (skin_config.conf)
    std::unique_ptr<juce::ToggleButton> filterSwitchToggles[2];
    juce::Label filterSwitchInfoLabel;
    bool filterSwitchMatch = true;
    void setFilterSwitchMatch(bool match);

    // Output spectrum displays (skin_config.conf)
    std::unique_ptr<juce::ToggleButton> retroSpectrumToggle, spectrumWaterfallToggle, curvesWaterfallToggle;
    juce::Label spectrumInfoLabel;
    std::unique_ptr<juce::Slider> retroFilterOpacityKnob, retroCurvesOpacityKnob, waterfallOpacityKnob, retroRandomKnob;
    std::unique_ptr<juce::Label> retroFilterOpacityLabel, retroCurvesOpacityLabel, waterfallOpacityLabel, retroRandomLabel;

    ColorSwatchButton swatchButton;
    PaletteSwatchStrip swatchStrip;

    ModernTheme customTheme;
    std::vector<ModernTheme> userThemes;
    int currentEditingRole = 0;
    float savedWindowScale = 1.0f;
    bool debugMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsTab)
};
