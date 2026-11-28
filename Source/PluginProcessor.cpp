#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;

// ---------------------------------------------------------------- synth voice
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
    MxVoice(MultiverseXProcessor& p) : proc(p) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<MxSound*>(s) != nullptr; }

    void startNote(int note, float vel, juce::SynthesiserSound*, int) override {
        freq = (float)juce::MidiMessage::getMidiNoteInHertz(note);
        level = vel;
        for (auto& p : ph) p = 0;
        adsr.setSampleRate(getSampleRate());
        fadsr.setSampleRate(getSampleRate());
        filt.prepare({getSampleRate(), 512, 1});
        filt.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filt.reset();
        adsr.noteOn(); fadsr.noteOn();
    }
    void stopNote(float, bool tail) override {
        if (tail) { adsr.noteOff(); fadsr.noteOff(); } else { adsr.reset(); fadsr.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override {
        if (!isVoiceActive()) return;
        auto& a = proc.apvts;
        auto P = [&](const char* id) { return a.getRawParameterValue(id)->load(); };
        const auto& engA = kEngines[juce::jlimit(0, kNumEngines - 1, (int)P("engine"))];
        const auto& engB = kEngines[juce::jlimit(0, kNumEngines - 1, (int)P("engine2"))];
        const float eb = P("engBlend");
        adsr.setParameters({P("atk"), P("dec"), P("sus"), P("rel")});
        fadsr.setParameters({P("fAtk"), P("fDec"), P("fSus"), P("fRel")});
        const float cutoff = P("cutoff"), res = P("res"), drive = P("drive"), det = P("detune"), fenv = P("fEnv");
        const float l1r = P("lfo1Rate"), l1c = P("lfo1Cut"), l2r = P("lfo2Rate"), l2p = P("lfo2Pitch");
        const float sr = (float)getSampleRate();
        float l1 = 0, l2 = 0;
        for (int i = 0; i < num; ++i) {
            lp1 += l1r / sr; if (lp1 >= 1) lp1 -= 1;
            lp2 += l2r / sr; if (lp2 >= 1) lp2 -= 1;
            l1 = std::sin(juce::MathConstants<float>::twoPi * lp1);
            l2 = std::sin(juce::MathConstants<float>::twoPi * lp2);
            float s = 0;
            for (int o = 0; o < 4; ++o) {
                float f = freq * std::pow(2.f, ((o - 1.5f) * det) / 1200.f + l2 * l2p * 2.f / 12.f);
                ph[o] += f / sr; if (ph[o] >= 1) ph[o] -= 1;
                s += ((1.f - eb) * osc(engA.shapes[o], ph[o]) + eb * osc(engB.shapes[o], ph[o])) * 0.25f;
            }
            const float fe = fadsr.getNextSample();
            float fc = juce::jlimit(20.f, 18000.f, cutoff * std::pow(2.f, l1 * l1c * 3.f + fe * fenv * 5.f));
            filt.setCutoffFrequency(fc);
            filt.setResonance(0.5f + res * 4.f);
            s = filt.processSample(0, s);
            s = std::tanh(s * (1.f + drive * 8.f)) / (1.f + drive * 1.5f);
            s *= adsr.getNextSample() * level;
            for (int c = 0; c < out.getNumChannels(); ++c) out.addSample(c, start + i, s);
        }
        proc.visLfo1 = l1; proc.visLfo2 = l2;
        if (!adsr.isActive()) clearCurrentNote();
    }

    MultiverseXProcessor& proc;
    juce::ADSR adsr, fadsr;
    juce::dsp::StateVariableTPTFilter<float> filt;
    float freq = 440, level = 0, ph[4] = {}, lp1 = 0, lp2 = 0;
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
        if ((int)P("sMode") == 1) {
            const int n = juce::jmax(1, (int)P("sSlices")), idx = note - 36;
            if (idx < 0 || idx >= n) return;
            const double w = (b - a) / n;
            a = a + idx * w; b = a + w;
        } else {
            semis += note - 60;
        }
        s0 = a * len; s1 = b * len;
        if (s1 - s0 < 2) return;
        ratio = std::pow(2.0, semis / 12.0 + P("sPitch") / 1200.0) * P("sSpeed") * snd->sr / getSampleRate();
        loop = P("sLoop") > 0.5f; rev = P("sRev") > 0.5f;
        pos = rev ? s1 - 1 : s0;
        gain = vel;   // level (dB) is applied in processBlock
        const float ang = (juce::jlimit(-1.f, 1.f, P("sPan")) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
        gl = std::cos(ang) * 1.4142f; gr = std::sin(ang) * 1.4142f;
        adsr.setSampleRate(getSampleRate());
        adsr.setParameters({0.002f, 0.f, 1.f, 0.05f});
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

// ---------------------------------------------------------------- parameters
juce::StringArray MultiverseXProcessor::fxIds()
{
    juce::StringArray ids;
    for (int s = 1; s <= kNumFxSlots; ++s) {
        const juce::String p = "fx" + juce::String(s) + "_";
        for (auto* suf : { "type", "on", "mix", "a", "b", "c" }) ids.add(p + suf);
    }
    ids.add("fxMix");
    return ids;
}

juce::AudioProcessorValueTreeState::ParameterLayout MultiverseXProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray names; for (auto& e : kEngines) names.add(e.name);
    l.add(std::make_unique<juce::AudioParameterChoice>("engine", "Engine A", names, 0));
    l.add(std::make_unique<juce::AudioParameterChoice>("engine2", "Engine B", names, 1));
    auto f = [&](const juce::String& id, const juce::String& n, float lo, float hi, float def, float skew = 1.f) {
        l.add(std::make_unique<APF>(id, n, juce::NormalisableRange<float>(lo, hi, 0.0001f, skew), def));
    };
    f("engBlend", "Engine A<>B Blend", 0, 1, 0);
    f("cutoff", "Cutoff", 20, 18000, 8000, 0.3f);
    f("res", "Resonance", 0, 1, 0.2f);
    f("drive", "Drive", 0, 1, 0.1f);
    f("atk", "Attack", 0.001f, 5, 0.01f, 0.4f);
    f("dec", "Decay", 0.01f, 5, 0.4f, 0.4f);
    f("sus", "Sustain", 0, 1, 0.8f);
    f("rel", "Release", 0.01f, 8, 0.3f, 0.4f);
    f("fAtk", "Filter Attack", 0.001f, 5, 0.01f, 0.4f);
    f("fDec", "Filter Decay", 0.01f, 5, 0.4f, 0.4f);
    f("fSus", "Filter Sustain", 0, 1, 0.5f);
    f("fRel", "Filter Release", 0.01f, 8, 0.3f, 0.4f);
    f("fEnv", "Filter Env Amount", -1, 1, 0);
    f("detune", "Detune", 0, 50, 8);
    f("lfo1Rate", "LFO1 Rate", 0.05f, 20, 0.5f, 0.4f);
    f("lfo1Cut", "LFO1>Cutoff", 0, 1, 0.75f);
    f("lfo2Rate", "LFO2 Rate", 0.05f, 20, 4, 0.4f);
    f("lfo2Pitch", "LFO2>Pitch", 0, 1, 0.1f);
    f("mixBlend", "Synth+FX <> Sample Blend", 0, 1, 0.5f);
    {   // volumes in dB. Synth: -100..0.  Sample: -100..+21.6 (-100 = silent)
        juce::NormalisableRange<float> sr(-100.f, 21.6f, 0.01f); sr.setSkewForCentre(-12.f);
        juce::NormalisableRange<float> yr(-100.f, 0.f, 0.01f);   yr.setSkewForCentre(-12.f);
        l.add(std::make_unique<APF>("synthVol", "Synth Volume", yr, 0.f, juce::AudioParameterFloatAttributes().withLabel(" dB")));
        l.add(std::make_unique<APF>("sVol", "Sample Volume", sr, 0.f, juce::AudioParameterFloatAttributes().withLabel(" dB")));
    }
    f("sFxSend", "Sample > FX Send", 0, 1, 0);
    f("master", "Master", 0, 1, 0.7f);
    f("inGain", "Input Gain", 0, 2, 1);
    // ---- FX rack
    f("fxMix", "FX Rack Wet/Dry", 0, 1, 1);
    f("morph", "FX Morph A<>B", 0, 1, 0);
    juce::StringArray fxNames; for (auto& t : kFxTypes) fxNames.add(t.name);
    struct Def { int type; bool on; float mix, a, b, c; };
    const Def defs[kNumFxSlots] = {
        {1, false, 1.f, 0.3f, 0.8f, 0.f}, {4, false, 0.5f, 0.3f, 0.4f, 0.5f}, {7, true, 0.2f, 0.7f, 0.4f, 0.3f},
        {8, true, 0.25f, 0.8f, 0.4f, 1.f}, {0, false, 1.f, 0.5f, 0.5f, 0.5f}, {0, false, 1.f, 0.5f, 0.5f, 0.5f},
        {0, false, 1.f, 0.5f, 0.5f, 0.5f}, {0, false, 1.f, 0.5f, 0.5f, 0.5f} };
    for (int s = 1; s <= kNumFxSlots; ++s) {
        const juce::String p = "fx" + juce::String(s) + "_", n = "FX" + juce::String(s) + " ";
        const auto& d = defs[s - 1];
        l.add(std::make_unique<juce::AudioParameterChoice>(p + "type", n + "Type", fxNames, d.type));
        l.add(std::make_unique<juce::AudioParameterBool>(p + "on", n + "On", d.on));
        f(p + "mix", n + "Mix", 0, 1, d.mix);
        f(p + "a", n + "A", 0, 1, d.a); f(p + "b", n + "B", 0, 1, d.b); f(p + "c", n + "C", 0, 1, d.c);
    }
    // ---- WAV sampler
    f("sStart", "Start", 0, 1, 0);
    f("sEnd", "End", 0, 1, 1);
    f("sTune", "Tune", -24, 24, 0);
    f("sSpeed", "Speed", 0.25f, 4, 1);
    f("sPitch", "Pitch", -100, 100, 0);
    f("sPan", "Pan", -1, 1, 0);
    l.add(std::make_unique<juce::AudioParameterInt>("sSlices", "Slices", 2, 32, 8));
    l.add(std::make_unique<juce::AudioParameterBool>("sLoop", "Loop", false));
    l.add(std::make_unique<juce::AudioParameterBool>("sRev", "Reverse", false));
    l.add(std::make_unique<juce::AudioParameterBool>("synthOn", "Synth On", true));
    l.add(std::make_unique<juce::AudioParameterBool>("randLevels", "Randomizer Sets Levels (-5..0 dB)", true));
    l.add(std::make_unique<juce::AudioParameterChoice>("sMode", "Sampler Mode", juce::StringArray{"Map", "Slice"}, 0));
    return l;
}

juce::AudioProcessor::BusesProperties MultiverseXProcessor::makeBuses()
{
    BusesProperties b;
#if ! JucePlugin_IsSynth
    b = b.withInput("Input", juce::AudioChannelSet::stereo(), true);
#endif
    return b.withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

static juce::File defaultSkinFile()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Hazel Bandz").getChildFile("MultiverseX").getChildFile("default.mvxskin");
}

MultiverseXProcessor::MultiverseXProcessor()
    : AudioProcessor(makeBuses()),
      apvts(*this, nullptr, "MVX", layout())
{
    for (auto& n : noteVel) n = 0.f;
    synth.addSound(new MxSound());
    for (int i = 0; i < 128; ++i) synth.addVoice(new MxVoice(*this));
    for (int i = 0; i < 32; ++i) sampler.addVoice(new MxSampleVoice(*this));

    for (int s = 0; s < kNumFxSlots; ++s) {
        const juce::String p = "fx" + juce::String(s + 1) + "_";
        pType[s] = apvts.getRawParameterValue(p + "type"); pOn[s] = apvts.getRawParameterValue(p + "on");
        pMix[s] = apvts.getRawParameterValue(p + "mix");   pA[s] = apvts.getRawParameterValue(p + "a");
        pB[s] = apvts.getRawParameterValue(p + "b");       pC[s] = apvts.getRawParameterValue(p + "c");
    }
    pFxMix = apvts.getRawParameterValue("fxMix"); pMaster = apvts.getRawParameterValue("master");
    pBlend = apvts.getRawParameterValue("mixBlend"); pSynthOn = apvts.getRawParameterValue("synthOn");
    pInGain = apvts.getRawParameterValue("inGain"); pMorph = apvts.getRawParameterValue("morph");
    pSynthVol = apvts.getRawParameterValue("synthVol"); pSampVol = apvts.getRawParameterValue("sVol");
    pSend = apvts.getRawParameterValue("sFxSend"); pRandLvl = apvts.getRawParameterValue("randLevels");
    apvts.addParameterListener("morph", this);

    // user's default skin (Documents\Hazel Bandz\MultiverseX\default.mvxskin), else the red matrix
    const auto df = defaultSkinFile();
    if (df.existsAsFile())
        if (auto xml = juce::parseXML(df)) skin.fromXml(*xml);
}

MultiverseXProcessor::~MultiverseXProcessor()
{
    apvts.removeParameterListener("morph", this);
    cancelPendingUpdate();
}

bool MultiverseXProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
#if ! JucePlugin_IsSynth
    if (l.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
#endif
    return true;
}

void MultiverseXProcessor::prepareToPlay(double sr, int block) {
    synth.setCurrentPlaybackSampleRate(sr);
    sampler.setCurrentPlaybackSampleRate(sr);
    for (auto& f : fx) f.prepare(sr, block);
    synthBuf.setSize(2, juce::jmax(block, 512)); sampBuf.setSize(2, juce::jmax(block, 512)); dryBuf.setSize(2, juce::jmax(block, 512));
    keyboardState.reset();
}

void MultiverseXProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buf.clear(c, 0, n);
#if JucePlugin_IsSynth
    buf.clear();
#else
    buf.applyGain(pInGain->load());
#endif
    const int nch = juce::jmin(2, buf.getNumChannels());
    if (nch < 1 || n < 1) return;

    keyboardState.processNextMidiBuffer(midi, 0, n, true);
    for (const auto m : midi) {
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) noteVel[msg.getNoteNumber() & 127] = msg.getFloatVelocity();
        else if (msg.isNoteOff()) noteVel[msg.getNoteNumber() & 127] = 0.f;
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) for (auto& v : noteVel) v = 0.f;
    }

    synthBuf.setSize(2, n, false, false, true); sampBuf.setSize(2, n, false, false, true);
    synthBuf.clear(); sampBuf.clear();
    if (pSynthOn->load() > 0.5f) synth.renderNextBlock(synthBuf, midi, 0, n);
    sampler.renderNextBlock(sampBuf, midi, 0, n);

    // ---- mix: Synth(+FX) <> Sample blend, plus separate volumes in dB (-100 = silent)
    auto dbGain = [](float db) { return db <= -99.9f ? 0.f : juce::Decibels::decibelsToGain(db); };
    const float bl = pBlend->load();
    const float gS = juce::jmin(1.f, 2.f * (1.f - bl)), gP = juce::jmin(1.f, 2.f * bl);
    const float synthG = gS * dbGain(pSynthVol->load()), sampG = gP * dbGain(pSampVol->load());
    const float send = juce::jlimit(0.f, 1.f, pSend->load());
    sampBuf.applyGain(0, n, sampG);
    for (int c = 0; c < nch; ++c) {
        buf.addFrom(c, 0, synthBuf, c, 0, n, synthG);     // synth (+ any input audio) goes into the FX rack
        buf.addFrom(c, 0, sampBuf, c, 0, n, send);        // optional amount of the sample sent into the FX rack
    }
    sampBuf.applyGain(0, n, 1.f - send);                  // the rest of the sample stays DRY (bypasses the FX)

    // ---- FX rack
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    const float wetAll = pFxMix->load();
    const bool blendAll = wetAll < 0.999f;
    if (blendAll) { dryBuf.setSize(2, n, false, false, true); for (int c = 0; c < nch; ++c) dryBuf.copyFrom(c, 0, buf, c, 0, n); }
    for (int s = 0; s < kNumFxSlots; ++s) {
        const int type = (int)pType[s]->load();
        if (type > 0 && pOn[s]->load() > 0.5f)
            fx[s].process(buf, type, pA[s]->load(), pB[s]->load(), pC[s]->load(), pMix[s]->load(), bpm);
    }
    if (blendAll)
        for (int c = 0; c < nch; ++c) {
            auto* d = buf.getWritePointer(c); auto* dr = dryBuf.getReadPointer(c);
            for (int i = 0; i < n; ++i) d[i] = dr[i] + (d[i] - dr[i]) * wetAll;
        }
    for (int c = 0; c < nch; ++c) buf.addFrom(c, 0, sampBuf, c, 0, n, 1.f);   // dry sample on top of the FX

    buf.applyGain(pMaster->load());
    visLevel = juce::jlimit(0.f, 1.f, buf.getMagnitude(0, n) * 2.f);
}

