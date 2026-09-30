#pragma once

#include "ModernTabContext.h"
#include "../components/ModernSectionCard.h"

// MOD MATRIX tab, laid out like the AFX tab: the performance controllers
// (pitch bend, mod wheel, aftertouch) in the left card; the 8-slot
// modulation matrix in the main card as a routing overview (select a slot,
// switch it, drag its depth), the editor of the selected slot and quick
// assignments for it.
class ModMatrixTab : public ModernTabModule {
public:
    explicit ModMatrixTab(ModernTabContext& context);

    void setup() override;
    void updateFromEngine() override;
    void resized() override;

    void selectSlot(int slot);
    int getSelectedSlot() const { return selectedSlot; }

    // Writes a slot to the model and the host parameters (the engine follows
    // through them). Used by the editor and the knobs' modulation menu.
    void setSlot(int slot, const ModMatrixSlot& value);
    // Puts `source` on `dest` in the first free slot (depth +50 %) and
    // selects it; returns the slot or -1 when all slots are in use.
    int addModulation(modSource_t source, modDest_t dest);

    // The eight slots as rows: switch, source (via), destination and a
    // bipolar depth bar. Click a row to select it, click its switch to turn
    // it on or off, drag on the bar to set the depth (double click: 0).
    class RoutingView : public juce::Component {
    public:
        RoutingView(SynthModel& model, ModernLookAndFeel& lnf);
        void setSelectedSlot(int slot) { selected = slot; repaint(); }
        juce::Rectangle<int> rowBounds(int slot) const;
        juce::Rectangle<int> switchBounds(int slot) const;
        juce::Rectangle<int> barBounds(int slot) const;
        int depthAt(int slot, int x) const;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseDoubleClick(const juce::MouseEvent& e) override;
        bool keyPressed(const juce::KeyPress& key) override;

        std::function<void(int slot)> onSelect;
        std::function<void(int slot, bool enabled)> onSwitch;
        std::function<void(int slot, int depth)> onDepth;

    private:
        int slotAt(juce::Point<int> p) const;
        SynthModel& model;
        ModernLookAndFeel& lnf;
        int selected = 0;
        int draggingSlot = -1;
    };

private:
    void assignComponentIDs();
    void setupControllers();
    void setupSlotEditor();
    void showSelectedSlot();
    void notifyHost(int slot, const ModMatrixSlot& value);

    ModernSectionCard controllersCard{"PERFORMANCE CONTROLLERS", "MIDI"};
    ModernSectionCard modMatrixCard{"MODULATION MATRIX", "8 SLOTS"};

    // Performance controllers
    std::unique_ptr<juce::ToggleButton> benderRangeToggles[3];
    std::unique_ptr<juce::ToggleButton> benderTargetToggles[5];
    std::unique_ptr<juce::ToggleButton> modwheelRangeToggles[4];
    std::unique_ptr<juce::ToggleButton> modwheelTargetToggles[2];
    std::unique_ptr<juce::ToggleButton> pressureRangeToggles[4];
    std::unique_ptr<juce::ToggleButton> pressureTargetToggles[7];

    // Modulation matrix: overview, editor of the selected slot, quick assignments
    int selectedSlot = 0;
    std::unique_ptr<RoutingView> routingView;
    std::unique_ptr<juce::ToggleButton> slotEnableToggle;
    juce::ComboBox sourceCombo, viaCombo, destCombo;
    std::unique_ptr<juce::Label> sourceLabel, viaLabel, destLabel, depthLabel;
    std::unique_ptr<juce::Slider> depthKnob;
    juce::TextButton clearSlotButton{"CLEAR SLOT"};
    struct QuickAssign {
        const char* text;
        modSource_t source;
        modDest_t dest;
        int depth;
    };
    static const QuickAssign kQuickAssigns[6];
    std::unique_ptr<juce::TextButton> quickButtons[6];
    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrixTab)
};
