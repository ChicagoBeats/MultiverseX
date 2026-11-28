#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;

// ---------------------------------------------------------------- voice
struct MxSound : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

static float osc(int shape, float p) {
    switch (shape) {
        case 0: return std::sin(juce::MathConstants<float>::twoPi * p);
        case 1: return 2.f * p - 1.f;
        case 2: return p < 0.5f ? 1.f : -1.f;
        default: return 4.f * std::abs(p - 0.5f) - 1.f;
    }
}

struct MxVoice : juce::SynthesiserVoice {
    MxVoice(MultiverseXProcessor& p) : proc(p) {
        for (int i = 0; i < kModSlots; ++i) {
            const auto id = "m" + juce::String(i);
            srcP[i] = p.apvts.getRawParameterValue(id + "src");
            dstP[i] = p.apvts.getRawParameterValue(id + "dst");
            amtP[i] = p.apvts.getRawParameterValue(id + "amt");
        }
    }
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<MxSound*>(s) != nullptr; }

    void startNote(int note, float vel, juce::SynthesiserSound*, int) override {
        freq = (float)juce::MidiMessage::getMidiNoteInHertz(note);
        level = vel;
        rnd = rng.nextFloat() * 2.f - 1.f;
        proc.lastVel = vel; proc.lastRnd = rnd; ++proc.noteCount;
        for (auto& p : ph) p = 0;
        adsr.setSampleRate(getSampleRate());
        filt.prepare({getSampleRate(), 512, 1});
        filt.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filt.reset();
        adsr.noteOn();
    }
    void stopNote(float, bool tail) override {
        if (tail) adsr.noteOff(); else { adsr.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override {
        if (!isVoiceActive()) return;
        auto& a = proc.apvts;
        auto P = [&](const char* id) { return a.getRawParameterValue(id)->load(); };
        const auto& eng = kEngines[juce::jlimit(0, kNumEngines - 1, (int)P("engine"))];
        adsr.setParameters({P("atk"), P("dec"), P("sus"), P("rel")});
        const float cutoff = P("cutoff"), res = P("res"), drive = P("drive"), det = P("detune");
        const float l1r = P("lfo1Rate"), l2r = P("lfo2Rate");
        const float sr = (float)getSampleRate();
        int S[kModSlots], D[kModSlots]; float A[kModSlots];
        for (int k = 0; k < kModSlots; ++k) { S[k] = (int)srcP[k]->load(); D[k] = (int)dstP[k]->load(); A[k] = amtP[k]->load(); }
        float dr[4];
        for (int o = 0; o < 4; ++o) dr[o] = std::exp2(((o - 1.5f) * det) / 1200.f);
        const float aft = proc.aftertouch, mw = proc.modWheel, br = proc.breath;
        float l1 = 0, l2 = 0;
        for (int i = 0; i < num; ++i) {
            lp1 += l1r / sr; if (lp1 >= 1) lp1 -= 1;
            lp2 += l2r / sr; if (lp2 >= 1) lp2 -= 1;
            l1 = std::sin(juce::MathConstants<float>::twoPi * lp1);
            l2 = std::sin(juce::MathConstants<float>::twoPi * lp2);
            const float env = adsr.getNextSample();

            const float sv[S_COUNT] = {l1, l2, env, level, aft, mw, br, rnd};
            float off[D_COUNT] = {};
            evalModMatrix(S, D, A, sv, off);

            const float pm = off[D_PIT] != 0.f ? std::exp2(off[D_PIT] * 2.f / 12.f) : 1.f;   // 100% = 2 semitones
            float s = 0;
            for (int o = 0; o < 4; ++o) {
                ph[o] += freq * dr[o] * pm / sr; if (ph[o] >= 1) ph[o] -= 1;
                s += osc(eng.shapes[o], ph[o]) * 0.25f;
            }
            const float fc = juce::jlimit(20.f, 18000.f, cutoff * std::exp2(off[D_CUT] * 4.f));  // 100% = 4 octaves
            filt.setCutoffFrequency(fc);
            filt.setResonance(0.5f + juce::jlimit(0.f, 1.f, res + off[D_RES] * 0.5f) * 4.f);
            s = filt.processSample(0, s);
            const float dv = juce::jlimit(0.f, 1.f, drive + off[D_DRV]);
            s = std::tanh(s * (1.f + dv * 8.f)) / (1.f + dv * 1.5f);
            s *= env * level * juce::jlimit(0.f, 1.5f, 1.f + off[D_AMP]) * 0.6f;

            float gl = 1.f, gr = 1.f;
            if (off[D_PAN] != 0.f) {
                const float ang = (juce::jlimit(-1.f, 1.f, off[D_PAN]) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
                gl = std::cos(ang) * 1.4142f; gr = std::sin(ang) * 1.4142f;
            }
            for (int c = 0; c < out.getNumChannels(); ++c) out.addSample(c, start + i, s * (c == 0 ? gl : gr));
        }
        proc.visLfo1 = l1; proc.visLfo2 = l2;
        if (!adsr.isActive()) clearCurrentNote();
    }

    MultiverseXProcessor& proc;
    juce::ADSR adsr;
    juce::dsp::StateVariableTPTFilter<float> filt;
    juce::Random rng;
    std::atomic<float>* srcP[kModSlots]; std::atomic<float>* dstP[kModSlots]; std::atomic<float>* amtP[kModSlots];
    float freq = 440, level = 0, rnd = 0, ph[4] = {}, lp1 = 0, lp2 = 0;
};

// ---------------------------------------------------------------- sampler
struct MxSampleSound : juce::SynthesiserSound {
    juce::AudioBuffer<float> data; double sr = 44100.0;
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

struct MxSampleVoice : juce::SynthesiserVoice {
    MxSampleVoice(MultiverseXProcessor& p) : proc(p) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<MxSampleSound*>(s) != nullptr; }

    void startNote(int note, float vel, juce::SynthesiserSound* s, int) override {
        playing = false;
        auto* snd = dynamic_cast<MxSampleSound*>(s);
        if (snd == nullptr || snd->data.getNumSamples() < 2) return;
        auto P = [&](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
        const double len = snd->data.getNumSamples();
        double a = juce::jlimit(0.0, 1.0, (double)P("sStart")), b = juce::jlimit(0.0, 1.0, (double)P("sEnd"));
        if (b <= a) return;
        double semis = P("sTune");
        if ((int)P("sMode") == 1) {                       // Slice mode: C1 (note 36) upward = slice 1, 2, 3...
            const int n = juce::jmax(1, (int)P("sSlices")), idx = note - 36;
            if (idx < 0 || idx >= n) return;
            const double w = (b - a) / n;
            a = a + idx * w; b = a + w;
        } else {
            semis += note - 60;                           // Map mode: C3 (note 60) = original pitch
        }
        s0 = a * len; s1 = b * len;
        if (s1 - s0 < 2) return;
        ratio = std::pow(2.0, semis / 12.0 + P("sPitch") / 1200.0) * P("sSpeed") * snd->sr / getSampleRate();
        loop = P("sLoop") > 0.5f; rev = P("sRev") > 0.5f;
        pos = rev ? s1 - 1 : s0;
        gain = P("sVol") * vel;
        const float ang = (juce::jlimit(-1.f, 1.f, P("sPan")) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
        gl = std::cos(ang) * 1.4142f; gr = std::sin(ang) * 1.4142f;
        adsr.setSampleRate(getSampleRate());
        adsr.setParameters({0.002f, 0.f, 1.f, 0.05f});    // tiny fades = no clicks
        adsr.noteOn();
        playing = true;
    }
    void stopNote(float, bool tail) override {
        if (tail && playing) adsr.noteOff();
        else { playing = false; adsr.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override {
        auto* snd = dynamic_cast<MxSampleSound*>(getCurrentlyPlayingSound().get());
        if (!playing || snd == nullptr) { playing = false; clearCurrentNote(); return; }
        const int nch = snd->data.getNumChannels(), len = snd->data.getNumSamples();
        for (int i = 0; i < num; ++i) {
            if (pos < s0 || pos >= s1) {
                if (loop) pos = rev ? s1 - 1 : s0; else { playing = false; break; }
            }
            const int i0 = juce::jlimit(0, len - 1, (int)pos), i1 = juce::jmin(i0 + 1, len - 1);
            const float fr = (float)(pos - i0), env = adsr.getNextSample();
            for (int c = 0; c < out.getNumChannels(); ++c) {
                const float* src = snd->data.getReadPointer(juce::jmin(c, nch - 1));
                const float v = src[i0] + (src[i1] - src[i0]) * fr;
                out.addSample(c, start + i, v * env * gain * (c == 0 ? gl : gr));
            }
            pos += rev ? -ratio : ratio;
        }
        if (!playing || !adsr.isActive()) { playing = false; clearCurrentNote(); }
    }

    MultiverseXProcessor& proc;
    juce::ADSR adsr;
    double pos = 0, s0 = 0, s1 = 0, ratio = 1;
    float gain = 1, gl = 1, gr = 1;
    bool playing = false, loop = false, rev = false;
};

// ---------------------------------------------------------------- processor
juce::AudioProcessorValueTreeState::ParameterLayout MultiverseXProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray names; for (auto& e : kEngines) names.add(e.name);
    l.add(std::make_unique<juce::AudioParameterChoice>("engine", "Engine", names, 0));
    auto f = [&](const char* id, const char* n, float lo, float hi, float def, float skew = 1.f) {
        l.add(std::make_unique<APF>(id, n, juce::NormalisableRange<float>(lo, hi, 0.0001f, skew), def));
    };
    f("cutoff", "Cutoff", 20, 18000, 8000, 0.3f);
    f("res", "Resonance", 0, 1, 0.2f);
    f("drive", "Drive", 0, 1, 0.1f);
    f("atk", "Attack", 0.001f, 5, 0.01f, 0.4f);
    f("dec", "Decay", 0.01f, 5, 0.4f, 0.4f);
    f("sus", "Sustain", 0, 1, 0.8f);
    f("rel", "Release", 0.01f, 8, 0.3f, 0.4f);
    f("detune", "Detune", 0, 50, 8);
    f("lfo1Rate", "LFO1 Rate", 0.05f, 20, 0.5f, 0.4f);
    f("lfo2Rate", "LFO2 Rate", 0.05f, 20, 4, 0.4f);
    f("reverb", "Reverb", 0, 1, 0.25f);
    f("delay", "Delay", 0, 1, 0.2f);
    f("master", "Master", 0, 1, 0.7f);
    // ---- modulation matrix: 8 slots of source > target, amount -100%..+100%
    for (int i = 0; i < kModSlots; ++i) {
        const auto id = "m" + juce::String(i);
        l.add(std::make_unique<juce::AudioParameterChoice>(id + "src", "Mod " + juce::String(i + 1) + " Source", modSources(), kModDefaults[i].src));
        l.add(std::make_unique<juce::AudioParameterChoice>(id + "dst", "Mod " + juce::String(i + 1) + " Target", modTargets(), kModDefaults[i].dst));
        l.add(std::make_unique<APF>(id + "amt", "Mod " + juce::String(i + 1) + " Amount", juce::NormalisableRange<float>(-1.f, 1.f, 0.001f), kModDefaults[i].amt));
    }
    // ---- effects rack: on / mix / rate per effect
    for (int i = 0; i < kNumFx; ++i) {
        const juce::String id = juce::String("fx_") + kFx[i].key;
        l.add(std::make_unique<juce::AudioParameterBool>(id + "_on", juce::String(kFx[i].name) + " On", kFx[i].on));
        l.add(std::make_unique<APF>(id + "_mix", juce::String(kFx[i].name) + " Mix", juce::NormalisableRange<float>(0.f, 1.f), kFx[i].mix));
        l.add(std::make_unique<APF>(id + "_rate", juce::String(kFx[i].name) + " Rate", juce::NormalisableRange<float>(0.f, 1.f), kFx[i].rate));
    }
    f("randAmount", "Random Amount", 0, 1, 1.0f);
    // ---- WAV sampler
    f("sStart", "Start", 0, 1, 0);
    f("sEnd", "End", 0, 1, 1);
    f("sTune", "Tune", -24, 24, 0);
    f("sSpeed", "Speed", 0.25f, 4, 1);
    f("sPitch", "Pitch", -100, 100, 0);
    f("sPan", "Pan", -1, 1, 0);
    f("sVol", "Volume", 0, 1.5f, 0.8f);
    l.add(std::make_unique<juce::AudioParameterInt>("sSlices", "Slices", 2, 32, 8));
    l.add(std::make_unique<juce::AudioParameterBool>("sLoop", "Loop", false));
    l.add(std::make_unique<juce::AudioParameterBool>("sRev", "Reverse", false));
    l.add(std::make_unique<juce::AudioParameterBool>("synthOn", "Synth On", true));
    l.add(std::make_unique<juce::AudioParameterChoice>("sMode", "Sampler Mode", juce::StringArray{"Map", "Slice"}, 0));
    return l;
}

MultiverseXProcessor::MultiverseXProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MVX", layout()) {
    synth.addSound(new MxSound());
    for (int i = 0; i < 128; ++i) synth.addVoice(new MxVoice(*this));
    for (int i = 0; i < 32; ++i) sampler.addVoice(new MxSampleVoice(*this));
}

void MultiverseXProcessor::prepareToPlay(double sr, int) {
    synth.setCurrentPlaybackSampleRate(sr);
    sampler.setCurrentPlaybackSampleRate(sr);
    fx.prepare(sr);
    reverb.setSampleRate(sr);
    delayBuf.setSize(2, (int)(sr * 2.0)); delayBuf.clear(); delayPos = 0;
    keyboardState.reset();
}

void MultiverseXProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    buf.clear();
    keyboardState.processNextMidiBuffer(midi, 0, buf.getNumSamples(), true);
    for (const auto meta : midi) {                         // performance controllers feed the mod matrix
        const auto m = meta.getMessage();
        if (m.isController()) {
            if (m.getControllerNumber() == 1) modWheel = (float)m.getControllerValue() / 127.f;
            else if (m.getControllerNumber() == 2) breath = (float)m.getControllerValue() / 127.f;
        } else if (m.isChannelPressure()) aftertouch = (float)m.getChannelPressureValue() / 127.f;
        else if (m.isAftertouch()) aftertouch = (float)m.getAfterTouchValue() / 127.f;
    }
    if (apvts.getRawParameterValue("synthOn")->load() > 0.5f) synth.renderNextBlock(buf, midi, 0, buf.getNumSamples());
    sampler.renderNextBlock(buf, midi, 0, buf.getNumSamples());

    float rv = apvts.getRawParameterValue("reverb")->load();
    float dl = apvts.getRawParameterValue("delay")->load();
    {   // global (FX) targets use block-rate source values
        int S[kModSlots], D[kModSlots]; float A[kModSlots];
        for (int k = 0; k < kModSlots; ++k) {
            const auto id = "m" + juce::String(k);
            S[k] = (int)apvts.getRawParameterValue(id + "src")->load();
            D[k] = (int)apvts.getRawParameterValue(id + "dst")->load();
            A[k] = apvts.getRawParameterValue(id + "amt")->load();
        }
        const float sv[S_COUNT] = {visLfo1, visLfo2, juce::jlimit(0.f, 1.f, visLevel.load() * 2.f), lastVel, aftertouch, modWheel, breath, lastRnd};
        float off[D_COUNT] = {};
        evalModMatrix(S, D, A, sv, off);
        rv = juce::jlimit(0.f, 1.f, rv + off[D_REV] + off[D_FX]);
        dl = juce::jlimit(0.f, 1.f, dl + off[D_DLY] + off[D_FX]);
        float e = 0;                                       // overall modulation activity for the visualizer
        for (int k = 0; k < kModSlots; ++k) if (D[k] > 0) e += std::abs(sv[S[k]] * A[k]);
        visModEnergy = juce::jlimit(0.f, 1.f, e * 0.35f);
    }
    const float mg = apvts.getRawParameterValue("master")->load();
    fx.processPre(buf);
    const int n = buf.getNumSamples(), ch = juce::jmin(2, buf.getNumChannels());
    const int dlen = delayBuf.getNumSamples(), dtime = (int)(getSampleRate() * 0.375);

    for (int c = 0; c < ch; ++c) {                       // feedback delay
        auto* d = buf.getWritePointer(c); auto* b = delayBuf.getWritePointer(c);
        int pos = delayPos;
        for (int i = 0; i < n; ++i) {
            int rp = (pos - dtime + dlen) % dlen;
            float x = d[i];
            b[pos] = x + b[rp] * 0.4f;
            d[i] = x + b[rp] * dl;
            pos = (pos + 1) % dlen;
        }
    }
    delayPos = (delayPos + n) % dlen;

    juce::Reverb::Parameters rp; rp.roomSize = 0.8f; rp.damping = 0.4f;
    rp.wetLevel = rv * 0.5f; rp.dryLevel = 1.f - rv * 0.25f; rp.width = 1.f;
    reverb.setParameters(rp);
    if (ch == 2) reverb.processStereo(buf.getWritePointer(0), buf.getWritePointer(1), n);

    fx.processPost(buf);
    buf.applyGain(mg);
    visLevel = juce::jlimit(0.f, 1.f, buf.getMagnitude(0, n) * 2.f);
}

void MultiverseXProcessor::setP(const char* id, float v) {
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v));
}

void MultiverseXProcessor::applyEngine(int i) {
    i = juce::jlimit(0, kNumEngines - 1, i);
    const auto& e = kEngines[i];
    setP("engine", (float)i);
    setP("detune", e.detune); setP("cutoff", e.cutoff); setP("res", e.res);
    setP("atk", e.atk); setP("rel", e.rel);
}

void MultiverseXProcessor::blendTo(const juce::String& id, float target, float amt) {
    if (auto* p = apvts.getParameter(id)) {
        const float o = p->getValue();
        p->setValueNotifyingHost(juce::jlimit(0.f, 1.f, o + (target - o) * amt));
    }
}

void MultiverseXProcessor::setChance(const juce::String& id, float amt, juce::Random& r, float target) {
    if (r.nextFloat() < amt)
        if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(target);
}

MultiverseXProcessor::Snapshot MultiverseXProcessor::snapshot() {
    Snapshot s;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) {
            const juce::String id = rp->paramID;
            if (id == "master" || id == "randAmount") continue;     // not part of undo
            s.emplace_back(id, rp->getValue());
        }
    return s;
}

