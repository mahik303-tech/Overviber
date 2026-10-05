#include "LuaEngine.h"

// Lua is built as C++ (see vst/Source/lua/README.md): no extern "C".
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
constexpr const char* kGraphicsType = "Overviber.Graphics";
constexpr int kHookInstructions = 1000;
constexpr int kMaxPathElements = 20000;
constexpr size_t kMaxTextLength = 4096;
constexpr int kMaxSaveDepth = 64;

std::optional<long long>& fixedSeed() {
    static std::optional<long long> seed;
    return seed;
}

LuaEngine* engineOf(lua_State* L) {
    return *static_cast<LuaEngine**>(lua_getextraspace(L));
}

// A finite number, limited to a range juce::Graphics handles without overflow.
float num(lua_State* L, int index) {
    const double v = luaL_checknumber(L, index);
    if (!std::isfinite(v)) luaL_argerror(L, index, "finite number expected");
    return (float)juce::jlimit(-1.0e6, 1.0e6, v);
}

float optNum(lua_State* L, int index, float fallback) {
    return lua_isnoneornil(L, index) ? fallback : num(L, index);
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parseHex(const char* s, size_t len, juce::Colour& out) {
    int d[8];
    for (size_t i = 0; i < len; ++i)
        if ((d[i] = hexDigit(s[i])) < 0) return false;
    auto byte = [&](int i) { return (juce::uint8)(d[i] * 16 + d[i + 1]); };
    if (len == 3 || len == 4) {
        auto nib = [&](int i) { return (juce::uint8)(d[i] * 17); };
        out = juce::Colour(nib(0), nib(1), nib(2), len == 4 ? nib(3) : (juce::uint8)255);
        return true;
    }
    if (len == 6 || len == 8) {
        out = juce::Colour(byte(0), byte(2), byte(4), len == 8 ? byte(6) : (juce::uint8)255);
        return true;
    }
    return false;
}

bool parseFunctional(const juce::String& text, juce::Colour& out) {
    const bool alpha = text.startsWithIgnoreCase("rgba(");
    if (!alpha && !text.startsWithIgnoreCase("rgb(")) return false;
    if (!text.endsWithChar(')')) return false;
    auto parts = juce::StringArray::fromTokens(text.fromFirstOccurrenceOf("(", false, false).dropLastCharacters(1), ",", "");
    if (parts.size() != (alpha ? 4 : 3)) return false;
    auto channel = [](const juce::String& p) { return (juce::uint8)juce::jlimit(0, 255, p.trim().getIntValue()); };
    float a = 1.0f;
    if (alpha) a = juce::jlimit(0.0f, 1.0f, parts[3].trim().getFloatValue());
    out = juce::Colour(channel(parts[0]), channel(parts[1]), channel(parts[2]), a);
    return true;
}
}

// ------------------------------------------------------------------------------
// Graphics object
// ------------------------------------------------------------------------------
struct LuaEngine::GraphicsState {
    explicit GraphicsState(PaintContext& c) : context(c) {}
    PaintContext& context;
    juce::Path path;
    // juce::Path::isEmpty() is still true after a lone startNewSubPath, so the
    // open subpath is tracked here: otherwise every lineTo would start a new one.
    bool subPathOpen = false;
    int pathElements = 0;
    int saveDepth = 0;
    struct Box { GraphicsState* state; }* box = nullptr;
};

