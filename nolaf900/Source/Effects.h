#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Every effect has: On, Mix (wet/amount) and Rate (its main control). What "Rate" means is listed in the README.
inline constexpr int kNumFx = 13;
struct FxInfo { const char* key; const char* name; bool on; float mix, rate; };
inline constexpr FxInfo kFx[kNumFx] = {
    {"filter",  "Filter FX",       false, 1.0f, 0.60f},
    {"ring",    "Ring Mod",        false, 0.5f, 0.40f},
    {"crush",   "Bit Crusher",     false, 0.5f, 0.40f},
    {"dist",    "Distortion",      false, 0.5f, 0.30f},
    {"pitch",   "Pitch Shifter",   false, 1.0f, 0.50f},
    {"gate",    "Trance Gate",     false, 0.8f, 0.40f},
    {"chorus",  "Chorus",          false, 0.6f, 0.30f},
    {"flanger", "Flanger",         false, 0.6f, 0.30f},
    {"phaser",  "Phaser",          false, 0.6f, 0.30f},
    {"eq",      "EQ Tilt",         false, 0.5f, 0.50f},
    {"stereo",  "Stereo Enhancer", false, 1.0f, 0.75f},
    {"comp",    "Compressor",      false, 1.0f, 0.40f},
    {"limit",   "Limiter",         true,  0.0f, 0.95f}};
enum { FX_FILTER, FX_RING, FX_CRUSH, FX_DIST, FX_PITCH, FX_GATE, FX_CHORUS, FX_FLANGER, FX_PHASER, FX_EQ, FX_STEREO, FX_COMP, FX_LIMIT };