void MultiverseXProcessor::restore(const Snapshot& s) {
    for (auto& kv : s)
        if (auto* p = apvts.getParameter(kv.first)) p->setValueNotifyingHost(kv.second);
}

void MultiverseXProcessor::pushUndo() {
    undoStack.push_back(snapshot());
    if (undoStack.size() > 50) undoStack.erase(undoStack.begin());
    redoStack.clear();
}
void MultiverseXProcessor::undo() {
    if (undoStack.empty()) return;
    redoStack.push_back(snapshot());
    auto s = undoStack.back(); undoStack.pop_back();
    restore(s);
}
void MultiverseXProcessor::redo() {
    if (redoStack.empty()) return;
    undoStack.push_back(snapshot());
    auto s = redoStack.back(); redoStack.pop_back();
    restore(s);
}

void MultiverseXProcessor::doRandomize(int sec) {
    juce::Random r;
    const float amt = apvts.getRawParameterValue("randAmount")->load();   // 100% = full jump, lower = nudge toward new values
    auto rf = [&] { return r.nextFloat(); };
    switch (sec) {
    case SEC_SYNTH:
        if (rf() < amt) applyEngine(r.nextInt(kNumEngines));
        blendTo("cutoff", 0.2f + 0.8f * rf(), amt);
        blendTo("res", 0.6f * rf(), amt);
        blendTo("drive", 0.5f * rf() * rf(), amt);
        blendTo("atk", 0.6f * rf() * rf(), amt);
        blendTo("dec", rf(), amt);
        blendTo("sus", 0.3f + 0.7f * rf(), amt);
        blendTo("rel", 0.7f * rf(), amt);
        blendTo("detune", 0.6f * rf(), amt);
        blendTo("lfo1Rate", rf(), amt);
        blendTo("lfo2Rate", rf(), amt);
        blendTo("reverb", 0.6f * rf(), amt);
        blendTo("delay", 0.5f * rf(), amt);
        break;
    case SEC_MATRIX:
        for (int k = 0; k < kModSlots; ++k) {
            const juce::String id = "m" + juce::String(k);
            setChance(id + "src", amt, r, rf());
            setChance(id + "dst", amt, r, rf());
            blendTo(id + "amt", 0.1f + 0.8f * rf(), amt);
        }
        break;
    case SEC_FX:
        for (int i = 0; i < kNumFx; ++i) {
            if (i == FX_LIMIT) continue;                                   // limiter is the safety stage
            const juce::String id = juce::String("fx_") + kFx[i].key;
            setChance(id + "_on", amt, r, rf() < 0.3f ? 1.f : 0.f);
            blendTo(id + "_mix", 0.3f + 0.7f * rf(), amt);
            blendTo(id + "_rate", rf(), amt);
        }
        break;
    case SEC_SAMPLER:
        blendTo("sPan", rf(), amt);
        blendTo("sPitch", 0.4f + 0.2f * rf(), amt);
        blendTo("sSpeed", 0.07f + 0.4f * rf(), amt);
        setChance("sRev", amt, r, rf() < 0.25f ? 1.f : 0.f);
        break;
    }
}

