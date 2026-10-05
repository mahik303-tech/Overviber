// ==============================================================================
// LuaTabScenarioTest - the Modern skin's LUA tab and its sandboxed Lua engine
// ==============================================================================
// Runs scripts in a LuaTab (with the plugin's processor, so parameters go
// through the APVTS as in a DAW) and checks that no script can crash, hang or
// escape the editor: the sandbox, the time and memory limits, graphics used
// after paint, failing callbacks, reloads and destruction. Also the built-in
// example and the tab in ModernEditorView.
//
// The script folder is redirected into the output folder, so the user's real
// Documents/Overviber/LUA is never read or written.
// ==============================================================================
#include "PluginProcessor.h"
#include "data/OverviberPaths.h"
#include "ui/ModernEditorView.h"
#include "ui/lua/LuaTab.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>

namespace {

int failures = 0;

void check(bool ok, const juce::String& what) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << what << "\n";
    if (!ok) ++failures;
}

bool logContains(const LuaTab& tab, const juce::String& text) {
    for (const auto& line : tab.getLogLines())
        if (line.contains(text)) return true;
    return false;
}

// juce::Button's private click message, handled synchronously: the production
// click path without a message loop (as in ModernSkinScenarioTest).
constexpr int juceButtonClickMessageId = 0x2f3f4f99;

void click(LuaTab& tab, const char* id) {
    if (auto* button = dynamic_cast<juce::Button*>(tab.findScriptComponent(id)))
        static_cast<juce::Component&>(*button).handleCommandMessage(juceButtonClickMessageId);
    else
        std::cout << "       (no button '" << id << "')\n";
}

void dumpLog(const LuaTab& tab) {
    for (const auto& line : tab.getLogLines()) std::cout << "       | " << line << "\n";
}

// A LuaTab on its own, wired to the processor like the editor's tabs.
struct Fixture {
    explicit Fixture(OvercyclerAudioProcessor& p) : processor(p) {
        context.setContinuousParam = [this](continuousParameter_t cp, float v) { processor.setContinuousParamFromUI(cp, v); };
        context.setSteppedParam = [this](steppedParameter_t sp, uint8_t v) { processor.setSteppedParamFromUI(sp, v); };
        tab = std::make_unique<LuaTab>(context);
        tab->setLookAndFeel(&lnf);
        tab->setup();
        tab->setBounds(0, 0, 1068, 626);
    }
    ~Fixture() {
        tab->setLookAndFeel(nullptr);
        tab.reset();
    }

    LuaTab& load(const juce::String& source) {
        tab->loadScript(source, "test");
        return *tab;
    }

    // Paints the tab with all its children, like the editor does.
    juce::Image render() {
        juce::Image image(juce::Image::ARGB, tab->getWidth(), tab->getHeight(), true);
        juce::Graphics g(image);
        tab->paintEntireComponent(g, false);
        return image;
    }

    int cutoffPot() const { return scan_potFrom16bits(processor.getModel().getCurrentPreset().continuousParams[cpCutoff]); }

    OvercyclerAudioProcessor& processor;
    ModernLookAndFeel lnf;
    ModernTabContext context{ processor.getModel(), &processor, lnf };
    std::unique_ptr<LuaTab> tab;
};

template <typename Fn>
double millisecondsFor(Fn&& fn) {
    const double start = juce::Time::getMillisecondCounterHiRes();
    fn();
    return juce::Time::getMillisecondCounterHiRes() - start;
}

void builtInExample(OvercyclerAudioProcessor& p) {
    Fixture f(p);
    auto& tab = f.load(LuaTab::getBuiltInScript());
    check(tab.getErrorCount() == 0, "built-in example loads without errors");
    for (const char* id : { "filterCurve", "envCurves", "lfoWave", "cutoff", "animate", "randomFilter", "toolsInfo" })
        check(tab.findScriptComponent(id) != nullptr, juce::String("built-in example creates '") + id + "'");
    f.render();
    tab.tick();
    check(tab.getErrorCount() == 0, "built-in example paints and polls without errors");
    if (tab.getErrorCount() > 0) dumpLog(tab);

    // Its buttons run Lua that sets parameters.
    const int before = f.cutoffPot();
    for (int i = 0; i < 5; ++i) click(tab, "randomFilter");
    const bool changed = f.cutoffPot() != before;
    for (int i = 0; i < 5; ++i) click(tab, "undo");
    check(tab.getErrorCount() == 0 && changed && std::abs(f.cutoffPot() - before) <= 1,
          "RANDOM FILTER x5 changes the cutoff, UNDO x5 restores it");
    f.render();
    check(tab.getErrorCount() == 0, "built-in example repaints after parameter changes");
}