namespace {
using GraphicsState = LuaEngine::GraphicsState;

GraphicsState& gfx(lua_State* L) {
    auto* box = static_cast<GraphicsState::Box*>(luaL_checkudata(L, 1, kGraphicsType));
    if (box->state == nullptr) luaL_error(L, "graphics object used outside its paint function");
    return *box->state;
}

juce::Graphics& g(lua_State* L) { return gfx(L).context.g; }

juce::Colour colourArg(lua_State* L, int index) {
    juce::Colour c;
    if (!LuaEngine::parseColour(L, index, gfx(L).context.theme, c))
        luaL_argerror(L, index, "colour expected (\"#rrggbb\", \"rgba(...)\" or a theme role)");
    return c;
}

juce::Justification justification(lua_State* L, int index) {
    const char* a = luaL_optstring(L, index, "left");
    if (std::strcmp(a, "left") == 0) return juce::Justification::centredLeft;
    if (std::strcmp(a, "centre") == 0 || std::strcmp(a, "center") == 0) return juce::Justification::centred;
    if (std::strcmp(a, "right") == 0) return juce::Justification::centredRight;
    if (std::strcmp(a, "topLeft") == 0) return juce::Justification::topLeft;
    if (std::strcmp(a, "top") == 0) return juce::Justification::centredTop;
    if (std::strcmp(a, "bottom") == 0) return juce::Justification::centredBottom;
    luaL_argerror(L, index, "\"left\", \"centre\", \"right\", \"topLeft\", \"top\" or \"bottom\" expected");
    return juce::Justification::centredLeft;
}

void addPathElement(lua_State* L, GraphicsState& s) {
    if (++s.pathElements > kMaxPathElements) luaL_error(L, "path too long (more than %d elements)", kMaxPathElements);
}

int gSetColour(lua_State* L) {
    auto c = colourArg(L, 2);
    if (!lua_isnoneornil(L, 3)) c = c.withMultipliedAlpha(juce::jlimit(0.0f, 1.0f, num(L, 3)));
    g(L).setColour(c);
    return 0;
}
int gSetOpacity(lua_State* L) { g(L).setOpacity(juce::jlimit(0.0f, 1.0f, num(L, 2))); return 0; }
int gFillAll(lua_State* L) {
    if (lua_isnoneornil(L, 2)) g(L).fillAll(); else g(L).fillAll(colourArg(L, 2));
    return 0;
}
int gFillRect(lua_State* L) { g(L).fillRect(num(L, 2), num(L, 3), num(L, 4), num(L, 5)); return 0; }
int gDrawRect(lua_State* L) { g(L).drawRect(num(L, 2), num(L, 3), num(L, 4), num(L, 5), optNum(L, 6, 1.0f)); return 0; }
int gFillRoundedRect(lua_State* L) {
    g(L).fillRoundedRectangle(num(L, 2), num(L, 3), num(L, 4), num(L, 5), num(L, 6));
    return 0;
}
int gDrawRoundedRect(lua_State* L) {
    g(L).drawRoundedRectangle(num(L, 2), num(L, 3), num(L, 4), num(L, 5), num(L, 6), optNum(L, 7, 1.0f));
    return 0;
}
int gDrawLine(lua_State* L) { g(L).drawLine(num(L, 2), num(L, 3), num(L, 4), num(L, 5), optNum(L, 6, 1.0f)); return 0; }
int gFillEllipse(lua_State* L) { g(L).fillEllipse(num(L, 2), num(L, 3), num(L, 4), num(L, 5)); return 0; }
int gDrawEllipse(lua_State* L) {
    g(L).drawEllipse(num(L, 2), num(L, 3), num(L, 4), num(L, 5), optNum(L, 6, 1.0f));
    return 0;
}

// g:drawText(text, x, y, w, h [, align [, size [, bold]]])
int gDrawText(lua_State* L) {
    auto& s = gfx(L);
    size_t len = 0;
    const char* text = luaL_checklstring(L, 2, &len);
    if (len > kMaxTextLength) luaL_argerror(L, 2, "text longer than 4096 bytes");
    const auto area = juce::Rectangle<float>(num(L, 3), num(L, 4), num(L, 5), num(L, 6));
    const auto just = justification(L, 7);
    const float size = juce::jlimit(4.0f, 96.0f, optNum(L, 8, 12.0f));
    const bool bold = lua_toboolean(L, 9) != 0;
    s.context.g.setFont(s.context.font(size, bold));
    s.context.g.drawText(juce::String::fromUTF8(text, (int)len), area, just, true);
    return 0;
}

// g:setGradient{ x1=, y1=, x2=, y2=, radial=false, stops = { {0, "#000"}, {1, "accent"} } }
int gSetGradient(lua_State* L) {
    auto& s = gfx(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    auto field = [&](const char* key, float fallback) {
        lua_getfield(L, 2, key);
        const float v = lua_isnil(L, -1) ? fallback : num(L, -1);
        lua_pop(L, 1);
        return v;
    };
    const float x1 = field("x1", 0.0f), y1 = field("y1", 0.0f), x2 = field("x2", 0.0f), y2 = field("y2", 1.0f);
    lua_getfield(L, 2, "radial");
    const bool radial = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getfield(L, 2, "stops");
    if (!lua_istable(L, -1)) luaL_error(L, "setGradient: 'stops' must be a table of {offset, colour}");
    const int stopsIndex = lua_gettop(L);
    const auto count = (int)luaL_len(L, stopsIndex);
    if (count < 2 || count > 32) luaL_error(L, "setGradient: 2 to 32 stops expected");
    juce::ColourGradient gradient;
    gradient.point1 = { x1, y1 };
    gradient.point2 = { x2, y2 };
    gradient.isRadial = radial;
    for (int i = 1; i <= count; ++i) {
        lua_rawgeti(L, stopsIndex, i);
        if (!lua_istable(L, -1)) luaL_error(L, "setGradient: stop %d is no {offset, colour} table", i);
        lua_rawgeti(L, -1, 1);
        lua_rawgeti(L, -2, 2);
        const float offset = juce::jlimit(0.0f, 1.0f, num(L, -2));
        juce::Colour c;
        if (!LuaEngine::parseColour(L, -1, s.context.theme, c)) luaL_error(L, "setGradient: stop %d has no colour", i);
        gradient.addColour(offset, c);
        lua_pop(L, 3);
    }
    lua_pop(L, 1);
    s.context.g.setGradientFill(gradient);
    return 0;
}

int gBeginPath(lua_State* L) {
    auto& s = gfx(L);
    s.path.clear();
    s.pathElements = 0;
    s.subPathOpen = false;
    return 0;
}
int gMoveTo(lua_State* L) {
    auto& s = gfx(L);
    addPathElement(L, s);
    s.path.startNewSubPath(num(L, 2), num(L, 3));
    s.subPathOpen = true;
    return 0;
}
int gLineTo(lua_State* L) {
    auto& s = gfx(L);
    addPathElement(L, s);
    const float x = num(L, 2), y = num(L, 3);
    if (s.subPathOpen) {
        s.path.lineTo(x, y);
    } else {
        s.path.startNewSubPath(x, y);
        s.subPathOpen = true;
    }
    return 0;
}
int gQuadTo(lua_State* L) {
    auto& s = gfx(L);
    addPathElement(L, s);
    if (!s.subPathOpen) luaL_error(L, "quadTo needs a moveTo or lineTo first");
    s.path.quadraticTo(num(L, 2), num(L, 3), num(L, 4), num(L, 5));
    return 0;
}
int gCubicTo(lua_State* L) {
    auto& s = gfx(L);
    addPathElement(L, s);
    if (!s.subPathOpen) luaL_error(L, "cubicTo needs a moveTo or lineTo first");
    s.path.cubicTo(num(L, 2), num(L, 3), num(L, 4), num(L, 5), num(L, 6), num(L, 7));
    return 0;
}
// g:arc(cx, cy, radius, fromRadians, toRadians): clockwise from 12 o'clock, as juce::Path.
int gArc(lua_State* L) {
    auto& s = gfx(L);
    addPathElement(L, s);
    const float r = std::abs(num(L, 4));
    s.path.addCentredArc(num(L, 2), num(L, 3), r, r, 0.0f, num(L, 5), num(L, 6), !s.subPathOpen);
    s.subPathOpen = true;
    return 0;
}
int gClosePath(lua_State* L) {
    auto& s = gfx(L);
    s.path.closeSubPath();
    s.subPathOpen = false;
    return 0;
}
int gStroke(lua_State* L) {
    auto& s = gfx(L);
    const float thickness = juce::jlimit(0.1f, 100.0f, optNum(L, 2, 1.0f));
    s.context.g.strokePath(s.path, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    return 0;
}
int gFill(lua_State* L) { auto& s = gfx(L); s.context.g.fillPath(s.path); return 0; }

int gSave(lua_State* L) {
    auto& s = gfx(L);
    if (s.saveDepth >= kMaxSaveDepth) luaL_error(L, "save: more than %d nested saves", kMaxSaveDepth);
    s.context.g.saveState();
    ++s.saveDepth;
    return 0;
}
int gRestore(lua_State* L) {
    auto& s = gfx(L);
    if (s.saveDepth == 0) luaL_error(L, "restore without save");
    s.context.g.restoreState();
    --s.saveDepth;
    return 0;
}
int gTranslate(lua_State* L) { g(L).addTransform(juce::AffineTransform::translation(num(L, 2), num(L, 3))); return 0; }
int gRotate(lua_State* L) {
    g(L).addTransform(juce::AffineTransform::rotation(num(L, 2), optNum(L, 3, 0.0f), optNum(L, 4, 0.0f)));
    return 0;
}
int gScale(lua_State* L) {
    const float sx = num(L, 2);
    g(L).addTransform(juce::AffineTransform::scale(sx, optNum(L, 3, sx)));
    return 0;
}
int gClip(lua_State* L) {
    g(L).reduceClipRegion(juce::Rectangle<float>(num(L, 2), num(L, 3), num(L, 4), num(L, 5)).toNearestInt());
    return 0;
}
int gTextWidth(lua_State* L) {
    auto& s = gfx(L);
    const char* text = luaL_checkstring(L, 2);
    const float size = juce::jlimit(4.0f, 96.0f, optNum(L, 3, 12.0f));
    const auto font = s.context.font(size, lua_toboolean(L, 4) != 0);
    lua_pushnumber(L, font.getStringWidthFloat(juce::String::fromUTF8(text)));
    return 1;
}
}

// ------------------------------------------------------------------------------
// Engine
// ------------------------------------------------------------------------------
void LuaEngine::setFixedRandomSeed(std::optional<long long> seed) { fixedSeed() = seed; }

LuaEngine::LuaEngine(Limits l) : limits(l) {
    L = lua_newstate(&LuaEngine::allocate, this);
    if (L == nullptr) return;
    *static_cast<LuaEngine**>(lua_getextraspace(L)) = this;
    lua_sethook(L, &LuaEngine::hook, LUA_MASKCOUNT, kHookInstructions);
    openSandbox();
    registerGraphics();
}

LuaEngine::~LuaEngine() {
    if (L != nullptr) lua_close(L);
}

void* LuaEngine::allocate(void* ud, void* ptr, size_t oldSize, size_t newSize) {
    auto* self = static_cast<LuaEngine*>(ud);
    const size_t old = ptr != nullptr ? oldSize : 0;
    if (newSize == 0) {
        std::free(ptr);
        self->memoryUsed -= old;
        return nullptr;
    }
    // Growing past the cap fails; Lua then raises "not enough memory".
    if (newSize > old && self->memoryUsed - old + newSize > self->limits.memoryBytes) return nullptr;
    void* block = std::realloc(ptr, newSize);
    if (block != nullptr) self->memoryUsed = self->memoryUsed - old + newSize;
    return block;
}

void LuaEngine::hook(lua_State* L, lua_Debug*) {
    auto* self = engineOf(L);
    if (self->timedOut || (self->deadline > 0.0 && juce::Time::getMillisecondCounterHiRes() > self->deadline)) {
        self->timedOut = true;
        luaL_error(L, "time limit exceeded");
    }
}

int LuaEngine::traceback(lua_State* L) {
    const char* message = lua_tostring(L, 1);
    if (message == nullptr) message = luaL_typename(L, 1);
    luaL_traceback(L, L, message, 1);
    return 1;
}

// pcall, xpcall and coroutine.resume catch errors. After a time-out they pass
// it on, so a script cannot catch "time limit exceeded" and keep running.
int LuaEngine::guardedCall(lua_State* L) {
    const int n = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, n, LUA_MULTRET);
    if (engineOf(L)->timedOut) luaL_error(L, "time limit exceeded");
    return lua_gettop(L);
}

int LuaEngine::luaPrint(lua_State* L) {
    juce::String line;
    const int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        if (i > 1) line << "  ";
        line << juce::String::fromUTF8(s, (int)juce::jmin(len, kMaxTextLength));
        lua_pop(L, 1);
    }
    auto* self = engineOf(L);
    if (self->onLog) self->onLog(line);
    return 0;
}

