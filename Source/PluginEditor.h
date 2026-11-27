#pragma once
#include "PluginProcessor.h"
#include "Skin.h"
#include "Vfx.h"
#if MVX_VIDEO
 #include <juce_video/juce_video.h>
#endif

// ------------------------------------------------------------------ look & feel (follows the skin accent)
class MxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent{ 0xffff2a2a };

    void applyTheme(const SkinData& s)
    {
        accent = s.accent;
        setColour(juce::Label::textColourId, s.text);
        setColour(juce::ToggleButton::textColourId, s.text);
        setColour(juce::ToggleButton::tickColourId, s.accent);
        setColour(juce::ToggleButton::tickDisabledColourId, s.accent.darker());
        setColour(juce::TextButton::textColourOffId, s.text);
        setColour(juce::TextButton::textColourOnId, s.text);
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.55f));
        setColour(juce::ComboBox::textColourId, s.text);
        setColour(juce::ComboBox::outlineColourId, s.accent.withAlpha(0.7f));
        setColour(juce::ComboBox::arrowColourId, s.accent);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0c0608));
        setColour(juce::PopupMenu::textColourId, s.text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, s.accent.darker(0.4f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.55f));
        setColour(juce::TextEditor::textColourId, s.text);
        setColour(juce::TextEditor::outlineColourId, s.accent.withAlpha(0.6f));
        setColour(juce::TextEditor::focusedOutlineColourId, s.accent);
        setColour(juce::Slider::textBoxTextColourId, s.text);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.4f));
        setColour(juce::Slider::thumbColourId, s.accent);
        setColour(juce::Slider::rotarySliderFillColourId, s.accent);
        setColour(juce::BubbleComponent::backgroundColourId, juce::Colour(0xf0100608));
        setColour(juce::BubbleComponent::outlineColourId, s.accent);
        setColour(juce::TooltipWindow::textColourId, s.text);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
    {
        auto r = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(5.f);
        const float rad = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        juce::Path track; track.addCentredArc(c.x, c.y, rad, rad, 0.f, a0, a1, true);
        g.setColour(juce::Colours::white.withAlpha(0.14f)); g.strokePath(track, juce::PathStrokeType(3.f));
        juce::Path val; val.addCentredArc(c.x, c.y, rad, rad, 0.f, a0, ang, true);
        g.setColour(accent.withAlpha(0.28f)); g.strokePath(val, juce::PathStrokeType(8.f));
        g.setColour(accent); g.strokePath(val, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff120608).withAlpha(0.88f));
        g.fillEllipse(c.x - rad * 0.68f, c.y - rad * 0.68f, rad * 1.36f, rad * 1.36f);
        juce::Path p; p.addRectangle(-1.6f, -rad * 0.62f, 3.2f, rad * 0.4f);
        g.setColour(juce::Colours::white);
        g.fillPath(p, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cy = (float)y + (float)h * 0.5f;
        g.setColour(juce::Colours::white.withAlpha(0.15f)); g.fillRoundedRectangle((float)x, cy - 2.f, (float)w, 4.f, 2.f);
        g.setColour(accent); g.fillRoundedRectangle((float)x, cy - 2.f, juce::jmax(0.f, pos - (float)x), 4.f, 2.f);
        g.setColour(accent.withAlpha(0.3f)); g.fillEllipse(pos - 9.f, cy - 9.f, 18.f, 18.f);
        g.setColour(accent.brighter(0.3f)); g.fillEllipse(pos - 6.f, cy - 6.f, 12.f, 12.f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(1.f);
        const bool on = b.getToggleState();
        auto base = on ? accent.withAlpha(0.55f) : juce::Colours::black.withAlpha(0.5f);
        if (hi) base = base.brighter(0.2f);
        if (down) base = base.brighter(0.4f);
        g.setColour(base); g.fillRoundedRectangle(r, 6.f);
        g.setColour(accent.withAlpha(on ? 1.f : 0.6f)); g.drawRoundedRectangle(r, 6.f, 1.3f);
    }
};

