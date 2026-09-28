#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// ==============================================================================
// Component Design Tokens (Specification v1.0)
// ==============================================================================
namespace ComponentTokens {
    constexpr float cornerRadius = 0.0f;
    constexpr int   grid = 5;
    constexpr float borderWidth = 1.0f;
    constexpr int   cardGap = 5;
    constexpr int   headerHeightMin = 20;
    constexpr int   headerHeightMax = 25;
    constexpr int   protectedVoiceMonitorWidth = 236;
    constexpr float maxLabelWidthRatio = 1.5f;

    namespace KnobSizes {
        // Design Guideline Standard: All rotary knobs across the UI strictly use
        // the uniform Standard XL hardware size (55px).
        constexpr int Standard = 55; // 11x5 (Standard: 55px Hardware Knobs)
        constexpr int Large    = 55; // 11x5 (Standard alias)
        constexpr int XL       = 55; // 11x5 (Standard alias)

        // Deprecated sizes (retained for backward compatibility):
        constexpr int Medium   = 45; // 9x5 (Deprecated)
        constexpr int Small    = 25; // 5x5 (Deprecated)
        constexpr int Compact  = 35; // 7x5 (Deprecated)
        constexpr int Hero     = 65; // 13x5 (Deprecated)
    }
}

// ==============================================================================
// ModernTheme: Comprehensive Color Palette & Visual Style System
// ==============================================================================
struct ModernTheme {
    juce::String name;
    juce::String description;

    // Primary Accents & Illumination
    juce::Colour accent;             // Main glowing accent (rotary active arc, highlights, headers)
    juce::Colour accentDark;         // Deep accent for toggled button backgrounds
    juce::Colour accentGlow;         // Subtle translucent glow for badges, fills, halos

    // Chassis & Surfaces
    juce::Colour windowBg;           // Main plugin background / metal chassis
    juce::Colour cardBg;             // Section card surface
    juce::Colour cardHeader;         // Section card header bar
    juce::Colour cardBorder;         // Frame border outlines and divider lines

    // Controls (Dials, Buttons, Combos)
    juce::Colour knobTrack;          // Rotary slider unlit background track arc
    juce::Colour knobBodyTop;        // Rotary knob metal radial gradient top
    juce::Colour knobBodyBot;        // Rotary knob metal radial gradient bottom
    juce::Colour knobBorder;         // Rotary dial perimeter edge
    juce::Colour knobNeedle;         // Industrial needle pointer
    juce::Colour buttonBg;           // Normal unpressed button body
    juce::Colour buttonBorder;       // Button border line

    // Typography
    juce::Colour textTitle;          // Card titles and active headers
    juce::Colour textBody;           // Knob values, combobox text, body copy
    juce::Colour textMuted;          // Divider labels, subheadings, unit markers

    // Visualizers (Waveform, Filter curve, ADSR, LED Meters)
    juce::Colour visualizerGrid;     // Background grid lines in curves & wave displays
    juce::Colour visualizerCurve;    // Active response curve / waveform stroke
    juce::Colour visualizerFill;     // Shaded translucent area under curves
    juce::Colour meterActive;        // LED voice meter active indicator