void LuaEngine::openSandbox() {
    const luaL_Reg libs[] = {
        { LUA_GNAME, luaopen_base },         { LUA_TABLIBNAME, luaopen_table },
        { LUA_STRLIBNAME, luaopen_string },  { LUA_MATHLIBNAME, luaopen_math },
        { LUA_UTF8LIBNAME, luaopen_utf8 },   { LUA_COLIBNAME, luaopen_coroutine },
    };
    for (const auto& lib : libs) {
        luaL_requiref(L, lib.name, lib.func, 1);
        lua_pop(L, 1);
    }
    for (const char* name : { "dofile", "loadfile", "load", "require", "collectgarbage" }) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
    lua_getglobal(L, LUA_STRLIBNAME);
    lua_pushnil(L);
    lua_setfield(L, -2, "dump");
    lua_pop(L, 1);

    auto guard = [this](const char* table, const char* name) {
        if (table != nullptr) lua_getglobal(L, table); else lua_pushglobaltable(L);
        lua_getfield(L, -1, name);
        lua_pushcclosure(L, &LuaEngine::guardedCall, 1);
        lua_setfield(L, -2, name);
        lua_pop(L, 1);
    };
    guard(nullptr, "pcall");
    guard(nullptr, "xpcall");
    guard(LUA_COLIBNAME, "resume");

    lua_pushcfunction(L, &LuaEngine::luaPrint);
    lua_setglobal(L, "print");
    lua_pushcfunction(L, &LuaEngine::luaPrint);
    lua_setglobal(L, "log");

    if (fixedSeed().has_value()) {
        lua_getglobal(L, LUA_MATHLIBNAME);
        lua_getfield(L, -1, "randomseed");
        lua_pushinteger(L, (lua_Integer)*fixedSeed());
        lua_pcall(L, 1, 0, 0);
        lua_pop(L, 1);
    }
}

