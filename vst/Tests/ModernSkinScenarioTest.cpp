// ==============================================================================
// ModernSkinScenarioTest - characterization harness for the Modern skin
// ==============================================================================
// Builds ModernEditorView headlessly and records, for every tab and scenario:
//   layout.txt    geometry, visibility and state of every component with an ID
//   bindings.txt  engine change + UI reaction caused by each visible control
//   pixels/*.png  software-rendered snapshots (local refactoring aid)
// layout.txt and bindings.txt must match vst/Tests/fixtures/modern-skin/.
//
// Usage: ModernSkinScenarioTest [--out <dir>] [--update] [--pixels <baselineDir>]
//   --update   rewrite the fixtures after an intentional UI change
//   --pixels   also compare the snapshots with an earlier run's pixels/ folder
//              (pixel output depends on the platform's font rasteriser, so it
//              is not part of the committed fixture)
//
// The editor never touches the user's configuration: the config directory is
// redirected into the output folder and reset before every editor instance.
// ==============================================================================
#include "TestData.h"
#include "data/OverviberPaths.h"
#include "data/PresetManager.h"
#include "ui/ModernEditorView.h"
#include "ui/components/ModernLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <typeinfo>
#include <vector>

namespace {

constexpr int viewWidth = 1100;   // PluginEditor: 1100 x 700 window minus 50 px preset bar
constexpr int viewHeight = 650;
constexpr int tabCount = 7;
const char* const tabNames[tabCount] = { "osc", "filter-vca", "envelopes", "lfo-arp", "afx", "mod-matrix", "settings" };

// juce::Button delivers clicks through this private command id; handling it
// synchronously runs the exact production click path (toggle, radio group, onClick).
constexpr int juceButtonClickMessageId = 0x2f3f4f99;

struct Options {
    juce::File outDir = juce::File::getCurrentWorkingDirectory();
    juce::File fixtureDir{ OVERVIBER_MODERN_SKIN_FIXTURES };
    juce::File pixelBaseline;
    bool update = false;
    bool debugTree = false;
};

struct Scenario {
    juce::String name;
    std::vector<int> tabs;
    std::function<void(SynthModel&)> prepare;
};

// ------------------------------------------------------------------------------
// Engine state capture
// ------------------------------------------------------------------------------
struct EngineSnapshot {
    PresetData preset;
    PartRoute routes[16];
    bool customRouting = false;
    bool masterMute = false;   // mixer state: a MUTE click must not leak into the next scenario
    uint8_t noteMap[128]{};
    std::string slotNames[AFX_SLOT_COUNT];
    PresetData slotPresets[AFX_SLOT_COUNT];
};

EngineSnapshot capture(SynthModel& model) {
    EngineSnapshot s;
    s.preset = model.getCurrentPreset();
    for (int r = 0; r < 16; ++r) s.routes[r] = model.getPartRoute(r);
    s.customRouting = model.usesCustomRouting();
    s.masterMute = model.isMasterMuted();
    for (int n = 0; n < 128; ++n) s.noteMap[n] = model.getAfxKit().getSlotForNote(static_cast<uint8_t>(n));
    for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
        s.slotNames[i] = model.getAfxKit().getSlot(i).name;
        s.slotPresets[i] = model.getAfxKit().getSlot(i).preset;
    }
    return s;
}

void restore(SynthModel& model, const EngineSnapshot& s) {
    model.getCurrentPreset() = s.preset;
    for (int r = 0; r < 16; ++r) model.getPartRoute(r) = s.routes[r];
    model.setCustomRouting(s.customRouting);
    model.setMasterMute(s.masterMute);
    for (int n = 0; n < 128; ++n) model.getAfxKit().setNoteMapping(static_cast<uint8_t>(n), s.noteMap[n]);
    for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
        model.getAfxKit().getSlot(i).name = s.slotNames[i];
        model.getAfxKit().getSlot(i).preset = s.slotPresets[i];
    }
}

bool sameSlot(const ModMatrixSlot& a, const ModMatrixSlot& b) {
    return a.source == b.source && a.dest == b.dest && a.viaSource == b.viaSource &&
           a.depth == b.depth && a.curve == b.curve && a.enabled == b.enabled;
}

juce::String describe(const ModMatrixSlot& s) {
    return juce::String(s.enabled ? "on" : "off") + "/" + juce::String(s.source) + "/" + juce::String(s.viaSource) +
           "/" + juce::String(s.dest) + "/" + juce::String(s.depth);
}

