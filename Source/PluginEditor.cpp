#include "PluginEditor.h"

// ============================================================================
//  Backdrop (the animated skin, full window)
// ============================================================================
void BackdropCanvas::drawLayers(juce::Graphics& g, int w, int h)
{
    const auto& sk = proc.skin;
    for (int i = 0; i < kNumLayers; ++i)
        if (sk.layers[i].enabled && sk.layers[i].type != VfxNone)
            renderers[i].paint(g, juce::Rectangle<float>(0.f, 0.f, (float)w, (float)h), sk.layers[i], ctx);
}

void BackdropCanvas::paint(juce::Graphics& g)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    float dt = last > 0 ? (float)((now - last) / 1000.0) : 0.033f;
    last = now; dt = juce::jlimit(0.f, 0.1f, dt);
    ctx.dt = dt; ctx.time += dt;
    ctx.level += (proc.visLevel.load() - ctx.level) * 0.35f;
    ctx.lfo1 = proc.visLfo1.load(); ctx.lfo2 = proc.visLfo2.load();
    for (int i = 0; i < 128; ++i) ctx.noteVel[i] = proc.noteVel[i].load();

    const int w = getWidth(), h = getHeight();
    const float rs = proc.skin.renderScale;
    const juce::Colour base(0xff030204);
    if (rs < 0.99f) {
        const int bw = juce::jmax(2, (int)((float)w * rs)), bh = juce::jmax(2, (int)((float)h * rs));
        if (buffer.getWidth() != bw || buffer.getHeight() != bh) buffer = juce::Image(juce::Image::ARGB, bw, bh, true);
        {
            juce::Graphics ig(buffer);
            ig.fillAll(base);
            ig.addTransform(juce::AffineTransform::scale((float)bw / (float)w, (float)bh / (float)h));
            drawLayers(ig, w, h);
        }
        g.drawImageTransformed(buffer, juce::AffineTransform::scale((float)w / (float)bw, (float)h / (float)bh));
    } else {
        g.fillAll(base);
        drawLayers(g, w, h);
    }
}

// ============================================================================
//  Content panel (title + stage)
// ============================================================================
void ContentPanel::paint(juce::Graphics& g)
{
    const auto& s = proc.skin;
    g.setColour(s.panel.withAlpha(s.panelAlpha)); g.fillRoundedRectangle(8.f, 4.f, 1084.f, 72.f, 12.f);
    g.setColour(s.accent.withAlpha(0.55f)); g.drawRoundedRectangle(8.f, 4.f, 1084.f, 72.f, 12.f, 1.5f);
    g.setColour(s.accent);
    g.setFont(juce::Font(26.f, juce::Font::italic | juce::Font::bold));
    g.drawText("MultiverseX", 18, 6, 250, 32, juce::Justification::left);
    g.setColour(s.text.withAlpha(0.6f)); g.setFont(11.f);
    g.drawText(juce::String("Hazel Bandz  -  ") + JucePlugin_Name, 20, 36, 260, 14, juce::Justification::left);

    // stage (shows the skin; big title unless a video is playing there)
    if (!(s.videoOn && s.videoPath.isNotEmpty())) {
        g.setFont(juce::Font(64.f, juce::Font::italic | juce::Font::bold));
        g.setColour(s.accent.withAlpha(0.18f)); g.drawText("MULTIVERSE X", 22, 84, 1060, 100, juce::Justification::centred);
        g.setColour(s.accent.withAlpha(0.55f)); g.drawText("MULTIVERSE X", 20, 82, 1060, 100, juce::Justification::centred);
    }
}

// ============================================================================
//  Envelope
// ============================================================================
EnvelopeView::Pts EnvelopeView::pts() const
{
    Pts r; auto b = getLocalBounds().toFloat();
    r.pad = 14.f; r.top = b.getY() + 28.f; r.bottom = b.getBottom() - 14.f;
    const float pw = b.getWidth() - 2.f * r.pad; r.segW = pw * 0.27f;
    const float sus = pw * 0.19f;
    const float nA = norm(idA), nD = norm(idD), nS = norm(idS), nR = norm(idR);
    r.p0 = juce::Point<float>(r.pad, r.bottom);
    r.p1 = juce::Point<float>(r.pad + nA * r.segW, r.top);
    r.p2 = juce::Point<float>(r.p1.x + nD * r.segW, r.bottom - nS * (r.bottom - r.top));
    r.p2b = juce::Point<float>(r.p2.x + sus, r.p2.y);
    r.p3 = juce::Point<float>(r.p2b.x + nR * r.segW, r.bottom);
    return r;
}

void EnvelopeView::paint(juce::Graphics& g)
{
    const auto acc = findColour(juce::Slider::thumbColourId);
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRoundedRectangle(b, 8.f);
    g.setColour(acc.withAlpha(0.5f)); g.drawRoundedRectangle(b.reduced(0.5f), 8.f, 1.2f);
    g.setColour(findColour(juce::Label::textColourId)); g.setFont(12.f);
    g.drawText(title, 10, 4, 220, 18, juce::Justification::left);
    const auto p = pts();
    juce::Path line; line.startNewSubPath(p.p0); line.lineTo(p.p1); line.lineTo(p.p2); line.lineTo(p.p2b); line.lineTo(p.p3);
    juce::Path fill(line); fill.lineTo(p.p0); fill.closeSubPath();
    g.setColour(acc.withAlpha(0.15f)); g.fillPath(fill);
    g.setColour(acc.withAlpha(0.3f)); g.strokePath(line, juce::PathStrokeType(6.f));
    g.setColour(acc); g.strokePath(line, juce::PathStrokeType(2.2f));
    for (auto* pt : { &p.p1, &p.p2, &p.p3 }) {
        g.setColour(juce::Colours::white); g.fillEllipse(pt->x - 6.f, pt->y - 6.f, 12.f, 12.f);
        g.setColour(acc); g.drawEllipse(pt->x - 6.f, pt->y - 6.f, 12.f, 12.f, 2.f);
    }
    g.setColour(findColour(juce::Label::textColourId).withAlpha(0.7f)); g.setFont(11.f);
    g.drawText("A " + apvts.getParameter(idA)->getCurrentValueAsText() + "s   D " + apvts.getParameter(idD)->getCurrentValueAsText()
               + "s   S " + apvts.getParameter(idS)->getCurrentValueAsText() + "   R " + apvts.getParameter(idR)->getCurrentValueAsText() + "s",
               getLocalBounds().removeFromBottom(14), juce::Justification::centred);
}

