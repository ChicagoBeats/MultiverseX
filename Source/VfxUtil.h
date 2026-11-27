#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <cmath>

inline constexpr float kPi = juce::MathConstants<float>::pi;
inline constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

inline uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
}
inline float hash01(uint32_t x) { return (float)(hash32(x) & 0xFFFFFFu) / 16777216.f; }

// 64x64 soft white blob. Drawn with "fill alpha channel with current colour" => cheap coloured glow.
inline const juce::Image& softBlob()
{
    static juce::Image img = [] {
        juce::Image i(juce::Image::ARGB, 64, 64, true);
        juce::Graphics g(i);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white, 32.f, 32.f,
                                               juce::Colours::transparentWhite, 64.f, 32.f, true));
        g.fillEllipse(0.f, 0.f, 64.f, 64.f);
        return i;
    }();
    return img;
}
