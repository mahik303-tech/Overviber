#include "LuaTab.h"
#include "../components/ModernSectionCard.h"
#include "../../data/OverviberPaths.h"
#include "../../data/PresetManager.h"
#include "LuaScripts.h"
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
#include "../../PluginProcessor.h"
#endif

#include "lua.h"
#include "lauxlib.h"

#include <cstring>
#include <string>

namespace {
constexpr const char* kWidgetType = "Overviber.Widget";
constexpr int kToolbarH = 30;
constexpr int kLogH = 170;
constexpr int kMaxLogLines = 300;
constexpr int kMaxWidgets = 400;
constexpr int kMaxListeners = 512;
constexpr juce::int64 kMaxScriptBytes = 1024 * 1024;

// Host parameter IDs (the APVTS IDs, e.g. "cpCutoff", "spFilterModel").
struct ParamRef {
    bool stepped;
    int index;
};

const std::map<std::string, ParamRef>& paramIndex() {
    static const auto index = [] {
        std::map<std::string, ParamRef> m;
        for (int i = 0; i < cpCount; ++i)
            if (const char* n = PresetManager::getContinuousParamName((continuousParameter_t)i); n != nullptr && *n != 0)
                m.emplace(n, ParamRef{ false, i });
        for (int i = 0; i < spCount; ++i)
            if (const char* n = PresetManager::getSteppedParamName((steppedParameter_t)i); n != nullptr && *n != 0)
                m.emplace(n, ParamRef{ true, i });
        return m;
    }();
    return index;
}

const char* paramId(bool stepped, int index) {
    return stepped ? PresetManager::getSteppedParamName((steppedParameter_t)index)
                   : PresetManager::getContinuousParamName((continuousParameter_t)index);
}

int clampCoord(lua_Number v) { return (int)juce::jlimit(-4000.0, 4000.0, std::isfinite(v) ? v : 0.0); }
}

// ==============================================================================
// Canvas: a component the script paints
// ==============================================================================
class LuaTab::Canvas : public juce::Component {
public:
    Canvas(LuaTab& t, int i) : owner(t), index(i) { setOpaque(false); }
    void paint(juce::Graphics& g) override { owner.paintCanvas(index, g); }
    void mouseDown(const juce::MouseEvent& e) override { owner.mouseCanvas(index, "down", e); }
    void mouseDrag(const juce::MouseEvent& e) override { owner.mouseCanvas(index, "drag", e); }
    void mouseUp(const juce::MouseEvent& e) override { owner.mouseCanvas(index, "up", e); }
    void mouseDoubleClick(const juce::MouseEvent& e) override { owner.mouseCanvas(index, "double", e); }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        owner.mouseCanvas(index, "wheel", e, wheel.deltaY);
    }

private:
    LuaTab& owner;
    const int index;
};

// ==============================================================================
// Lua bindings: static functions with the tab as their first upvalue
// ==============================================================================
struct LuaTab::Binding {
    static LuaTab& tab(lua_State* L) { return *static_cast<LuaTab*>(lua_touserdata(L, lua_upvalueindex(1))); }

    static void registerLib(lua_State* L, LuaTab& t, const char* name, const luaL_Reg* functions) {
        lua_newtable(L);
        lua_pushlightuserdata(L, &t);
        luaL_setfuncs(L, functions, 1);
        lua_setglobal(L, name);
    }

    static void registerAll(lua_State* L, LuaTab& t) {
        const luaL_Reg ui[] = {
            { "canvas", uiCanvas }, { "button", uiButton }, { "label", uiLabel }, { "knob", uiKnob },
            { "card", uiCard }, { "size", uiSize }, { "setFrameRate", uiSetFrameRate }, { "onFrame", uiOnFrame },
            { nullptr, nullptr },
        };
        const luaL_Reg params[] = {
            { "get", pGet }, { "set", pSet }, { "getText", pGetText }, { "getInfo", pGetInfo }, { "list", pList },
            { "onChange", pOnChange }, { "removeListener", pRemoveListener },
            { nullptr, nullptr },
        };
        const luaL_Reg theme[] = {
            { "get", tGet }, { "colour", tColour }, { "color", tColour }, { "onChange", tOnChange },
            { nullptr, nullptr },
        };
        const luaL_Reg widget[] = {
            { "repaint", wRepaint }, { "setBounds", wSetBounds }, { "getBounds", wGetBounds },
            { "setVisible", wSetVisible }, { "setText", wSetText }, { "getText", wGetText },
            { "setToggled", wSetToggled }, { "isToggled", wIsToggled }, { "getId", wGetId },
            { nullptr, nullptr },
        };
        registerLib(L, t, "ui", ui);
        registerLib(L, t, "params", params);
        registerLib(L, t, "theme", theme);

        luaL_newmetatable(L, kWidgetType);
        lua_newtable(L);
        lua_pushlightuserdata(L, &t);
        luaL_setfuncs(L, widget, 1);
        lua_setfield(L, -2, "__index");
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
        lua_pop(L, 1);
    }