void EnvelopeView::mouseDown(const juce::MouseEvent& e)
{
    const auto p = pts();
    const juce::Point<float> m = e.position;
    const float d1 = m.getDistanceFrom(p.p1), d2 = m.getDistanceFrom(p.p2), d3 = m.getDistanceFrom(p.p3);
    const float best = juce::jmin(d1, d2, d3);
    dragging = best > 24.f ? -1 : (best == d1 ? 0 : (best == d2 ? 1 : 2));
    if (dragging >= 0)
        for (auto* id : { &idA, &idD, &idS, &idR }) apvts.getParameter(*id)->beginChangeGesture();
}

void EnvelopeView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const auto p = pts(); const auto m = e.position;
    if (dragging == 0) setNorm(idA, (m.x - p.pad) / p.segW);
    else if (dragging == 1) { setNorm(idD, (m.x - p.p1.x) / p.segW); setNorm(idS, 1.f - (m.y - p.top) / (p.bottom - p.top)); }
    else setNorm(idR, (m.x - p.p2b.x) / p.segW);
    repaint();
}

void EnvelopeView::mouseUp(const juce::MouseEvent&)
{
    if (dragging >= 0)
        for (auto* id : { &idA, &idD, &idS, &idR }) apvts.getParameter(*id)->endChangeGesture();
    dragging = -1;
}

// ============================================================================
//  Waveform
// ============================================================================
void WaveformView::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto acc = proc.skin.accent;
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(b, 8.f);
    std::vector<float> pk;
    { juce::SpinLock::ScopedLockType l(proc.peakLock); pk = proc.peaks; }
    g.setColour(acc); g.drawRoundedRectangle(b.reduced(0.5f), 8.f, 1.5f);
    if (pk.empty()) {
        g.setColour(juce::Colours::grey);
        g.drawText("Load a WAV (or drop one here)", getLocalBounds(), juce::Justification::centred);
        return;
    }
    const float mid = b.getCentreY();
    g.setColour(acc);
    for (size_t i = 0; i < pk.size(); ++i) {
        const float x = b.getX() + (float)i / (float)pk.size() * b.getWidth(), h = pk[i] * b.getHeight() * 0.48f;
        g.drawLine(x, mid - h, x, mid + h);
    }
    auto P = [&](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
    const float st = juce::jlimit(0.f, 1.f, P("sStart")), en = juce::jlimit(0.f, 1.f, P("sEnd"));
    const float xs = b.getX() + st * b.getWidth(), xe = b.getX() + en * b.getWidth();
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRect(b.getX(), b.getY(), juce::jmax(0.f, xs - b.getX()), b.getHeight());
    g.fillRect(xe, b.getY(), juce::jmax(0.f, b.getRight() - xe), b.getHeight());
    g.setColour(juce::Colours::white);
    g.drawLine(xs, b.getY(), xs, b.getBottom(), 2.f); g.drawLine(xe, b.getY(), xe, b.getBottom(), 2.f);
    if ((int)P("sMode") == 1) {
        const int n = juce::jmax(1, (int)P("sSlices"));
        g.setColour(proc.skin.accent2);
        for (int k = 1; k < n; ++k) { const float x = xs + (xe - xs) * (float)k / (float)n; g.drawLine(x, b.getY(), x, b.getBottom(), 1.f); }
    }
}

// ============================================================================
//  FX slot
// ============================================================================
FxSlotView::FxSlotView(MultiverseXProcessor& p, int s)
    : proc(p), slot(s), pre("fx" + juce::String(s) + "_"),
      ka(p.apvts, pre + "a", "A"), kb(p.apvts, pre + "b", "B"), kc(p.apvts, pre + "c", "C"), kmix(p.apvts, pre + "mix", "Mix")
{
    for (int i = 0; i < kNumFxTypes; ++i) typeBox.addItem(kFxTypes[i].name, i + 1);
    typeAtt = std::make_unique<CA>(p.apvts, pre + "type", typeBox);
    onAtt = std::make_unique<BA>(p.apvts, pre + "on", onBtn);
    addAndMakeVisible(typeBox); addAndMakeVisible(onBtn);
    addAndMakeVisible(ka); addAndMakeVisible(kb); addAndMakeVisible(kc); addAndMakeVisible(kmix);
    refresh();
}

void FxSlotView::refresh()
{
    const int t = juce::jlimit(0, kNumFxTypes - 1, (int)proc.apvts.getRawParameterValue(pre + "type")->load());
    if (t == lastType) return;
    lastType = t;
    ka.setLabel(kFxTypes[t].a); kb.setLabel(kFxTypes[t].b); kc.setLabel(kFxTypes[t].c);
    const bool vis = t > 0;
    ka.setVisible(vis); kb.setVisible(vis); kc.setVisible(vis); kmix.setVisible(vis);
}