void MultiverseXProcessor::setP(const juce::String& id, float v) {
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v));
}

void MultiverseXProcessor::applyEngine(int i) {
    i = juce::jlimit(0, kNumEngines - 1, i);
    const auto& e = kEngines[i];
    setP("engine", (float)i);
    setP("detune", e.detune); setP("cutoff", e.cutoff); setP("res", e.res);
    setP("atk", e.atk); setP("rel", e.rel);
}

void MultiverseXProcessor::randomizeLevels() {
    // the randomizer never leaves -5..0 dB. Only YOU can push the volumes past that.
    juce::Random r;
    setP("synthVol", -5.f + r.nextFloat() * 5.f);
    setP("sVol", -5.f + r.nextFloat() * 5.f);
}

void MultiverseXProcessor::randomize() {
    juce::Random r;
    for (auto* id : {"cutoff","sus","rel","dec","atk","detune","lfo1Rate","lfo1Cut","lfo2Rate","lfo2Pitch","engBlend","fEnv","fAtk","fDec","fSus","fRel"})
        if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(r.nextFloat());
    if (auto* p = apvts.getParameter("res")) p->setValueNotifyingHost(r.nextFloat() * 0.7f);     // keep resonance tame
    if (auto* p = apvts.getParameter("drive")) p->setValueNotifyingHost(r.nextFloat() * 0.5f);   // keep drive tame
    if (auto* p = apvts.getParameter("engine2")) p->setValueNotifyingHost(r.nextFloat());
    if (pRandLvl->load() > 0.5f) randomizeLevels();
}