    // --------------------------------------------------------------------------
    // Curated Hardware Color Palettes (Refined & Eye-Friendly)
    // --------------------------------------------------------------------------
    static std::vector<ModernTheme> getPresetThemes() {
        return {
            // 1. Cyber Cyan (Modern High-Tech Default)
            {
                "Cyber Cyan (Default)",
                "Refined aerospace cyan illumination with deep slate industrial metal chassis.",
                juce::Colour(0xff18b5c9), // accent (Refined avionics cyan)
                juce::Colour(0xff0d6e7b), // accentDark (Deep teal)
                juce::Colour(0x2818b5c9), // accentGlow
                juce::Colour(0xff0d0f14), // windowBg (Deep pitch slate)
                juce::Colour(0xff13161c), // cardBg (Dark charcoal card)
                juce::Colour(0xff1a1e26), // cardHeader (Header bar)
                juce::Colour(0xff292f3c), // cardBorder (Slate border)
                juce::Colour(0xff222732), // knobTrack
                juce::Colour(0xff2a2e38), // knobBodyTop
                juce::Colour(0xff15171d), // knobBodyBot
                juce::Colour(0xff3b4352), // knobBorder
                juce::Colour(0xffe0f7fa), // knobNeedle
                juce::Colour(0xff1e222a), // buttonBg
                juce::Colour(0xff383f4e), // buttonBorder
                juce::Colour(0xffe0f7fa), // textTitle
                juce::Colour(0xffcfd8dc), // textBody
                juce::Colour(0xff607d8b), // textMuted
                juce::Colour(0xff1a212d), // visualizerGrid
                juce::Colour(0xff18b5c9), // visualizerCurve
                juce::Colour(0x2218b5c9), // visualizerFill
                juce::Colour(0xff18b5c9)  // meterActive
            },

            // 2. Solar Amber (Warm Vintage Tube / Studio Hardware)
            {
                "Solar Amber",
                "Warm golden amber vacuum-tube glow over dark tungsten-bronze chassis.",
                juce::Colour(0xffd4881e), // accent (Warm amber)
                juce::Colour(0xff915609), // accentDark
                juce::Colour(0x28d4881e), // accentGlow
                juce::Colour(0xff14110e), // windowBg (Dark tungsten bronze)
                juce::Colour(0xff1a1612), // cardBg
                juce::Colour(0xff252019), // cardHeader
                juce::Colour(0xff3d3226), // cardBorder
                juce::Colour(0xff2a231b), // knobTrack
                juce::Colour(0xff332b21), // knobBodyTop
                juce::Colour(0xff191510), // knobBodyBot
                juce::Colour(0xff4a3d2e), // knobBorder
                juce::Colour(0xfffff3e0), // knobNeedle
                juce::Colour(0xff241e17), // buttonBg
                juce::Colour(0xff453829), // buttonBorder
                juce::Colour(0xffffe0b2), // textTitle
                juce::Colour(0xffe0d7cd), // textBody
                juce::Colour(0xff8d7a65), // textMuted
                juce::Colour(0xff2a2218), // visualizerGrid
                juce::Colour(0xffe0952a), // visualizerCurve
                juce::Colour(0x22e0952a), // visualizerFill
                juce::Colour(0xffd4881e)  // meterActive
            },

            // 3. Acid Neon Green (Phosphor Oscilloscope / Boutique Synth)
            {
                "Acid Neon Green",
                "Analog phosphor instrument green over obsidian stealth black enclosure.",
                juce::Colour(0xff2bb673), // accent (Phosphor green)
                juce::Colour(0xff1b7348), // accentDark
                juce::Colour(0x282bb673), // accentGlow
                juce::Colour(0xff0a0f0b), // windowBg (Obsidian black)
                juce::Colour(0xff101712), // cardBg
                juce::Colour(0xff17211a), // cardHeader
                juce::Colour(0xff243829), // cardBorder
                juce::Colour(0xff1b261d), // knobTrack
                juce::Colour(0xff243327), // knobBodyTop
                juce::Colour(0xff101712), // knobBodyBot
                juce::Colour(0xff2f4734), // knobBorder
                juce::Colour(0xffe8f5e9), // knobNeedle
                juce::Colour(0xff172219), // buttonBg
                juce::Colour(0xff2d4231), // buttonBorder
                juce::Colour(0xffc8e6c9), // textTitle
                juce::Colour(0xffdcedc8), // textBody
                juce::Colour(0xff68856c), // textMuted
                juce::Colour(0xff182b1c), // visualizerGrid
                juce::Colour(0xff2bb673), // visualizerCurve
                juce::Colour(0x222bb673), // visualizerFill
                juce::Colour(0xff2bb673)  // meterActive
            },

            // 4. Nordic Frost (Anodized Aluminum & Navy Steel)
            {
                "Nordic Frost",
                "Calm Nordic anodized blue with platinum lettering and navy aluminum chassis.",
                juce::Colour(0xff4f8bc9), // accent (Nordic blue)
                juce::Colour(0xff29598a), // accentDark
                juce::Colour(0x284f8bc9), // accentGlow
                juce::Colour(0xff0b111a), // windowBg (Deep arctic navy)
                juce::Colour(0xff101824), // cardBg
                juce::Colour(0xff162233), // cardHeader
                juce::Colour(0xff28384f), // cardBorder
                juce::Colour(0xff1d2a3d), // knobTrack
                juce::Colour(0xff27374f), // knobBodyTop
                juce::Colour(0xff121b29), // knobBodyBot
                juce::Colour(0xff394f6e), // knobBorder
                juce::Colour(0xfff5f9fc), // knobNeedle
                juce::Colour(0xff192436), // buttonBg
                juce::Colour(0xff334661), // buttonBorder
                juce::Colour(0xffe1f5fe), // textTitle
                juce::Colour(0xffd0dfea), // textBody
                juce::Colour(0xff607794), // textMuted
                juce::Colour(0xff19283d), // visualizerGrid
                juce::Colour(0xff4f8bc9), // visualizerCurve
                juce::Colour(0x224f8bc9), // visualizerFill
                juce::Colour(0xff4f8bc9)  // meterActive
            },

            // 5. Synthwave 80s (Retro Outrun & Deep Rose)
            {
                "Synthwave 80s",
                "Deep magenta-rose synthwave illumination with violet-navy twilight casing.",
                juce::Colour(0xffc83366), // accent (Deep magenta-rose)
                juce::Colour(0xff7d1d3d), // accentDark
                juce::Colour(0x28c83366), // accentGlow
                juce::Colour(0xff130d1d), // windowBg (Deep purple dusk)
                juce::Colour(0xff191126), // cardBg
                juce::Colour(0xff231736), // cardHeader
                juce::Colour(0xff3d245c), // cardBorder
                juce::Colour(0xff281b3d), // knobTrack
                juce::Colour(0xff362252), // knobBodyTop
                juce::Colour(0xff170f24), // knobBodyBot
                juce::Colour(0xff4f2d78), // knobBorder
                juce::Colour(0xfffce4ec), // knobNeedle
                juce::Colour(0xff241538), // buttonBg
                juce::Colour(0xff4a2a70), // buttonBorder
                juce::Colour(0xfff8bbd0), // textTitle
                juce::Colour(0xffe1bee7), // textBody
                juce::Colour(0xff8c6e9f), // textMuted
                juce::Colour(0xff2b1942), // visualizerGrid
                juce::Colour(0xffc83366), // visualizerCurve
                juce::Colour(0x22c83366), // visualizerFill
                juce::Colour(0xff00bfa5)  // meterActive (Subtle teal contrast)
            },

            // 6. Industrial Slate (Dieter Rams / Braun Hardware)
            {
                "Industrial Slate",
                "Matte safety orange accent, precision ash grey matte body and silver lettering.",
                juce::Colour(0xffd25228), // accent (Matte industrial orange)
                juce::Colour(0xff8c3214), // accentDark
                juce::Colour(0x28d25228), // accentGlow
                juce::Colour(0xff181a1c), // windowBg (Neutral ash grey)
                juce::Colour(0xff1e2124), // cardBg
                juce::Colour(0xff272b2f), // cardHeader
                juce::Colour(0xff393e45), // cardBorder
                juce::Colour(0xff2b3036), // knobTrack
                juce::Colour(0xff383e46), // knobBodyTop
                juce::Colour(0xff1c2024), // knobBodyBot
                juce::Colour(0xff4d545e), // knobBorder
                juce::Colour(0xfffafafa), // knobNeedle
                juce::Colour(0xff24282c), // buttonBg
                juce::Colour(0xff414852), // buttonBorder
                juce::Colour(0xffffffff), // textTitle
                juce::Colour(0xffcfd4d9), // textBody
                juce::Colour(0xff757d87), // textMuted
                juce::Colour(0xff282d33), // visualizerGrid
                juce::Colour(0xffd25228), // visualizerCurve
                juce::Colour(0x22d25228), // visualizerFill
                juce::Colour(0xffd25228)  // meterActive
            },

            // 7. Obsidian Crimson (Military Radar & Tactical Red)
            {
                "Obsidian Crimson",
                "Military tactical red LED illumination against carbon-black stealth radar casing.",
                juce::Colour(0xffc62828), // accent (Tactical red LED)
                juce::Colour(0xff7a1515), // accentDark
                juce::Colour(0x28c62828), // accentGlow
                juce::Colour(0xff0e0a0b), // windowBg (Carbon black)
                juce::Colour(0xff150f11), // cardBg
                juce::Colour(0xff1f1518), // cardHeader
                juce::Colour(0xff381f25), // cardBorder
                juce::Colour(0xff26161a), // knobTrack
                juce::Colour(0xff331c22), // knobBodyTop
                juce::Colour(0xff140d0f), // knobBodyBot
                juce::Colour(0xff4a242c), // knobBorder
                juce::Colour(0xffffebee), // knobNeedle
                juce::Colour(0xff1e1215), // buttonBg
                juce::Colour(0xff3d1f25), // buttonBorder
                juce::Colour(0xffffcdd2), // textTitle
                juce::Colour(0xffe6cfd3), // textBody
                juce::Colour(0xff8c5d66), // textMuted
                juce::Colour(0xff2b151b), // visualizerGrid
                juce::Colour(0xffc62828), // visualizerCurve
                juce::Colour(0x22c62828), // visualizerFill
                juce::Colour(0xffc62828)  // meterActive
            },

            // 8. Monochrome Stealth (High-Contrast Platinum & Titanium)
            {
                "Monochrome Stealth",
                "Brushed platinum off-white illumination, matte titanium and pitch black body.",
                juce::Colour(0xffdcdfe3), // accent (Platinum off-white, anti-glare)
                juce::Colour(0xff555a62), // accentDark
                juce::Colour(0x28dcdfe3), // accentGlow
                juce::Colour(0xff0a0a0a), // windowBg (Pitch black)
                juce::Colour(0xff121212), // cardBg
                juce::Colour(0xff1c1c1c), // cardHeader
                juce::Colour(0xff303030), // cardBorder
                juce::Colour(0xff242424), // knobTrack
                juce::Colour(0xff333333), // knobBodyTop
                juce::Colour(0xff141414), // knobBodyBot
                juce::Colour(0xff424242), // knobBorder
                juce::Colour(0xffffffff), // knobNeedle
                juce::Colour(0xff1a1a1a), // buttonBg
                juce::Colour(0xff383838), // buttonBorder
                juce::Colour(0xffffffff), // textTitle
                juce::Colour(0xffdcdcdc), // textBody
                juce::Colour(0xff707070), // textMuted
                juce::Colour(0xff202020), // visualizerGrid
                juce::Colour(0xffdcdfe3), // visualizerCurve
                juce::Colour(0x22dcdfe3), // visualizerFill
                juce::Colour(0xffdcdfe3)  // meterActive
            }
        };
    }