void FxSlotView::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.f);
    g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(r, 10.f);
    g.setColour(proc.skin.accent.withAlpha(0.45f)); g.drawRoundedRectangle(r, 10.f, 1.2f);
}

void FxSlotView::resized()
{
    auto r = getLocalBounds().reduced(4);
    auto top = r.removeFromTop(30);
    onBtn.setBounds(top.removeFromRight(66));
    typeBox.setBounds(top.reduced(2, 2));
    r.removeFromTop(4);
    const int w = r.getWidth() / 4;
    ka.setBounds(r.removeFromLeft(w)); kb.setBounds(r.removeFromLeft(w)); kc.setBounds(r.removeFromLeft(w)); kmix.setBounds(r);
}

// ============================================================================
//  Colour popup
// ============================================================================
namespace
{
    // Simple HSV colour picker (no dependency on juce::ColourSelector)
    struct ColourPopup : public juce::Component
    {
        std::function<void(juce::Colour)> cb;
        juce::Slider sh, ss, sb;
        juce::Label lh, ls, lb;
        juce::Colour cur;
        bool silent = true;

        ColourPopup(juce::Colour c, std::function<void(juce::Colour)> f) : cb(std::move(f)), cur(c)
        {
            setupSlider(sh, lh, "Hue", c.getHue());
            setupSlider(ss, ls, "Saturation", c.getSaturation());
            setupSlider(sb, lb, "Brightness", c.getBrightness());
            setSize(300, 200);
            silent = false;
        }
        void setupSlider(juce::Slider& s, juce::Label& l, const juce::String& name, float v)
        {
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setRange(0.0, 1.0, 0.001); s.setValue(v, juce::dontSendNotification);
            s.onValueChange = [this] {
                if (silent) return;
                cur = juce::Colour::fromHSV((float)sh.getValue(), (float)ss.getValue(), (float)sb.getValue(), 1.f);
                repaint();
                if (cb) cb(cur);
            };
            l.setText(name, juce::dontSendNotification); l.setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(s); addAndMakeVisible(l);
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff15090c));
            g.setColour(cur); g.fillRoundedRectangle(10.f, 10.f, (float)getWidth() - 20.f, 40.f, 8.f);
            g.setColour(juce::Colours::white.withAlpha(0.7f)); g.drawRoundedRectangle(10.f, 10.f, (float)getWidth() - 20.f, 40.f, 8.f, 1.5f);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced(10); r.removeFromTop(48);
            for (auto* p : { &sh, &ss, &sb }) {
                auto row = r.removeFromTop(40);
                auto* l = (p == &sh) ? &lh : (p == &ss ? &ls : &lb);
                l->setBounds(row.removeFromLeft(90)); p->setBounds(row);
            }
        }
    };

    const char* const kTabNames[4] = { "SYNTH", "SAMPLER", "FX RACK", "SKIN / VFX" };
}

