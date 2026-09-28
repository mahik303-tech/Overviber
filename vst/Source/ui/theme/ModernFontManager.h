#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>

#if __has_include(<BinaryData.h>)
#include <BinaryData.h>
#define OVERCYCLER_HAS_BINARY_DATA 1
#endif

// ==============================================================================
// ModernFontManager: Curated Hardware Typography & System Font Discovery
// ==============================================================================
class ModernFontManager {
public:
    struct FontOption {
        juce::String displayName;
        juce::String fontName;
        juce::String category;
    };

    static juce::Typeface::Ptr getDDINTypeface(bool bold) {
#if defined(OVERCYCLER_HAS_BINARY_DATA)
        if (bold) {
            static auto tfBold = juce::Typeface::createSystemTypefaceFor(
                BinaryData::DDINBold_ttf, BinaryData::DDINBold_ttfSize
            );
            return tfBold;
        }
        static auto tfReg = juce::Typeface::createSystemTypefaceFor(
            BinaryData::DDIN_ttf, BinaryData::DDIN_ttfSize
        );
        return tfReg;
#else
        juce::ignoreUnused(bold);
        return nullptr;
#endif
    }

    static std::vector<FontOption> getCuratedFonts() {
        return {
            { "D-DIN (Embedded DIN 1451)",       "D-DIN",                   "Embedded DIN" },
            { "Bahnschrift (DIN Engineering)",   "Bahnschrift",             "Geometric / DIN" },
            { "Segoe UI (Modern Windows)",       "Segoe UI",                "Modern Sans" },
            { "Consolas (Technical Mono)",       "Consolas",                "Monospace" },
            { "Trebuchet MS (Humanist Sans)",    "Trebuchet MS",            "Humanist Sans" },
            { "Century Gothic (Futuristic Wide)","Century Gothic",          "Geometric Sans" },
            { "Franklin Gothic (Hardware Bold)", "Franklin Gothic Medium",  "Industrial Sans" },
            { "Verdana (High Readability)",      "Verdana",                 "Screen Sans" },
            { "Arial (Universal Neutral)",       "Arial",                   "Neutral Sans" },
            { "Courier New (Retro Terminal)",    "Courier New",             "Monospace" },
            { "JUCE Sans-Serif (Default)",       juce::Font::getDefaultSansSerifFontName(), "System" }
        };
    }

    // Discovers all available system fonts installed on the host machine
    static juce::StringArray getAllSystemFonts() {
        auto names = juce::Font::findAllTypefaceNames();
        names.sort(true);
        return names;
    }

    // Factory method to produce a scaled, styled juce::Font
    static juce::Font createFont(const juce::String& familyName, float size, int styleFlags = juce::Font::plain, float scale = 1.0f) {
        float effectiveSize = std::max(6.0f, size * scale);
        bool isBold = (styleFlags & juce::Font::bold) != 0;

        // Default or explicitly requested D-DIN
        if (familyName == "D-DIN" || familyName.isEmpty() || familyName == "Default" || familyName == "DIN 1451") {
            auto tf = getDDINTypeface(isBold);
            if (tf != nullptr) {
                return juce::Font(tf).withPointHeight(effectiveSize);
            }
        }

#if !defined(_WIN32)
        if (familyName == "Bahnschrift") {
            // Mac / Linux fallback to embedded D-DIN or system DIN
            auto tf = getDDINTypeface(isBold);
            if (tf != nullptr) return juce::Font(tf).withPointHeight(effectiveSize);
            return juce::Font("DIN Alternate", effectiveSize, styleFlags);
        }
#endif

        juce::String targetFamily = familyName.isEmpty() ? juce::Font::getDefaultSansSerifFontName() : familyName;
        return juce::Font(targetFamily, effectiveSize, styleFlags);
    }
};

