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

class MultiverseXEditor : public juce::AudioProcessorEditor {
public:
    explicit MultiverseXEditor(MultiverseXProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    MultiverseXProcessor& proc;
    GalaxyVisualizer galaxy;
    juce::ComboBox engineBox;
    juce::TextButton prev{"Prev"}, next{"Next"}, rnd{"Random"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> engineAtt;
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> atts;
    juce::MidiKeyboardComponent keys;
};