juce::StringArray diffPreset(const juce::String& prefix, const PresetData& a, const PresetData& b) {
    juce::StringArray out;
    for (int i = 0; i < cpCount; ++i)
        if (a.continuousParams[i] != b.continuousParams[i])
            out.add(prefix + PresetManager::getContinuousParamName(static_cast<continuousParameter_t>(i)) + "=" +
                    juce::String(a.continuousParams[i]) + ">" + juce::String(b.continuousParams[i]));
    for (int i = 0; i < spCount; ++i)
        if (a.steppedParams[i] != b.steppedParams[i])
            out.add(prefix + PresetManager::getSteppedParamName(static_cast<steppedParameter_t>(i)) + "=" +
                    juce::String(a.steppedParams[i]) + ">" + juce::String(b.steppedParams[i]));
    for (int s = 0; s < MOD_MATRIX_SLOT_COUNT; ++s)
        if (!sameSlot(a.modMatrix[s], b.modMatrix[s]))
            out.add(prefix + "matrix" + juce::String(s) + "=" + describe(a.modMatrix[s]) + ">" + describe(b.modMatrix[s]));
    for (int w = 0; w < abxCount; ++w)
        if (a.oscBank[w] != b.oscBank[w] || a.oscWave[w] != b.oscWave[w])
            out.add(prefix + "wave" + juce::String(w) + "=" + juce::String(a.oscBank[w] + "/" + a.oscWave[w]) + ">" +
                    juce::String(b.oscBank[w] + "/" + b.oscWave[w]));
    return out;
}

juce::StringArray diff(const EngineSnapshot& a, const EngineSnapshot& b) {
    auto out = diffPreset({}, a.preset, b.preset);
    for (int r = 0; r < 16; ++r) {
        const auto& x = a.routes[r];
        const auto& y = b.routes[r];
        if (x.enabled != y.enabled || x.channel != y.channel || x.low != y.low || x.high != y.high)
            out.add("route" + juce::String(r) + "=" + juce::String(x.enabled) + "/" + juce::String(x.channel) + "/" +
                    juce::String(x.low) + "-" + juce::String(x.high) + ">" + juce::String(y.enabled) + "/" +
                    juce::String(y.channel) + "/" + juce::String(y.low) + "-" + juce::String(y.high));
    }
    if (a.customRouting != b.customRouting) out.add("customRouting>" + juce::String(b.customRouting ? 1 : 0));
    int changedNotes = 0;
    for (int n = 0; n < 128; ++n) changedNotes += a.noteMap[n] != b.noteMap[n] ? 1 : 0;
    if (changedNotes > 0) out.add("noteMap:" + juce::String(changedNotes) + "notes");
    for (int i = 0; i < AFX_SLOT_COUNT; ++i) {
        if (a.slotNames[i] != b.slotNames[i])
            out.add("slot" + juce::String(i) + ".name>\"" + juce::String(b.slotNames[i]) + "\"");
        auto slotDiff = diffPreset("slot" + juce::String(i) + ".", a.slotPresets[i], b.slotPresets[i]);
        if (!slotDiff.isEmpty()) out.add("slot" + juce::String(i) + ".preset:" + juce::String(slotDiff.size()) + "changes");
    }
    return out;
}

// ------------------------------------------------------------------------------
// Component inspection
// ------------------------------------------------------------------------------
bool effectivelyVisible(const juce::Component& component, const juce::Component& root) {
    for (auto* c = &component; c != nullptr; c = c->getParentComponent()) {
        if (!c->isVisible()) return false;
        if (c == &root) return true;
    }
    return false;
}

juce::String kindOf(juce::Component& c) {
    if (dynamic_cast<juce::Slider*>(&c) != nullptr) return "Slider";
    if (dynamic_cast<juce::ToggleButton*>(&c) != nullptr) return "Toggle";
    if (dynamic_cast<juce::TextButton*>(&c) != nullptr) return "TextButton";
    if (dynamic_cast<juce::Button*>(&c) != nullptr) return "Button";
    if (dynamic_cast<juce::ComboBox*>(&c) != nullptr) return "Combo";
    if (dynamic_cast<juce::Label*>(&c) != nullptr) return "Label";
    return "Component";
}

