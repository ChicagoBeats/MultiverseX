#pragma once
#include "PluginProcessor.h"

// Galaxy that spins, pulses and bounces with the synth's modulation
class GalaxyVisualizer : public juce::Component, private juce::Timer {
public:
    GalaxyVisualizer(MultiverseXProcessor& p);
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    MultiverseXProcessor& proc;
    struct Star { float angle, radius, speed, size; };
    std::vector<Star> stars;
    float rot = 0, cx = 0, cy = 0, vx = 0, vy = 0, scale = 1;
};

// Sample overview with start/end markers and slice lines
class WaveformView : public juce::Component, private juce::Timer {
public:
    explicit WaveformView(MultiverseXProcessor& p) : proc(p) { startTimerHz(15); }
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    MultiverseXProcessor& proc;
};

class MultiverseXEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget {
public:
    explicit MultiverseXEditor(MultiverseXProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
    bool isInterestedInFileDrag(const juce::StringArray& f) override;
    void filesDropped(const juce::StringArray& f, int, int) override;
private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void addKnob(const char* id, const char* name, bool sampler);
    void loadFile(const juce::File&);

    MultiverseXProcessor& proc;
    GalaxyVisualizer galaxy;
    WaveformView wave;
    juce::ComboBox engineBox, modeBox;
    juce::TextButton prev{"Prev"}, next{"Next"}, rnd{"Random"}, loadBtn{"Load WAV"};
    juce::ToggleButton loopBtn{"Loop"}, revBtn{"Reverse"}, synthBtn{"Synth On"};
    juce::Label sampleName;
    std::unique_ptr<CA> engineAtt, modeAtt;
    std::unique_ptr<BA> loopAtt, revAtt, synthAtt;
    std::vector<std::unique_ptr<juce::Slider>> knobs, sKnobs;
    std::vector<std::unique_ptr<juce::Label>> labels, sLabels;
    std::vector<std::unique_ptr<SA>> atts;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComboBox scaleBox;
    juce::Label scaleLabel;
    juce::MidiKeyboardComponent keys;
};