void sandbox(OvercyclerAudioProcessor& p) {
    Fixture f(p);
    auto& tab = f.load(R"lua(
        local missing = {}
        for _, name in ipairs({ "io", "os", "package", "debug", "require", "load", "loadfile", "dofile",
                                "collectgarbage" }) do
          if _G[name] ~= nil then missing[#missing + 1] = name end
        end
        if string.dump ~= nil then missing[#missing + 1] = "string.dump" end
        log(#missing == 0 and "sandbox closed" or ("sandbox open: " .. table.concat(missing, ", ")))
    )lua");
    check(tab.getErrorCount() == 0 && logContains(tab, "sandbox closed"), "no io, os, package, debug, load, dofile, string.dump");

    f.load("\x1bLua binary chunk");
    check(tab.getErrorCount() == 1, "binary chunks are refused");
    f.load("this is not lua");
    check(tab.getErrorCount() == 1 && tab.isScriptLoaded(), "a syntax error is reported, the tab stays usable");
}

void limits(OvercyclerAudioProcessor& p) {
    Fixture f(p);
    double ms = millisecondsFor([&] { f.load("while true do end"); });
    check(logContains(*f.tab, "time limit exceeded") && ms < 3000.0, "an endless loop stops at the time limit (" + juce::String(ms, 0) + " ms)");

    ms = millisecondsFor([&] { f.load("while true do pcall(function() while true do end end) end"); });
    check(logContains(*f.tab, "time limit exceeded") && ms < 3000.0, "pcall cannot catch the time limit (" + juce::String(ms, 0) + " ms)");

    ms = millisecondsFor([&] {
        f.load("local co = coroutine.wrap(function() while true do end end)\n"
               "while true do coroutine.resume(coroutine.create(function() while true do end end)) end");
    });
    check(logContains(*f.tab, "time limit exceeded") && ms < 3000.0, "coroutine.resume cannot catch the time limit");

    f.load("local t = {} for i = 1, 1e9 do t[i] = string.rep('x', 1000) .. i end");
    check(logContains(*f.tab, "out of memory"), "the memory limit stops a growing table");

    // A callback that hangs is stopped too, and only switched off.
    auto& tab = f.load(R"lua(
        ui.button{ id = "hang", text = "HANG", onClick = function() while true do end end }
        ui.button{ id = "fine", text = "FINE", onClick = function() log("fine clicked") end }
    )lua");
    ms = millisecondsFor([&] { click(tab, "hang"); });
    click(tab, "fine");
    check(ms < 3000.0 && logContains(tab, "time limit exceeded") && logContains(tab, "fine clicked"),
          "a hanging onClick is stopped, other callbacks keep working");
}

void graphics(OvercyclerAudioProcessor& p) {
    Fixture f(p);
    auto& tab = f.load(R"lua(
        paints = 0
        ui.canvas{ id = "keep", x = 0, y = 0, w = 100, h = 50, paint = function(g, w, h)
          kept = g
          for i = 1, 3 do g:save() end            -- never restored: the tab restores them
          g:translate(5, 5); g:rotate(0.3)
          g:setColour("accent"); g:fillRect(0, 0, w, h)
          g:setGradient{ x1 = 0, y1 = 0, x2 = w, y2 = 0, stops = { { 0, "#102030" }, { 1, "rgba(255, 0, 0, 0.5)" } } }
          g:beginPath(); g:moveTo(0, 0); g:lineTo(w, h); g:arc(50, 25, 20, 0, 3.14); g:closePath(); g:fill(); g:stroke(2)
          g:drawText("Text " .. g:textWidth("abc"), 0, 0, w, h, "centre", 12, true)
          paints = paints + 1
        end }
        ui.canvas{ id = "broken", x = 0, y = 60, w = 100, h = 50, paint = function(g)
          brokenPaints = (brokenPaints or 0) + 1
          g:setColour("not a colour")
        end }
        ui.button{ id = "late", text = "LATE", onClick = function() kept:fillRect(0, 0, 1, 1) end }
        ui.button{ id = "count", text = "COUNT", onClick = function()
          log("paints " .. paints .. " broken " .. tostring(brokenPaints))
        end }
    )lua");
    f.render();
    f.render();
    click(tab, "late");
    click(tab, "count");
    check(logContains(tab, "outside its paint function"), "a graphics object kept after paint raises an error");
    check(logContains(tab, "colour expected"), "a bad colour is reported");
    check(logContains(tab, "paints 2 broken 1"), "a failing paint is switched off, the others keep painting");
}

int countPixels(const juce::Image& image, juce::Colour colour) {
    int found = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x) {
            const auto p = image.getPixelAt(x, y);
            if (std::abs(p.getRed() - colour.getRed()) < 40 && std::abs(p.getGreen() - colour.getGreen()) < 40 &&
                std::abs(p.getBlue() - colour.getBlue()) < 40)
                ++found;
        }
    return found;
}

// A stroked and a filled path must put pixels on the screen (a path that is
// built but never drawn is the mistake a script cannot see by itself).
void paths(OvercyclerAudioProcessor& p) {
    Fixture f(p);
    auto& tab = f.load(R"lua(
        ui.canvas{ id = "paths", x = 0, y = 0, w = 200, h = 100, paint = function(g, w, h)
          g:setColour("#000000"); g:fillRect(0, 0, w, h)
          g:beginPath(); g:moveTo(0, 0); g:lineTo(w, h); g:setColour("#ff0000"); g:stroke(4)
          g:beginPath(); g:moveTo(10, 90); g:lineTo(60, 20); g:lineTo(110, 90); g:closePath()
          g:setColour("#00ff00"); g:fill()
          g:beginPath(); g:arc(160, 50, 30, 0, 3.0); g:setColour("#0000ff"); g:stroke(3)
        end }
    )lua");
    const auto image = f.render();
    const int red = countPixels(image, juce::Colour(0xffff0000));
    const int green = countPixels(image, juce::Colour(0xff00ff00));
    const int blue = countPixels(image, juce::Colour(0xff0000ff));
    check(tab.getErrorCount() == 0, "the path script runs");
    check(red > 400, "a stroked line is drawn (" + juce::String(red) + " pixels)");
    check(green > 1000, "a filled triangle is drawn (" + juce::String(green) + " pixels)");
    check(blue > 100, "an arc is drawn (" + juce::String(blue) + " pixels)");
}

void parameters(OvercyclerAudioProcessor& p) {
    Fixture f(p);
    auto& tab = f.load(R"lua(
        changes = 0
        params.set("cpCutoff", 700)
        params.set("spFilterModel", 99)        -- clamped to the last choice
        log("cutoff " .. params.get("cpCutoff"))
        log("model " .. params.getText("spFilterModel"))
        log("text " .. params.getText("cpCutoff"))
        local info = params.getInfo("spFilterModel")
        log("choices " .. #info.choices .. " max " .. info.max)
        listener = params.onChange("cpResonance", function(v, id) changes = changes + 1; log(id .. " -> " .. v) end)
        ui.knob{ id = "reso", param = "cpResonance", caption = "RESO", x = 10, y = 10 }
        ui.button{ id = "remove", onClick = function() log("removed " .. tostring(params.removeListener(listener))) end }
        ui.button{ id = "count", onClick = function() log("changes " .. changes) end }
        local ok, err = pcall(params.get, "noSuchParam")
        log("unknown: " .. tostring(err))
        ok, err = pcall(ui.knob, { param = "spFilterModel" })
        log("stepped knob: " .. tostring(err))
    )lua");
    check(tab.getErrorCount() == 0, "the parameter script runs");
    if (tab.getErrorCount() > 0) dumpLog(tab);
    check(std::abs(f.cutoffPot() - 700) <= 1 && logContains(tab, "cutoff 70"), "params.set and params.get reach the model");
    auto* apvtsCutoff = p.getAPVTS().getParameter("cpCutoff");
    check(apvtsCutoff != nullptr && std::abs(apvtsCutoff->convertFrom0to1(apvtsCutoff->getValue()) - 70.07f) < 0.2f,
          "params.set reaches the host parameter (APVTS)");
    check(logContains(tab, "model SST Vintage (Moog)") && logContains(tab, "choices 4 max 3"), "stepped parameters: clamp, text and choices");
    check(logContains(tab, "text 70 %"), "params.getText uses the host parameter's text");
    check(logContains(tab, "unknown parameter 'noSuchParam'") && logContains(tab, "no continuous parameter"),
          "bad parameter IDs are errors for the script");

    p.setContinuousParamFromUI(cpResonance, 321.0f);
    tab.tick();
    auto* knob = dynamic_cast<juce::Slider*>(tab.findScriptComponent("reso"));
    check(logContains(tab, "cpResonance -> 32"), "params.onChange fires after a change from outside");
    check(knob != nullptr && std::abs(knob->getValue() - 321.0) <= 1.0, "a Lua knob follows the parameter");
    if (knob != nullptr) knob->setValue(800.0, juce::sendNotificationSync);
    check(std::abs(scan_potFrom16bits(p.getModel().getCurrentPreset().continuousParams[cpResonance]) - 800) <= 1,
          "a Lua knob sets the parameter");
    click(tab, "remove");
    p.setContinuousParamFromUI(cpResonance, 100.0f);
    tab.tick();
    click(tab, "count");
    check(logContains(tab, "removed true") && logContains(tab, "changes 1"), "params.removeListener stops the listener");
}

void themeAndFrames(OvercyclerAudioProcessor& p) {
    Fixture f(p);
    auto& tab = f.load(R"lua(
        theme.onChange(function() log("theme now " .. theme.get().name .. " " .. theme.colour("accent")) end)
        log("half accent " .. theme.colour("accent", 0.5))
        frames = 0
        ui.onFrame(function(dt) frames = frames + 1; if frames == 3 then error("frame failure") end end)
        ui.setFrameRate(60)
        ui.button{ id = "count", onClick = function() log("frames " .. frames) end }
    )lua");
    auto themes = ModernTheme::getPresetThemes();
    f.lnf.setTheme(themes[1]);
    tab.sendLookAndFeelChange();
    check(logContains(tab, "theme now " + themes[1].name + " " + LuaEngine::colourToString(themes[1].accent)),
          "theme.onChange fires with the new palette");
    check(logContains(tab, "half accent " + LuaEngine::colourToString(themes[0].accent.withAlpha(0.5f))),
          "theme.colour adds an alpha value");
    for (int i = 0; i < 12; ++i) {
        tab.tick();
        juce::Thread::sleep(20);
    }
    click(tab, "count");
    check(logContains(tab, "frame failure") && logContains(tab, "frames 3") && logContains(tab, "frame callback switched off"),
          "a failing frame callback is switched off");
}

void lifetime(OvercyclerAudioProcessor& p) {
    // Destroying the tab with listeners, a frame callback and kept handles.
    for (int i = 0; i < 20; ++i) {
        Fixture f(p);
        f.load(R"lua(
            handle = ui.canvas{ x = 0, y = 0, w = 10, h = 10, paint = function(g) g:fillAll("cardBg") end }
            for i = 1, 50 do params.onChange("cpCutoff", function() handle:repaint() end) end
            ui.onFrame(function() handle:repaint() end)
            ui.setFrameRate(60)
        )lua");
        f.render();
        f.load(LuaTab::getBuiltInScript());   // a reload replaces the engine and every widget
        f.render();
    }
    check(true, "20 tabs with listeners, frames and reloads destroyed");
}

void scriptFile(OvercyclerAudioProcessor& p, const juce::File& luaDir) {
    Fixture f(p);
    auto file = LuaTab::getScriptFile();
    check(file.getParentDirectory() == luaDir, "skin.lua lives in the redirected folder");
    f.tab->reload();
    check(f.tab->getErrorCount() == 0 && f.tab->findScriptComponent("filterCurve") != nullptr,
          "without skin.lua the built-in example runs");
    luaDir.createDirectory();
    file.replaceWithText("log('version 1')");
    for (int i = 0; i < 31; ++i) f.tab->tick();
    check(logContains(*f.tab, "version 1"), "a new skin.lua is picked up");
    file.replaceWithText("log('version 2')");
    file.setLastModificationTime(juce::Time::getCurrentTime() + juce::RelativeTime::seconds(5));
    for (int i = 0; i < 31; ++i) f.tab->tick();
    check(logContains(*f.tab, "version 2"), "a changed skin.lua is reloaded");
    file.deleteFile();
}

void editorIntegration(OvercyclerAudioProcessor& p, const juce::File& snapshot) {
    ModernEditorView view(p.getModel(), &p);
    view.setSize(1100, 650);
    view.selectTab((int)ModernTabBar::Tab::Lua);
    check(view.getLuaTab().isVisible() && !view.getSettingsTab().isVisible(), "the LUA tab is the 8th tab");
    check(!view.getLuaTab().isScriptLoaded(), "no Lua runs before the tab is shown on screen");
    view.getLuaTab().reload();
    check(view.getLuaTab().getErrorCount() == 0, "the built-in example runs in the editor");
    juce::Image image(juce::Image::ARGB, 1100, 650, true);
    juce::Graphics g(image);
    view.paintEntireComponent(g, false);
    check(view.getLuaTab().getErrorCount() == 0, "the editor paints the LUA tab without errors");
    if (snapshot != juce::File()) {
        snapshot.deleteFile();
        juce::FileOutputStream out(snapshot);
        juce::PNGImageFormat().writeImageToStream(image, out);
        std::cout << "[INFO] snapshot " << snapshot.getFullPathName() << "\n";
    }

    // The tab buttons end before the voice meter (its label starts 226 px from the right).
    juce::Component* last = nullptr;
    std::function<void(juce::Component&)> find = [&](juce::Component& c) {
        if (c.getComponentID() == "tabButtons[TabLua]") last = &c;
        for (auto* child : c.getChildren()) find(*child);
    };
    find(view);
    check(last != nullptr && last->getRight() <= 1100 - 226, "the 8 tab buttons leave the voice meter free");
}

}  // namespace

// Usage: LuaTabScenarioTest [--snapshot <file.png>]  (the editor on the LUA tab)
int main(int argc, char* argv[]) {
    std::cout << "=== LUA tab ===\n";
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) {
        std::cout << "[SKIP] No X display available for the headless editor.\n";
        return 77;
    }
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File snapshot;
    if (argc == 3 && juce::String(argv[1]) == "--snapshot") snapshot = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
    const auto outDir = juce::File::getCurrentWorkingDirectory();
    const auto luaDir = outDir.getChildFile("lua-scripts");
    luaDir.deleteRecursively();
    OverviberPaths::setLuaDirectoryOverride(luaDir);
    OverviberPaths::setAppConfigDirectoryOverride(outDir.getChildFile("config"));
    LuaEngine::setFixedRandomSeed(7);
    {
        auto processor = std::make_unique<OvercyclerAudioProcessor>(false);
        builtInExample(*processor);
        sandbox(*processor);
        limits(*processor);
        graphics(*processor);
        paths(*processor);
        parameters(*processor);
        themeAndFrames(*processor);
        lifetime(*processor);
        scriptFile(*processor, luaDir);
        editorIntegration(*processor, snapshot);
    }
    OverviberPaths::setLuaDirectoryOverride({});
    OverviberPaths::setAppConfigDirectoryOverride({});
    std::cout << (failures == 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return failures == 0 ? 0 : 1;
}
