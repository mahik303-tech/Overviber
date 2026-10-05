#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>

#include "data/SynthModel.h"
#include "ui/theme/ModernTheme.h"
#include "ui/theme/ModernFontManager.h"
#include "ui/ModernEditorView.h"
#include "ui/designer/ModernShowcaseComponent.h"
#include "data/OverviberPaths.h"

// ==============================================================================
// ModernSkinDesignerContent: Main workspace holding top toolbar and dual views
// ==============================================================================
class ModernSkinDesignerContent : public juce::Component {
public:
    ModernSkinDesignerContent()
        : showcaseView(model, modernView.getModernLookAndFeel()),
          modernView(model, nullptr) {

        OverviberPaths::initializeStorage();

        auto presetsDir = OverviberPaths::getPresetsDirectory();
        auto waveDir = OverviberPaths::getWaveDataDirectory();

        if (presetsDir.exists()) model.getPresetManager().setBaseDirectory(presetsDir.getFullPathName().toStdString());
        if (waveDir.exists()) model.getWaveManager().setBaseDirectory(waveDir.getFullPathName().toStdString());

        if (model.getPresetManager().getPresetCount() == 0) {
            auto factory = OverviberPaths::findFactoryDiskDirectory();
            if (factory.exists()) {
                auto fp = factory.getChildFile("PRESETS");
                auto fw = factory.getChildFile("WAVEDATA");
                if (fp.exists()) model.getPresetManager().setBaseDirectory(fp.getFullPathName().toStdString());
                if (fw.exists()) model.getWaveManager().setBaseDirectory(fw.getFullPathName().toStdString());
            }
        }

        // Available Themes
        themes = ModernTheme::getPresetThemes();

        // Top Toolbar Components
        addAndMakeVisible(toolbar);

        lblTheme.setText("PALETTE:", juce::dontSendNotification);
        lblTheme.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(lblTheme);

        for (size_t i = 0; i < themes.size(); ++i) {
            themeCombo.addItem(themes[i].name, (int)i + 1);
        }
        themeCombo.setSelectedId(1, juce::dontSendNotification);
        themeCombo.onChange = [this]() {
            int idx = themeCombo.getSelectedId() - 1;
            if (idx >= 0 && idx < (int)themes.size()) {
                applyTheme(themes[idx]);
            }
        };
        addAndMakeVisible(themeCombo);

        lblFont.setText("FONT:", juce::dontSendNotification);
        lblFont.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(lblFont);

        // Populate Fonts: Curated hardware first, then installed system fonts
        auto curated = ModernFontManager::getCuratedFonts();
        for (const auto& f : curated) {
            fontCombo.addItem(f.displayName, (int)availableFonts.size() + 1);
            availableFonts.push_back(f.fontName);
        }
        fontCombo.addSeparator();
        auto sysFonts = ModernFontManager::getAllSystemFonts();
        for (const auto& sf : sysFonts) {
            fontCombo.addItem(sf, (int)availableFonts.size() + 1);
            availableFonts.push_back(sf);
        }
        fontCombo.setSelectedId(1, juce::dontSendNotification);
        fontCombo.onChange = [this]() {
            int idx = fontCombo.getSelectedId() - 1;
            if (idx >= 0 && idx < (int)availableFonts.size()) {
                applyFont(availableFonts[idx]);
            }
        };
        addAndMakeVisible(fontCombo);

        lblScale.setText("SCALE:", juce::dontSendNotification);
        lblScale.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(lblScale);

        scaleCombo.addItem("85% (Compact)", 1);
        scaleCombo.addItem("90%", 2);
        scaleCombo.addItem("100% (Standard)", 3);
        scaleCombo.addItem("110%", 4);
        scaleCombo.addItem("120% (Large)", 5);
        scaleCombo.setSelectedId(3, juce::dontSendNotification);
        scaleCombo.onChange = [this]() {
            float scales[] = { 0.85f, 0.90f, 1.0f, 1.10f, 1.20f };
            int id = scaleCombo.getSelectedId() - 1;
            if (id >= 0 && id < 5) {
                applyScale(scales[id]);
            }
        };
        addAndMakeVisible(scaleCombo);

        lblView.setText("VIEW:", juce::dontSendNotification);
        lblView.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(lblView);

        viewCombo.addItem("Full Synth (7 Tabs)", 1);
        viewCombo.addItem("UI Gallery Showcase", 2);
        viewCombo.setSelectedId(1, juce::dontSendNotification);
        viewCombo.onChange = [this]() {
            bool isFullSynth = (viewCombo.getSelectedId() == 1);
            modernView.setVisible(isFullSynth);
            showcaseView.setVisible(!isFullSynth);
            tabJumpCombo.setVisible(isFullSynth);
            resized();
        };
        addAndMakeVisible(viewCombo);

        // Tab Quick Jump (visible in Full Synth mode)
        const char* tabNames[] = { "Tab 1: OSC", "Tab 2: FILTER", "Tab 3: VCA", "Tab 4: ENV", "Tab 5: LFOS", "Tab 6: ARP", "Tab 7: SETTINGS", "Tab 8: LUA" };
        for (int i = 0; i < 8; ++i) tabJumpCombo.addItem(tabNames[i], i + 1);
        tabJumpCombo.setSelectedId(1, juce::dontSendNotification);
        tabJumpCombo.onChange = [this]() {
            int tab = tabJumpCombo.getSelectedId() - 1;
            modernView.selectTab(tab);
        };
        addAndMakeVisible(tabJumpCombo);

        btnExport.setButtonText("EXPORT C++ CODE");
        btnExport.onClick = [this]() {
            exportCurrentTheme();
        };
        addAndMakeVisible(btnExport);

        // Content Views
        addAndMakeVisible(modernView);
        addChildComponent(showcaseView);

        // Initial styling
        applyTheme(themes[0]);
        setSize(1220, 840);
    }