void LuaEngine::registerGraphics() {
    const luaL_Reg methods[] = {
        { "setColour", gSetColour },   { "setColor", gSetColour },     { "setOpacity", gSetOpacity },
        { "fillAll", gFillAll },       { "fillRect", gFillRect },      { "drawRect", gDrawRect },
        { "fillRoundedRect", gFillRoundedRect }, { "drawRoundedRect", gDrawRoundedRect },
        { "drawLine", gDrawLine },     { "fillEllipse", gFillEllipse }, { "drawEllipse", gDrawEllipse },
        { "drawText", gDrawText },     { "textWidth", gTextWidth },    { "setGradient", gSetGradient },
        { "beginPath", gBeginPath },   { "moveTo", gMoveTo },          { "lineTo", gLineTo },
        { "quadTo", gQuadTo },         { "cubicTo", gCubicTo },        { "arc", gArc },
        { "closePath", gClosePath },   { "stroke", gStroke },          { "fill", gFill },
        { "save", gSave },             { "restore", gRestore },        { "translate", gTranslate },
        { "rotate", gRotate },         { "scale", gScale },            { "clip", gClip },
        { nullptr, nullptr },
    };
    luaL_newmetatable(L, kGraphicsType);
    luaL_newlib(L, methods);
    lua_setfield(L, -2, "__index");
    lua_pushstring(L, "locked");
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
}