void MultiverseXProcessor::randomizeFx() {
    juce::Random r;
    for (int s = 1; s <= kNumFxSlots; ++s) {
        const juce::String p = "fx" + juce::String(s) + "_";
        const bool use = r.nextFloat() < 0.7f;
        const int type = use ? 1 + r.nextInt(kNumFxTypes - 1) : 0;
        float a = r.nextFloat(), b = r.nextFloat(), cc = r.nextFloat();
        if (type == 11) cc *= 0.2f;                       // compressor make-up: at most ~+3.6 dB
        if (type == 15) { a = 0.08f + r.nextFloat() * 0.32f; cc *= 0.1f; }   // limiter ceiling -1..-5 dB, tiny gain
        if (type == 1) a *= 0.6f;                         // drive amount stays musical
        if (auto* t = apvts.getParameter(p + "type")) t->setValueNotifyingHost(t->convertTo0to1((float)type));
        if (auto* o = apvts.getParameter(p + "on")) o->setValueNotifyingHost(use ? 1.f : 0.f);
        if (auto* m = apvts.getParameter(p + "mix")) m->setValueNotifyingHost(0.25f + r.nextFloat() * 0.5f);
        if (auto* q = apvts.getParameter(p + "a")) q->setValueNotifyingHost(a);
        if (auto* q = apvts.getParameter(p + "b")) q->setValueNotifyingHost(b);
        if (auto* q = apvts.getParameter(p + "c")) q->setValueNotifyingHost(cc);
    }
    if (pRandLvl->load() > 0.5f) randomizeLevels();
}