    // --------------------------------------------------------------------------
    // Dynamic Custom Hardware Theme Generator
    // --------------------------------------------------------------------------
    static ModernTheme createCustomTheme(juce::Colour customAccent, const juce::String& name = "Custom User Palette") {
        float h = customAccent.getHue();
        float s = customAccent.getSaturation();
        float b = customAccent.getBrightness();

        juce::Colour accentDark = juce::Colour::fromHSV(h, std::min(1.0f, s * 1.1f), std::max(0.2f, b * 0.55f), 1.0f);
        juce::Colour accentGlow = customAccent.withAlpha(0.22f);

        juce::Colour windowBg = juce::Colour::fromHSV(h, std::min(0.20f, s * 0.18f), 0.06f, 1.0f);
        juce::Colour cardBg   = juce::Colour::fromHSV(h, std::min(0.18f, s * 0.15f), 0.09f, 1.0f);
        juce::Colour cardHdr  = juce::Colour::fromHSV(h, std::min(0.16f, s * 0.14f), 0.13f, 1.0f);
        juce::Colour cardBdr  = juce::Colour::fromHSV(h, std::min(0.22f, s * 0.20f), 0.20f, 1.0f);

        juce::Colour knobTrk  = juce::Colour::fromHSV(h, std::min(0.22f, s * 0.20f), 0.16f, 1.0f);
        juce::Colour knobTop  = juce::Colour::fromHSV(h, std::min(0.14f, s * 0.12f), 0.21f, 1.0f);
        juce::Colour knobBot  = juce::Colour::fromHSV(h, std::min(0.14f, s * 0.12f), 0.10f, 1.0f);
        juce::Colour knobBdr  = juce::Colour::fromHSV(h, std::min(0.20f, s * 0.18f), 0.28f, 1.0f);
        juce::Colour needle   = juce::Colour(0xffeceff1);
        juce::Colour btnBg    = juce::Colour::fromHSV(h, std::min(0.16f, s * 0.14f), 0.14f, 1.0f);
        juce::Colour btnBdr   = cardBdr;

        juce::Colour textTitle = juce::Colour(0xffeceff1);
        juce::Colour textBody  = juce::Colour(0xffcfd8dc);
        juce::Colour textMuted = juce::Colour(0xff78909c);

        juce::Colour visGrid  = juce::Colour::fromHSV(h, std::min(0.25f, s * 0.25f), 0.15f, 1.0f);
        juce::Colour visCurve = customAccent;
        juce::Colour visFill  = customAccent.withAlpha(0.18f);
        juce::Colour meterAct = customAccent;

        return {
            name,
            "Custom user-defined hardware theme",
            customAccent,
            accentDark,
            accentGlow,
            windowBg,
            cardBg,
            cardHdr,
            cardBdr,
            knobTrk,
            knobTop,
            knobBot,
            knobBdr,
            needle,
            btnBg,
            btnBdr,
            textTitle,
            textBody,
            textMuted,
            visGrid,
            visCurve,
            visFill,
            meterAct
        };
    }