void LuaEngine::report(const juce::String& message) {
    if (onError) onError(message);
}

bool LuaEngine::protectedCall(int nargs, int nresults, double budgetMs, const juce::String& what) {
    // The message handler sits below the function and its arguments.
    const int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, &LuaEngine::traceback);
    lua_insert(L, base);
    const bool outermost = callDepth++ == 0;
    if (outermost) {
        deadline = juce::Time::getMillisecondCounterHiRes() + budgetMs;
        timedOut = false;
    }
    const int status = lua_pcall(L, nargs, nresults, base);
    if (outermost) { deadline = 0.0; timedOut = false; }
    --callDepth;
    lua_remove(L, base);
    if (status == LUA_OK) return true;
    const char* message = lua_tostring(L, -1);
    juce::String text = message != nullptr ? juce::String::fromUTF8(message) : juce::String("unknown error");
    if (status == LUA_ERRMEM) text = "out of memory (limit " + juce::String((int)(limits.memoryBytes >> 20)) + " MB)";
    lua_pop(L, 1);
    report(what.isEmpty() ? text : what + ": " + text);
    return false;
}

bool LuaEngine::run(const juce::String& source, const juce::String& chunkName) {
    if (L == nullptr) { report("Lua could not start"); return false; }
    const auto utf8 = source.toStdString();
    const auto name = "=" + chunkName.toStdString();
    const int status = luaL_loadbufferx(L, utf8.data(), utf8.size(), name.c_str(), "t");
    if (status != LUA_OK) {
        const char* message = lua_tostring(L, -1);
        report(message != nullptr ? juce::String::fromUTF8(message) : juce::String("syntax error"));
        lua_pop(L, 1);
        return false;
    }
    return protectedCall(0, 0, limits.loadBudgetMs, {});
}

