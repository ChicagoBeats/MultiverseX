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
    MxVoice(MultiverseXProcessor& p) : proc(p) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<MxSound*>(s) != nullptr; }

    void startNote(int note, float vel, juce::SynthesiserSound*, int) override {
        freq = (float)juce::MidiMessage::getMidiNoteInHertz(note);
        level = vel;
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
                s += osc(eng.shapes[o], ph[o]) * 0.25f;
            }
            float fc = juce::jlimit(20.f, 18000.f, cutoff * std::pow(2.f, l1 * l1c * 3.f));
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
    juce::ADSR adsr;
    juce::dsp::StateVariableTPTFilter<float> filt;
    float freq = 440, level = 0, ph[4] = {}, lp1 = 0, lp2 = 0;
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
    f("lfo1Cut", "LFO1>Cutoff", 0, 1, 0.75f);
    f("lfo2Rate", "LFO2 Rate", 0.05f, 20, 4, 0.4f);
    f("lfo2Pitch", "LFO2>Pitch", 0, 1, 0.1f);
    f("reverb", "Reverb", 0, 1, 0.25f);
    f("delay", "Delay", 0, 1, 0.2f);
    f("master", "Master", 0, 1, 0.7f);
    return l;
}

MultiverseXProcessor::MultiverseXProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MVX", layout()) {
    synth.addSound(new MxSound());
    for (int i = 0; i < 128; ++i) synth.addVoice(new MxVoice(*this));
}

void MultiverseXProcessor::prepareToPlay(double sr, int) {
    synth.setCurrentPlaybackSampleRate(sr);
    reverb.setSampleRate(sr);
    delayBuf.setSize(2, (int)(sr * 2.0)); delayBuf.clear(); delayPos = 0;
    keyboardState.reset();
}

void MultiverseXProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    buf.clear();
    keyboardState.processNextMidiBuffer(midi, 0, buf.getNumSamples(), true);
    synth.renderNextBlock(buf, midi, 0, buf.getNumSamples());

    const float rv = apvts.getRawParameterValue("reverb")->load();
    const float dl = apvts.getRawParameterValue("delay")->load();
    const float mg = apvts.getRawParameterValue("master")->load();
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

void MultiverseXProcessor::randomize() {
    juce::Random r;
    for (auto* id : {"cutoff","res","drive","atk","dec","sus","rel","detune","lfo1Rate","lfo1Cut","lfo2Rate","lfo2Pitch","reverb","delay"})
        if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(r.nextFloat());
}

void MultiverseXProcessor::getStateInformation(juce::MemoryBlock& d) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d);
}
void MultiverseXProcessor::setStateInformation(const void* d, int n) {
    if (auto xml = getXmlFromBinary(d, n)) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* MultiverseXProcessor::createEditor() { return new MultiverseXEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MultiverseXProcessor(); }