    // ---- Argument tables
    static lua_Number number(lua_State* L, const char* key, lua_Number fallback) {
        lua_getfield(L, 1, key);
        lua_Number v = fallback;
        if (!lua_isnil(L, -1)) {
            if (!lua_isnumber(L, -1)) luaL_error(L, "'%s' must be a number", key);
            v = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
        return v;
    }
    static juce::String string(lua_State* L, const char* key, const char* fallback = "") {
        lua_getfield(L, 1, key);
        juce::String v = fallback;
        if (!lua_isnil(L, -1)) {
            if (!lua_isstring(L, -1)) luaL_error(L, "'%s' must be a string", key);
            v = juce::String::fromUTF8(lua_tostring(L, -1)).substring(0, 256);
        }
        lua_pop(L, 1);
        return v;
    }
    static bool boolean(lua_State* L, const char* key) {
        lua_getfield(L, 1, key);
        const bool v = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return v;
    }
    static int function(lua_State* L, LuaTab& t, const char* key) {
        lua_getfield(L, 1, key);
        if (!lua_isnil(L, -1) && !lua_isfunction(L, -1)) luaL_error(L, "'%s' must be a function", key);
        const int ref = t.engine->makeFunctionRef(-1);
        lua_pop(L, 1);
        return ref;
    }
    static juce::Rectangle<int> bounds(lua_State* L, int defaultW, int defaultH) {
        return { clampCoord(number(L, "x", 0)), clampCoord(number(L, "y", 0)),
                 clampCoord(number(L, "w", defaultW)), clampCoord(number(L, "h", defaultH)) };
    }

    // ---- Widgets
    static Widget& newWidget(lua_State* L, LuaTab& t, Kind kind, const char* prefix) {
        luaL_checktype(L, 1, LUA_TTABLE);
        if ((int)t.widgets.size() >= kMaxWidgets) luaL_error(L, "more than %d widgets", kMaxWidgets);
        auto w = std::make_unique<Widget>();
        w->kind = kind;
        w->id = string(L, "id");
        if (w->id.isEmpty()) w->id = prefix + juce::String((int)t.widgets.size() + 1);
        t.widgets.push_back(std::move(w));
        return *t.widgets.back();
    }

    // Pushes the handle of the newest widget.
    static int pushHandle(lua_State* L, LuaTab& t) {
        auto* index = static_cast<int*>(lua_newuserdatauv(L, sizeof(int), 0));
        *index = (int)t.widgets.size() - 1;
        luaL_setmetatable(L, kWidgetType);
        return 1;
    }

    static void place(LuaTab& t, Widget& w, juce::Rectangle<int> r) {
        w.component->setComponentID("lua:" + w.id);
        w.component->setBounds(r);
        t.scriptArea.addAndMakeVisible(*w.component);
    }

    // ui.canvas{ id=, x=, y=, w=, h=, paint = function(g, w, h) end, mouse = function(event, x, y, wheel) end }
    static int uiCanvas(lua_State* L) {
        auto& t = tab(L);
        auto& w = newWidget(L, t, Kind::Canvas, "canvas");
        w.paintRef = function(L, t, "paint");
        w.mouseRef = function(L, t, "mouse");
        w.component = std::make_unique<Canvas>(t, (int)t.widgets.size() - 1);
        w.component->setInterceptsMouseClicks(w.mouseRef != LuaEngine::noRef, false);
        place(t, w, bounds(L, 100, 100));
        return pushHandle(L, t);
    }

    // ui.button{ id=, text=, x=, y=, w=, h=, toggle=false, onClick = function(toggled) end }
    static int uiButton(lua_State* L) {
        auto& t = tab(L);
        auto& w = newWidget(L, t, Kind::Button, "button");
        auto button = std::make_unique<juce::TextButton>(string(L, "text", "BUTTON"));
        button->setClickingTogglesState(boolean(L, "toggle"));
        w.clickRef = function(L, t, "onClick");
        const int index = (int)t.widgets.size() - 1;
        auto* raw = button.get();
        button->onClick = [&t, index, raw] {
            auto* widget = t.widgetAt(index);
            if (widget == nullptr || widget->failed || t.engine == nullptr || t.engine->isInCall()) return;
            const bool toggled = raw->getToggleState();
            if (!t.engine->call(widget->clickRef, [toggled](lua_State* s) { lua_pushboolean(s, toggled); return 1; }))
                widget->failed = t.engine != nullptr && widget->clickRef != LuaEngine::noRef;
        };
        w.component = std::move(button);
        place(t, w, bounds(L, 90, 26));
        return pushHandle(L, t);
    }

    // ui.label{ id=, text=, x=, y=, w=, h=, size=12, bold=false, colour="textBody", align="left" }
    static int uiLabel(lua_State* L) {
        auto& t = tab(L);
        auto& w = newWidget(L, t, Kind::Label, "label");
        auto label = std::make_unique<juce::Label>("", string(L, "text"));
        const float size = juce::jlimit(4.0f, 96.0f, (float)number(L, "size", 12));
        label->setFont(t.modernLnf.getCustomFont(size, boolean(L, "bold") ? juce::Font::bold : juce::Font::plain));
        lua_getfield(L, 1, "colour");
        juce::Colour colour = t.modernLnf.getTheme().textBody;
        if (!lua_isnil(L, -1) && !LuaEngine::parseColour(L, -1, t.modernLnf.getTheme(), colour))
            luaL_error(L, "label: 'colour' is no colour");
        if (lua_type(L, -1) == LUA_TSTRING) label->getProperties().set("luaColour", juce::String::fromUTF8(lua_tostring(L, -1)));
        lua_pop(L, 1);
        label->setColour(juce::Label::textColourId, colour);
        const auto align = string(L, "align", "left");
        label->setJustificationType(align == "centre" || align == "center" ? juce::Justification::centred
                                    : align == "right"                     ? juce::Justification::centredRight
                                                                           : juce::Justification::centredLeft);
        label->setInterceptsMouseClicks(false, false);
        w.component = std::move(label);
        place(t, w, bounds(L, 120, 20));
        return pushHandle(L, t);
    }

    // ui.knob{ id=, param="cpCutoff", x=, y=, size=55, caption="CUTOFF", mode="percent" }
    static int uiKnob(lua_State* L) {
        auto& t = tab(L);
        const auto id = string(L, "param").toStdString();
        const auto it = paramIndex().find(id);
        if (it == paramIndex().end() || it->second.stepped)
            luaL_error(L, "knob: '%s' is no continuous parameter (see params.list())", id.c_str());
        const auto cp = (continuousParameter_t)it->second.index;
        auto& w = newWidget(L, t, Kind::Knob, "knob");
        w.param = cp;
        const auto mode = string(L, "mode", PresetManager::isContinuousParamZeroCentered(cp) ? "bipolar" : "percent");
        const auto knobMode = mode == "bipolar"     ? KnobMode::BipolarPercent
                              : mode == "cutoff"    ? KnobMode::CutoffHz
                              : mode == "time"      ? KnobMode::TimeMs
                              : mode == "semitones" ? KnobMode::PitchSemitones
                              : mode == "raw"       ? KnobMode::Raw
                                                    : KnobMode::Percent;
        auto knob = t.createKnob("lua" + w.id, 0, 999, t.readParam(false, cp), knobMode);
        auto* raw = knob.get();
        knob->onValueChange = [&t, raw, cp] { t.setContinuousParam(cp, (float)raw->getValue()); };
        w.component = std::move(knob);
        const int size = juce::jlimit(25, 200, (int)number(L, "size", t.getStandardKnobSize()));
        const int x = clampCoord(number(L, "x", 0)), y = clampCoord(number(L, "y", 0));
        place(t, w, { x, y, size, size });
        const auto caption = string(L, "caption");
        if (caption.isNotEmpty()) {
            w.caption = t.createLabel(caption, t.scriptArea);
            layoutKnob(w.component, w.caption, x, y, size);
        }
        return pushHandle(L, t);
    }

    // ui.card{ id=, title=, badge=, x=, y=, w=, h= }: a section card behind other widgets
    static int uiCard(lua_State* L) {
        auto& t = tab(L);
        auto& w = newWidget(L, t, Kind::Card, "card");
        w.component = std::make_unique<ModernSectionCard>(string(L, "title"), string(L, "badge"));
        w.component->setInterceptsMouseClicks(false, false);
        place(t, w, bounds(L, 200, 120));
        w.component->toBack();
        return pushHandle(L, t);
    }

    static int uiSize(lua_State* L) {
        auto& t = tab(L);
        lua_pushinteger(L, t.scriptArea.getWidth());
        lua_pushinteger(L, t.scriptArea.getHeight());
        return 2;
    }

    // ui.setFrameRate(hz): 0 (default) stops the frame callback, at most 60.
    static int uiSetFrameRate(lua_State* L) {
        auto& t = tab(L);
        t.frameHz = (int)juce::jlimit(0.0, 60.0, (double)luaL_checknumber(L, 1));
        t.updateTimer();
        return 0;
    }

    // ui.onFrame(function(dt, time) end), nil to remove
    static int uiOnFrame(lua_State* L) {
        auto& t = tab(L);
        if (!lua_isnoneornil(L, 1)) luaL_checktype(L, 1, LUA_TFUNCTION);
        t.engine->releaseRef(t.frameRef);
        t.frameRef = t.engine->makeFunctionRef(1);
        return 0;
    }

    // ---- Widget handles
    static Widget& widget(lua_State* L) {
        auto& t = tab(L);
        const int index = *static_cast<int*>(luaL_checkudata(L, 1, kWidgetType));
        auto* w = t.widgetAt(index);
        if (w == nullptr || w->component == nullptr) luaL_error(L, "widget no longer exists");
        return *w;
    }
    static int wRepaint(lua_State* L) { widget(L).component->repaint(); return 0; }
    static int wSetBounds(lua_State* L) {
        auto& w = widget(L);
        const juce::Rectangle<int> r{ clampCoord(luaL_checknumber(L, 2)), clampCoord(luaL_checknumber(L, 3)),
                                      clampCoord(luaL_checknumber(L, 4)), clampCoord(luaL_checknumber(L, 5)) };
        w.component->setBounds(r);
        if (w.caption != nullptr) layoutKnob(w.component, w.caption, r.getX(), r.getY(), r.getWidth());
        return 0;
    }
    static int wGetBounds(lua_State* L) {
        const auto r = widget(L).component->getBounds();
        lua_pushinteger(L, r.getX()); lua_pushinteger(L, r.getY());
        lua_pushinteger(L, r.getWidth()); lua_pushinteger(L, r.getHeight());
        return 4;
    }
    static int wSetVisible(lua_State* L) {
        auto& w = widget(L);
        const bool visible = lua_toboolean(L, 2) != 0;
        w.component->setVisible(visible);
        if (w.caption != nullptr) w.caption->setVisible(visible);
        return 0;
    }
    static int wSetText(lua_State* L) {
        auto& w = widget(L);
        const auto text = juce::String::fromUTF8(luaL_checkstring(L, 2)).substring(0, 1024);
        if (auto* label = dynamic_cast<juce::Label*>(w.component.get())) label->setText(text, juce::dontSendNotification);
        else if (auto* button = dynamic_cast<juce::Button*>(w.component.get())) button->setButtonText(text);
        else if (w.caption != nullptr) w.caption->setText(text, juce::dontSendNotification);
        else luaL_error(L, "setText: this widget has no text");
        return 0;
    }
    static int wGetText(lua_State* L) {
        auto& w = widget(L);
        juce::String text;
        if (auto* label = dynamic_cast<juce::Label*>(w.component.get())) text = label->getText();
        else if (auto* button = dynamic_cast<juce::Button*>(w.component.get())) text = button->getButtonText();
        else if (w.caption != nullptr) text = w.caption->getText();
        lua_pushstring(L, text.toRawUTF8());
        return 1;
    }
    static int wSetToggled(lua_State* L) {
        auto* button = dynamic_cast<juce::Button*>(widget(L).component.get());
        if (button == nullptr) luaL_error(L, "setToggled: no button");
        button->setToggleState(lua_toboolean(L, 2) != 0, juce::dontSendNotification);
        return 0;
    }
    static int wIsToggled(lua_State* L) {
        auto* button = dynamic_cast<juce::Button*>(widget(L).component.get());
        lua_pushboolean(L, button != nullptr && button->getToggleState());
        return 1;
    }
    static int wGetId(lua_State* L) { lua_pushstring(L, widget(L).id.toRawUTF8()); return 1; }

    // ---- params
    static ParamRef param(lua_State* L, int index) {
        const char* id = luaL_checkstring(L, index);
        const auto it = paramIndex().find(id);
        if (it == paramIndex().end()) luaL_error(L, "unknown parameter '%s' (see params.list())", id);
        return it->second;
    }

    struct SteppedInfo {
        int max = 255;
        juce::StringArray choices;
    };
    static SteppedInfo steppedInfo(LuaTab& t, int sp) {
        SteppedInfo info;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
        if (t.processor != nullptr) {
            auto* p = t.processor->getAPVTS().getParameter(paramId(true, sp));
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(p)) info.choices = choice->choices;
            else if (dynamic_cast<juce::AudioParameterBool*>(p) != nullptr) info.choices = { "Off", "On" };
            if (!info.choices.isEmpty()) info.max = info.choices.size() - 1;
        }
#else
        juce::ignoreUnused(t, sp);
#endif
        return info;
    }