    // --------------------------------------------------------------------------
    // Color Roles for Granular Palette Customization
    // --------------------------------------------------------------------------
    enum ColorRole {
        RoleAccent = 0,
        RoleWindowBg,
        RoleCardBg,
        RoleCardHeader,
        RoleCardBorder,
        RoleKnobs,
        RoleText,
        NumColorRoles
    };

    static juce::String getRoleName(int role) {
        switch (role) {
            case RoleAccent:     return "Accent / Illumination";
            case RoleWindowBg:   return "Chassis / Background";
            case RoleCardBg:     return "Card & Panel Surface";
            case RoleCardHeader: return "Header Bar & Top Bar";
            case RoleCardBorder: return "Borders & Dividers";
            case RoleKnobs:      return "Knobs & Buttons Body";
            case RoleText:       return "Typography & Headings";
            default:             return "Accent";
        }
    }

    juce::Colour getColorForRole(int role) const {
        switch (role) {
            case RoleAccent:     return accent;
            case RoleWindowBg:   return windowBg;
            case RoleCardBg:     return cardBg;
            case RoleCardHeader: return cardHeader;
            case RoleCardBorder: return cardBorder;
            case RoleKnobs:      return knobBodyTop;
            case RoleText:       return textTitle;
            default:             return accent;
        }
    }