// ============================================================================
//  Editor
// ============================================================================
MultiverseXEditor::MultiverseXEditor(MultiverseXProcessor& p)
    : AudioProcessorEditor(&p), proc(p), backdrop(p), content(p),
      pageSynth(p), pageSamp(p), pageFx(p), pageSkin(p),
      kBlend(p.apvts, "engBlend", "Eng A<>B"), kDetune(p.apvts, "detune", "Detune"), kCutoff(p.apvts, "cutoff", "Cutoff"),
      kRes(p.apvts, "res", "Resonance"), kDrive(p.apvts, "drive", "Drive"), kMix(p.apvts, "mixBlend", "Synth<>Smp"),
      kMaster(p.apvts, "master", "Master"), kIn(p.apvts, "inGain", "Input"), kFenv(p.apvts, "fEnv", "Filt Env Amt"),
      kL1R(p.apvts, "lfo1Rate", "LFO1 Rate"), kL1C(p.apvts, "lfo1Cut", "LFO1>Cut"),
      kL2R(p.apvts, "lfo2Rate", "LFO2 Rate"), kL2P(p.apvts, "lfo2Pitch", "LFO2>Pitch"),
      envAmp(p.apvts, "atk", "dec", "sus", "rel", "AMP ENVELOPE  (drag the dots)"),
      envFilt(p.apvts, "fAtk", "fDec", "fSus", "fRel", "FILTER ENVELOPE  (drag the dots)"),
      wave(p),
      kFxMix(p.apvts, "fxMix", "FX Wet/Dry"), kMorph(p.apvts, "morph", "Morph A<>B"),
      sVariant("Variant", 0, 99, 0, 1), sSpeed("Speed", 0.1, 4, 1, 0.01), sInten("Intensity", 0, 1, 0.8, 0.01),
      sScale("Size / Scale", 0, 1, 0.5, 0.01), sDens("Density", 0, 1, 0.5, 0.01), sOpac("Opacity", 0, 1, 1, 0.01),
      sReact("Audio React", 0, 1, 0.6, 0.01), sPanel("Panel opacity", 0, 1, 0.55, 0.01),
      c1b("Colour 1"), c2b("Colour 2"), c3b("Colour 3"), accentB("Accent"), accent2B("Accent 2"), panelB("Panel"), textB("Text"),
      keys(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      resizer(this, &constrainer)
{
    setLookAndFeel(&laf);
    laf.applyTheme(proc.skin);

    addAndMakeVisible(backdrop);
    addAndMakeVisible(content);

    // ---------------- header
    for (int i = 0; i < kNumEngines; ++i) { engineBox.addItem(kEngines[i].name, i + 1); engine2Box.addItem(kEngines[i].name, i + 1); }
    content.addAndMakeVisible(engineBox); content.addAndMakeVisible(engine2Box);
    engineAtt = std::make_unique<CA>(p.apvts, "engine", engineBox);
    engine2Att = std::make_unique<CA>(p.apvts, "engine2", engine2Box);
    engineBox.onChange = [this] { proc.applyEngine(engineBox.getSelectedItemIndex()); };
    prev.onClick = [this] { proc.applyEngine((engineBox.getSelectedItemIndex() + kNumEngines - 1) % kNumEngines); };
    next.onClick = [this] { proc.applyEngine((engineBox.getSelectedItemIndex() + 1) % kNumEngines); };
    rnd.onClick = [this] { proc.randomize(); };
    hideBtn.onClick = [this] { content.setVisible(false); };
    for (auto* b : { &prev, &next, &rnd, &hideBtn }) content.addAndMakeVisible(*b);

    for (int i = 0; i < 4; ++i) {
        tabBtn[i].setButtonText(kTabNames[i]);
        tabBtn[i].setClickingTogglesState(true); tabBtn[i].setRadioGroupId(77);
        tabBtn[i].onClick = [this, i] { showTab(i); };
        content.addAndMakeVisible(tabBtn[i]);
    }
    for (auto* pg : { &pageSynth, &pageSamp, &pageFx, &pageSkin }) content.addAndMakeVisible(*pg);

    // ---------------- synth page
    for (auto* k : { &kBlend, &kDetune, &kCutoff, &kRes, &kDrive, &kMix, &kMaster, &kFenv, &kL1R, &kL1C, &kL2R, &kL2P }) pageSynth.addAndMakeVisible(*k);
    pageSynth.addChildComponent(kIn); kIn.setVisible(!JucePlugin_IsSynth);
    pageSynth.addAndMakeVisible(envAmp); pageSynth.addAndMakeVisible(envFilt);

    // ---------------- sampler page
    pageSamp.addAndMakeVisible(wave);
    modeBox.addItem("Map", 1); modeBox.addItem("Slice", 2);
    pageSamp.addAndMakeVisible(modeBox); modeAtt = std::make_unique<CA>(p.apvts, "sMode", modeBox);
    for (auto* t : { &loopBtn, &revBtn, &synthBtn }) pageSamp.addAndMakeVisible(*t);
    loopAtt = std::make_unique<BA>(p.apvts, "sLoop", loopBtn);
    revAtt = std::make_unique<BA>(p.apvts, "sRev", revBtn);
    synthAtt = std::make_unique<BA>(p.apvts, "synthOn", synthBtn);
    pageSamp.addAndMakeVisible(loadBtn); pageSamp.addAndMakeVisible(sampleName);
    sampleName.setText(p.getSampleName(), juce::dontSendNotification);
    sampleName.setColour(juce::Label::textColourId, proc.skin.accent2);
    loadBtn.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Load a sample", juce::File(mvxGetSetting("lastSampleDir", mvxMediaDir().getFullPathName())), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) loadSampleFile(f); });
    };
    const std::pair<const char*, const char*> sdefs[] = {
        {"sStart","Start"},{"sEnd","End"},{"sTune","Tune"},{"sSpeed","Speed"},{"sPitch","Pitch"},{"sPan","Pan"},{"sVol","Volume"},{"sSlices","Slices"} };
    for (auto& d : sdefs) { auto k = std::make_unique<Knob>(p.apvts, d.first, d.second); pageSamp.addAndMakeVisible(*k); sKnobs.push_back(std::move(k)); }

    // ---------------- FX page
    pageFx.addAndMakeVisible(kFxMix); pageFx.addAndMakeVisible(kMorph);
    for (auto* b : { &storeA, &storeB, &rndFx }) pageFx.addAndMakeVisible(*b);
    fxHint.setText("Store two FX setups (A and B), then sweep Morph to blend every effect between them.", juce::dontSendNotification);
    fxHint.setFont(juce::Font(12.f)); pageFx.addAndMakeVisible(fxHint);
    storeA.onClick = [this] { proc.storeScene(true); fxHint.setText("Scene A stored.", juce::dontSendNotification); };
    storeB.onClick = [this] { proc.storeScene(false); fxHint.setText("Scene B stored - now move Morph.", juce::dontSendNotification); };
    rndFx.onClick = [this] { proc.randomizeFx(); };
    for (int i = 0; i < kNumFxSlots; ++i) { slots[i] = std::make_unique<FxSlotView>(p, i + 1); pageFx.addAndMakeVisible(*slots[i]); }

    // ---------------- skin page
    for (int i = 0; i < kNumLayers; ++i) {
        layerBtn[i].setButtonText("Layer " + juce::String(i + 1));
        layerBtn[i].setClickingTogglesState(true); layerBtn[i].setRadioGroupId(88);
        layerBtn[i].onClick = [this, i] { selLayer = i; syncSkinControls(); };
        pageSkin.addAndMakeVisible(layerBtn[i]);
    }
    layerBtn[0].setToggleState(true, juce::dontSendNotification);
    for (int i = 0; i < VfxNumTypes; ++i) typeBox.addItem(kVfxNames[i], i + 1);
    for (auto* c : std::initializer_list<juce::Component*>{ &layerOn, &rainbowBtn, &typeBox, &sVariant, &sSpeed, &sInten, &sScale, &sDens, &sOpac, &sReact,
                                                            &c1b, &c2b, &c3b, &variantHint, &textEd, &browseBtn, &pathLabel,
                                                            &presetBox, &presetPrev, &presetNext, &randSkin, &saveSkin, &loadSkin, &setDefault,
                                                            &accentB, &accent2B, &panelB, &textB, &sPanel, &fpsBox, &qualBox, &fpsLbl, &qualLbl,
                                                            &videoBtn, &videoOn, &videoPathLbl, &helpLabel })
        pageSkin.addAndMakeVisible(*c);

    layerOn.onClick = [this] { const bool v = layerOn.getToggleState(); edit([&](SkinData& d) { d.layers[selLayer].enabled = v; }); };
    rainbowBtn.onClick = [this] { const bool v = rainbowBtn.getToggleState(); edit([&](SkinData& d) { d.layers[selLayer].rainbow = v; }); };
    typeBox.onChange = [this] {
        const int t = typeBox.getSelectedId() - 1; if (t < 0) return;
        edit([&](SkinData& d) {
            auto& l = d.layers[selLayer]; l.type = t; if (t != VfxNone) l.enabled = true;
            if ((t == VfxStudio || t == VfxCar) && l.text.isEmpty()) l.text = "CHICAGO BEATS";
        });
        syncSkinControls();
    };
    sVariant.s.onValueChange = [this] { const int v = (int)sVariant.s.getValue(); edit([&](SkinData& d) { d.layers[selLayer].variant = v; }); };
    auto bindF = [this](LSlider& ls, float SkinLayer::* member) {
        ls.s.onValueChange = [this, &ls, member] { const float v = (float)ls.s.getValue(); edit([&](SkinData& d) { d.layers[selLayer].*member = v; }); };
    };
    bindF(sSpeed, &SkinLayer::speed); bindF(sInten, &SkinLayer::intensity); bindF(sScale, &SkinLayer::scale);
    bindF(sDens, &SkinLayer::density); bindF(sOpac, &SkinLayer::opacity); bindF(sReact, &SkinLayer::react);
    sPanel.s.onValueChange = [this] { const float v = (float)sPanel.s.getValue(); edit([&](SkinData& d) { d.panelAlpha = v; }); };

    c1b.onClick = [this] { pickColour(c1b, [this](const SkinData& d) { return d.layers[selLayer].c1; }, [this](SkinData& d, juce::Colour c) { d.layers[selLayer].c1 = c; }); };
    c2b.onClick = [this] { pickColour(c2b, [this](const SkinData& d) { return d.layers[selLayer].c2; }, [this](SkinData& d, juce::Colour c) { d.layers[selLayer].c2 = c; }); };
    c3b.onClick = [this] { pickColour(c3b, [this](const SkinData& d) { return d.layers[selLayer].c3; }, [this](SkinData& d, juce::Colour c) { d.layers[selLayer].c3 = c; }); };
    accentB.onClick = [this] { pickColour(accentB, [](const SkinData& d) { return d.accent; }, [](SkinData& d, juce::Colour c) { d.accent = c; }); };
    accent2B.onClick = [this] { pickColour(accent2B, [](const SkinData& d) { return d.accent2; }, [](SkinData& d, juce::Colour c) { d.accent2 = c; }); };
    panelB.onClick = [this] { pickColour(panelB, [](const SkinData& d) { return d.panel; }, [](SkinData& d, juce::Colour c) { d.panel = c; }); };
    textB.onClick = [this] { pickColour(textB, [](const SkinData& d) { return d.text; }, [](SkinData& d, juce::Colour c) { d.text = c; }); };

    textEd.setTextToShowWhenEmpty("Branding text (Studio sign / car)", juce::Colours::grey);
    textEd.onTextChange = [this] { const auto s = textEd.getText(); edit([&](SkinData& d) { d.layers[selLayer].text = s; }); };
    browseBtn.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Pick an image / GIF / SVG", juce::File(mvxGetSetting("lastMediaDir", mvxMediaDir().getFullPathName())), "*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.svg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) setLayerMedia(f); });
    };
    pathLabel.setFont(juce::Font(11.f));
    variantHint.setFont(juce::Font(11.f)); variantHint.setColour(juce::Label::textColourId, juce::Colours::grey);

    for (int i = 0; i < kNumSkinPresets; ++i) presetBox.addItem(kSkinPresetNames[i], i + 1);
    auto applyPreset = [this](int i) {
        edit([&](SkinData& d) {
            auto n = makeFactorySkin(i);
            n.w = d.w; n.h = d.h; n.fps = d.fps; n.renderScale = d.renderScale; n.videoPath = d.videoPath; n.videoOn = d.videoOn;
            d = n;
        });
        syncSkinControls();
    };
    presetBox.setTextWhenNothingSelected("Skin presets...");
    presetBox.onChange = [this, applyPreset] { const int i = presetBox.getSelectedId() - 1; if (i >= 0) applyPreset(i); };
    presetPrev.onClick = [this] { presetBox.setSelectedId((juce::jmax(1, presetBox.getSelectedId()) - 2 + kNumSkinPresets) % kNumSkinPresets + 1); };
    presetNext.onClick = [this] { presetBox.setSelectedId(juce::jmax(0, presetBox.getSelectedId()) % kNumSkinPresets + 1); };
    randSkin.onClick = [this] {
        edit([&](SkinData& d) { auto n = makeRandomSkin(); n.w = d.w; n.h = d.h; n.fps = d.fps; n.renderScale = d.renderScale; n.videoPath = d.videoPath; n.videoOn = d.videoOn; d = n; });
        syncSkinControls();
    };
    saveSkin.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Save skin", mvxSkinsDir().getChildFile("MySkin.mvxskin"), "*.mvxskin");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult(); if (f == juce::File()) return;
                const juce::ScopedLock sl(proc.skinLock);
                proc.skin.toXml()->writeTo(f.withFileExtension("mvxskin"));
            });
    };
    loadSkin.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Load skin", mvxSkinsDir(), "*.mvxskin");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult(); if (!f.existsAsFile()) return;
                if (auto xml = juce::parseXML(f)) {
                    edit([&](SkinData& d) { SkinData n; n.fromXml(*xml); n.w = d.w; n.h = d.h; d = n; });
                    syncSkinControls();
                }
            });
    };
    setDefault.onClick = [this] {
        proc.saveDefaultSkin();
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Default skin saved",
            "New MultiverseX instances will now open with this skin.\n" + mvxRoot().getFullPathName());
    };

    for (int f : { 15, 24, 30, 45, 60 }) fpsBox.addItem(juce::String(f) + " fps", f);
    fpsBox.onChange = [this] { const int f = fpsBox.getSelectedId(); if (f > 0) edit([&](SkinData& d) { d.fps = f; }); };
    qualBox.addItem("Full (100%)", 100); qualBox.addItem("Balanced (75%)", 75); qualBox.addItem("Light (50%)", 50); qualBox.addItem("Potato (35%)", 35);
    qualBox.onChange = [this] { const int q = qualBox.getSelectedId(); if (q > 0) edit([&](SkinData& d) { d.renderScale = (float)q / 100.f; }); };
    fpsLbl.setText("Frame rate", juce::dontSendNotification); qualLbl.setText("Render quality", juce::dontSendNotification);

    videoBtn.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Pick a video", juce::File(mvxGetSetting("lastVideoDir", mvxMediaDir().getFullPathName())), "*.mp4;*.mov;*.avi;*.wmv;*.mkv;*.m4v;*.mpg;*.mpeg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) setVideoFile(f); });
    };
    videoOn.onClick = [this] { const bool v = videoOn.getToggleState(); edit([&](SkinData& d) { d.videoOn = v; }); };
    videoPathLbl.setFont(juce::Font(11.f));
    helpLabel.setFont(juce::Font(12.f)); helpLabel.setJustificationType(juce::Justification::topLeft);
    helpLabel.setText("Tip: drop PNG / JPG / GIF / SVG on the window to put it on the selected layer (it becomes an Image layer). "
                      "Drop MP4 / AVI / WMV to play it on the stage strip. Imported images are copied to Documents\\Hazel Bandz\\MultiverseX\\Media "
                      "and remembered with your project. Stack up to 4 layers and tune every colour: that's endless skins.",
                      juce::dontSendNotification);

    // ---------------- keyboard, video, resizer
    content.addAndMakeVisible(keys);