    static int pGet(lua_State* L) {
        auto& t = tab(L);
        const auto p = param(L, 1);
        lua_pushinteger(L, t.readParam(p.stepped, p.index));
        return 1;
    }

    static int pSet(lua_State* L) {
        auto& t = tab(L);
        const auto p = param(L, 1);
        const lua_Number v = luaL_checknumber(L, 2);
        if (!std::isfinite(v)) luaL_argerror(L, 2, "finite number expected");
        if (p.stepped) {
            const int max = steppedInfo(t, p.index).max;
            t.setSteppedParam((steppedParameter_t)p.index, (uint8_t)juce::jlimit(0, max, (int)std::lround(v)));
        } else {
            t.setContinuousParam((continuousParameter_t)p.index, (float)juce::jlimit(0.0, 999.0, (double)v));
        }
        return 0;
    }

    static int pGetText(lua_State* L) {
        auto& t = tab(L);
        const auto p = param(L, 1);
        const int value = t.readParam(p.stepped, p.index);
        juce::String text;
        if (p.stepped) {
            const auto info = steppedInfo(t, p.index);
            text = juce::isPositiveAndBelow(value, info.choices.size()) ? info.choices[value] : juce::String(value);
        } else {
            const auto cp = (continuousParameter_t)p.index;
#if !defined(MODERN_SKIN_DESIGNER_STANDALONE)
            if (t.processor != nullptr)
                if (auto* hp = t.processor->getAPVTS().getParameter(paramId(false, p.index))) {
                    const float v = OvercyclerAudioProcessor::potToParamVal(cp, (float)value);
                    text = (hp->getText(hp->convertTo0to1(v), 16) + " " + hp->getLabel()).trim();
                }
#endif
            if (text.isEmpty()) {
                const bool bipolar = PresetManager::isContinuousParamZeroCentered(cp);
                const int pct = bipolar ? (int)std::lround((value - 500) * 100.0 / 499.0) : (int)std::lround(value * 100.0 / 999.0);
                text = (bipolar && pct > 0 ? "+" : "") + juce::String(pct) + " %";
            }
        }
        lua_pushstring(L, text.toRawUTF8());
        return 1;
    }

