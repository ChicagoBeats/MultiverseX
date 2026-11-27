#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

// ============================================================================
//  MultiverseX FX rack: 8 slots, each slot can be any of these effects.
//  Every effect has 3 generic 0..1 knobs (A,B,C) + a per-slot Mix (blend).
// ============================================================================
struct FxTypeInfo { const char* name; const char* a; const char* b; const char* c; };

inline const FxTypeInfo kFxTypes[] = {
    {"None",         "-",       "-",        "-"},
    {"Drive",        "Drive",   "Tone",     "Fold"},
    {"Bitcrusher",   "Bits",    "Rate",     "Noise"},
    {"Auto Filter",  "Cutoff",  "Reso",     "LFO"},
    {"Chorus",       "Rate",    "Depth",    "Spread"},
    {"Flanger",      "Rate",    "Depth",    "Feedback"},
    {"Phaser",       "Rate",    "Depth",    "Feedback"},
    {"Delay",        "Time",    "Feedback", "PingPong"},
    {"Reverb",       "Size",    "Damp",     "Width"},
    {"Tremolo",      "Rate",    "Depth",    "Shape"},
    {"Auto Pan",     "Rate",    "Depth",    "Width"},
    {"Compressor",   "Thresh",  "Ratio",    "Makeup"},
    {"Stutter Gate", "Rate",    "Length",   "Smooth"},
    {"Ring Mod",     "Freq",    "AM Mix",   "Drive"},
    {"Tape",         "Wow",     "Hiss",     "Warmth"},
    {"Limiter",      "Ceiling", "Release",  "Gain"},
};
inline constexpr int kNumFxTypes = (int)(sizeof(kFxTypes) / sizeof(kFxTypes[0]));
inline constexpr int kNumFxSlots = 8;

struct FxUnit
{
    void prepare(double sr, int maxBlock)
    {
        sampleRate = sr;
        len = (int)(sr * 2.5) + 8;
        for (auto& d : dl) d.assign((size_t)len, 0.f);
        reverb.setSampleRate(sr);
        wet.setSize(2, juce::jmax(16, maxBlock));
        wet.clear();
        resetState();
        lastType = -1;
    }

    void resetState()
    {
        for (auto& d : dl) std::fill(d.begin(), d.end(), 0.f);
        wp = 0; lfoPh = 0; lfoPh2 = 0; gatePh = 0; gGain = 1.f; envDb = -100.f; limG = 1.f; dtS = 0.f;
        for (int c = 0; c < 2; ++c) {
            lp[c] = 0; ic1[c] = ic2[c] = 0; held[c] = 0; holdCnt[c] = 0; fbS[c] = 0;
            for (int s = 0; s < 6; ++s) ap[c][s] = 0;
        }
        reverb.reset();
    }

