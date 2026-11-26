#include "PluginEditor.h"

// ------------------------------------------------------------ galaxy
GalaxyVisualizer::GalaxyVisualizer(MultiverseXProcessor& p) : proc(p) {
    juce::Random r(42);
    for (int i = 0; i < 700; ++i) {
        float t = r.nextFloat();
        stars.push_back({r.nextFloat() * 6.2832f, t, 0.2f + r.nextFloat(), 0.8f + r.nextFloat() * 2.2f});
    }
    startTimerHz(30);
}

void GalaxyVisualizer::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff05030f));
    const float l1 = proc.visLfo1, l2 = proc.visLfo2, lev = proc.visLevel;
    auto col = juce::Colour(kEngines[juce::jlimit(0, kNumEngines - 1, (int)proc.apvts.getRawParameterValue("engine")->load())].colour);

    // modulation drives rotation, scale and a spring-damped "bounce" of the whole galaxy
    const float en = proc.visModEnergy, mw = proc.modWheel, at = proc.aftertouch;
    const float twist = 7.f + at * 6.f + en * 3.f;
    rot += 0.004f + std::abs(l1) * 0.02f + lev * 0.05f + en * 0.03f;
    scale += ((0.8f + lev * 0.7f + mw * 0.4f + en * 0.2f) - scale) * 0.25f;
    const int nc = proc.noteCount.load();                   // every note kicks the galaxy in a random direction
    if (nc != lastNotes) {
        lastNotes = nc;
        vx += (rng.nextFloat() - 0.5f) * 30.f * (0.4f + proc.lastVel); vy += (rng.nextFloat() - 0.5f) * 30.f * (0.4f + proc.lastVel);
    }
    vx += (l1 * 30.f * (0.5f + en) - cx) * 0.08f; vy += (l2 * 30.f * (0.5f + en) - cy) * 0.08f;
    vx *= 0.85f; vy *= 0.85f; cx += vx; cy += vy;
    cx = juce::jlimit(-b.getWidth() * 0.25f, b.getWidth() * 0.25f, cx);
    cy = juce::jlimit(-b.getHeight() * 0.25f, b.getHeight() * 0.25f, cy);
    const auto c = b.getCentre() + juce::Point<float>(cx, cy);
    const float R = juce::jmin(b.getWidth(), b.getHeight()) * 0.45f * scale;

    g.setGradientFill(juce::ColourGradient(col.withAlpha(0.5f + lev * 0.5f), c.x, c.y, col.withAlpha(0.f), c.x + R, c.y, true));
    g.fillEllipse(c.x - R, c.y - R, R * 2, R * 2);

    for (auto& s : stars) {
        s.angle += s.speed * 0.002f * (1.f + std::abs(l2) * 4.f);
        float a = s.angle + s.radius * twist + rot;               // spiral arms
        float rr = s.radius * R;
        float x = c.x + std::cos(a) * rr, y = c.y + std::sin(a) * rr * 0.55f;
        g.setColour(col.brighter(1.f - s.radius).withAlpha(juce::jlimit(0.f, 1.f, 0.25f + 0.6f * (1.f - s.radius) + lev * 0.2f)));
        g.fillEllipse(x, y, s.size, s.size);
    }
    g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.f, 1.f, 0.6f + lev * 0.4f)));
    g.fillEllipse(c.x - 6 - lev * 10, c.y - 4 - lev * 6, 12 + lev * 20, 8 + lev * 12);
    g.setColour(col); g.drawRect(b, 2.f);
}

// ------------------------------------------------------------ waveform
void WaveformView::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff05030f));
    std::vector<float> pk;
    { juce::SpinLock::ScopedLockType l(proc.peakLock); pk = proc.peaks; }
    g.setColour(juce::Colour(0xff2fd0ff)); g.drawRect(b, 1.5f);
    if (pk.empty()) {
        g.setColour(juce::Colours::grey);
        g.drawText("Load a WAV (or drop one here)", getLocalBounds(), juce::Justification::centred);
        return;
    }
    const float mid = b.getCentreY();
    g.setColour(juce::Colour(0xff2fd0ff));
    for (size_t i = 0; i < pk.size(); ++i) {
        float x = b.getX() + (float)i / (float)pk.size() * b.getWidth(), h = pk[i] * b.getHeight() * 0.48f;
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
        g.setColour(juce::Colour(0xffff2fd0));
        for (int k = 1; k < n; ++k) { float x = xs + (xe - xs) * (float)k / (float)n; g.drawLine(x, b.getY(), x, b.getBottom(), 1.f); }
    }
}