// ------------------------------------------------------------------ full-window animated skin
class BackdropCanvas : public juce::Component, private juce::Timer
{
public:
    explicit BackdropCanvas(MultiverseXProcessor& p) : proc(p) { setOpaque(true); setInterceptsMouseClicks(false, false); startTimerHz(30); }
    void setFps(int fps) { if (fps != curFps) { curFps = fps; startTimerHz(juce::jlimit(10, 60, fps)); } }
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    void drawLayers(juce::Graphics& g, int w, int h);
    MultiverseXProcessor& proc;
    VfxRenderer renderers[kNumLayers];
    VfxContext ctx;
    juce::Image buffer;
    double last = 0; int curFps = 30;
};

// ------------------------------------------------------------------ knob = slider + label (+ attachment)
class Knob : public juce::Component
{
public:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    Knob(juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& name)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled(true, false, nullptr);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(12.f));
        l.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(s); addAndMakeVisible(l);
        att = std::make_unique<SA>(a, id, s);
    }
    void setLabel(const juce::String& n) { l.setText(n, juce::dontSendNotification); }
    void resized() override { auto r = getLocalBounds(); l.setBounds(r.removeFromBottom(16)); s.setBounds(r); }
    juce::Slider s; juce::Label l; std::unique_ptr<SA> att;
};

// ------------------------------------------------------------------ draggable ADSR
class EnvelopeView : public juce::Component
{
public:
    EnvelopeView(juce::AudioProcessorValueTreeState& a, const juce::String& ia, const juce::String& id,
                 const juce::String& is, const juce::String& ir, const juce::String& t)
        : apvts(a), idA(ia), idD(id), idS(is), idR(ir), title(t) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    struct Pts { juce::Point<float> p0, p1, p2, p2b, p3; float segW, pad, top, bottom; };
    Pts pts() const;
    float norm(const juce::String& id) const { return apvts.getParameter(id)->getValue(); }
    void setNorm(const juce::String& id, float v) { apvts.getParameter(id)->setValueNotifyingHost(juce::jlimit(0.f, 1.f, v)); }
    juce::AudioProcessorValueTreeState& apvts;
    juce::String idA, idD, idS, idR, title;
    int dragging = -1;
};

// ------------------------------------------------------------------ sample overview
class WaveformView : public juce::Component, private juce::Timer
{
public:
    explicit WaveformView(MultiverseXProcessor& p) : proc(p) { startTimerHz(15); }
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    MultiverseXProcessor& proc;
};

// ------------------------------------------------------------------ translucent panel
class Page : public juce::Component
{
public:
    explicit Page(MultiverseXProcessor& p) : proc(p) { setInterceptsMouseClicks(false, true); }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        g.setColour(proc.skin.panel.withAlpha(proc.skin.panelAlpha)); g.fillRoundedRectangle(r, 12.f);
        g.setColour(proc.skin.accent.withAlpha(0.55f)); g.drawRoundedRectangle(r, 12.f, 1.5f);
    }
    MultiverseXProcessor& proc;
};

class ContentPanel : public juce::Component
{
public:
    explicit ContentPanel(MultiverseXProcessor& p) : proc(p) { setInterceptsMouseClicks(false, true); }
    void paint(juce::Graphics& g) override;
    MultiverseXProcessor& proc;
};

// ------------------------------------------------------------------ one FX rack slot
class FxSlotView : public juce::Component
{
public:
    FxSlotView(MultiverseXProcessor& p, int slot);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void refresh();
private:
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    MultiverseXProcessor& proc; int slot; juce::String pre;
    juce::ComboBox typeBox; juce::ToggleButton onBtn{ "On" };
    Knob ka, kb, kc, kmix;
    std::unique_ptr<CA> typeAtt; std::unique_ptr<BA> onAtt;
    int lastType = -1;
};

// ------------------------------------------------------------------ small skin widgets
class ColourButton : public juce::TextButton
{
public:
    explicit ColourButton(const juce::String& name) { setButtonText(name); }
    juce::Colour swatch{ juce::Colours::red };
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        g.setColour(swatch); g.fillRoundedRectangle(r, 6.f);
        g.setColour(juce::Colours::white.withAlpha(0.85f)); g.drawRoundedRectangle(r, 6.f, 1.5f);
        g.setColour(swatch.contrasting(1.f)); g.setFont(12.5f);
        g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
    }
};