#if MVX_VIDEO
    addChildComponent(video);
#endif
    addAndMakeVisible(resizer);
    constrainer.setSizeLimits(480, 360, 4096, 2560);
    setConstrainer(&constrainer);
    setResizable(true, false);

    layoutPages();
    showTab(0); tabBtn[0].setToggleState(true, juce::dontSendNotification);
    seenVersion = proc.skinVersion.load();
    applyTheme();
    syncSkinControls();
    setSize(proc.skin.w, proc.skin.h);
    startTimerHz(15);
}

MultiverseXEditor::~MultiverseXEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MultiverseXEditor::layoutPages()
{
    // all coordinates below are in the fixed 1100x780 design space; the content panel is scaled to fit any window size
    content.setBounds(0, 0, kDW, kDH);
    prev.setBounds(306, 10, 34, 28); next.setBounds(344, 10, 34, 28);
    engineBox.setBounds(384, 10, 190, 28); engine2Box.setBounds(580, 10, 190, 28);
    rnd.setBounds(776, 10, 76, 28); hideBtn.setBounds(988, 10, 96, 28);
    for (int i = 0; i < 4; ++i) tabBtn[i].setBounds(18 + i * 140, 44, 132, 28);
    const juce::Rectangle<int> pr(20, 196, 1060, 496);
    for (auto* pg : { &pageSynth, &pageSamp, &pageFx, &pageSkin }) pg->setBounds(pr);
    keys.setBounds(20, 698, 1060, 76);

    // synth page
    int x = 10;
    for (auto* k : { &kBlend, &kDetune, &kCutoff, &kRes, &kDrive, &kMix, &kMaster, &kIn }) { k->setBounds(x, 12, 84, 104); x += 92; }
    envAmp.setBounds(10, 130, 330, 160); envFilt.setBounds(350, 130, 330, 160);
    kFenv.setBounds(690, 156, 84, 104);
    x = 10;
    for (auto* k : { &kL1R, &kL1C, &kL2R, &kL2P }) { k->setBounds(x, 310, 84, 104); x += 92; }

    // sampler page
    loadBtn.setBounds(10, 10, 96, 28); loopBtn.setBounds(114, 10, 70, 28); revBtn.setBounds(190, 10, 90, 28);
    modeBox.setBounds(286, 10, 90, 28); synthBtn.setBounds(950, 10, 100, 28); sampleName.setBounds(384, 10, 560, 28);
    wave.setBounds(10, 46, 1040, 170);
    for (size_t i = 0; i < sKnobs.size(); ++i) sKnobs[i]->setBounds(10 + (int)i * 130, 230, 120, 120);

    // FX page
    kFxMix.setBounds(10, 8, 84, 92); kMorph.setBounds(102, 8, 84, 92);
    storeA.setBounds(200, 14, 90, 28); storeB.setBounds(296, 14, 90, 28); rndFx.setBounds(392, 14, 130, 28);
    fxHint.setBounds(200, 50, 700, 24);
    for (int i = 0; i < kNumFxSlots; ++i) slots[i]->setBounds(10 + (i % 4) * 262, 104 + (i / 4) * 196, 254, 190);

    // skin page (left: layer, right: global)
    for (int i = 0; i < kNumLayers; ++i) layerBtn[i].setBounds(10 + i * 104, 10, 100, 28);
    layerOn.setBounds(10, 46, 110, 26); typeBox.setBounds(124, 46, 250, 26); rainbowBtn.setBounds(384, 46, 130, 26);
    sVariant.setBounds(10, 80, 500, 26); variantHint.setBounds(10, 106, 500, 18);
    int y = 130;
    for (auto* s : { &sSpeed, &sInten, &sScale, &sDens, &sOpac, &sReact }) { s->setBounds(10, y, 500, 26); y += 28; }
    c1b.setBounds(10, 304, 110, 30); c2b.setBounds(126, 304, 110, 30); c3b.setBounds(242, 304, 110, 30);
    textEd.setBounds(10, 346, 240, 26); browseBtn.setBounds(258, 346, 252, 26); pathLabel.setBounds(10, 376, 500, 20);

    presetPrev.setBounds(530, 10, 36, 28); presetBox.setBounds(570, 10, 400, 28); presetNext.setBounds(974, 10, 36, 28);
    randSkin.setBounds(530, 48, 160, 28); saveSkin.setBounds(696, 48, 100, 28); loadSkin.setBounds(802, 48, 100, 28); setDefault.setBounds(908, 48, 140, 28);
    accentB.setBounds(530, 88, 120, 30); accent2B.setBounds(656, 88, 120, 30); panelB.setBounds(782, 88, 120, 30); textB.setBounds(908, 88, 120, 30);
    sPanel.setBounds(530, 126, 500, 26);
    fpsLbl.setBounds(530, 160, 90, 24); fpsBox.setBounds(620, 160, 110, 26); qualLbl.setBounds(746, 160, 100, 24); qualBox.setBounds(848, 160, 180, 26);
    videoBtn.setBounds(530, 200, 150, 28); videoOn.setBounds(690, 200, 160, 28); videoPathLbl.setBounds(530, 232, 500, 20);
    helpLabel.setBounds(530, 262, 510, 120);
}

