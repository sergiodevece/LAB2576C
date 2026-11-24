#pragma once

#include <array>
#include <cstddef>

namespace vitaplug::lab2576c
{
enum class Unit { db, ratio, ms, hz, percent };
enum Knob : std::size_t
{
    input, threshold, ratio, attack, release, knee, morph, hpf, tilt, link, mix, output, knobCount
};

struct KnobSpec
{
    const char* id;
    const char* faceLabel;
    const char* hostLabel;
    Unit unit;
    float minimum;
    float maximum;
    float step;
    float initial;
    float centre;
};

inline constexpr std::array<KnobSpec, knobCount> knobs {{
    { "input",     "INPUT",     "Input",               Unit::db,      -24.0f,  24.0f, 0.1f,   0.0f,   0.0f },
    { "threshold", "THRESHOLD", "Threshold",           Unit::db,      -60.0f,   0.0f, 0.1f, -18.0f, -30.0f },
    { "ratio",     "RATIO",     "Ratio",               Unit::ratio,     1.0f,  20.0f, 0.1f,   4.0f,   4.0f },
    { "attack",    "ATTACK",    "Attack",              Unit::ms,       0.02f, 100.0f, 0.01f,  3.0f,   3.0f },
    { "release",   "RELEASE",   "Release",             Unit::ms,       30.0f, 3000.0f, 1.0f, 120.0f, 170.0f },
    { "knee",      "KNEE",      "Knee",                Unit::db,        0.0f,  18.0f, 0.1f,   6.0f,   9.0f },
    { "morph",     "MORPH",     "VCA/FET Morph",        Unit::percent,   0.0f, 100.0f, 0.1f,  50.0f,  50.0f },
    { "scHpf",     "SC HPF",    "Sidechain High Pass", Unit::hz,        0.0f, 500.0f, 1.0f,   0.0f, 100.0f },
    { "scTilt",    "SC TILT",   "Sidechain Tilt",      Unit::db,        0.0f,   6.0f, 0.1f,   0.0f,   3.0f },
    { "link",      "LINK",      "Stereo Link",         Unit::percent,   0.0f, 100.0f, 1.0f, 100.0f,  50.0f },
    { "mix",       "MIX",       "Dry/Wet Mix",         Unit::percent,   0.0f, 100.0f, 1.0f, 100.0f,  50.0f },
    { "output",    "OUTPUT",    "Output",              Unit::db,      -24.0f,  24.0f, 0.1f,   0.0f,   0.0f },
}};

inline constexpr auto vcaModeId = "vcaMode";
inline constexpr auto bypassId = "bypass";
inline constexpr int defaultEditorWidth = 1100;
inline constexpr int minimumEditorWidth = 825;
inline constexpr int maximumEditorWidth = 1650;
}