uint64_t fnv1a(const void* data, size_t size, uint64_t hash = 1469598103934665603ull) {
    auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

juce::String hex(uint64_t value) { return juce::String::toHexString(static_cast<juce::int64>(value)).paddedLeft('0', 16); }

juce::String stateOf(juce::Component& c) {
    juce::String s;
    if (auto* slider = dynamic_cast<juce::Slider*>(&c)) {
        s << "value=" << juce::String(slider->getValue(), 3) << " text=\"" << slider->getTextFromValue(slider->getValue())
          << "\" range=" << juce::String(slider->getMinimum(), 3) << ".." << juce::String(slider->getMaximum(), 3);
    } else if (auto* button = dynamic_cast<juce::Button*>(&c)) {
        s << "on=" << (button->getToggleState() ? 1 : 0) << " text=\"" << button->getButtonText() << "\"";
        if (button->getRadioGroupId() != 0) s << " radio=" << button->getRadioGroupId();
    } else if (auto* combo = dynamic_cast<juce::ComboBox*>(&c)) {
        s << "id=" << combo->getSelectedId() << " text=\"" << combo->getText() << "\"";
        // The AFX preset list mirrors the factory preset folder; its size is not UI behaviour.
        if (c.getComponentID() != "slotPresetCombo") {
            juce::String items;
            for (int i = 0; i < combo->getNumItems(); ++i) items << combo->getItemId(i) << ':' << combo->getItemText(i) << '|';
            s << " items=" << combo->getNumItems() << "#" << hex(fnv1a(items.toRawUTF8(), items.getNumBytesAsUTF8())).substring(0, 8);
        }
    } else if (auto* label = dynamic_cast<juce::Label*>(&c)) {
        s << "text=\"" << label->getText() << "\"";
    }
    if (!c.isEnabled()) s << " disabled";
    return s;
}

struct Entry {
    juce::String id;
    juce::Component* component = nullptr;
    juce::Rectangle<int> bounds;
    juce::String line;
};

void walk(juce::Component& c, const std::function<void(juce::Component&)>& visit) {
    visit(c);
    for (auto* child : c.getChildren()) walk(*child, visit);
}

// Every effectively visible component carrying an ID, ordered independently of
// the component hierarchy so that re-parenting during refactoring is neutral.
std::vector<Entry> collect(juce::Component& root) {
    std::vector<Entry> entries;
    walk(root, [&](juce::Component& c) {
        if (&c == &root || c.getComponentID().isEmpty() || !effectivelyVisible(c, root)) return;
        Entry e;
        e.id = c.getComponentID();
        e.component = &c;
        e.bounds = root.getLocalArea(&c, c.getLocalBounds());
        e.line = e.id + " " + kindOf(c) + " " + juce::String(e.bounds.getX()) + "," + juce::String(e.bounds.getY()) + "," +
                 juce::String(e.bounds.getWidth()) + "," + juce::String(e.bounds.getHeight());
        const auto state = stateOf(c);
        if (state.isNotEmpty()) e.line << " " << state;
        entries.push_back(e);
    });
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.id != b.id) return a.id < b.id;
        if (a.bounds.getY() != b.bounds.getY()) return a.bounds.getY() < b.bounds.getY();
        return a.bounds.getX() < b.bounds.getX();
    });
    return entries;
}

juce::String visibleSummary(juce::Component& root) {
    std::map<juce::String, int> counts;
    int total = 0;
    walk(root, [&](juce::Component& c) {
        if (&c == &root || !effectivelyVisible(c, root)) return;
        ++counts[kindOf(c)];
        ++total;
    });
    juce::String s;
    s << total << " visible components:";
    for (const auto& [kind, count] : counts) s << " " << kind << "=" << count;
    return s;
}

std::map<juce::String, juce::String> uiState(juce::Component& root) {
    std::map<juce::String, juce::String> state;
    for (const auto& e : collect(root)) {
        auto key = e.id;
        for (int n = 2; state.count(key) != 0; ++n) key = e.id + "#" + juce::String(n);
        state[key] = e.line;
    }
    return state;
}