void MultiverseXEditor::showTab(int i)
{
    pageSynth.setVisible(i == 0); pageSamp.setVisible(i == 1); pageFx.setVisible(i == 2); pageSkin.setVisible(i == 3);
}

void MultiverseXEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    backdrop.setBounds(0, 0, w, h);
    const float sc = juce::jmin((float)w / (float)kDW, (float)h / (float)kDH);
    content.setBounds(0, 0, kDW, kDH);
    content.setTransform(juce::AffineTransform::scale(sc).translated(((float)w - (float)kDW * sc) * 0.5f, ((float)h - (float)kDH * sc) * 0.5f));
    resizer.setBounds(w - 22, h - 22, 22, 22);
    resizer.toFront(false);
    if (w > 300 && h > 250) { const juce::ScopedLock sl(proc.skinLock); proc.skin.w = w; proc.skin.h = h; }
    updateVideo();
}

void MultiverseXEditor::mouseDown(const juce::MouseEvent&)
{
    if (!content.isVisible()) content.setVisible(true);   // click anywhere to bring the UI back after "Hide UI"
}

void MultiverseXEditor::applyTheme()
{
    const auto& s = proc.skin;
    const juce::String th = s.accent.toString() + s.accent2.toString() + s.panel.toString() + s.text.toString();
    if (th != lastTheme) {
        lastTheme = th;
        laf.applyTheme(s);
        sendLookAndFeelChange();
        keys.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, s.accent.withAlpha(0.8f));
        keys.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, s.accent.withAlpha(0.35f));
        sampleName.setColour(juce::Label::textColourId, s.accent2);
    }
    const auto& l = s.layers[juce::jlimit(0, kNumLayers - 1, selLayer)];
    c1b.swatch = l.c1; c2b.swatch = l.c2; c3b.swatch = l.c3;
    accentB.swatch = s.accent; accent2B.swatch = s.accent2; panelB.swatch = s.panel; textB.swatch = s.text;
    for (auto* b : { &c1b, &c2b, &c3b, &accentB, &accent2B, &panelB, &textB }) b->repaint();
    backdrop.setFps(s.fps);
    content.repaint();
    pageSynth.repaint(); pageSamp.repaint(); pageFx.repaint(); pageSkin.repaint();
}