    static int pGetInfo(lua_State* L) {
        auto& t = tab(L);
        const auto p = param(L, 1);
        lua_newtable(L);
        lua_pushstring(L, paramId(p.stepped, p.index));
        lua_setfield(L, -2, "id");
        lua_pushstring(L, p.stepped ? PresetManager::getSteppedParamDisplayName((steppedParameter_t)p.index)
                                    : PresetManager::getContinuousParamDisplayName((continuousParameter_t)p.index));
        lua_setfield(L, -2, "name");
        lua_pushstring(L, p.stepped ? "stepped" : "continuous");
        lua_setfield(L, -2, "kind");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "min");
        if (p.stepped) {
            const auto info = steppedInfo(t, p.index);
            lua_pushinteger(L, info.max);
            lua_setfield(L, -2, "max");
            lua_createtable(L, info.choices.size(), 0);
            for (int i = 0; i < info.choices.size(); ++i) {
                lua_pushstring(L, info.choices[i].toRawUTF8());
                lua_rawseti(L, -2, i + 1);
            }
            lua_setfield(L, -2, "choices");
        } else {
            lua_pushinteger(L, 999);
            lua_setfield(L, -2, "max");
            lua_pushboolean(L, PresetManager::isContinuousParamZeroCentered((continuousParameter_t)p.index));
            lua_setfield(L, -2, "bipolar");
        }
        return 1;
    }

    static int pList(lua_State* L) {
        lua_createtable(L, (int)paramIndex().size(), 0);
        int i = 0;
        for (bool stepped : { false, true })
            for (int p = 0; p < (stepped ? (int)spCount : (int)cpCount); ++p)
                if (const char* id = paramId(stepped, p); id != nullptr && *id != 0) {
                    lua_pushstring(L, id);
                    lua_rawseti(L, -2, ++i);
                }
        return 1;
    }

    // params.onChange(id, function(value, id) end) -> listener id
    static int pOnChange(lua_State* L) {
        auto& t = tab(L);
        const auto p = param(L, 1);
        luaL_checktype(L, 2, LUA_TFUNCTION);
        if ((int)t.listeners.size() >= kMaxListeners) luaL_error(L, "more than %d listeners", kMaxListeners);
        const int id = t.nextListenerId++;
        t.listeners.push_back({ id, p.stepped, p.index, t.engine->makeFunctionRef(2), t.readParam(p.stepped, p.index) });
        lua_pushinteger(L, id);
        return 1;
    }

    static int pRemoveListener(lua_State* L) {
        auto& t = tab(L);
        const auto id = (int)luaL_checkinteger(L, 1);
        bool removed = false;
        for (auto& l : t.listeners)
            if (l.id == id && l.ref != LuaEngine::noRef) {
                t.engine->releaseRef(l.ref);
                l.ref = LuaEngine::noRef;   // erased after the running poll
                removed = true;
            }
        lua_pushboolean(L, removed);
        return 1;
    }

    // ---- theme
    static int tGet(lua_State* L) {
        auto& t = tab(L);
        const auto roles = LuaEngine::themeRoles(t.modernLnf.getTheme());
        lua_createtable(L, 0, (int)roles.size() + 1);
        for (const auto& [name, colour] : roles) {
            lua_pushstring(L, LuaEngine::colourToString(colour).toRawUTF8());
            lua_setfield(L, -2, name);
        }
        lua_pushstring(L, t.modernLnf.getTheme().name.toRawUTF8());
        lua_setfield(L, -2, "name");
        return 1;
    }
    // theme.colour(role [, alpha]) -> "#rrggbb" or "#rrggbbaa"
    static int tColour(lua_State* L) {
        auto& t = tab(L);
        juce::Colour c;
        if (!LuaEngine::parseColour(L, 1, t.modernLnf.getTheme(), c)) luaL_argerror(L, 1, "theme role or colour expected");
        if (!lua_isnoneornil(L, 2)) c = c.withAlpha((float)juce::jlimit(0.0, 1.0, luaL_checknumber(L, 2)));
        lua_pushstring(L, LuaEngine::colourToString(c).toRawUTF8());
        return 1;
    }
    static int tOnChange(lua_State* L) {
        auto& t = tab(L);
        luaL_checktype(L, 1, LUA_TFUNCTION);
        if ((int)t.themeRefs.size() >= 64) luaL_error(L, "more than 64 theme listeners");
        t.themeRefs.push_back(t.engine->makeFunctionRef(1));
        return 0;
    }
};