juce::String uiDiff(const std::map<juce::String, juce::String>& before, const std::map<juce::String, juce::String>& after) {
    juce::StringArray changed;
    juce::String content;
    for (const auto& [id, line] : after) {
        auto it = before.find(id);
        if (it == before.end()) { changed.add("+" + id); content << line << '\n'; }
        else if (it->second != line) { changed.add(id); content << line << '\n'; }
    }
    for (const auto& [id, line] : before)
        if (after.count(id) == 0) changed.add("-" + id);
    if (changed.isEmpty()) return "ui: none";
    return "ui[" + juce::String(changed.size()) + "]: " + changed.joinIntoString(" ") + " #" +
           hex(fnv1a(content.toRawUTF8(), content.getNumBytesAsUTF8())).substring(0, 8);
}

bool skipInteraction(const juce::String& id) {
    return id.startsWith("tabButtons")            // switches tabs, covered by selectTab()
        || id.endsWith("_presetBtn")               // opens a native popup menu
        || id == "saveSetupButton" || id == "loadSetupButton";  // open native file choosers
}

juce::String interact(juce::Component& c) {
    if (auto* slider = dynamic_cast<juce::Slider*>(&c)) {
        const double lo = slider->getMinimum();
        const double hi = slider->getMaximum();
        auto snap = [&](double v) {
            const double step = slider->getInterval();
            return step > 0.0 ? lo + std::round((v - lo) / step) * step : v;
        };
        double target = snap(lo + (hi - lo) * 0.73);
        if (std::abs(target - slider->getValue()) < 1.0e-9) target = snap(lo + (hi - lo) * 0.27);
        slider->setValue(target, juce::sendNotificationSync);
        return "set " + juce::String(target, 3);
    }
    if (auto* button = dynamic_cast<juce::Button*>(&c)) {
        if (!button->isEnabled()) return {};
        static_cast<juce::Component&>(*button).handleCommandMessage(juceButtonClickMessageId);
        return "click";
    }
    if (auto* combo = dynamic_cast<juce::ComboBox*>(&c)) {
        if (combo->getNumItems() < 2) return {};
        const int index = combo->getSelectedItemIndex() == 1 ? 0 : 1;
        combo->setSelectedId(combo->getItemId(index), juce::sendNotificationSync);
        return "select " + juce::String(combo->getItemId(index));
    }
    return {};
}

// ------------------------------------------------------------------------------
// Harness
// ------------------------------------------------------------------------------
class Harness {
public:
    Harness(const Options& o, SynthModel& e) : options(o), model(e) {
        configDir = options.outDir.getChildFile("config");
        OverviberPaths::setAppConfigDirectoryOverride(configDir);
        pristine = capture(model);
    }

    ~Harness() { OverviberPaths::setAppConfigDirectoryOverride({}); }

    std::unique_ptr<ModernEditorView> makeView() {
        // saveSkinConfig() runs on many settings interactions; every editor must
        // start from the same pinned configuration.
        configDir.deleteRecursively();
        configDir.createDirectory();
        configDir.getChildFile("skin_config.conf")
            .replaceWithText("themeId=1\nfontId=1\nscaleId=3\nwindowScaleId=2\nwindowScale=1.0000\ndebugMode=0\n");
        configDir.getChildFile("user_palettes.conf").replaceWithText({});
        auto view = std::make_unique<ModernEditorView>(model, nullptr);
        view->setVisible(true);  // PluginEditor shows the view in Modern mode
        view->setSize(viewWidth, viewHeight);
        return view;
    }

    void runScenario(const Scenario& scenario) {
        restore(model, pristine);
        scenario.prepare(model);
        const auto prepared = capture(model);

        auto view = makeView();
        for (int tab : scenario.tabs) {
            view->selectTab(tab);
            const auto label = scenario.name + "/" + tabNames[tab];
            layout << "## " << label << " | " << visibleSummary(*view) << "\n";
            for (const auto& e : collect(*view)) layout << e.line << "\n";
            snapshot(*view, scenario.name + "_" + tabNames[tab]);
            if (options.debugTree) dumpTree(*view, label);
        }
        view.reset();

        for (int tab : scenario.tabs) {
            bindings << "## " << scenario.name << "/" << tabNames[tab] << "\n";
            // Enumerate once, then exercise every control on a fresh editor so
            // that each line documents one isolated interaction.
            juce::StringArray ids;
            {
                restore(model, prepared);
                auto probe = makeView();
                probe->selectTab(tab);
                for (const auto& e : collect(*probe))
                    if (!skipInteraction(e.id) && e.component->isEnabled() &&
                        (dynamic_cast<juce::Slider*>(e.component) != nullptr ||
                         dynamic_cast<juce::Button*>(e.component) != nullptr ||
                         dynamic_cast<juce::ComboBox*>(e.component) != nullptr))
                        ids.addIfNotAlreadyThere(e.id);
            }
            for (const auto& id : ids) {
                restore(model, prepared);
                auto v = makeView();
                v->selectTab(tab);
                juce::Component* target = nullptr;
                for (const auto& e : collect(*v))
                    if (e.id == id) { target = e.component; break; }
                if (target == nullptr) { bindings << id << " missing\n"; continue; }
                const auto engineBefore = capture(model);
                const auto uiBefore = uiState(*v);
                const auto action = interact(*target);
                if (action.isEmpty()) continue;
                const auto engineChanges = diff(engineBefore, capture(model));
                bindings << id << " " << action << " | engine: "
                         << (engineChanges.isEmpty() ? juce::String("none") : engineChanges.joinIntoString(" "))
                         << " | " << uiDiff(uiBefore, uiState(*v)) << "\n";
                ++interactions;
            }
        }
        restore(model, pristine);
    }