// ------------------------------------------------------------ editor
void MultiverseXEditor::addKnob(const char* id, const char* name, bool sampler) {
    auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    auto l = std::make_unique<juce::Label>(); l->setText(name, juce::dontSendNotification);
    l->setJustificationType(juce::Justification::centred);
    l->setFont(juce::Font(13.f));
    addAndMakeVisible(*s); addAndMakeVisible(*l);
    atts.push_back(std::make_unique<SA>(proc.apvts, id, *s));
    (sampler ? sKnobs : knobs).push_back(std::move(s));
    (sampler ? sLabels : labels).push_back(std::move(l));
}

MultiverseXEditor::MultiverseXEditor(MultiverseXProcessor& p)
    : AudioProcessorEditor(&p), proc(p), galaxy(p), wave(p),
      keys(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
    setSize(1300, 900);
    setLookAndFeel(&laf);
    juce::Random srnd(7);
    for (int i = 0; i < 220; ++i) stars.push_back({srnd.nextFloat() * 1300.f, srnd.nextFloat() * 900.f});
    startTimerHz(10);
    addAndMakeVisible(galaxy); addAndMakeVisible(wave);
    for (int i = 0; i < kNumEngines; ++i) engineBox.addItem(kEngines[i].name, i + 1);
    addAndMakeVisible(engineBox);
    // display-only link: the combo follows the parameter, and only a user pick loads that engine's preset
    engineParamAtt = std::make_unique<juce::ParameterAttachment>(*p.apvts.getParameter("engine"),
        [this](float v) { engineBox.setSelectedItemIndex((int)std::round(v), juce::dontSendNotification); });
    engineParamAtt->sendInitialUpdate();
    engineBox.onChange = [this] { proc.applyEngine(engineBox.getSelectedItemIndex()); };
    prev.onClick = [this] { proc.applyEngine((engineBox.getSelectedItemIndex() + kNumEngines - 1) % kNumEngines); };
    next.onClick = [this] { proc.applyEngine((engineBox.getSelectedItemIndex() + 1) % kNumEngines); };
    rnd.setButtonText("Full Mix");
    rnd.onClick  = [this] { proc.randomizeAll(); };
    for (auto* b : {&prev, &next, &rnd, &loadBtn}) addAndMakeVisible(*b);

    const std::pair<const char*, const char*> defs[] = {
        {"cutoff","Cutoff"},{"res","Resonance"},{"drive","Drive"},{"detune","Detune"},
        {"atk","Attack"},{"dec","Decay"},{"sus","Sustain"},{"rel","Release"},
        {"lfo1Rate","LFO1 Rate"},{"lfo2Rate","LFO2 Rate"},
        {"reverb","Reverb"},{"delay","Delay"},{"master","Master"}};
    for (auto& d : defs) addKnob(d.first, d.second, false);

    const std::pair<const char*, const char*> sdefs[] = {
        {"sStart","Start"},{"sEnd","End"},{"sTune","Tune"},{"sSpeed","Speed"},
        {"sPitch","Pitch"},{"sPan","Pan"},{"sVol","Volume"},{"sSlices","Slices"}};
    for (auto& d : sdefs) addKnob(d.first, d.second, true);

    modeBox.addItem("Map", 1); modeBox.addItem("Slice", 2);
    addAndMakeVisible(modeBox); modeAtt = std::make_unique<CA>(p.apvts, "sMode", modeBox);
    for (auto* t : {&loopBtn, &revBtn, &synthBtn}) addAndMakeVisible(*t);
    loopAtt = std::make_unique<BA>(p.apvts, "sLoop", loopBtn);
    revAtt = std::make_unique<BA>(p.apvts, "sRev", revBtn);
    synthAtt = std::make_unique<BA>(p.apvts, "synthOn", synthBtn);
    sampleName.setText(p.getSampleName(), juce::dontSendNotification);
    sampleName.setColour(juce::Label::textColourId, juce::Colour(0xff2fd0ff));
    addAndMakeVisible(sampleName);
    loadBtn.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Load a sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) loadFile(f); });
    };
    for (int i = 0; i < kModSlots; ++i) {                  // modulation matrix rows
        const auto id = "m" + juce::String(i);
        auto src = std::make_unique<juce::ComboBox>(), dst = std::make_unique<juce::ComboBox>();
        int n = 1; for (auto& t : modSources()) src->addItem(t, n++);
        n = 1;     for (auto& t : modTargets()) dst->addItem(t, n++);
        auto amt = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        amt->textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; };
        amt->valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() / 100.0; };
        amt->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
        amt->setDoubleClickReturnValue(true, 0.0);
        for (juce::Component* c : {(juce::Component*)src.get(), (juce::Component*)dst.get(), (juce::Component*)amt.get()}) addAndMakeVisible(*c);
        mCombo.push_back(std::make_unique<CA>(p.apvts, id + "src", *src));
        mCombo.push_back(std::make_unique<CA>(p.apvts, id + "dst", *dst));
        atts.push_back(std::make_unique<SA>(p.apvts, id + "amt", *amt));
        mSrc.push_back(std::move(src)); mDst.push_back(std::move(dst)); mAmt.push_back(std::move(amt));
    }
    for (int s = 0; s < SEC_COUNT; ++s) {
        auto b = std::make_unique<SectionButton>(proc, s);
        b->onClick = [this, s] { proc.randomizeSection(s); };
        addAndMakeVisible(*b); secBtn.push_back(std::move(b));
    }
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    addAndMakeVisible(undoBtn); addAndMakeVisible(redoBtn);
    amtSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amtSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
    amtSlider.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; };
    addAndMakeVisible(amtSlider);
    amtAtt = std::make_unique<SA>(p.apvts, "randAmount", amtSlider);
    for (int i = 0; i < kNumFx; ++i) {                     // effects rack rows
        const juce::String id = juce::String("fx_") + kFx[i].key;
        auto t = std::make_unique<juce::ToggleButton>(juce::String(kFx[i].name));
        auto ms = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        auto rs = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        for (juce::Component* c : {(juce::Component*)t.get(), (juce::Component*)ms.get(), (juce::Component*)rs.get()}) addAndMakeVisible(*c);
        fxAtt.push_back(std::make_unique<BA>(p.apvts, id + "_on", *t));
        atts.push_back(std::make_unique<SA>(p.apvts, id + "_mix", *ms));
        atts.push_back(std::make_unique<SA>(p.apvts, id + "_rate", *rs));
        fxOn.push_back(std::move(t)); fxMix.push_back(std::move(ms)); fxRate.push_back(std::move(rs));
    }
    keys.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffe9e4ff));
    keys.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff150c33));
    keys.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff4a3a8a));
    keys.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xaa9b5cff));
    keys.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x449b5cff));
    addAndMakeVisible(keys);
}