    // io is processed in place: out = dry + (fx - dry) * mix
    void process(juce::AudioBuffer<float>& io, int type, float a, float b, float c, float mix, double bpm)
    {
        const int n = io.getNumSamples();
        if (type <= 0 || type >= kNumFxTypes || n <= 0 || io.getNumChannels() < 1) return;
        if (type != lastType) { resetState(); lastType = type; mixS = mix; }

        const int nch = juce::jmin(2, io.getNumChannels());
        wet.setSize(2, n, false, false, true);
        for (int ch = 0; ch < 2; ++ch) wet.copyFrom(ch, 0, io, juce::jmin(ch, nch - 1), 0, n);
        float* L = wet.getWritePointer(0);
        float* R = wet.getWritePointer(1);
        const float sr = (float)sampleRate;
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float pi = juce::MathConstants<float>::pi;

        switch (type)
        {
        case 1: { // Drive
            const float drive = 1.f + a * a * 40.f;
            const float k = 1.f - std::exp(-twoPi * (400.f + b * b * 17000.f) / sr);
            const float comp = 1.f / (1.f + a * 2.f);
            float* ch[2] = {L, R};
            for (int c2 = 0; c2 < 2; ++c2)
                for (int i = 0; i < n; ++i) {
                    float y = std::tanh(ch[c2][i] * drive);
                    if (c > 0.001f) y = y * (1.f - c) + std::sin(y * 1.5707963f * (1.f + c * 3.f)) * c;
                    lp[c2] += k * (y - lp[c2]);
                    ch[c2][i] = lp[c2] * comp * 1.3f;
                }
            break; }
        case 2: { // Bitcrusher
            const float bits = 16.f - a * 14.f;
            const float levels = std::pow(2.f, bits - 1.f);
            const float hold = 1.f + b * b * 63.f;
            float* ch[2] = {L, R};
            for (int c2 = 0; c2 < 2; ++c2)
                for (int i = 0; i < n; ++i) {
                    holdCnt[c2] -= 1.f;
                    if (holdCnt[c2] <= 0.f) {
                        float x = ch[c2][i] + (rnd.nextFloat() - 0.5f) * c * 0.05f;
                        held[c2] = std::round(x * levels) / levels;
                        holdCnt[c2] += hold;
                    }
                    ch[c2][i] = held[c2];
                }
            break; }
        case 3: { // Auto Filter (SVF lowpass swept by an LFO)
            const float baseHz = 40.f * std::pow(2.f, a * 9.f);
            const float Q = 0.6f + b * b * 12.f, kk = 1.f / Q;
            const float rate = 0.2f + c * 6.f;
            float* ch[2] = {L, R};
            for (int i = 0; i < n; ++i) {
                lfoPh += rate / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
                const float m = std::sin(twoPi * lfoPh);
                const float fc = juce::jlimit(30.f, 18000.f, baseHz * std::pow(2.f, m * c * 3.f));
                const float g = std::tan(pi * fc / sr);
                const float a1 = 1.f / (1.f + g * (g + kk)), a2 = g * a1, a3 = g * a2;
                for (int c2 = 0; c2 < 2; ++c2) {
                    const float v3 = ch[c2][i] - ic2[c2];
                    const float v1 = a1 * ic1[c2] + a2 * v3;
                    const float v2 = ic2[c2] + a2 * ic1[c2] + a3 * v3;
                    ic1[c2] = 2.f * v1 - ic1[c2];
                    ic2[c2] = 2.f * v2 - ic2[c2];
                    ch[c2][i] = v2;
                }
            }
            break; }
        case 4: { // Chorus
            const float rate = 0.1f + a * 4.f, depthMs = 1.f + b * 9.f;
            float* ch[2] = {L, R};
            for (int i = 0; i < n; ++i) {
                lfoPh += rate / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
                for (int c2 = 0; c2 < 2; ++c2) {
                    const float ph = lfoPh + (c2 ? c * 0.5f : 0.f);
                    const float m = std::sin(twoPi * ph);
                    dl[c2][(size_t)wp] = ch[c2][i];
                    const float ds = (14.f + m * depthMs) * 0.001f * sr;
                    ch[c2][i] = rd(c2, ds);
                }
                if (++wp >= len) wp = 0;
            }
            break; }
        case 5: { // Flanger
            const float rate = 0.05f + a * 3.f, depthMs = 0.5f + b * 6.f, fb = c * 0.9f;
            float* ch[2] = {L, R};
            for (int i = 0; i < n; ++i) {
                lfoPh += rate / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
                const float m = 0.5f + 0.5f * std::sin(twoPi * lfoPh);
                const float ds = juce::jmax(1.f, (0.4f + m * depthMs) * 0.001f * sr);
                for (int c2 = 0; c2 < 2; ++c2) {
                    const float x = ch[c2][i];
                    dl[c2][(size_t)wp] = x + fbS[c2] * fb;
                    const float y = rd(c2, ds);
                    fbS[c2] = y;
                    ch[c2][i] = (x + y) * 0.7071f;
                }
                if (++wp >= len) wp = 0;
            }
            break; }
        case 6: { // Phaser (6-stage allpass)
            const float rate = 0.05f + a * 3.f, fb = c * 0.85f;
            float* ch[2] = {L, R};
            for (int i = 0; i < n; ++i) {
                lfoPh += rate / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
                const float sweep = 0.5f + 0.5f * std::sin(twoPi * lfoPh);
                const float fc = 200.f * std::pow(2.f, sweep * (0.5f + b * 3.8f));
                const float g = std::tan(pi * juce::jlimit(20.f, 18000.f, fc) / sr);
                const float a1 = (g - 1.f) / (g + 1.f);
                for (int c2 = 0; c2 < 2; ++c2) {
                    float x = ch[c2][i] + fbS[c2] * fb;
                    for (int s = 0; s < 6; ++s) { const float y = a1 * x + ap[c2][s]; ap[c2][s] = x - a1 * y; x = y; }
                    fbS[c2] = x;
                    ch[c2][i] = x;
                }
            }
            break; }
        case 7: { // Delay with ping-pong
            const float target = juce::jlimit(8.f, (float)(len - 4), 0.02f * std::pow(60.f, a) * sr);
            const float fb = b * 0.95f;
            if (dtS <= 0.f) dtS = target;
            for (int i = 0; i < n; ++i) {
                dtS += (target - dtS) * 0.0008f;
                const float dL = rd(0, dtS), dR = rd(1, dtS);
                const float inL = L[i], inR = R[i];
                const float fbL = dL * (1.f - c) + dR * c, fbR = dR * (1.f - c) + dL * c;
                dl[0][(size_t)wp] = inL + fbL * fb;
                dl[1][(size_t)wp] = inR * (1.f - c) + fbR * fb;
                L[i] = dL; R[i] = dR;
                if (++wp >= len) wp = 0;
            }
            break; }
        case 8: { // Reverb (wet only; slot mix blends)
            juce::Reverb::Parameters rp;
            rp.roomSize = a; rp.damping = b; rp.width = c;
            rp.wetLevel = 0.33f; rp.dryLevel = 0.f; rp.freezeMode = 0.f;
            reverb.setParameters(rp);
            reverb.processStereo(L, R, n);
            break; }
        case 9: { // Tremolo
            const float rate = 0.5f * std::pow(2.f, a * 5.f);
            for (int i = 0; i < n; ++i) {
                lfoPh += rate / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
                const float m = std::sin(twoPi * lfoPh);
                const float lfo = m * (1.f - c) + std::tanh(m * 6.f) * c;
                const float g = 1.f - b * (0.5f - 0.5f * lfo);
                L[i] *= g; R[i] *= g;
            }
            break; }
        case 10: { // Auto Pan + width
            const float rate = 0.5f * std::pow(2.f, a * 5.f);
            for (int i = 0; i < n; ++i) {
                lfoPh += rate / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
                const float pan = std::sin(twoPi * lfoPh) * b;
                const float ang = (pan + 1.f) * pi * 0.25f;
                const float M = (L[i] + R[i]) * 0.5f, S = (L[i] - R[i]) * 0.5f * (c * 2.f);
                L[i] = (M + S) * std::cos(ang) * 1.4142f;
                R[i] = (M - S) * std::sin(ang) * 1.4142f;
            }
            break; }
        case 11: { // Compressor
            const float thr = -a * 40.f, ratio = 1.f + b * 19.f;
            const float mk = juce::Decibels::decibelsToGain(c * 18.f);
            const float atk = std::exp(-1.f / (0.005f * sr)), rel = std::exp(-1.f / (0.12f * sr));
            for (int i = 0; i < n; ++i) {
                const float det = juce::jmax(std::abs(L[i]), std::abs(R[i]));
                const float db = juce::Decibels::gainToDecibels(det, -100.f);
                const float coef = db > envDb ? atk : rel;
                envDb = db + coef * (envDb - db);
                const float over = envDb - thr;
                const float gr = over > 0.f ? over * (1.f - 1.f / ratio) : 0.f;
                const float g = juce::Decibels::decibelsToGain(-gr) * mk;
                L[i] *= g; R[i] *= g;
            }
            break; }
        case 12: { // Stutter / trance gate (host tempo)
            const int div = juce::jlimit(0, 3, (int)(a * 3.99f));
            const float beats = 1.f / (float)(1 << div);
            const float inc = (float)(bpm / 60.0) / beats / sr;
            const float openLen = 0.05f + b * 0.9f;
            const float coef = juce::jlimit(0.00001f, 1.f, std::pow(10.f, -0.3f - 3.2f * c));
            for (int i = 0; i < n; ++i) {
                gatePh += inc; if (gatePh >= 1.f) gatePh -= 1.f;
                gGain += ((gatePh < openLen ? 1.f : 0.f) - gGain) * coef;
                L[i] *= gGain; R[i] *= gGain;
            }
            break; }
        case 13: { // Ring mod / AM
            const float freq = 20.f * std::pow(2.f, a * 9.f);
            for (int i = 0; i < n; ++i) {
                lfoPh += freq / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
                const float m = std::sin(twoPi * lfoPh);
                const float mod = (1.f - b) * m + b * (0.5f + 0.5f * m);
                const float dg = 1.f + c * 6.f;
                L[i] = std::tanh(L[i] * mod * dg) / (0.6f + 0.4f * dg);
                R[i] = std::tanh(R[i] * mod * dg) / (0.6f + 0.4f * dg);
            }
            break; }
        case 14: { // Tape: wow/flutter + hiss + saturation
            const float k = 1.f - std::exp(-twoPi * (16000.f - c * 11000.f) / sr);
            float* ch[2] = {L, R};
            for (int i = 0; i < n; ++i) {
                lfoPh += 0.6f / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
                lfoPh2 += 7.f / sr; if (lfoPh2 >= 1.f) lfoPh2 -= 1.f;
                const float ds = (6.f + std::sin(twoPi * lfoPh) * a * 2.5f + std::sin(twoPi * lfoPh2) * a * 0.3f) * 0.001f * sr;
                for (int c2 = 0; c2 < 2; ++c2) {
                    dl[c2][(size_t)wp] = ch[c2][i];
                    float y = rd(c2, ds) + (rnd.nextFloat() - 0.5f) * b * 0.05f;
                    y = std::tanh(y * (1.f + c * 3.f)) / (1.f + c);
                    lp[c2] += k * (y - lp[c2]);
                    ch[c2][i] = lp[c2] * (1.f + c * 0.8f);
                }
                if (++wp >= len) wp = 0;
            }
            break; }
        case 15: { // Limiter
            const float ceil = juce::Decibels::decibelsToGain(-a * 12.f);
            const float inG = juce::Decibels::decibelsToGain(c * 18.f);
            const float relCoef = std::exp(-1.f / ((0.02f + b * 0.5f) * sr));
            for (int i = 0; i < n; ++i) {
                L[i] *= inG; R[i] *= inG;
                const float peak = juce::jmax(std::abs(L[i]), std::abs(R[i]));
                const float target = peak > ceil ? ceil / peak : 1.f;
                if (target < limG) limG = target; else limG = target + relCoef * (limG - target);
                L[i] = juce::jlimit(-ceil, ceil, L[i] * limG);
                R[i] = juce::jlimit(-ceil, ceil, R[i] * limG);
            }
            break; }
        default: break;
        }

        // ---- blend (per-slot Mix) with smoothing across the block
        const float dm = (mix - mixS) / (float)n;
        for (int i = 0; i < n; ++i) {
            mixS += dm;
            for (int c2 = 0; c2 < nch; ++c2) {
                float* d = io.getWritePointer(c2);
                const float w = wet.getReadPointer(c2)[i];
                d[i] += (w - d[i]) * mixS;
            }
        }
        mixS = mix;
    }

private:
    float rd(int ch, float ds) const
    {
        float pos = (float)wp - ds;
        while (pos < 0.f) pos += (float)len;
        int i0 = (int)pos; const float fr = pos - (float)i0;
        if (i0 >= len) i0 -= len;
        int i1 = i0 + 1; if (i1 >= len) i1 = 0;
        return dl[ch][(size_t)i0] * (1.f - fr) + dl[ch][(size_t)i1] * fr;
    }

    double sampleRate = 44100.0;
    int len = 1, wp = 0, lastType = -1;
    std::vector<float> dl[2];
    juce::AudioBuffer<float> wet;
    juce::Reverb reverb;
    juce::Random rnd;
    float lfoPh = 0, lfoPh2 = 0, gatePh = 0, gGain = 1.f, envDb = -100.f, limG = 1.f, dtS = 0.f, mixS = 1.f;
    float lp[2] = {}, ic1[2] = {}, ic2[2] = {}, held[2] = {}, holdCnt[2] = {}, fbS[2] = {};
    float ap[2][6] = {};
};
