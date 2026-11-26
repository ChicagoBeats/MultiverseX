#pragma once
#include <juce_core/juce_core.h>

inline constexpr int kModSlots = 8;

enum ModSrc { S_LFO1, S_LFO2, S_ENV1, S_VEL, S_AFT, S_MOD, S_BREATH, S_RANDOM, S_COUNT };
enum ModDst { D_NONE, D_CUT, D_PIT, D_AMP, D_RES, D_DRV, D_PAN, D_FX, D_REV, D_DLY, D_COUNT };

inline juce::StringArray modSources() {
    return {"LFO 1", "LFO 2", "ENV 1", "Velocity", "Aftertouch", "Mod Wheel", "Breath", "Random"};
}
inline juce::StringArray modTargets() {
    return {"None", "Filter Cutoff", "Pitch", "Amp", "Filter Reso", "Drive", "Pan", "FX Mix", "Reverb", "Delay"};
}

// defaults copied from the mockup's Modulation Matrix
struct ModDefault { int src, dst; float amt; };
inline constexpr ModDefault kModDefaults[kModSlots] = {
    {S_LFO1, D_CUT, 0.75f}, {S_LFO2, D_PIT, 0.50f}, {S_ENV1, D_AMP, 1.00f}, {S_VEL, D_RES, 0.60f},
    {S_AFT, D_FX, 0.40f},   {S_MOD, D_REV, 0.70f},  {S_BREATH, D_DLY, 0.50f}, {S_RANDOM, D_PAN, 0.30f}};

// sv[] = current value of each source; off[] (zeroed by caller) receives the summed amount per target
inline void evalModMatrix(const int* S, const int* D, const float* A, const float* sv, float* off) {
    for (int k = 0; k < kModSlots; ++k)
        if (D[k] > 0 && D[k] < D_COUNT && S[k] >= 0 && S[k] < S_COUNT) off[D[k]] += sv[S[k]] * A[k];
}