class FxChain {
public:
    explicit FxChain(juce::AudioProcessorValueTreeState& a) {
        for (int i = 0; i < kNumFx; ++i) {
            const juce::String id = juce::String("fx_") + kFx[i].key;
            on[i] = a.getRawParameterValue(id + "_on");
            mixP[i] = a.getRawParameterValue(id + "_mix");
            rateP[i] = a.getRawParameterValue(id + "_rate");
        }
    }

    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        auto sz = [&](double sec) { return (size_t)(sampleRate * sec) + 8; };
        for (int c = 0; c < 2; ++c) {
            pbuf[c].assign(sz(0.25), 0.f); cbuf[c].assign(sz(0.06), 0.f); fbuf[c].assign(sz(0.02), 0.f);
            svf[c].prepare({sampleRate, 512, 1});
            svf[c].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            svf[c].setResonance(1.2f); svf[c].reset();
            eqLo[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 200.f, 0.7f, 1.f);
            eqHi[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 3500.f, 0.7f, 1.f);
            eqLo[c].reset(); eqHi[c].reset();
            for (auto& z : apz[c]) z = 0;
            pfb[c] = 0; crushHold[c] = 0;
        }
        pw = cw = fw = 0; crushCount = 0; eqMix = eqRate = -1.f;
        compEnv = 0; limGain = 1;
    }

    // before delay / reverb
    void processPre(juce::AudioBuffer<float>& b) {
        const int n = b.getNumSamples(), ch = juce::jmin(2, b.getNumChannels());
        if (ch == 0 || n == 0) return;
        float* d[2] = {b.getWritePointer(0), ch > 1 ? b.getWritePointer(1) : nullptr};
        auto isOn = [&](int i) { return on[i]->load() > 0.5f; };
        auto M = [&](int i) { return mixP[i]->load(); };
        auto R = [&](int i) { return rateP[i]->load(); };
        const float pi = juce::MathConstants<float>::pi, twoPi = juce::MathConstants<float>::twoPi;

        if (isOn(FX_FILTER)) {                                   // rate = cutoff 100 Hz .. 12 kHz
            const float m = M(FX_FILTER), fc = juce::jmin(100.f * std::exp(R(FX_FILTER) * std::log(120.f)), sr * 0.45f);
            for (int c = 0; c < ch; ++c) {
                svf[c].setCutoffFrequency(fc);
                for (int i = 0; i < n; ++i) { const float x = d[c][i], w = svf[c].processSample(0, x); d[c][i] = x + (w - x) * m; }
            }
        }
        if (isOn(FX_RING)) {                                     // rate = carrier 20 Hz .. 2 kHz
            const float m = M(FX_RING), f = 20.f * std::pow(100.f, R(FX_RING));
            for (int i = 0; i < n; ++i) {
                ringPh += f / sr; ringPh -= std::floor(ringPh);
                const float car = std::sin(twoPi * ringPh);
                for (int c = 0; c < ch; ++c) d[c][i] += (d[c][i] * car - d[c][i]) * m;
            }
        }
        if (isOn(FX_CRUSH)) {                                    // rate = fewer bits + lower sample rate
            const float m = M(FX_CRUSH), r = R(FX_CRUSH);
            const float levels = std::pow(2.f, juce::jmax(2.f, 16.f - r * 13.f) - 1.f);
            const int hold = 1 + (int)(r * 15.f);
            for (int i = 0; i < n; ++i) {
                if (crushCount == 0) for (int c = 0; c < ch; ++c) crushHold[c] = std::round(d[c][i] * levels) / levels;
                for (int c = 0; c < ch; ++c) d[c][i] += (crushHold[c] - d[c][i]) * m;
                crushCount = (crushCount + 1) % hold;
            }
        }
        if (isOn(FX_DIST)) {                                     // rate = drive
            const float m = M(FX_DIST), drive = 1.f + R(FX_DIST) * 29.f;
            for (int c = 0; c < ch; ++c)
                for (int i = 0; i < n; ++i) { const float x = d[c][i]; d[c][i] = x + (std::tanh(x * drive) * 0.6f - x) * m; }
        }
        if (isOn(FX_PITCH)) {                                    // rate = -12 .. +12 semitones (0.5 = no shift)
            const int N = (int)pbuf[0].size();
            const float m = M(FX_PITCH), win = 0.06f * sr, ratio = std::exp2((R(FX_PITCH) - 0.5f) * 2.f), inc = (1.f - ratio) / win;
            for (int i = 0; i < n; ++i) {
                pphase += inc; pphase -= std::floor(pphase);
                const float p1 = pphase; float p2 = pphase + 0.5f; p2 -= std::floor(p2);
                const float g1 = std::sin(pi * p1), g2 = std::sin(pi * p2), d1 = p1 * win + 2.f, d2 = p2 * win + 2.f;
                for (int c = 0; c < ch; ++c) {
                    pbuf[c][(size_t)pw] = d[c][i];
                    const float wet = readDelay(pbuf[c], pw, d1) * g1 + readDelay(pbuf[c], pw, d2) * g2;
                    d[c][i] += (wet - d[c][i]) * m;
                }
                pw = (pw + 1) % N;
            }
        }
        if (isOn(FX_GATE)) {                                     // rate = step speed, mix = depth
            static const float pat[8] = {1, 0, 1, 1, 0, 1, 0, 1};
            const float m = M(FX_GATE), sps = 2.f + R(FX_GATE) * 22.f;
            for (int i = 0; i < n; ++i) {
                gatePh += sps / sr; if (gatePh >= 8.f) gatePh -= 8.f;
                gateGain += (pat[(int)gatePh & 7] - gateGain) * 0.002f;
                const float g = 1.f - m * (1.f - gateGain);
                for (int c = 0; c < ch; ++c) d[c][i] *= g;
            }
        }
        if (isOn(FX_CHORUS)) {                                   // rate = LFO speed
            const int N = (int)cbuf[0].size();
            const float m = M(FX_CHORUS), f = 0.1f + R(FX_CHORUS) * 3.f;
            for (int i = 0; i < n; ++i) {
                clfo += f / sr; clfo -= std::floor(clfo);
                for (int c = 0; c < ch; ++c) {
                    float ph = clfo + 0.25f * (float)c; ph -= std::floor(ph);
                    const float dly = (0.015f + 0.004f * std::sin(twoPi * ph)) * sr, x = d[c][i];
                    const float wet = readDelay(cbuf[c], cw, dly);
                    cbuf[c][(size_t)cw] = x;
                    d[c][i] = x * (1.f - m * 0.5f) + wet * m * 0.5f;
                }
                cw = (cw + 1) % N;
            }
        }
        if (isOn(FX_FLANGER)) {                                  // rate = LFO speed (feedback fixed)
            const int N = (int)fbuf[0].size();
            const float m = M(FX_FLANGER), f = 0.05f + R(FX_FLANGER) * 2.f;
            for (int i = 0; i < n; ++i) {
                flfo += f / sr; flfo -= std::floor(flfo);
                for (int c = 0; c < ch; ++c) {
                    float ph = flfo + 0.25f * (float)c; ph -= std::floor(ph);
                    const float dly = (0.003f + 0.002f * std::sin(twoPi * ph)) * sr, x = d[c][i];
                    const float wet = readDelay(fbuf[c], fw, dly);
                    fbuf[c][(size_t)fw] = x + wet * 0.6f;
                    d[c][i] = x + wet * m * 0.7f;
                }
                fw = (fw + 1) % N;
            }
        }
        if (isOn(FX_PHASER)) {                                   // rate = LFO speed, 4 stages
            const float m = M(FX_PHASER), f = 0.1f + R(FX_PHASER) * 3.9f;
            for (int i = 0; i < n; ++i) {
                plfo += f / sr; plfo -= std::floor(plfo);
                const float fcen = 300.f + 1700.f * (0.5f + 0.5f * std::sin(twoPi * plfo));
                const float t = std::tan(pi * fcen / sr), a = (t - 1.f) / (t + 1.f);
                for (int c = 0; c < ch; ++c) {
                    const float x = d[c][i]; float y = x + 0.5f * pfb[c];
                    for (int s = 0; s < 4; ++s) { const float o = a * y + apz[c][s]; apz[c][s] = y - a * o; y = o; }
                    pfb[c] = y;
                    d[c][i] = x * (1.f - m * 0.5f) + y * m * 0.5f;
                }
            }
        }
        if (isOn(FX_EQ)) {                                       // rate = tilt (low <-> high), mix = amount (up to 12 dB)
            const float m = M(FX_EQ), r = R(FX_EQ);
            if (m != eqMix || r != eqRate) {
                eqMix = m; eqRate = r;
                const float g = (r - 0.5f) * 2.f * m * 12.f;
                for (int c = 0; c < 2; ++c) {
                    eqLo[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 200.f, 0.7f, juce::Decibels::decibelsToGain(-g));
                    eqHi[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 3500.f, 0.7f, juce::Decibels::decibelsToGain(g));
                }
            }
            for (int c = 0; c < ch; ++c)
                for (int i = 0; i < n; ++i) d[c][i] = eqHi[c].processSample(eqLo[c].processSample(d[c][i]));
        }
        if (isOn(FX_STEREO) && ch == 2) {                        // rate = side level (0.5 = unchanged), mix = blend
            const float m = M(FX_STEREO), side = R(FX_STEREO) * 2.f;
            for (int i = 0; i < n; ++i) {
                const float l = d[0][i], r = d[1][i], mid = (l + r) * 0.5f, sd = (l - r) * 0.5f * side;
                d[0][i] = l + ((mid + sd) - l) * m; d[1][i] = r + ((mid - sd) - r) * m;
            }
        }
    }

    // after reverb, last in the chain
    void processPost(juce::AudioBuffer<float>& b) {
        const int n = b.getNumSamples(), ch = juce::jmin(2, b.getNumChannels());
        if (ch == 0 || n == 0) return;
        float* d[2] = {b.getWritePointer(0), ch > 1 ? b.getWritePointer(1) : nullptr};
        if (on[FX_COMP]->load() > 0.5f) {                        // rate = amount (threshold -6 .. -36 dB, ratio 4:1), mix = parallel blend
            const float m = mixP[FX_COMP]->load(), thr = -6.f - rateP[FX_COMP]->load() * 30.f, makeup = -thr * 0.25f;
            const float ac = std::exp(-1.f / (0.01f * sr)), rc = std::exp(-1.f / (0.12f * sr));
            for (int i = 0; i < n; ++i) {
                float pk = std::abs(d[0][i]); if (ch > 1) pk = juce::jmax(pk, std::abs(d[1][i]));
                const float coef = pk > compEnv ? ac : rc;
                compEnv = coef * compEnv + (1.f - coef) * pk;
                const float over = juce::Decibels::gainToDecibels(compEnv, -100.f) - thr;
                const float g = juce::Decibels::decibelsToGain((over > 0.f ? -over * 0.75f : 0.f) + makeup);
                for (int c = 0; c < ch; ++c) d[c][i] += (d[c][i] * g - d[c][i]) * m;
            }
        }
        if (on[FX_LIMIT]->load() > 0.5f) {                       // mix = input drive 0..12 dB, rate = ceiling -12 .. -0.3 dB
            const float drive = juce::Decibels::decibelsToGain(mixP[FX_LIMIT]->load() * 12.f);
            const float ceil = juce::Decibels::decibelsToGain(-12.f + rateP[FX_LIMIT]->load() * 11.7f);
            const float rel = 1.f - std::exp(-1.f / (0.1f * sr));
            for (int i = 0; i < n; ++i) {
                float pk = std::abs(d[0][i]) * drive; if (ch > 1) pk = juce::jmax(pk, std::abs(d[1][i]) * drive);
                const float target = pk > ceil ? ceil / pk : 1.f;
                if (target < limGain) limGain = target; else limGain += (target - limGain) * rel;
                for (int c = 0; c < ch; ++c) d[c][i] = juce::jlimit(-1.f, 1.f, d[c][i] * drive * limGain);
            }
        }
    }

private:
    static float readDelay(const std::vector<float>& b, int w, float d) {
        const int n = (int)b.size();
        float rp = (float)w - d; while (rp < 0.f) rp += (float)n;
        int i0 = (int)rp; const float fr = rp - (float)i0;
        const int i1 = (i0 + 1) % n; i0 %= n;
        return b[(size_t)i0] + (b[(size_t)i1] - b[(size_t)i0]) * fr;
    }

    std::atomic<float>* on[kNumFx]; std::atomic<float>* mixP[kNumFx]; std::atomic<float>* rateP[kNumFx];
    float sr = 44100.f;
    juce::dsp::StateVariableTPTFilter<float> svf[2];
    float ringPh = 0, crushHold[2] = {}, pphase = 0, gatePh = 0, gateGain = 1, clfo = 0, flfo = 0, plfo = 0;
    int crushCount = 0, pw = 0, cw = 0, fw = 0;
    std::vector<float> pbuf[2], cbuf[2], fbuf[2];
    float apz[2][4] = {}, pfb[2] = {};
    juce::dsp::IIR::Filter<float> eqLo[2], eqHi[2];
    float eqMix = -1.f, eqRate = -1.f, compEnv = 0, limGain = 1;
};
