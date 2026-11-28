#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Engines.h"
#include "ModMatrix.h"
#include "Effects.h"

enum { SEC_SYNTH, SEC_MATRIX, SEC_FX, SEC_SAMPLER, SEC_COUNT };

class MultiverseXProcessor : public juce::AudioProcessor {
public:
    MultiverseXProcessor();
    void prepareToPlay(double sr, int block) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MultiverseX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void applyEngine(int index);   // loads an engine's preset into the parameters
    // Click = randomize a section, Right-Click = lock it (locked sections are skipped by Full Mix)
    void randomizeSection(int sec);
    void randomizeAll();
    void undo();
    void redo();
    std::atomic<bool> locked[SEC_COUNT] = {false, false, false, false};
    bool loadSample(const juce::File&);   // message thread only
    juce::String getSampleName() const { return juce::File(samplePath).getFileName(); }
    juce::SpinLock peakLock; std::vector<float> peaks;   // waveform overview for the UI

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    // modulation activity, read by the visualizer
    std::atomic<float> visLfo1{0}, visLfo2{0}, visLevel{0};
    std::atomic<float> modWheel{0}, breath{0}, aftertouch{0}, lastVel{0}, lastRnd{0}, visModEnergy{0};
    std::atomic<int> noteCount{0};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void setP(const char* id, float v);
    using Snapshot = std::vector<std::pair<juce::String, float>>;
    Snapshot snapshot();
    void restore(const Snapshot&);
    void pushUndo();
    void doRandomize(int sec);
    void blendTo(const juce::String& id, float target, float amt);
    void setChance(const juce::String& id, float amt, juce::Random& r, float target);
    std::vector<Snapshot> undoStack, redoStack;
    juce::Synthesiser synth, sampler;
    juce::String samplePath;
    juce::Reverb reverb;
    FxChain fx{apvts};
    juce::AudioBuffer<float> delayBuf;
    int delayPos = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiverseXProcessor)
};