    void setColorForRole(int role, juce::Colour col) {
        float h = col.getHue();
        float s = col.getSaturation();
        float b = col.getBrightness();

        switch (role) {
            case RoleAccent:
                accent = col;
                accentDark = juce::Colour::fromHSV(h, std::min(1.0f, s * 1.1f), std::max(0.2f, b * 0.55f), 1.0f);
                accentGlow = col.withAlpha(0.22f);
                visualizerCurve = col;
                visualizerFill = col.withAlpha(0.18f);
                meterActive = col;
                break;
            case RoleWindowBg:
                windowBg = col;
                break;
            case RoleCardBg:
                cardBg = col;
                break;
            case RoleCardHeader:
                cardHeader = col;
                break;
            case RoleCardBorder:
                cardBorder = col;
                buttonBorder = col;
                knobBorder = col;
                break;
            case RoleKnobs:
                knobBodyTop = col;
                knobBodyBot = col.darker(0.35f);
                buttonBg = col;
                knobTrack = col.darker(0.20f);
                break;
            case RoleText:
                textTitle = col;
                textBody = col.interpolatedWith(juce::Colour(0xffcfd8dc), 0.35f);
                textMuted = col.interpolatedWith(juce::Colour(0xff78909c), 0.50f);
                break;
        }
    }

    // Exports C++ code snippet for adopting the active palette into source files
    juce::String toCppCode() const {
        juce::String s;
        s << "// ModernTheme: " << name << "\n";
        s << "ModernTheme theme;\n";
        s << "theme.name = \"" << name << "\";\n";
        s << "theme.accent = juce::Colour(0x" << accent.toDisplayString(true) << ");\n";
        s << "theme.accentDark = juce::Colour(0x" << accentDark.toDisplayString(true) << ");\n";
        s << "theme.windowBg = juce::Colour(0x" << windowBg.toDisplayString(true) << ");\n";
        s << "theme.cardBg = juce::Colour(0x" << cardBg.toDisplayString(true) << ");\n";
        s << "theme.cardHeader = juce::Colour(0x" << cardHeader.toDisplayString(true) << ");\n";
        s << "theme.cardBorder = juce::Colour(0x" << cardBorder.toDisplayString(true) << ");\n";
        s << "theme.knobBodyTop = juce::Colour(0x" << knobBodyTop.toDisplayString(true) << ");\n";
        s << "theme.buttonBg = juce::Colour(0x" << buttonBg.toDisplayString(true) << ");\n";
        s << "theme.textTitle = juce::Colour(0x" << textTitle.toDisplayString(true) << ");\n";
        s << "theme.textBody = juce::Colour(0x" << textBody.toDisplayString(true) << ");\n";
        s << "theme.visualizerCurve = juce::Colour(0x" << visualizerCurve.toDisplayString(true) << ");\n";
        return s;
    }
};
