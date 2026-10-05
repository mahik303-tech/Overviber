#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../theme/ModernTheme.h"
#include <functional>
#include <optional>

struct lua_State;

// ==============================================================================
// LuaEngine: one sandboxed Lua 5.4 state for the Lua tab (LuaTab)
// ==============================================================================
// Everything runs on the message thread. Nothing outside the engine keeps the
// lua_State: callbacks are registry references (ints) that the owner passes
// back to call(), so a destroyed engine cannot be reached from a timer or an
// async message (the crash pattern of other plugins' Lua skins, see
// doc/LUA_SKINS.md).
//
// Sandbox: only the base, string, table, math, utf8 and coroutine libraries;
// no io, os, package, debug, load, dofile, loadfile, require, collectgarbage or
// string.dump. Scripts load as text only, never as binary chunks.
// Limits: a memory cap for the whole state and a time budget for every call
// (an instruction hook raises "time limit exceeded"). An error never leaves
// the engine; it is reported through onError and call() returns false.
// ==============================================================================
class LuaEngine {
public:
    struct Limits {
        size_t memoryBytes = 16 * 1024 * 1024;
        double loadBudgetMs = 1000.0;   // running the script's main chunk
        double callBudgetMs = 200.0;    // every later callback
    };

    explicit LuaEngine(Limits limits);
    LuaEngine() : LuaEngine(Limits{}) {}
    ~LuaEngine();

    // The state, for registering bindings. Never store it.
    lua_State* state() const noexcept { return L; }
    const Limits& getLimits() const noexcept { return limits; }
    size_t getMemoryUsed() const noexcept { return memoryUsed; }

    // Compiles `source` as text and runs it with the load budget.
    bool run(const juce::String& source, const juce::String& chunkName);

    // Calls the function `ref` (from makeRef) with the arguments pushArgs
    // pushes (it returns their count). With wantResult, the first result
    // stays on the stack for the caller, who pops it.
    bool call(int ref, const std::function<int(lua_State*)>& pushArgs = {}, bool wantResult = false);

    // Registry reference to the value at `index`, or noRef if it is no function.
    int makeFunctionRef(int index);
    void releaseRef(int ref);
    static constexpr int noRef = -2;   // LUA_NOREF

    bool isInCall() const noexcept { return callDepth > 0; }

    std::function<void(const juce::String&)> onError;
    std::function<void(const juce::String&)> onLog;

    // ---- Graphics (a paint callback's first argument)
    struct PaintContext {
        juce::Graphics& g;
        const ModernTheme& theme;
        std::function<juce::Font(float size, bool bold)> font;
    };
    // Pushes a graphics object valid until endPaint(); later use raises an error.
    void beginPaint(PaintContext& context);
    void endPaint();

    // "#rgb", "#rrggbb", "#rrggbbaa", "rgb(r, g, b)", "rgba(r, g, b, a)", a
    // theme role ("accent", "textBody", ...) or a number 0xAARRGGBB.
    static bool parseColour(lua_State* L, int index, const ModernTheme& theme, juce::Colour& out);
    static juce::String colourToString(juce::Colour c);
    // The theme roles scripts can name, with their colours.
    static std::vector<std::pair<const char*, juce::Colour>> themeRoles(const ModernTheme& theme);

    // Tests: a fixed math.random seed for every new engine (nullopt: random).
    static void setFixedRandomSeed(std::optional<long long> seed);

    struct GraphicsState;   // the state behind a paint's graphics object

    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

private:
    static void* allocate(void* ud, void* ptr, size_t oldSize, size_t newSize);
    static void hook(lua_State* L, struct lua_Debug* ar);
    static int traceback(lua_State* L);
    static int luaPrint(lua_State* L);
    static int guardedCall(lua_State* L);
    void openSandbox();
    void registerGraphics();
    bool protectedCall(int nargs, int nresults, double budgetMs, const juce::String& what);
    void report(const juce::String& message);

    Limits limits;
    lua_State* L = nullptr;
    size_t memoryUsed = 0;
    double deadline = 0.0;   // ms counter; 0 = no call running
    int callDepth = 0;
    bool timedOut = false;   // the running call exceeded its budget
    GraphicsState* activeGraphics = nullptr;
};