void MultiverseXEditor::syncSkinControls()
{
    const auto& s = proc.skin;
    const auto& l = s.layers[juce::jlimit(0, kNumLayers - 1, selLayer)];
    layerBtn[selLayer].setToggleState(true, juce::dontSendNotification);
    layerOn.setToggleState(l.enabled, juce::dontSendNotification);
    rainbowBtn.setToggleState(l.rainbow, juce::dontSendNotification);
    typeBox.setSelectedId(l.type + 1, juce::dontSendNotification);
    sVariant.s.setValue(l.variant, juce::dontSendNotification);
    sSpeed.s.setValue(l.speed, juce::dontSendNotification); sInten.s.setValue(l.intensity, juce::dontSendNotification);
    sScale.s.setValue(l.scale, juce::dontSendNotification); sDens.s.setValue(l.density, juce::dontSendNotification);
    sOpac.s.setValue(l.opacity, juce::dontSendNotification); sReact.s.setValue(l.react, juce::dontSendNotification);
    sPanel.s.setValue(s.panelAlpha, juce::dontSendNotification);
    textEd.setText(l.text, false);
    variantHint.setText(juce::String("Variant: ") + kVfxVariantHint[l.type], juce::dontSendNotification);
    pathLabel.setText(l.path.isEmpty() ? juce::String("(no image on this layer)") : "File: " + l.path, juce::dontSendNotification);
    fpsBox.setSelectedId(s.fps, juce::dontSendNotification);
    qualBox.setSelectedId((int)std::round(s.renderScale * 100.f), juce::dontSendNotification);
    videoOn.setToggleState(s.videoOn, juce::dontSendNotification);
    videoPathLbl.setText(s.videoPath.isEmpty() ? juce::String("(no video chosen)") : "Video: " + s.videoPath, juce::dontSendNotification);
    applyTheme();
}