    // SETTINGS knobs: the wheel scrolls the page once it overflows (87 %
    // window: 960 x 610 less the preset bar) and turns the knob otherwise;
    // a touch drag on a knob never scrolls the page.
    bool checkSettingsScrolling() {
        auto view = makeView();
        view->selectTab(6);
        auto knobsMatch = [&](bool wheel) {
            int knobs = 0;
            bool match = true;
            walk(*view, [&](juce::Component& c) {
                const auto id = c.getComponentID();
                if (id != "customHueKnob" && id != "retroFilterOpacityKnob") return;
                auto* knob = dynamic_cast<juce::Slider*>(&c);
                ++knobs;
                match = match && knob != nullptr && knob->isScrollWheelEnabled() == wheel && knob->getViewportIgnoreDragFlag();
            });
            return knobs == 2 && match;
        };
        const bool full = knobsMatch(true);
        view->setSize(960, 560);
        const bool small = knobsMatch(false);
        view->setSize(viewWidth, viewHeight);
        const bool back = knobsMatch(true);
        return full && small && back;
    }

    bool finish() {
        // Plain \n so the fixtures are identical on every platform.
        options.outDir.getChildFile("layout.txt").replaceWithText(layout, false, false, "\n");
        options.outDir.getChildFile("bindings.txt").replaceWithText(bindings, false, false, "\n");
        options.outDir.getChildFile("pixels.txt").replaceWithText(pixelHashes, false, false, "\n");
        std::cout << "[INFO] " << interactions << " interactions recorded, output in "
                  << options.outDir.getFullPathName() << "\n";

        bool ok = true;
        for (const char* name : { "layout.txt", "bindings.txt" }) {
            const auto actual = options.outDir.getChildFile(name);
            const auto expected = options.fixtureDir.getChildFile(name);
            if (options.update) {
                options.fixtureDir.createDirectory();
                expected.replaceWithText(actual.loadFileAsString(), false, false, "\n");
                std::cout << "[UPDATE] " << expected.getFullPathName() << "\n";
                continue;
            }
            ok = compareText(expected, actual) && ok;
        }
        if (options.pixelBaseline != juce::File()) ok = comparePixels() && ok;
        return ok;
    }

private:
    // Full hierarchy incl. unnamed components; diagnostic only (--debug-tree).
    void dumpTree(juce::Component& root, const juce::String& label) {
        tree << "## " << label << "\n";
        std::function<void(juce::Component&, int)> visit = [&](juce::Component& c, int depth) {
            if (!effectivelyVisible(c, root)) return;
            const auto b = root.getLocalArea(&c, c.getLocalBounds());
            tree << juce::String::repeatedString("  ", depth) << typeid(c).name() << " '" << c.getComponentID() << "' '"
                 << c.getName() << "' " << b.toString() << "\n";
            for (auto* child : c.getChildren()) visit(*child, depth + 1);
        };
        visit(root, 0);
        options.outDir.getChildFile("tree.txt").replaceWithText(tree, false, false, "\n");
    }

