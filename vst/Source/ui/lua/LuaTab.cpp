#include "LuaTab.h"
#include "../components/ModernSectionCard.h"
#include "../../data/OverviberPaths.h"
#include "../../data/PresetManager.h"
#include "../../dsp/MasterBus.h"
#include "../../dsp/adsr.h"
#include "../../dsp/lfo.h"
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
constexpr int kToolbarH = 26;
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
    void mouseMove(const juce::MouseEvent& e) override { owner.mouseCanvas(index, "move", e); }
    void mouseEnter(const juce::MouseEvent& e) override { owner.mouseCanvas(index, "enter", e); }
    void mouseExit(const juce::MouseEvent& e) override { owner.mouseCanvas(index, "exit", e); }

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
            { "get", pGet }, { "set", pSet }, { "getText", pGetText }, { "display", pDisplay }, { "getInfo", pGetInfo }, { "list", pList },
            { "onChange", pOnChange }, { "removeListener", pRemoveListener },
            { nullptr, nullptr },
        };
        const luaL_Reg theme[] = {
            { "get", tGet }, { "colour", tColour }, { "color", tColour }, { "onChange", tOnChange },
            { "list", tList }, { "select", tSelect },
            { nullptr, nullptr },
        };
        const luaL_Reg synth[] = {
            { "voices", sVoices }, { "arp", sArp }, { "bpm", sBpm }, { "wave", sWave }, { "waveName", sWaveName },
            { "scope", sScope }, { "openWaveBrowser", sOpenWaveBrowser },
            { "fader", sFader }, { "pan", sPan }, { "mute", sMute }, { "envMs", sEnvMs }, { "lfoHz", sLfoHz },
            { nullptr, nullptr },
        };
        const luaL_Reg matrix[] = {
            { "count", mCount }, { "get", mGet }, { "set", mSet }, { "sources", mSources }, { "destinations", mDestinations },
            { nullptr, nullptr },
        };
        const luaL_Reg afx[] = {
            { "padCount", aPadCount }, { "pad", aPad }, { "padForNote", aPadForNote }, { "setKey", aSetKey }, { "map", aMap },
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
        registerLib(L, t, "synth", synth);
        registerLib(L, t, "matrix", matrix);
        registerLib(L, t, "afx", afx);

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

    // params.display(id [, value]) -> the text the native skin's knob shows (Hz, ms, st, ct, ...)
    static int pDisplay(lua_State* L) {
        auto& t = tab(L);
        const auto p = param(L, 1);
        if (p.stepped) {
            lua_settop(L, 1);
            return pGetText(L);
        }
        const auto cp = (continuousParameter_t)p.index;
        const auto& sp = t.model.getCurrentPreset().steppedParams;
        const double v = lua_isnoneornil(L, 2) ? (double)t.readParam(false, p.index) : juce::jlimit(0.0, 999.0, (double)luaL_checknumber(L, 2));
        juce::String text;
        switch (cp) {
            case cpCutoff: {
                const float hz = 20.0f * std::pow(10.0f, (float)v / 999.0f * 3.0f);
                text = hz >= 1000.0f ? juce::String(hz / 1000.0f, 2) + " kHz" : juce::String((int)std::round(hz)) + " Hz";
                break;
            }
            case cpFilAtt: case cpFilDec: case cpFilRel: text = formatEnvelopeTime(v, sp[spFilEnvSlow] != 0); break;
            case cpAmpAtt: case cpAmpDec: case cpAmpRel: text = formatEnvelopeTime(v, sp[spAmpEnvSlow] != 0); break;
            case cpWModAtt: case cpWModDec: case cpWModRel: text = formatEnvelopeTime(v, sp[spWModEnvSlow] != 0); break;
            case cpLFOFreq: text = formatLfoSpeed(v, sp[spLFOSpeed]); break;
            case cpLFO2Freq: text = formatLfoSpeed(v, sp[spLFO2Speed]); break;
            case cpGlide: text = formatGlideTime(v); break;
            case cpModDelay: text = formatModDelayTime(v); break;
            case cpAFreq: case cpBFreq:
                text = "+" + juce::String((float)scan_potTo16bits((int)std::round(v)) / 1024.0f, 1) + " st";
                break;
            case cpDetune: {
                const int ct = (int)std::round((v - 500.0) / 499.0 * 50.0);
                text = (ct > 0 ? "+" : "") + juce::String(ct) + " ct";
                break;
            }
            case cpMasterTune: {
                const int ct = (int)std::round(((scan_potTo16bits((int)std::round(v)) >> 7) - 256) * 100.0f / 256.0f);
                text = (ct > 0 ? "+" : "") + juce::String(ct) + " ct";
                break;
            }
            default: {
                lua_settop(L, 1);
                return pGetText(L);
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
    // theme.list() -> { { name=, accent=, windowBg=, ... }, ... }: the preset palettes of SETTINGS
    static int tList(lua_State* L) {
        const auto themes = ModernTheme::getPresetThemes();
        lua_createtable(L, (int)themes.size(), 0);
        for (size_t i = 0; i < themes.size(); ++i) {
            const auto roles = LuaEngine::themeRoles(themes[i]);
            lua_createtable(L, 0, (int)roles.size() + 1);
            for (const auto& [name, colour] : roles) {
                lua_pushstring(L, LuaEngine::colourToString(colour).toRawUTF8());
                lua_setfield(L, -2, name);
            }
            lua_pushstring(L, themes[i].name.toRawUTF8());
            lua_setfield(L, -2, "name");
            lua_rawseti(L, -2, (lua_Integer)i + 1);
        }
        return 1;
    }
    // theme.select(index): applies preset palette `index` (1-based) as SETTINGS does
    static int tSelect(lua_State* L) {
        auto& t = tab(L);
        const auto index = luaL_checkinteger(L, 1);
        if (index < 1 || index > (lua_Integer)ModernTheme::getPresetThemes().size()) luaL_argerror(L, 1, "no such palette");
        if (!t.context.selectPalette) luaL_error(L, "theme.select is not available here");
        t.context.selectPalette((int)index);
        return 0;
    }

    // ---- synth: display data of the engine (read only)
    // synth.voices() -> { level, ... } per voice, 0..1 of the console bus load
    static int sVoices(lua_State* L) {
        auto& t = tab(L);
        lua_createtable(L, SYNTH_VOICE_COUNT, 0);
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            lua_pushnumber(L, juce::jlimit(0.0, 1.0, (double)t.model.getVoiceActivity(v) / 65535.0 / MasterBus::kConsoleKnee));
            lua_rawseti(L, -2, v + 1);
        }
        return 1;
    }
    static void pushNotes(lua_State* L, const std::array<uint8_t, 16>& notes, int count, const char* key) {
        lua_createtable(L, count, 0);
        for (int i = 0; i < count; ++i) {
            lua_pushinteger(L, notes[(size_t)i]);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, key);
    }
    // synth.arp() -> { valid, tick, step (0-based), gate, notes = {held...}, pattern = {16 steps, -1 = none} }
    static int sArp(lua_State* L) {
        const auto& a = tab(L).model.getArpVisualizationState();
        lua_newtable(L);
        lua_pushboolean(L, a.valid); lua_setfield(L, -2, "valid");
        lua_pushinteger(L, a.tick); lua_setfield(L, -2, "tick");
        lua_pushinteger(L, a.currentStep); lua_setfield(L, -2, "step");
        lua_pushboolean(L, a.gateActive); lua_setfield(L, -2, "gate");
        const int count = juce::jlimit(0, 16, a.activeCount);
        pushNotes(L, a.activeNotes, count, "notes");
        // 16 steps; -1 where the step has no note
        lua_createtable(L, 16, 0);
        for (int i = 0; i < 16; ++i) {
            const uint8_t n = a.patternNotes[(size_t)i];
            lua_pushinteger(L, n == ASSIGNER_NO_NOTE || n > 127 ? -1 : n);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "pattern");
        return 1;
    }
    static int sBpm(lua_State* L) { lua_pushnumber(L, tab(L).model.getEffectiveBpm()); return 1; }
    static abx_t osc(lua_State* L, int index) {
        if (lua_type(L, index) == LUA_TSTRING) {
            const juce::String name = juce::String(lua_tostring(L, index)).toUpperCase();
            if (name == "A") return abxAMain;
            if (name == "B") return abxBMain;
        } else if (lua_isinteger(L, index)) {
            const auto n = lua_tointeger(L, index);
            if (n == 1) return abxAMain;
            if (n == 2) return abxBMain;
        }
        luaL_argerror(L, index, "\"A\" or \"B\" expected");
        return abxAMain;
    }
    // synth.wave("A" | "B" [, points=256]) -> { -1..1, ... }: the oscillator's wave, resampled
    static int sWave(lua_State* L) {
        auto& t = tab(L);
        const auto abx = osc(L, 1);
        const int points = (int)juce::jlimit<lua_Integer>(2, WTOSC_SAMPLE_COUNT, luaL_optinteger(L, 2, 256));
        const uint16_t* data = t.model.getWaveManager().getWaveData(abx);
        lua_createtable(L, points, 0);
        for (int i = 0; i < points; ++i) {
            const int at = (int)((int64_t)i * WTOSC_SAMPLE_COUNT / points);
            lua_pushnumber(L, data != nullptr ? (data[at] - 32768.0) / 32768.0 : 0.0);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }
    static int sWaveName(lua_State* L) {
        auto& t = tab(L);
        lua_pushstring(L, t.model.getWaveManager().getCurrentWave(osc(L, 1)).c_str());
        return 1;
    }
    // synth.scope([count=512]) -> the latest master output samples, oldest first
    static int sScope(lua_State* L) {
        auto& t = tab(L);
        const int count = (int)juce::jlimit<lua_Integer>(1, 2048, luaL_optinteger(L, 1, 512));
        std::vector<float> samples((size_t)count);
        t.model.getOutputScope().copyLatest(samples.data(), count);
        lua_createtable(L, count, 0);
        for (int i = 0; i < count; ++i) {
            lua_pushnumber(L, std::isfinite(samples[(size_t)i]) ? samples[(size_t)i] : 0.0f);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }
    // synth.openWaveBrowser("A" | "B"): the skin's own wave browser
    static int sOpenWaveBrowser(lua_State* L) {
        auto& t = tab(L);
        const auto abx = osc(L, 1);
        if (!t.context.openWaveBrowser) luaL_error(L, "the wave browser is not available here");
        // After the Lua call: the browser may run a modal loop.
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<LuaTab>(&t), abx] {
            if (safe != nullptr && safe->context.openWaveBrowser) safe->context.openWaveBrowser(abx);
        });
        return 0;
    }

    // ---- synth: the voice mixer (session state, like the console's faders)
    static int voiceArg(lua_State* L) {
        const auto v = luaL_checkinteger(L, 1);
        if (v < 1 || v > SYNTH_VOICE_COUNT) luaL_argerror(L, 1, "voice 1..6 expected");
        return (int)v - 1;
    }
    // synth.envMs(pot [, slow]) -> milliseconds of an envelope stage, as the engine times it
    static int sEnvMs(lua_State* L) {
        const double pot = juce::jlimit(0.0, 999.0, (double)luaL_checknumber(L, 1));
        lua_pushnumber(L, adsrStageMilliseconds((uint16_t)scan_potTo16bits((int)std::round(pot)), lua_toboolean(L, 2) != 0));
        return 1;
    }
    // synth.lfoHz(pot [, range 0..3]) -> the LFO's cycle frequency in Hz
    static int sLfoHz(lua_State* L) {
        const int pot = (int)juce::jlimit(0.0, 999.0, (double)luaL_checknumber(L, 1));
        const int range = (int)juce::jlimit<lua_Integer>(0, 3, luaL_optinteger(L, 2, 0));
        lua_pushnumber(L, lfoCycleHz(scan_potFrom16bits(scan_potTo16bits(pot)), (int8_t)range));
        return 1;
    }
    // synth.fader(voice [, dB]) -> dB (-100 = off); -60 dB and below switch the voice off, at most +12 dB
    static int sFader(lua_State* L) {
        auto& t = tab(L);
        const int v = voiceArg(L);
        if (!lua_isnoneornil(L, 2)) {
            const lua_Number db = luaL_checknumber(L, 2);
            t.model.setVoiceFader(v, !std::isfinite(db) || db <= -60.0 ? 0.0f : std::pow(10.0f, (float)juce::jmin(12.0, db) / 20.0f));
        }
        const float gain = t.model.getVoiceFader(v);
        lua_pushnumber(L, gain <= 0.0f ? -100.0 : 20.0 * std::log10(gain));
        return 1;
    }
    // synth.pan(voice [, -1..1]) -> pan
    static int sPan(lua_State* L) {
        auto& t = tab(L);
        const int v = voiceArg(L);
        if (!lua_isnoneornil(L, 2)) {
            const lua_Number pan = luaL_checknumber(L, 2);
            if (std::isfinite(pan)) t.model.setVoicePan(v, (float)juce::jlimit(-1.0, 1.0, pan));
        }
        lua_pushnumber(L, t.model.getVoicePan(v));
        return 1;
    }
    // synth.mute([on]) -> whether the mixer's master is muted
    static int sMute(lua_State* L) {
        auto& t = tab(L);
        if (!lua_isnoneornil(L, 1)) t.model.setMasterMute(lua_toboolean(L, 1) != 0);
        lua_pushboolean(L, t.model.isMasterMuted());
        return 1;
    }

    // ---- matrix: the modulation matrix of part 1, slots 1..count()
    static int mCount(lua_State* L) { lua_pushinteger(L, MOD_MATRIX_SLOT_COUNT); return 1; }
    static int slotIndex(lua_State* L) {
        const auto slot = luaL_checkinteger(L, 1);
        if (slot < 1 || slot > MOD_MATRIX_SLOT_COUNT) luaL_argerror(L, 1, "slot 1..8 expected");
        return (int)slot - 1;
    }
    // matrix.get(slot) -> { source, via, dest, depth (-100..100), curve, enabled }
    static int mGet(lua_State* L) {
        const auto& m = tab(L).model.getCurrentPreset().modMatrix[slotIndex(L)];
        lua_newtable(L);
        lua_pushinteger(L, m.source); lua_setfield(L, -2, "source");
        lua_pushinteger(L, m.viaSource); lua_setfield(L, -2, "via");
        lua_pushinteger(L, m.dest); lua_setfield(L, -2, "dest");
        lua_pushinteger(L, m.depth); lua_setfield(L, -2, "depth");
        lua_pushinteger(L, m.curve); lua_setfield(L, -2, "curve");
        lua_pushboolean(L, m.enabled); lua_setfield(L, -2, "enabled");
        return 1;
    }
    static int field(lua_State* L, const char* key, int value, int lo, int hi) {
        lua_getfield(L, 2, key);
        if (!lua_isnil(L, -1)) {
            if (!lua_isnumber(L, -1)) luaL_error(L, "matrix.set: '%s' must be a number", key);
            const lua_Number v = lua_tonumber(L, -1);
            value = juce::jlimit(lo, hi, std::isfinite(v) ? (int)std::lround(v) : value);
        }
        lua_pop(L, 1);
        return value;
    }
    // matrix.set(slot, { source=, via=, dest=, depth=, enabled= }): fields left out keep their value
    static int mSet(lua_State* L) {
        auto& t = tab(L);
        const int slot = slotIndex(L);
        luaL_checktype(L, 2, LUA_TTABLE);
        auto value = t.model.getCurrentPreset().modMatrix[slot];
        value.source = (uint8_t)field(L, "source", value.source, 0, modSrcCount - 1);
        value.viaSource = (uint8_t)field(L, "via", value.viaSource, 0, modSrcCount - 1);
        value.dest = (uint8_t)field(L, "dest", value.dest, 0, modDestCount - 1);
        value.depth = (int16_t)field(L, "depth", value.depth, -100, 100);
        lua_getfield(L, 2, "enabled");
        if (!lua_isnil(L, -1)) value.enabled = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (t.context.setMatrixSlot) t.context.setMatrixSlot(slot, value);
        else t.model.getCurrentPreset().modMatrix[slot] = value;
        return 0;
    }
    // matrix.sources() / destinations() -> { [0] = "None", "Velocity", ... } by id
    static int mSources(lua_State* L) {
        lua_createtable(L, modSrcCount, 1);
        for (int i = 0; i < modSrcCount; ++i) {
            lua_pushstring(L, PresetManager::getModSourceDisplayName((modSource_t)i));
            lua_rawseti(L, -2, i);
        }
        return 1;
    }
    static int mDestinations(lua_State* L) {
        lua_createtable(L, modDestCount, 1);
        for (int i = 0; i < modDestCount; ++i) {
            lua_pushstring(L, PresetManager::getModDestDisplayName((modDest_t)i));
            lua_rawseti(L, -2, i);
        }
        return 1;
    }

    // ---- afx: the AFX kit's pads (1..16) and key map
    static int aPadCount(lua_State* L) { lua_pushinteger(L, AFX_SLOT_COUNT); return 1; }
    static int padArg(lua_State* L, int index) {
        const auto pad = luaL_checkinteger(L, index);
        if (pad < 1 || pad > AFX_SLOT_COUNT) luaL_argerror(L, index, "pad 1..16 expected");
        return (int)pad - 1;
    }
    static int noteArg(lua_State* L, int index) {
        const auto note = luaL_checkinteger(L, index);
        if (note < 0 || note > 127) luaL_argerror(L, index, "note 0..127 expected");
        return (int)note;
    }
    // afx.pad(i) -> { name, customized }
    static int aPad(lua_State* L) {
        const auto& slot = tab(L).model.getAfxKit().getSlot(padArg(L, 1));
        lua_newtable(L);
        lua_pushstring(L, slot.name.c_str()); lua_setfield(L, -2, "name");
        lua_pushboolean(L, slot.isCustomized); lua_setfield(L, -2, "customized");
        return 1;
    }
    static int aPadForNote(lua_State* L) {
        lua_pushinteger(L, tab(L).model.getAfxKit().getSlotForNote((uint8_t)noteArg(L, 1)) + 1);
        return 1;
    }
    static int aSetKey(lua_State* L) {
        auto& t = tab(L);
        t.model.getAfxKit().setNoteMapping((uint8_t)noteArg(L, 1), (uint8_t)padArg(L, 2));
        return 0;
    }
    // afx.map("octaves" | "chromatic" | "default" | "all", [pad for "all"])
    static int aMap(lua_State* L) {
        auto& kit = tab(L).model.getAfxKit();
        const juce::String mode = luaL_checkstring(L, 1);
        if (mode == "octaves") kit.mapOctaveZones();
        else if (mode == "chromatic") kit.mapChromatic16();
        else if (mode == "default") kit.mapDefault();
        else if (mode == "all") kit.mapAllToSlot((uint8_t)padArg(L, 2));
        else luaL_argerror(L, 1, "\"octaves\", \"chromatic\", \"default\" or \"all\" expected");
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

const std::vector<LuaTab::Page>& LuaTab::getPages() {
    static const std::vector<Page> pages{
        { "osc", "OSC" },      { "filter", "FILTER / VCA" }, { "env", "ENV" },           { "lfo", "LFO / ARP" },
        { "afx", "AFX" },      { "matrix", "MOD MATRIX" },   { "settings", "SETTINGS" }, { "skin", "SKIN.LUA" },
    };
    return pages;
}

juce::File LuaTab::getPageFile(int index) {
    const auto& pages = getPages();
    const auto& page = pages[(size_t)juce::jlimit(0, (int)pages.size() - 1, index)];
    return OverviberPaths::getLuaDirectory().getChildFile(juce::String(page.id) + ".lua");
}

namespace {
juce::String resource(const char* name) {
    int size = 0;
    const char* data = LuaScripts::getNamedResource(name, size);
    return data != nullptr ? juce::String::fromUTF8(data, size) : juce::String();
}
}

juce::String LuaTab::getBuiltInPage(int index) {
    const auto& pages = getPages();
    const juce::String id = pages[(size_t)juce::jlimit(0, (int)pages.size() - 1, index)].id;
    if (id == "skin") return getBuiltInScript();
    return resource((id + "_lua").toRawUTF8());
}

juce::String LuaTab::getBuiltInLibrary() { return resource("lib_lua"); }

juce::File LuaTab::getScriptFile() { return getPageFile((int)getPages().size() - 1); }

juce::String LuaTab::getBuiltInScript() {
    return juce::String::fromUTF8(LuaScripts::LuaDefaultSkin_lua, LuaScripts::LuaDefaultSkin_luaSize);
}

juce::Time LuaTab::libraryFileTime() const {
    const auto file = OverviberPaths::getLuaDirectory().getChildFile("lib.lua");
    return file.existsAsFile() ? file.getLastModificationTime() : juce::Time();
}

void LuaTab::selectPage(int index) {
    pageIndex = juce::jlimit(0, (int)getPages().size() - 1, index);
    for (size_t i = 0; i < pageButtons.size(); ++i)
        pageButtons[i]->setToggleState((int)i == pageIndex, juce::dontSendNotification);
    reload();
}

void LuaTab::setup() {
    statusLabel.setComponentID("luaStatus");
    statusLabel.setFont(modernLnf.getCustomFont(10.5f, juce::Font::bold));
    statusLabel.setColour(juce::Label::textColourId, modernLnf.getTheme().textMuted);
    statusLabel.setText("NOT LOADED", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel);

    const auto& pages = getPages();
    for (int i = 0; i < (int)pages.size(); ++i) {
        auto button = std::make_unique<juce::TextButton>(pages[(size_t)i].title);
        button->setComponentID(juce::String("luaPage[") + pages[(size_t)i].id + "]");
        button->setTooltip(juce::String("Lua page ") + pages[(size_t)i].id + ".lua");
        button->setToggleState(i == pageIndex, juce::dontSendNotification);
        button->getProperties().set("compactFont", true);
        button->onClick = [this, i] { selectPage(i); };
        addAndMakeVisible(*button);
        pageButtons.push_back(std::move(button));
    }

    reloadButton.setComponentID("luaReloadButton");
    reloadButton.setTooltip("Run the page's script again");
    reloadButton.onClick = [this] { reload(); };
    folderButton.setComponentID("luaFolderButton");
    folderButton.setTooltip("Show the page's script file; creates it from the built-in page first");
    folderButton.onClick = [this] {
        auto file = getPageFile(pageIndex);
        if (!file.existsAsFile()) {
            file.getParentDirectory().createDirectory();
            file.replaceWithText(getBuiltInPage(pageIndex));
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
    logButton.setBounds(bar.removeFromRight(50));
    bar.removeFromRight(4);
    folderButton.setBounds(bar.removeFromRight(64));
    bar.removeFromRight(4);
    reloadButton.setBounds(bar.removeFromRight(64));
    bar.removeFromRight(4);
    statusLabel.setBounds(bar.removeFromRight(120));
    bar.removeFromRight(4);
    const int n = (int)pageButtons.size();
    const int w = n > 0 ? juce::jmin(100, (bar.getWidth() - (n - 1) * 4) / n) : 0;
    for (auto& b : pageButtons) {
        b->setBounds(bar.removeFromLeft(w));
        bar.removeFromLeft(4);
    }
    r.removeFromTop(4);
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
    const auto file = getPageFile(pageIndex);
    const juce::String title = getPages()[(size_t)pageIndex].id + juce::String(".lua");
    if (file.existsAsFile()) {
        loadedFile = file;
        loadedFileTime = file.getLastModificationTime();
        if (file.getSize() > kMaxScriptBytes) {
            unload();
            addLog("ERROR " + title + " is larger than 1 MB, not loaded", true);
            return;
        }
        loadScript(file.loadFileAsString(), title);
    } else {
        loadedFile = juce::File();
        loadScript(getBuiltInPage(pageIndex), title + " (built-in)");
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
    // lib.lua: Documents/Overviber/LUA/lib.lua while it exists, else built in.
    const auto libFile = OverviberPaths::getLuaDirectory().getChildFile("lib.lua");
    loadedLibraryTime = libraryFileTime();
    if (libFile.existsAsFile() && libFile.getSize() <= kMaxScriptBytes) engine->run(libFile.loadFileAsString(), "lib.lua");
    else engine->run(getBuiltInLibrary(), "lib.lua");
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
    juce::String text;
    if (engine != nullptr) text << juce::String((int)std::lround((double)engine->getMemoryUsed() / 1024.0)) << " KB  |  ";
    text << (errorCount == 0 ? juce::String("OK") : juce::String(errorCount) + (errorCount == 1 ? " ERROR" : " ERRORS"));
    statusLabel.setTooltip(scriptName);
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
        const auto file = getPageFile(pageIndex);
        const bool appeared = loadedFile == juce::File() && file.existsAsFile();
        const bool changed = loadedFile != juce::File() &&
                             (!file.existsAsFile() || file.getLastModificationTime() != loadedFileTime);
        if (appeared || changed || libraryFileTime() != loadedLibraryTime) {
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
