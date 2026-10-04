#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

// ==============================================================================
// Names of the choices of stepped parameters, one table per parameter, in the
// parameter's value order. Each choice has three spellings: the Modern
// editor's label, the host parameter's short text and the Classic skin's
// four-character LCD text.
// ==============================================================================
namespace paramlabels {

struct Choice {
    const char* label;   // Modern editor (full name, e.g. a button's tooltip)
    const char* host;    // host automation parameter
    const char* lcd;     // Classic LCD, four characters
};

// spLFOShape / spLFO2Shape (lfoShape_t)
inline constexpr Choice kLfoShapes[] = {
    { "Pulse / Square", "Pulse", "Sqr " }, { "Triangle", "Triangle", "Tri " },
    { "Random S&H", "Random", "Rand" },    { "Sine", "Sine", "Sine" },
    { "Noise", "Noise", "Nois" },          { "Sawtooth", "Saw", "Saw " },
    { "Inverted Saw", "RevSaw", "RSaw" },
};

// spArpMode (arpMode_t)
inline constexpr Choice kArpModes[] = {
    { "Off (Disabled)", "Off", "Off " },
    { "Up", "Up", "Up  " },
    { "Down", "Down", "Down" },
    { "Up / Down", "Up/Down", "UpDn" },
    { "Random", "Random", "Rand" },
    { "As Played", "As Played", "Asgn" },
    { "Chord (All Voices)", "Chord", "Chrd" },
    { "Converge (Outside-In)", "Converge", "Cnvr" },
    { "Chord Degree (Arpligner)", "Chord Degree", "Degr" },
    { "Poly Strum (Arpligner)", "Poly Strum", "Strm" },
};

// Envelope curve types: index = slow (bit 0) + linear (bit 1).
inline constexpr const char* kEnvelopeTypes[] = { "Fast Exp", "Slow Exp x4", "Fast Lin", "Slow Lin x4" };

// The label in capitals without its explanation in brackets, for badges:
// "Chord Degree (Arpligner)" -> "CHORD DEGREE".
inline std::string title(const Choice& choice) {
    std::string text(choice.label);
    const auto bracket = text.find(" (");
    if (bracket != std::string::npos) text.resize(bracket);
    for (auto& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

template <std::size_t N>
std::vector<std::string> lcdOptions(const Choice (&choices)[N]) {
    std::vector<std::string> options;
    for (const auto& choice : choices) options.emplace_back(choice.lcd);
    return options;
}

}  // namespace paramlabels