    void snapshot(ModernEditorView& view, const juce::String& name) {
        auto image = view.createComponentSnapshot(view.getLocalBounds(), true, 1.0f);
        juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
        uint64_t hash = 1469598103934665603ull;
        for (int y = 0; y < data.height; ++y)
            for (int x = 0; x < data.width; ++x) {
                const auto argb = data.getPixelColour(x, y).getARGB();
                hash = fnv1a(&argb, sizeof(argb), hash);
            }
        pixelHashes << name << " " << hex(hash) << "\n";
        auto dir = options.outDir.getChildFile("pixels");
        dir.createDirectory();
        auto file = dir.getChildFile(name + ".png");
        file.deleteFile();
        juce::FileOutputStream stream(file);
        juce::PNGImageFormat().writeImageToStream(image, stream);
        snapshots.add(name);
    }

    static bool compareText(const juce::File& expected, const juce::File& actual) {
        if (!expected.existsAsFile()) {
            std::cout << "[FAIL] Missing fixture " << expected.getFullPathName() << " (run with --update)\n";
            return false;
        }
        juce::StringArray want, got;
        want.addLines(expected.loadFileAsString());
        got.addLines(actual.loadFileAsString());
        want.removeEmptyStrings();
        got.removeEmptyStrings();
        if (want == got) {
            std::cout << "[PASS] " << expected.getFileName() << " matches (" << got.size() << " lines)\n";
            return true;
        }
        std::cout << "[FAIL] " << expected.getFileName() << " differs from the fixture\n";
        int shown = 0;
        const int n = std::max(want.size(), got.size());
        for (int i = 0; i < n && shown < 40; ++i) {
            if (want[i] == got[i]) continue;
            std::cout << "  line " << (i + 1) << "\n    expected: " << want[i] << "\n    actual:   " << got[i] << "\n";
            ++shown;
        }
        return false;
    }

    bool comparePixels() {
        bool ok = true;
        auto diffDir = options.outDir.getChildFile("pixels-diff");
        diffDir.deleteRecursively();
        for (const auto& name : snapshots) {
            auto baseline = juce::ImageFileFormat::loadFrom(options.pixelBaseline.getChildFile(name + ".png"));
            auto current = juce::ImageFileFormat::loadFrom(options.outDir.getChildFile("pixels").getChildFile(name + ".png"));
            if (!baseline.isValid() || !current.isValid() || baseline.getBounds() != current.getBounds()) {
                std::cout << "[FAIL] pixels " << name << ": missing or different size\n";
                ok = false;
                continue;
            }
            juce::Image delta(juce::Image::ARGB, current.getWidth(), current.getHeight(), true);
            int differing = 0;
            juce::Rectangle<int> area;
            for (int y = 0; y < current.getHeight(); ++y)
                for (int x = 0; x < current.getWidth(); ++x)
                    if (baseline.getPixelAt(x, y) != current.getPixelAt(x, y)) {
                        ++differing;
                        area = area.isEmpty() ? juce::Rectangle<int>(x, y, 1, 1) : area.getUnion({ x, y, 1, 1 });
                        delta.setPixelAt(x, y, juce::Colours::red);
                    } else {
                        delta.setPixelAt(x, y, current.getPixelAt(x, y).withMultipliedAlpha(0.25f));
                    }
            if (differing == 0) continue;
            ok = false;
            std::cout << "[FAIL] pixels " << name << ": " << differing << " px differ in " << area.toString() << "\n";
            diffDir.createDirectory();
            auto file = diffDir.getChildFile(name + ".png");
            juce::FileOutputStream stream(file);
            juce::PNGImageFormat().writeImageToStream(delta, stream);
        }
        if (ok) std::cout << "[PASS] " << snapshots.size() << " snapshots are pixel-identical to the baseline\n";
        return ok;
    }

    const Options& options;
    SynthModel& model;
    juce::File configDir;
    EngineSnapshot pristine;
    juce::String layout, bindings, pixelHashes, tree;
    juce::StringArray snapshots;
    int interactions = 0;
};

bool parse(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]);
        auto next = [&]() -> juce::String { return i + 1 < argc ? juce::String(argv[++i]) : juce::String(); };
        if (arg == "--update") options.update = true;
        else if (arg == "--debug-tree") options.debugTree = true;
        else if (arg == "--out") options.outDir = juce::File::getCurrentWorkingDirectory().getChildFile(next());
        else if (arg == "--pixels") options.pixelBaseline = juce::File::getCurrentWorkingDirectory().getChildFile(next());
        else if (arg == "--fixtures") options.fixtureDir = juce::File::getCurrentWorkingDirectory().getChildFile(next());
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "=== Modern skin characterization ===\n";
    Options options;
    if (!parse(argc, argv, options)) return 2;
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) {
        std::cout << "[SKIP] No X display available for the headless editor.\n";
        return 77;
    }