class LSlider : public juce::Component
{
public:
    LSlider(const juce::String& name, double lo, double hi, double def, double step)
    {
        l.setText(name, juce::dontSendNotification); l.setFont(juce::Font(13.f));
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setRange(lo, hi, step); s.setValue(def, juce::dontSendNotification);
        s.setDoubleClickReturnValue(true, def);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 20);
        addAndMakeVisible(l); addAndMakeVisible(s);
    }
    void resized() override { auto r = getLocalBounds(); l.setBounds(r.removeFromLeft(86)); s.setBounds(r); }
    juce::Label l; juce::Slider s;
};

// ------------------------------------------------------------------ the editor
class MultiverseXEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit MultiverseXEditor(MultiverseXProcessor&);
    ~MultiverseXEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override {}
    void mouseDown(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray& f) override;
    void filesDropped(const juce::StringArray& f, int, int) override;

private:
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    static constexpr int kDW = 1100, kDH = 780;

    void timerCallback() override;
    void showTab(int i);
    void layoutPages();
    void applyTheme();
    void syncSkinControls();
    void loadSampleFile(const juce::File&);
    void setLayerMedia(const juce::File&);
    void setVideoFile(const juce::File&);
    void updateVideo();
    void pickColour(ColourButton& b, std::function<juce::Colour(const SkinData&)> get, std::function<void(SkinData&, juce::Colour)> set);
    template <typename F> void edit(F&& f) { proc.editSkin(f); seenVersion = proc.skinVersion.load(); applyTheme(); }
    int selLayer = 0; int seenVersion = -1; juce::String lastTheme;

    MultiverseXProcessor& proc;
    MxLookAndFeel laf;
    BackdropCanvas backdrop;
    ContentPanel content;
    juce::TextButton tabBtn[4];
    Page pageSynth, pageSamp, pageFx, pageSkin;

    // header
    juce::ComboBox engineBox, engine2Box;
    juce::TextButton prev{ "<" }, next{ ">" }, rnd{ "Random" }, hideBtn{ "Hide UI" };
    std::unique_ptr<CA> engineAtt, engine2Att;

    // synth page
    Knob kBlend, kDetune, kCutoff, kRes, kDrive, kMix, kMaster, kIn, kFenv, kL1R, kL1C, kL2R, kL2P;
    EnvelopeView envAmp, envFilt;

    // sampler page
    WaveformView wave;
    juce::ComboBox modeBox; juce::TextButton loadBtn{ "Load WAV" };
    juce::ToggleButton loopBtn{ "Loop" }, revBtn{ "Reverse" }, synthBtn{ "Synth On" };
    juce::Label sampleName;
    std::unique_ptr<CA> modeAtt; std::unique_ptr<BA> loopAtt, revAtt, synthAtt;
    std::vector<std::unique_ptr<Knob>> sKnobs;

    // FX page
    Knob kFxMix, kMorph;
    juce::TextButton storeA{ "Store A" }, storeB{ "Store B" }, rndFx{ "Randomize FX" };
    juce::Label fxHint;
    std::unique_ptr<FxSlotView> slots[kNumFxSlots];

    // skin page
    juce::TextButton layerBtn[kNumLayers];
    juce::ToggleButton layerOn{ "Layer on" }, rainbowBtn{ "Rainbow colours" };
    juce::ComboBox typeBox;
    LSlider sVariant, sSpeed, sInten, sScale, sDens, sOpac, sReact, sPanel;
    ColourButton c1b, c2b, c3b, accentB, accent2B, panelB, textB;
    juce::Label variantHint, pathLabel, helpLabel, fpsLbl, qualLbl;
    juce::TextEditor textEd;
    juce::TextButton browseBtn{ "Browse image / GIF / SVG..." };
    juce::ComboBox presetBox, fpsBox, qualBox;
    juce::TextButton presetPrev{ "<" }, presetNext{ ">" }, randSkin{ "Randomize Skin" }, saveSkin{ "Save Skin" },
                     loadSkin{ "Load Skin" }, setDefault{ "Set as Default" }, videoBtn{ "Load video..." };
    juce::ToggleButton videoOn{ "Video on stage" };
    juce::Label videoPathLbl;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::MidiKeyboardComponent keys;
    juce::ComponentBoundsConstrainer constrainer;
    juce::ResizableCornerComponent resizer;
#if MVX_VIDEO
    juce::VideoComponent video{ false };
    juce::String videoLoaded;
#endif
    std::vector<std::unique_ptr<SA>> atts;
};