bool LuaEngine::call(int ref, const std::function<int(lua_State*)>& pushArgs, bool wantResult) {
    if (L == nullptr || ref == noRef) return false;
    if (!lua_checkstack(L, 32)) return false;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return false; }
    const int nargs = pushArgs ? pushArgs(L) : 0;
    const double budget = callDepth > 0 ? 0.0 : limits.callBudgetMs;
    return protectedCall(nargs, wantResult ? 1 : 0, budget, {});
}

int LuaEngine::makeFunctionRef(int index) {
    if (!lua_isfunction(L, index)) return noRef;
    lua_pushvalue(L, index);
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

void LuaEngine::releaseRef(int ref) {
    if (L != nullptr && ref != noRef) luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

void LuaEngine::beginPaint(PaintContext& context) {
    jassert(activeGraphics == nullptr);
    activeGraphics = new GraphicsState(context);
    auto* box = static_cast<GraphicsState::Box*>(lua_newuserdatauv(L, sizeof(GraphicsState::Box), 0));
    box->state = activeGraphics;
    activeGraphics->box = box;
    luaL_setmetatable(L, kGraphicsType);
}

void LuaEngine::endPaint() {
    if (activeGraphics == nullptr) return;
    // A script that returned (or failed) inside save() leaves no state behind.
    while (activeGraphics->saveDepth-- > 0) activeGraphics->context.g.restoreState();
    // The userdata may outlive the paint (a script can store it); it now raises errors.
    activeGraphics->box->state = nullptr;
    delete activeGraphics;
    activeGraphics = nullptr;
}

// ------------------------------------------------------------------------------
// Colours
// ------------------------------------------------------------------------------
std::vector<std::pair<const char*, juce::Colour>> LuaEngine::themeRoles(const ModernTheme& t) {
    return {
        { "accent", t.accent },           { "accentDark", t.accentDark },     { "accentGlow", t.accentGlow },
        { "windowBg", t.windowBg },       { "cardBg", t.cardBg },             { "cardHeader", t.cardHeader },
        { "cardBorder", t.cardBorder },   { "knobTrack", t.knobTrack },       { "knobNeedle", t.knobNeedle },
        { "buttonBg", t.buttonBg },       { "buttonBorder", t.buttonBorder }, { "textTitle", t.textTitle },
        { "textBody", t.textBody },       { "textMuted", t.textMuted },       { "visualizerGrid", t.visualizerGrid },
        { "visualizerCurve", t.visualizerCurve }, { "visualizerFill", t.visualizerFill },
        { "meterActive", t.meterActive },
    };
}

juce::String LuaEngine::colourToString(juce::Colour c) {
    auto hex2 = [](juce::uint8 v) { return juce::String::toHexString((int)v).paddedLeft('0', 2); };
    juce::String s = "#" + hex2(c.getRed()) + hex2(c.getGreen()) + hex2(c.getBlue());
    if (c.getAlpha() != 255) s << hex2(c.getAlpha());
    return s;
}

bool LuaEngine::parseColour(lua_State* L, int index, const ModernTheme& theme, juce::Colour& out) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        if (!lua_isinteger(L, index)) return false;
        out = juce::Colour((juce::uint32)(lua_tointeger(L, index) & 0xffffffff));
        return true;
    }
    if (lua_type(L, index) != LUA_TSTRING) return false;
    size_t len = 0;
    const char* s = lua_tolstring(L, index, &len);
    if (len > 64) return false;
    if (len > 0 && s[0] == '#') return parseHex(s + 1, len - 1, out);
    const auto text = juce::String::fromUTF8(s, (int)len);
    if (parseFunctional(text, out)) return true;
    for (const auto& [name, colour] : themeRoles(theme))
        if (text == name) { out = colour; return true; }
    return false;
}