void MultiverseXEditor::loadFile(const juce::File& f) {
    if (proc.loadSample(f)) sampleName.setText(f.getFileName(), juce::dontSendNotification);
    else sampleName.setText("Could not load " + f.getFileName(), juce::dontSendNotification);
}
bool MultiverseXEditor::isInterestedInFileDrag(const juce::StringArray& f) {
    for (auto& s : f) if (juce::File(s).hasFileExtension("wav;aif;aiff;flac")) return true;
    return false;
}
void MultiverseXEditor::filesDropped(const juce::StringArray& f, int, int) {
    for (auto& s : f) if (juce::File(s).hasFileExtension("wav;aif;aiff;flac")) { loadFile(juce::File(s)); break; }
}

static void drawCrown(juce::Graphics& g, juce::Rectangle<float> r) {
    juce::Path p; const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    p.startNewSubPath(x, y + h); p.lineTo(x, y + h * 0.3f); p.lineTo(x + w * 0.25f, y + h * 0.6f);
    p.lineTo(x + w * 0.5f, y); p.lineTo(x + w * 0.75f, y + h * 0.6f); p.lineTo(x + w, y + h * 0.3f);
    p.lineTo(x + w, y + h); p.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffe680), x, y, juce::Colour(0xffb8860b), x, y + h, false));
    g.fillPath(p);
    g.setColour(juce::Colour(0xff5a3a00)); g.strokePath(p, juce::PathStrokeType(1.f));
    g.setColour(juce::Colour(0xffff4f6a)); g.fillEllipse(x + w * 0.5f - 2.5f, y + h * 0.55f, 5.f, 5.f);
}