    ~ModernSkinDesignerContent() override = default;

    void applyTheme(const ModernTheme& theme) {
        currentTheme = theme;
        modernView.setTheme(theme);
        showcaseView.refreshTheme();

        // Update toolbar styling
        lblTheme.setColour(juce::Label::textColourId, theme.textMuted);
        lblFont.setColour(juce::Label::textColourId, theme.textMuted);
        lblScale.setColour(juce::Label::textColourId, theme.textMuted);
        lblView.setColour(juce::Label::textColourId, theme.textMuted);

        auto font = modernView.getModernLookAndFeel().getCustomFont(10.0f, juce::Font::bold);
        lblTheme.setFont(font);
        lblFont.setFont(font);
        lblScale.setFont(font);
        lblView.setFont(font);

        repaint();
    }

    void applyFont(const juce::String& family) {
        modernView.setFontFamily(family);
        showcaseView.refreshTheme();
        repaint();
    }

    void applyScale(float scale) {
        modernView.setFontScale(scale);
        showcaseView.refreshTheme();
        repaint();
    }

    void exportCurrentTheme() {
        juce::String cppCode = currentTheme.toCppCode();
        juce::SystemClipboard::copyTextToClipboard(cppCode);

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "C++ Theme Exported!",
            "The C++ code for theme '" + currentTheme.name + "' has been copied to your clipboard!\n\n"
            "You can paste it directly into ModernTheme.h or your code repository.\n\n"
            "Palette Details:\n"
            "  Accent: " + currentTheme.accent.toDisplayString(true) + "\n"
            "  Window Bg: " + currentTheme.windowBg.toDisplayString(true) + "\n"
            "  Card Bg: " + currentTheme.cardBg.toDisplayString(true),
            "OK"
        );
    }

    void paint(juce::Graphics& g) override {
        // Toolbar background
        auto topArea = getLocalBounds().removeFromTop(46).toFloat();
        g.setColour(currentTheme.cardBg);
        g.fillRect(topArea);

        // Bottom separator
        g.setColour(currentTheme.accent);
        g.fillRect(topArea.getX(), topArea.getBottom() - 1.5f, topArea.getWidth(), 1.5f);
    }

    void resized() override {
        auto bounds = getLocalBounds();
        auto topArea = bounds.removeFromTop(44).reduced(8, 6);

        int lblW = 54;
        int themeW = 160;
        int fontW = 160;
        int scaleW = 120;
        int viewW = 160;
        int tabW = 140;
        int exportW = 135;

        int x = topArea.getX();
        int y = topArea.getY();
        int h = topArea.getHeight();

        lblTheme.setBounds(x, y, lblW, h); x += lblW + 4;
        themeCombo.setBounds(x, y, themeW, h); x += themeW + 12;

        lblFont.setBounds(x, y, 42, h); x += 42 + 4;
        fontCombo.setBounds(x, y, fontW, h); x += fontW + 12;

        lblScale.setBounds(x, y, 48, h); x += 48 + 4;
        scaleCombo.setBounds(x, y, scaleW, h); x += scaleW + 12;

        lblView.setBounds(x, y, 42, h); x += 42 + 4;
        viewCombo.setBounds(x, y, viewW, h); x += viewW + 12;

        if (tabJumpCombo.isVisible()) {
            tabJumpCombo.setBounds(x, y, tabW, h);
            x += tabW + 12;
        }

        btnExport.setBounds(topArea.getRight() - exportW, y, exportW, h);

        // Content Area below toolbar
        auto contentArea = bounds;
        modernView.setBounds(contentArea);
        showcaseView.setBounds(contentArea);
    }

private:
    SynthModel model;
    std::vector<ModernTheme> themes;
    ModernTheme currentTheme;
    std::vector<juce::String> availableFonts;

    juce::Component toolbar;
    juce::Label lblTheme, lblFont, lblScale, lblView;
    juce::ComboBox themeCombo, fontCombo, scaleCombo, viewCombo, tabJumpCombo;
    juce::TextButton btnExport;

    ModernEditorView modernView;
    ModernShowcaseComponent showcaseView;
};

// ==============================================================================
// ModernSkinDesignerWindow: Top-level DocumentWindow
// ==============================================================================
class ModernSkinDesignerWindow : public juce::DocumentWindow {
public:
    ModernSkinDesignerWindow(const juce::String& name)
        : juce::DocumentWindow(name,
                               juce::Colour(0xff0d0f14),
                               juce::DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setContentOwned(new ModernSkinDesignerContent(), true);
        setResizable(true, true);
        setResizeLimits(980, 680, 2560, 1600);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

// ==============================================================================
// ModernSkinDesignerApplication: JUCE GUI Application
// ==============================================================================
class ModernSkinDesignerApplication : public juce::JUCEApplication {
public:
    ModernSkinDesignerApplication() = default;

    const juce::String getApplicationName() override       { return "Overviber - Modern Skin Designer (a Fork of GliGli Overcycle)"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& /*commandLine*/) override {
        mainWindow = std::make_unique<ModernSkinDesignerWindow>(getApplicationName());
    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override {}

private:
    std::unique_ptr<ModernSkinDesignerWindow> mainWindow;
};

// ==============================================================================
// JUCE Application Entry Point
// ==============================================================================
START_JUCE_APPLICATION(ModernSkinDesignerApplication)
