#pragma once
#include "PluginProcessor.h"
#include "NeonLookAndFeel.h"

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
    int lastNotes = 0;
    juce::Random rng;
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

// Click = randomize this section, Right-Click = lock / unlock it
class SectionButton : public juce::TextButton {
public:
    SectionButton(MultiverseXProcessor& p, int s) : proc(p), sec(s) {
        static const char* names[] = {"Synth", "Matrix", "FX", "Sampler"};
        name = names[s]; refresh();
    }
    void refresh() {
        const bool l = proc.locked[sec];
        setButtonText((l ? "LOCKED " : "Rand ") + name);
        setColour(juce::TextButton::buttonColourId, l ? juce::Colour(0xff7a1f1f) : juce::Colour(0xff2a2540));
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) { proc.locked[sec] = !proc.locked[sec]; refresh(); }
        else juce::TextButton::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::TextButton::mouseUp(e); }
private:
    MultiverseXProcessor& proc; int sec; juce::String name;
};

class MultiverseXEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer {
public:
    explicit MultiverseXEditor(MultiverseXProcessor&);
    ~MultiverseXEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    bool isInterestedInFileDrag(const juce::StringArray& f) override;
    void filesDropped(const juce::StringArray& f, int, int) override;
private:
    NeonLookAndFeel laf;                    // first member: outlives every component that uses it
    void timerCallback() override;
    void drawPanel(juce::Graphics&, juce::Rectangle<int>);
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
    std::vector<std::unique_ptr<juce::ComboBox>> mSrc, mDst;
    std::vector<std::unique_ptr<juce::Slider>> mAmt;
    std::vector<std::unique_ptr<CA>> mCombo;
    std::vector<std::unique_ptr<juce::ToggleButton>> fxOn;
    std::vector<std::unique_ptr<juce::Slider>> fxMix, fxRate;
    std::vector<std::unique_ptr<BA>> fxAtt;
    std::vector<std::unique_ptr<SectionButton>> secBtn;
    juce::TextButton undoBtn{"Undo"}, redoBtn{"Redo"};
    juce::Slider amtSlider;
    std::unique_ptr<SA> amtAtt;
    std::unique_ptr<juce::ParameterAttachment> engineParamAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> panelKnobs, panelSampler, panelGalaxy, panelKeys;
    std::vector<juce::Point<float>> stars;
    int lastEngine = -1;
    juce::MidiKeyboardComponent keys;
};