// ==============================================================================
// LuaTab
// ==============================================================================
LuaTab::LuaTab(ModernTabContext& ctx) : ModernTabModule(ctx) {}

LuaTab::~LuaTab() {
    unload();
}

juce::File LuaTab::getScriptFile() {
    return OverviberPaths::getLuaDirectory().getChildFile("skin.lua");
}

juce::String LuaTab::getBuiltInScript() {
    return juce::String::fromUTF8(LuaScripts::LuaDefaultSkin_lua, LuaScripts::LuaDefaultSkin_luaSize);
}

void LuaTab::setup() {
    statusLabel.setComponentID("luaStatus");
    statusLabel.setFont(modernLnf.getCustomFont(10.5f, juce::Font::bold));
    statusLabel.setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
    statusLabel.setText("LUA SCRIPT: NOT LOADED", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    reloadButton.setComponentID("luaReloadButton");
    reloadButton.setTooltip("Run the script again (skin.lua, or the built-in example)");
    reloadButton.onClick = [this] { reload(); };
    folderButton.setComponentID("luaFolderButton");
    folderButton.setTooltip("Show skin.lua; creates it from the built-in example first");
    folderButton.onClick = [this] {
        auto file = getScriptFile();
        if (!file.existsAsFile()) {
            file.getParentDirectory().createDirectory();
            file.replaceWithText(getBuiltInScript());
            reload();
        }
        file.revealToUser();
    };
    logButton.setComponentID("luaLogButton");
    logButton.setClickingTogglesState(true);
    logButton.setTooltip("Show the script's output and errors");
    logButton.onClick = [this] {
        logView.setVisible(logButton.getToggleState());
        resized();
    };
    for (auto* b : { &reloadButton, &folderButton, &logButton }) addAndMakeVisible(*b);

    logView.setComponentID("luaLog");
    logView.setMultiLine(true);
    logView.setReadOnly(true);
    logView.setScrollbarsShown(true);
    logView.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    addChildComponent(logView);

    scriptArea.setComponentID("luaScriptArea");
    addAndMakeVisible(scriptArea);
}

void LuaTab::resized() {
    auto r = getLocalBounds();
    auto bar = r.removeFromTop(kToolbarH);
    logButton.setBounds(bar.removeFromRight(60).reduced(0, 2));
    bar.removeFromRight(5);
    folderButton.setBounds(bar.removeFromRight(120).reduced(0, 2));
    bar.removeFromRight(5);
    reloadButton.setBounds(bar.removeFromRight(80).reduced(0, 2));
    statusLabel.setBounds(bar);
    r.removeFromTop(5);
    scriptArea.setBounds(r);
    logView.setBounds(r.removeFromBottom(juce::jmin(kLogH, r.getHeight())));
    logView.toFront(false);
}

void LuaTab::paint(juce::Graphics&) {}

void LuaTab::visibilityChanged() {
    if (isShowing() && !loadedOnce) {
        loadedOnce = true;
        reload();
    } else if (isShowing() && engine != nullptr) {
        pollParameters(true);   // catch up with changes made while hidden
    }
    updateTimer();
}

void LuaTab::lookAndFeelChanged() {
    statusLabel.setColour(juce::Label::textColourId, errorCount > 0 ? juce::Colour(0xffe57373) : modernLnf.getTheme().textMuted);
    if (engine != nullptr) fireThemeChanged();
}

void LuaTab::updateFromEngine() {
    if (engine != nullptr && isShowing()) pollParameters(true);
}

void LuaTab::reload() {
    const auto file = getScriptFile();
    if (file.existsAsFile()) {
        loadedFile = file;
        loadedFileTime = file.getLastModificationTime();
        if (file.getSize() > kMaxScriptBytes) {
            unload();
            addLog("ERROR skin.lua is larger than 1 MB, not loaded", true);
            return;
        }
        loadScript(file.loadFileAsString(), file.getFileName());
    } else {
        loadedFile = juce::File();
        loadScript(getBuiltInScript(), "built-in example");
    }
}

void LuaTab::unload() {
    stopTimer();
    widgets.clear();
    listeners.clear();
    themeRefs.clear();
    frameRef = LuaEngine::noRef;
    frameHz = 0;
    engine.reset();
}

void LuaTab::loadScript(const juce::String& source, const juce::String& name) {
    unload();
    loadedOnce = true;
    scriptName = name;
    logLines.clear();
    errorCount = 0;
    logView.clear();

    engine = std::make_unique<LuaEngine>();
    engine->onError = [this](const juce::String& m) { addLog("ERROR " + m, true); };
    engine->onLog = [this](const juce::String& m) { addLog(m, false); };
    if (engine->state() != nullptr) Binding::registerAll(engine->state(), *this);
    addLog("-- " + name + " (" + LUA_RELEASE + ")", false);
    engine->run(source, name);
    updateStatus();
    if (errorCount > 0 && !logButton.getToggleState()) {
        logButton.setToggleState(true, juce::dontSendNotification);
        logView.setVisible(true);
    }
    updateTimer();
}

juce::Component* LuaTab::findScriptComponent(const juce::String& id) const {
    for (const auto& w : widgets)
        if (w->id == id) return w->component.get();
    return nullptr;
}

void LuaTab::addLog(const juce::String& line, bool error) {
    if (error) ++errorCount;
    logLines.add(line);
    while (logLines.size() > kMaxLogLines) logLines.remove(0);
    logView.setText(logLines.joinIntoString("\n"), false);
    logView.moveCaretToEnd();
    if (error) updateStatus();
}

void LuaTab::updateStatus() {
    juce::String text = "LUA: " + scriptName.toUpperCase();
    if (engine != nullptr) text << "  |  " << juce::String((double)engine->getMemoryUsed() / 1024.0, 0) << " KB";
    text << "  |  " << (errorCount == 0 ? juce::String("OK") : juce::String(errorCount) + (errorCount == 1 ? " ERROR" : " ERRORS"));
    statusLabel.setText(text, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, errorCount > 0 ? juce::Colour(0xffe57373) : modernLnf.getTheme().textMuted);
}

int LuaTab::readParam(bool stepped, int param) const {
    const auto& preset = model.getCurrentPreset();
    return stepped ? (int)preset.steppedParams[param] : scan_potFrom16bits(preset.continuousParams[param]);
}

void LuaTab::paintCanvas(int index, juce::Graphics& g) {
    auto* w = widgetAt(index);
    if (w == nullptr || engine == nullptr || w->paintRef == LuaEngine::noRef) return;
    auto& c = *w->component;
    if (w->failed || engine->isInCall()) {
        if (w->failed) {
            g.setColour(juce::Colour(0x40e57373));
            g.fillRect(c.getLocalBounds());
            g.setColour(juce::Colour(0xffe57373));
            g.setFont(modernLnf.getCustomFont(10.0f, juce::Font::bold));
            g.drawText("LUA ERROR (SEE LOG)", c.getLocalBounds(), juce::Justification::centred, true);
        }
        return;
    }
    juce::Graphics::ScopedSaveState save(g);
    LuaEngine::PaintContext context{ g, modernLnf.getTheme(),
                                     [this](float size, bool bold) { return modernLnf.getCustomFont(size, bold ? juce::Font::bold : juce::Font::plain); } };
    const float width = (float)c.getWidth(), height = (float)c.getHeight();
    const bool ok = engine->call(w->paintRef, [&](lua_State* L) {
        engine->beginPaint(context);
        lua_pushnumber(L, width);
        lua_pushnumber(L, height);
        return 3;
    });
    engine->endPaint();
    if (!ok) {
        w->failed = true;
        c.repaint();
    }
}

void LuaTab::mouseCanvas(int index, const char* event, const juce::MouseEvent& e, float wheel) {
    auto* w = widgetAt(index);
    if (w == nullptr || engine == nullptr || w->failed || w->mouseRef == LuaEngine::noRef || engine->isInCall()) return;
    const auto pos = e.position;
    if (!engine->call(w->mouseRef, [&](lua_State* L) {
            lua_pushstring(L, event);
            lua_pushnumber(L, pos.x);
            lua_pushnumber(L, pos.y);
            lua_pushnumber(L, wheel);
            return 4;
        })) {
        w->failed = true;
        w->component->repaint();
    }
}

void LuaTab::pollParameters(bool fire) {
    if (engine == nullptr || engine->isInCall()) return;
    for (const auto& w : widgets)
        if (w->kind == Kind::Knob && w->param >= 0)
            safeSetKnob(static_cast<juce::Slider*>(w->component.get()), readParam(false, w->param));

    // Callbacks may add or remove listeners: index loop, removal marks the ref.
    for (size_t i = 0; i < listeners.size() && engine != nullptr; ++i) {
        const int value = readParam(listeners[i].stepped, listeners[i].param);
        if (value == listeners[i].last) continue;
        listeners[i].last = value;
        if (!fire || listeners[i].ref == LuaEngine::noRef) continue;
        const int ref = listeners[i].ref;
        const char* id = paramId(listeners[i].stepped, listeners[i].param);
        if (!engine->call(ref, [value, id](lua_State* L) {
                lua_pushinteger(L, value);
                lua_pushstring(L, id);
                return 2;
            })) {
            // Off until reload; the vector may have grown, so look the listener up again.
            for (auto& l : listeners)
                if (l.ref == ref) { engine->releaseRef(l.ref); l.ref = LuaEngine::noRef; }
            addLog("   (parameter listener for " + juce::String(id) + " switched off)", false);
        }
    }
    listeners.erase(std::remove_if(listeners.begin(), listeners.end(), [](const Listener& l) { return l.ref == LuaEngine::noRef; }),
                    listeners.end());
}

void LuaTab::callFrame() {
    if (engine == nullptr || frameRef == LuaEngine::noRef || frameHz <= 0 || engine->isInCall()) return;
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (lastFrameMs > 0.0 && now - lastFrameMs < 1000.0 / frameHz - 2.0) return;
    const double dt = lastFrameMs > 0.0 ? (now - lastFrameMs) / 1000.0 : 1.0 / frameHz;
    lastFrameMs = now;
    if (!engine->call(frameRef, [dt, now](lua_State* L) {
            lua_pushnumber(L, dt);
            lua_pushnumber(L, now / 1000.0);
            return 2;
        })) {
        engine->releaseRef(frameRef);
        frameRef = LuaEngine::noRef;
        addLog("   (frame callback switched off)", false);
    }
}

void LuaTab::fireThemeChanged() {
    const auto& theme = modernLnf.getTheme();
    for (const auto& w : widgets) {
        if (auto* label = dynamic_cast<juce::Label*>(w->component.get()); label != nullptr && w->kind == Kind::Label) {
            const auto spec = label->getProperties()["luaColour"].toString();
            for (const auto& [name, colour] : LuaEngine::themeRoles(theme))
                if (spec == name) label->setColour(juce::Label::textColourId, colour);
        }
        if (w->kind == Kind::Canvas) w->component->repaint();
    }
    if (engine->isInCall()) return;
    for (auto& ref : themeRefs)
        if (ref != LuaEngine::noRef && engine != nullptr && !engine->call(ref)) {
            engine->releaseRef(ref);
            ref = LuaEngine::noRef;
        }
}

void LuaTab::tick() {
    if (engine == nullptr) return;
    pollParameters(true);
    callFrame();
    if (--fileCheckCountdown <= 0) {
        fileCheckCountdown = 30;
        const auto file = getScriptFile();
        const bool appeared = loadedFile == juce::File() && file.existsAsFile();
        const bool changed = loadedFile != juce::File() &&
                             (!file.existsAsFile() || file.getLastModificationTime() != loadedFileTime);
        if (appeared || changed) {
            reload();
            return;
        }
    }
    updateStatus();
}

void LuaTab::timerCallback() { tick(); }

void LuaTab::updateTimer() {
    if (engine == nullptr || !isShowing()) {
        stopTimer();
        lastFrameMs = 0.0;
        return;
    }
    const int hz = juce::jmax(30, frameRef != LuaEngine::noRef ? frameHz : 0);
    if (!isTimerRunning() || getTimerInterval() != 1000 / hz) startTimer(1000 / hz);
}