// ---------------------------------------------------------------- A/B morph
void MultiverseXProcessor::storeScene(bool isA) {
    std::vector<float> v;
    for (auto& id : fxIds()) if (auto* p = apvts.getParameter(id)) v.push_back(p->getValue());
    (isA ? sceneA : sceneB) = v;
}
void MultiverseXProcessor::parameterChanged(const juce::String& id, float) { if (id == "morph") triggerAsyncUpdate(); }
void MultiverseXProcessor::handleAsyncUpdate() { applyMorph(); }
void MultiverseXProcessor::applyMorph() {
    if (!scenesReady()) return;
    const auto ids = fxIds(); const float m = pMorph->load();
    for (int i = 0; i < ids.size() && i < (int)sceneA.size() && i < (int)sceneB.size(); ++i) {
        auto* p = apvts.getParameter(ids[i]); if (p == nullptr) continue;
        const bool discrete = ids[i].endsWith("_type") || ids[i].endsWith("_on");
        const float a = sceneA[(size_t)i], b = sceneB[(size_t)i];
        const float v = discrete ? (m < 0.5f ? a : b) : a + (b - a) * m;
        if (std::abs(p->getValue() - v) > 1e-5f) p->setValueNotifyingHost(v);
    }
}

// ---------------------------------------------------------------- state
static juce::String joinFloats(const std::vector<float>& v) {
    juce::StringArray s; for (float f : v) s.add(juce::String(f, 5)); return s.joinIntoString(",");
}
static std::vector<float> splitFloats(const juce::String& s) {
    std::vector<float> v; for (auto& t : juce::StringArray::fromTokens(s, ",", "")) if (t.isNotEmpty()) v.push_back(t.getFloatValue()); return v;
}

