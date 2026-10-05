#pragma once

#include "../tabs/ModernTabContext.h"
#include "LuaEngine.h"
#include <map>
#include <memory>
#include <vector>

// ==============================================================================
// LUA tab: a page of the Modern skin that a Lua script lays out and draws
// ==============================================================================
// The script is Documents/Overviber/LUA/skin.lua, or the built-in example
// (LuaDefaultSkin.lua) while that file does not exist. It runs the first time
// the tab is shown, never while the editor opens, and is reloaded when the
// file changes. Its API (ui, params, theme, graphics) is documented in
// doc/LUA_SKINS.md.
//
// A callback that fails is switched off until the next reload and its error
// goes to the tab's log; the rest of the editor never sees a Lua error.
// ==============================================================================
class LuaTab : public ModernTabModule, private juce::Timer {
public:
    explicit LuaTab(ModernTabContext& context);
    ~LuaTab() override;

    void setup() override;
    void updateFromEngine() override;
    void resized() override;
    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;
    void lookAndFeelChanged() override;

    // Runs `source` in a fresh engine (tests, and reload()).
    void loadScript(const juce::String& source, const juce::String& name);
    // skin.lua if it exists, else the built-in example.
    void reload();
    static juce::File getScriptFile();
    static juce::String getBuiltInScript();

    // One timer tick: parameter listeners, knobs, frame callback, file check.
    void tick();

    const juce::StringArray& getLogLines() const noexcept { return logLines; }
    int getErrorCount() const noexcept { return errorCount; }
    bool isScriptLoaded() const noexcept { return engine != nullptr; }
    juce::Component& getScriptArea() noexcept { return scriptArea; }
    juce::Component* findScriptComponent(const juce::String& id) const;

    // The Lua side (static C functions with this tab as upvalue).
    struct Binding;

private:
    enum class Kind { Canvas, Button, Label, Knob, Card };
    struct Widget {
        Kind kind;
        juce::String id;
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<juce::Label> caption;   // knobs
        int paintRef = LuaEngine::noRef;
        int mouseRef = LuaEngine::noRef;
        int clickRef = LuaEngine::noRef;
        int param = -1;                          // knobs: continuousParameter_t
        bool failed = false;                     // a callback failed: off until reload
    };
    class Canvas;
    struct Listener {
        int id;
        bool stepped;
        int param;
        int ref;
        int last;
    };

    void timerCallback() override;
    void updateTimer();
    void unload();
    void addLog(const juce::String& line, bool error);
    void updateStatus();
    void paintCanvas(int index, juce::Graphics& g);
    void mouseCanvas(int index, const char* event, const juce::MouseEvent& e, float wheel = 0.0f);
    void pollParameters(bool fire);
    void callFrame();
    void fireThemeChanged();
    int readParam(bool stepped, int param) const;
    Widget* widgetAt(int index) { return index >= 0 && index < (int)widgets.size() ? widgets[(size_t)index].get() : nullptr; }

    std::unique_ptr<LuaEngine> engine;
    std::vector<std::unique_ptr<Widget>> widgets;
    std::vector<Listener> listeners;
    std::vector<int> themeRefs;
    int nextListenerId = 1;
    int frameRef = LuaEngine::noRef;
    int frameHz = 0;
    double lastFrameMs = 0.0;
    bool loadedOnce = false;
    juce::String scriptName;
    juce::File loadedFile;
    juce::Time loadedFileTime;
    int fileCheckCountdown = 0;

    juce::Component scriptArea;
    juce::Label statusLabel;
    juce::TextButton reloadButton{ "RELOAD" };
    juce::TextButton folderButton{ "SCRIPT FOLDER" };
    juce::TextButton logButton{ "LOG" };
    juce::TextEditor logView;
    juce::StringArray logLines;
    int errorCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LuaTab)
};
