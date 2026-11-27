#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "Engines.h"
#include "FxEngine.h"
#include "Skin.h"

class MultiverseXProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater
{
public:
    MultiverseXProcessor();
    ~MultiverseXProcessor() override;
    void prepareToPlay(double sr, int block) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
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

    void applyEngine(int index);            // loads an engine's preset into the parameters
    void randomize();                       // synth randomize
    void randomizeFx();                     // FX rack randomize
    bool loadSample(const juce::File&);     // message thread only
    juce::String getSampleName() const { return juce::File(samplePath).getFileName(); }
    juce::SpinLock peakLock; std::vector<float> peaks;

    // ---- A/B morph of the whole FX rack
    void storeScene(bool isA);
    bool scenesReady() const { return sceneA.size() > 0 && sceneB.size() > 0; }

    // ---- skin (edited on the message thread only)
    SkinData skin;
    juce::CriticalSection skinLock;
    std::atomic<int> skinVersion{ 0 };
    template <typename F> void editSkin(F&& f) { { const juce::ScopedLock sl(skinLock); f(skin); } ++skinVersion; }
    void saveDefaultSkin();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    std::atomic<float> visLfo1{ 0 }, visLfo2{ 0 }, visLevel{ 0 };
    std::atomic<float> noteVel[128];      // 0 = off, else velocity (read by the visuals)

    static juce::StringArray fxIds();

private:
    static BusesProperties makeBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void setP(const juce::String& id, float v);
    void parameterChanged(const juce::String& id, float v) override;
    void handleAsyncUpdate() override;
    void applyMorph();

    juce::Synthesiser synth, sampler;
    juce::String samplePath;
    FxUnit fx[kNumFxSlots];
    juce::AudioBuffer<float> synthBuf, sampBuf, dryBuf;
    std::vector<float> sceneA, sceneB;

    std::atomic<float>* pType[kNumFxSlots]; std::atomic<float>* pOn[kNumFxSlots]; std::atomic<float>* pMix[kNumFxSlots];
    std::atomic<float>* pA[kNumFxSlots]; std::atomic<float>* pB[kNumFxSlots]; std::atomic<float>* pC[kNumFxSlots];
    std::atomic<float>* pFxMix = nullptr; std::atomic<float>* pMaster = nullptr; std::atomic<float>* pBlend = nullptr;
    std::atomic<float>* pSynthOn = nullptr; std::atomic<float>* pInGain = nullptr; std::atomic<float>* pMorph = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiverseXProcessor)
};