#endif
    options.outDir.createDirectory();
    // The editor mirrors its settings into ./disk when that folder exists; keep
    // the working directory inside the output folder so no checkout is touched.
    options.outDir.setAsCurrentWorkingDirectory();

    juce::ScopedJuceInitialiser_GUI gui;
    SynthModel model;
    char* noArgs[] = { argv[0] };
    if (!initializeTestData(model, 1, noArgs)) return 1;

    {
        // Knob value boxes: empty when opened, so a value can be typed at once;
        // an empty entry keeps the value, a typed one is taken.
        ModernLookAndFeel lnf;
        juce::Slider knob(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        knob.setLookAndFeel(&lnf);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        knob.setRange(0, 999, 1);
        knob.setValue(500, juce::dontSendNotification);
        knob.setBounds(0, 0, 64, 90);
        // TextEditor posts the return key as a command message; without a
        // message loop the test delivers it directly (JUCE's returnKeyMessageId).
        auto pressReturn = [](juce::TextEditor& editor) {
            static_cast<juce::Component&>(editor).handleCommandMessage(0x10003002);
        };
        juce::Label* box = nullptr;
        for (auto* child : knob.getChildren())
            if (auto* label = dynamic_cast<juce::Label*>(child)) box = label;
        bool pass = box != nullptr;
        if (pass) {
            knob.showTextBox();
            auto* editor = box->getCurrentTextEditor();
            pass = editor != nullptr && editor->getText().isEmpty();
            if (pass) pressReturn(*editor);
            pass = pass && knob.getValue() == 500.0;
            knob.showTextBox();
            editor = box->getCurrentTextEditor();
            pass = pass && editor != nullptr;
            if (pass) {
                editor->setText("250");
                pressReturn(*editor);
            }
            pass = pass && knob.getValue() == 250.0;
            // The slider commits an open box itself before a wheel step:
            // still empty, the value stays.
            knob.showTextBox();
            pass = pass && box->getCurrentTextEditor() != nullptr;
            knob.hideTextBox(false);
            pass = pass && box->getCurrentTextEditor() == nullptr && knob.getValue() == 250.0;
        }
        knob.setLookAndFeel(nullptr);
        std::cout << (pass ? "[PASS]" : "[FAIL]") << " knob value box: empty when opened, empty entry keeps the value\n";
        if (!pass) return 1;
    }

    const std::vector<Scenario> scenarios = {
        { "default", { 0, 1, 2, 3, 4, 5, 6 }, [](SynthModel&) {} },
        { "elements", { 0 }, [](SynthModel& e) { e.getCurrentPreset().steppedParams[spOscEngine] = oeElements; } },
        { "hybrid", { 0 }, [](SynthModel& e) { e.getCurrentPreset().steppedParams[spOscEngine] = oeHybrid; } },
        { "shelves-eq", { 1 }, [](SynthModel& e) {
              e.getCurrentPreset().steppedParams[spFilterModel] = 2;
              e.getCurrentPreset().steppedParams[spFilterMode] = 0;
          } },
        // RIPPLES in the UI: the SEM model's Liquid variant (4).
        { "ripples", { 1 }, [](SynthModel& e) {
              e.getCurrentPreset().steppedParams[spFilterModel] = 1;
              e.getCurrentPreset().steppedParams[spSemModel] = 4;
          } },
        // SST 6 dB: shown under LADDER as its fourth entry.
        { "vintage-6db", { 1 }, [](SynthModel& e) {
              e.getCurrentPreset().steppedParams[spFilterModel] = 3;
              e.getCurrentPreset().steppedParams[spFilterMode] = 3;
          } },
    };

    bool ok = false;
    {
        Harness harness(options, model);
        const bool scrolling = harness.checkSettingsScrolling();
        std::cout << (scrolling ? "[PASS]" : "[FAIL]") << " settings knobs: wheel scrolls the overflowing page\n";
        if (!scrolling) return 1;
        for (const auto& scenario : scenarios) {
            std::cout << "[RUN] " << scenario.name << "\n";
            harness.runScenario(scenario);
        }
        ok = harness.finish();
    }
    std::cout << (ok ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return ok ? 0 : 1;
}