void MultiverseXProcessor::getStateInformation(juce::MemoryBlock& d) {
    apvts.state.setProperty("samplePath", samplePath, nullptr);
    if (auto xml = apvts.copyState().createXml()) {
        xml->setAttribute("sceneA", joinFloats(sceneA)); xml->setAttribute("sceneB", joinFloats(sceneB));
        { const juce::ScopedLock sl(skinLock); xml->addChildElement(skin.toXml().release()); }
        copyXmlToBinary(*xml, d);
    }
}
void MultiverseXProcessor::setStateInformation(const void* d, int n) {
    if (auto xml = getXmlFromBinary(d, n)) {
        if (auto* sx = xml->getChildByName("MvxSkin")) {
            editSkin([&](SkinData& s) { s.fromXml(*sx); });
            xml->removeChildElement(sx, true);
        }
        sceneA = splitFloats(xml->getStringAttribute("sceneA")); sceneB = splitFloats(xml->getStringAttribute("sceneB"));
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        auto path = apvts.state.getProperty("samplePath").toString();
        if (path.isNotEmpty()) { auto f = mvxResolve(path); if (f.existsAsFile()) loadSample(f); }
    }
}

void MultiverseXProcessor::saveDefaultSkin() {
    const auto df = defaultSkinFile();
    df.getParentDirectory().createDirectory();
    const juce::ScopedLock sl(skinLock);
    skin.toXml()->writeTo(df);
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

    std::vector<float> pk(600, 0.f);
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