void MultiverseXProcessor::randomizeSection(int sec) { pushUndo(); doRandomize(sec); }

void MultiverseXProcessor::randomizeAll() {
    pushUndo();
    for (int s = 0; s < SEC_COUNT; ++s) if (!locked[s]) doRandomize(s);
}

void MultiverseXProcessor::getStateInformation(juce::MemoryBlock& d) {
    apvts.state.setProperty("samplePath", samplePath, nullptr);
    for (int i = 0; i < SEC_COUNT; ++i) apvts.state.setProperty("lock" + juce::String(i), locked[i] ? 1 : 0, nullptr);
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d);
}
void MultiverseXProcessor::setStateInformation(const void* d, int n) {
    if (auto xml = getXmlFromBinary(d, n)) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        for (int i = 0; i < SEC_COUNT; ++i) locked[i] = (int)apvts.state.getProperty("lock" + juce::String(i), 0) == 1;
        auto path = apvts.state.getProperty("samplePath").toString();
        if (path.isNotEmpty() && juce::File(path).existsAsFile()) loadSample(juce::File(path));
    }
}

bool MultiverseXProcessor::loadSample(const juce::File& f) {
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r == nullptr || r->lengthInSamples < 2 || r->lengthInSamples > 20000000) return false;
    auto* snd = new MxSampleSound();
    const int len = (int)r->lengthInSamples;
    snd->sr = r->sampleRate;
    snd->data.setSize((int)juce::jmin<unsigned int>(r->numChannels, 2u), len);
    r->read(&snd->data, 0, len, 0, true, true);

    std::vector<float> pk(600, 0.f);                       // overview for the waveform display
    for (int i = 0; i < (int)pk.size(); ++i) {
        const int a = (int)((juce::int64)len * i / (int)pk.size()), b = juce::jmax(a + 1, (int)((juce::int64)len * (i + 1) / (int)pk.size()));
        float m = 0;
        for (int c = 0; c < snd->data.getNumChannels(); ++c)
            m = juce::jmax(m, snd->data.getMagnitude(c, a, juce::jmin(b, len) - a));
        pk[(size_t)i] = m;
    }
    { juce::SpinLock::ScopedLockType l(peakLock); peaks = std::move(pk); }
    sampler.clearSounds();
    sampler.addSound(snd);
    samplePath = f.getFullPathName();
    return true;
}

juce::AudioProcessorEditor* MultiverseXProcessor::createEditor() { return new MultiverseXEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MultiverseXProcessor(); }