static void neonText(juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Font f, juce::Colour c, juce::Justification j) {
    g.setFont(f);
    for (int k = 4; k >= 1; --k) {
        g.setColour(c.withAlpha(0.08f));
        for (auto o : {juce::Point<int>(-k, 0), juce::Point<int>(k, 0), juce::Point<int>(0, -k), juce::Point<int>(0, k)})
            g.drawText(t, r.translated(o.x, o.y), j);
    }
    g.setColour(c.brighter(0.7f)); g.drawText(t, r, j);
}

void MultiverseXEditor::drawPanel(juce::Graphics& g, juce::Rectangle<int> r) {
    const auto f = r.toFloat();
    g.setColour(juce::Colour(0xff0b0820).withAlpha(0.8f)); g.fillRoundedRectangle(f, 8.f);
    for (int i = 3; i >= 1; --i) {
        g.setColour(laf.accent.withAlpha(0.07f * (float)(4 - i)));
        g.drawRoundedRectangle(f.expanded((float)i * 1.5f), 8.f + (float)i, 2.f);
    }
    g.setColour(laf.accent.withAlpha(0.9f)); g.drawRoundedRectangle(f, 8.f, 1.5f);
}

void MultiverseXEditor::timerCallback() {          // accent colour follows the selected engine
    const int idx = juce::jlimit(0, kNumEngines - 1, (int)proc.apvts.getRawParameterValue("engine")->load());
    if (idx != lastEngine) {
        lastEngine = idx;
        laf.accent = juce::Colour(kEngines[idx].colour);
        sendLookAndFeelChange();
        repaint();
    }
}

MultiverseXEditor::~MultiverseXEditor() { setLookAndFeel(nullptr); }

void MultiverseXEditor::paint(juce::Graphics& g) {
    const auto acc = laf.accent;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff05031a), 0.f, 0.f, juce::Colour(0xff150b33), 0.f, (float)getHeight(), false));
    g.fillAll();
    for (size_t i = 0; i < stars.size(); ++i) {                 // fixed star backdrop
        g.setColour((i % 5 == 0 ? acc : juce::Colours::white).withAlpha(0.15f + 0.05f * (float)(i % 6)));
        g.fillEllipse(stars[i].x, stars[i].y, 1.5f, 1.5f);
    }

    // header
    drawCrown(g, {12.f, 8.f, 28.f, 22.f});
    const juce::Font logo(28.f, juce::Font::italic | juce::Font::bold);
    neonText(g, "Hazel Bandz", {46, 2, 240, 36}, logo, juce::Colour(0xffff4fd8), juce::Justification::centredLeft);
    drawCrown(g, {54.f + (float)logo.getStringWidth("Hazel Bandz"), 8.f, 28.f, 22.f});
    neonText(g, "MultiverseX", {330, 0, 260, 40}, juce::Font(32.f, juce::Font::bold), acc, juce::Justification::centred);
    g.setFont(juce::Font(11.f)); g.setColour(juce::Colour(0xffb9b0e8));
    g.drawText(juce::String::fromUTF8("DREAM \xe2\x80\xa2 CREATE \xe2\x80\xa2 COMPOSE \xe2\x80\xa2 PERFORM"), 600, 10, 290, 20, juce::Justification::centredRight);

    // panels
    drawPanel(g, panelKnobs); drawPanel(g, panelSampler); drawPanel(g, panelKeys);
    drawPanel(g, {908, 42, 384, 426}); drawPanel(g, {908, 470, 384, 346}); drawPanel(g, {908, 818, 384, 76});
    g.setColour(acc.withAlpha(0.35f)); g.drawRoundedRectangle(panelGalaxy.toFloat().expanded(1.f), 4.f, 3.f);

    g.setFont(juce::Font(18.f, juce::Font::bold)); g.setColour(acc.brighter(0.5f));
    g.drawText("MODULATION MATRIX", 920, 46, 370, 24, juce::Justification::left);
    g.drawText("EFFECTS", 920, 474, 120, 24, juce::Justification::left);
    g.setFont(juce::Font(12.f)); g.setColour(juce::Colour(0xff9a93c8));
    g.drawText("Source  >  Target  /  Amount (double-click = 0)", 920, 70, 370, 18, juce::Justification::left);
    g.drawText("Mix", 1050, 478, 120, 18, juce::Justification::centred);
    g.drawText("Rate / Tone", 1170, 478, 120, 18, juce::Justification::centred);
    g.setFont(juce::Font(11.f));
    g.drawText("Click = Randomize    Right-Click = Lock    (locked sections are skipped by Full Mix)", 910, 814, 380, 14, juce::Justification::centred);
    g.setFont(juce::Font(13.f)); g.setColour(juce::Colour(0xffff9ae8));
    g.drawText("Amount", 918, 860, 60, 26, juce::Justification::left);
}

