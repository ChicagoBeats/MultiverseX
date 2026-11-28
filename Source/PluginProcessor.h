#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Engines.h"

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
    void randomize();              // Click = Randomize (locks come next)
    bool loadSample(const juce::File&);   // message thread only
    juce::String getSampleName() const { return juce::File(samplePath).getFileName(); }
    juce::SpinLock peakLock; std::vector<float> peaks;   // waveform overview for the UI

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    // modulation activity, read by the visualizer
    std::atomic<float> visLfo1{0}, visLfo2{0}, visLevel{0};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void setP(const char* id, float v);
    juce::Synthesiser synth, sampler;
    juce::String samplePath;
    juce::Reverb reverb;
    juce::AudioBuffer<float> delayBuf;
    int delayPos = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiverseXProcessor)
};
