#pragma once
#include <juce_core/juce_core.h>

// shapes: 0 sine, 1 saw, 2 square, 3 triangle
struct EngineDef {
    const char* name; juce::uint32 colour;
    int shapes[4]; float detune, cutoff, res, atk, rel;
};

inline const EngineDef kEngines[] = {
 {"MultiverseX Init",       0xff9b5cff, {1,1,2,3}, 8,  8000, 0.2f, 0.01f, 0.30f},
 {"Alien Space Weaver",     0xff2f6bff, {1,3,0,1}, 18, 3500, 0.5f, 0.60f, 2.00f},
 {"AlioNoctis",             0xff2fff5a, {0,3,0,3}, 10, 2500, 0.3f, 0.90f, 2.50f},
 {"Alphatron",              0xffff2f2f, {1,2,1,2}, 14, 5000, 0.4f, 0.02f, 0.40f},
 {"Alphatron Pro",          0xffffd92f, {1,2,1,1}, 20, 6000, 0.5f, 0.02f, 0.50f},
 {"Altair IV",              0xffb02fff, {3,1,3,1}, 12, 4000, 0.3f, 0.40f, 1.50f},
 {"Arracis Gold",           0xff2ffff0, {1,1,3,0}, 16, 7000, 0.25f,0.05f, 0.80f},
 {"ASET-2121g",             0xffff8a2f, {2,1,2,1}, 22, 3000, 0.6f, 0.01f, 0.60f},
 {"Astral Dreamer",         0xffff2fd0, {0,3,1,0}, 9,  3000, 0.3f, 1.20f, 3.00f},
 {"Astral Dreamer Pro",     0xff2fb0ff, {0,3,1,3}, 11, 3500, 0.35f,1.00f, 3.50f},
 {"Atonoise",               0xff4dff2f, {2,2,1,2}, 25, 2000, 0.7f, 0.01f, 0.20f},
 {"Atonoise Pro",           0xff9a2fff, {2,1,2,1}, 30, 2200, 0.7f, 0.01f, 0.25f},
 {"Avatar ST Pro",          0xffe0e0ff, {1,0,3,0}, 7,  4500, 0.2f, 0.80f, 2.20f},
 {"Ghost Machine",          0xffff9a2f, {3,0,3,0}, 6,  1800, 0.4f, 0.70f, 2.80f},
 {"LaserBlade",             0xffb04dff, {1,1,1,2}, 28, 9000, 0.5f, 0.00f, 0.15f},
 {"LaserBlade S",           0xff7dff2f, {1,2,1,2}, 32, 9500, 0.55f,0.00f, 0.12f},
 {"ProtoPlasm",             0xffff2f4d, {1,3,2,0}, 15, 3800, 0.45f,0.30f, 1.20f},
 {"ProtoPlasm TS",          0xff2fd0ff, {1,3,2,3}, 17, 4200, 0.45f,0.25f, 1.10f},
 {"ProtoPlasm 21",          0xffffb02f, {2,3,1,0}, 19, 4600, 0.4f, 0.20f, 1.00f},
 {"ScapesWizard",           0xffffffff, {0,0,3,3}, 13, 2600, 0.3f, 1.50f, 4.00f},
 {"ScapesWizard 2",         0xff2fff9a, {0,3,0,1}, 15, 2800, 0.3f, 1.30f, 4.00f},
 {"Anvilia Pro",            0xff2f8aff, {1,2,2,1}, 21, 5200, 0.5f, 0.01f, 0.35f},
 {"X-Wheel Of Fortune",     0xffffc800, {1,3,1,2}, 24, 4800, 0.4f, 0.10f, 0.90f},
 {"STS-33",                 0xff6fa8ff, {1,1,0,3}, 12, 3300, 0.35f,0.50f, 1.80f},
 {"Shunji",                 0xffa0ff2f, {2,1,3,0}, 10, 3900, 0.3f, 0.08f, 0.70f},
 {"Silver Orbit",           0xffc8c8d8, {0,1,0,3}, 8,  5500, 0.2f, 0.60f, 2.40f},
 {"Swamp XT",               0xff3fa84a, {2,2,1,1}, 26, 1500, 0.65f,0.03f, 0.50f},
 {"The Dreammachine",       0xffff6fd8, {0,3,0,3}, 9,  2900, 0.3f, 1.40f, 3.80f},
 {"The Dreammachine 3",     0xff8f6fff, {0,3,1,3}, 12, 3100, 0.3f, 1.60f, 4.20f},
};
inline constexpr int kNumEngines = (int)(sizeof(kEngines) / sizeof(kEngines[0]));