void MultiverseXEditor::resized() {
    auto r = getLocalBounds().reduced(10);
    r.removeFromRight(400);                                 // right column is the matrix
    {
        const int rowH = 46, x0 = 910, y0 = 96;
        for (int i = 0; i < kModSlots; ++i) {
            juce::Rectangle<int> row(x0, y0 + i * rowH, 380, rowH);
            mSrc[(size_t)i]->setBounds(row.removeFromLeft(105).reduced(2, 10));
            mDst[(size_t)i]->setBounds(row.removeFromLeft(115).reduced(2, 10));
            mAmt[(size_t)i]->setBounds(row.reduced(2, 8));
        }
    }
    for (int i = 0; i < kNumFx; ++i) {
        juce::Rectangle<int> row(910, 500 + i * 24, 380, 24);
        fxOn[(size_t)i]->setBounds(row.removeFromLeft(140));
        fxMix[(size_t)i]->setBounds(row.removeFromLeft(120));
        fxRate[(size_t)i]->setBounds(row);
    }
    {
        juce::Rectangle<int> a(910, 830, 380, 26);
        for (auto& b : secBtn) b->setBounds(a.removeFromLeft(95).reduced(2));
        juce::Rectangle<int> c(910, 860, 380, 26);
        c.removeFromLeft(62);
        redoBtn.setBounds(c.removeFromRight(60).reduced(2));
        undoBtn.setBounds(c.removeFromRight(60).reduced(2));
        amtSlider.setBounds(c);
    }
    r.removeFromTop(40);
    auto top = r.removeFromTop(30);
    prev.setBounds(top.removeFromLeft(70)); next.setBounds(top.removeFromLeft(70).translated(6, 0));
    rnd.setBounds(top.removeFromRight(90)); engineBox.setBounds(top.reduced(90, 0));
    keys.setBounds(r.removeFromBottom(90));
    panelKeys = keys.getBounds().expanded(3);
    galaxy.setBounds(r.removeFromTop(250).reduced(0, 4));
    panelGalaxy = galaxy.getBounds();

    auto grid = r.removeFromTop(190);
    panelKnobs = grid;
    const int cols = 8, w = grid.getWidth() / cols, h = grid.getHeight() / 2;
    for (size_t i = 0; i < knobs.size(); ++i) {
        juce::Rectangle<int> cell(grid.getX() + (int)(i % cols) * w, grid.getY() + (int)(i / cols) * h, w, h);
        labels[i]->setBounds(cell.removeFromBottom(18)); knobs[i]->setBounds(cell.reduced(6));
    }

    panelSampler = r;
    auto bar = r.removeFromTop(30);                       // sampler header row
    loadBtn.setBounds(bar.removeFromLeft(90)); bar.removeFromLeft(6);
    loopBtn.setBounds(bar.removeFromLeft(70)); revBtn.setBounds(bar.removeFromLeft(90));
    modeBox.setBounds(bar.removeFromLeft(90).reduced(2)); synthBtn.setBounds(bar.removeFromRight(100));
    sampleName.setBounds(bar);
    wave.setBounds(r.removeFromTop(100).reduced(0, 2));
    const int sw = r.getWidth() / 8;
    for (size_t i = 0; i < sKnobs.size(); ++i) {
        juce::Rectangle<int> cell(r.getX() + (int)i * sw, r.getY(), sw, r.getHeight());
        sLabels[i]->setBounds(cell.removeFromBottom(18)); sKnobs[i]->setBounds(cell.reduced(6));
    }
}