void MultiverseXEditor::pickColour(ColourButton& b, std::function<juce::Colour(const SkinData&)> get,
                                   std::function<void(SkinData&, juce::Colour)> set)
{
    juce::Component::SafePointer<MultiverseXEditor> safe(this);
    auto popup = std::make_unique<ColourPopup>(get(proc.skin), [safe, set](juce::Colour c) {
        if (safe != nullptr) safe->edit([&](SkinData& d) { set(d, c); });
    });
    juce::CallOutBox::launchAsynchronously(std::move(popup), b.getScreenBounds(), nullptr);
}

void MultiverseXEditor::loadSampleFile(const juce::File& f)
{
    mvxSetSetting("lastSampleDir", f.getParentDirectory().getFullPathName());
    const juce::File lib(mvxImport(f, true));
    if (proc.loadSample(lib)) sampleName.setText(lib.getFileName(), juce::dontSendNotification);
    else sampleName.setText("Could not load " + f.getFileName(), juce::dontSendNotification);
}

void MultiverseXEditor::setLayerMedia(const juce::File& f)
{
    mvxSetSetting("lastMediaDir", f.getParentDirectory().getFullPathName());
    const auto path = mvxImport(f, true);
    edit([&](SkinData& d) { auto& l = d.layers[selLayer]; l.type = VfxMedia; l.enabled = true; l.path = path; });
    syncSkinControls();
}

void MultiverseXEditor::setVideoFile(const juce::File& f)
{
    mvxSetSetting("lastVideoDir", f.getParentDirectory().getFullPathName());
    const auto path = mvxImport(f, false);
    edit([&](SkinData& d) { d.videoPath = path; d.videoOn = true; });
    syncSkinControls();
#if ! MVX_VIDEO
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Video",
        "This build was compiled without video support (CMake option MVX_VIDEO=OFF).");
#endif
}

void MultiverseXEditor::updateVideo()
{
#if MVX_VIDEO
    const auto& sk = proc.skin;
    if (!(sk.videoOn && sk.videoPath.isNotEmpty())) {
        if (video.isVisible()) { video.stop(); video.setVisible(false); }
        videoLoaded = juce::String();
        return;
    }
    const auto f = mvxResolve(sk.videoPath);
    if (!f.existsAsFile()) { video.setVisible(false); return; }
    video.setBounds(getLocalArea(&content, juce::Rectangle<int>(20, 82, 1060, 106)));
    video.setVisible(true);
    if (videoLoaded != f.getFullPathName()) {
        videoLoaded = f.getFullPathName();
        juce::Component::SafePointer<MultiverseXEditor> safe(this);
        video.loadAsync(juce::URL(f), [safe](const juce::URL&, juce::Result r) {
            if (safe != nullptr && r.wasOk()) { safe->video.setAudioVolume(0.f); safe->video.play(); }
        });
    } else if (video.isVideoOpen() && video.getVideoDuration() > 0.0 && video.getPlayPosition() >= video.getVideoDuration() - 0.15) {
        video.setPlayPosition(0.0); video.play();     // loop
    }
#endif
}

void MultiverseXEditor::timerCallback()
{
    if (proc.skinVersion.load() != seenVersion) {      // skin changed from outside (project load etc.)
        seenVersion = proc.skinVersion.load();
        syncSkinControls();
    }
    for (auto& s : slots) if (s != nullptr) s->refresh();
    if (pageSynth.isVisible()) { envAmp.repaint(); envFilt.repaint(); }
    updateVideo();
}

bool MultiverseXEditor::isInterestedInFileDrag(const juce::StringArray& f)
{
    for (auto& s : f) if (juce::File(s).hasFileExtension("wav;aif;aiff;flac;png;jpg;jpeg;gif;bmp;svg;mp4;mov;avi;wmv;mkv;m4v;mpg;mpeg;mvxskin")) return true;
    return false;
}

void MultiverseXEditor::filesDropped(const juce::StringArray& f, int, int)
{
    for (auto& s : f) {
        const juce::File file(s);
        if (file.hasFileExtension("wav;aif;aiff;flac")) { loadSampleFile(file); break; }
        if (file.hasFileExtension("png;jpg;jpeg;gif;bmp;svg")) { setLayerMedia(file); break; }
        if (file.hasFileExtension("mp4;mov;avi;wmv;mkv;m4v;mpg;mpeg")) { setVideoFile(file); break; }
        if (file.hasFileExtension("mvxskin")) {
            if (auto xml = juce::parseXML(file)) {
                edit([&](SkinData& d) { SkinData n; n.fromXml(*xml); n.w = d.w; n.h = d.h; d = n; });
                syncSkinControls();
            }
            break;
        }
    }
}

