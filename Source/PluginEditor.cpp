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
    rot += 0.004f + std::abs(l1) * 0.02f + lev * 0.05f;
    scale += ((0.8f + lev * 0.7f) - scale) * 0.25f;
    vx += (l1 * 30.f - cx) * 0.08f; vy += (l2 * 30.f - cy) * 0.08f;
    vx *= 0.85f; vy *= 0.85f; cx += vx; cy += vy;
    const auto c = b.getCentre() + juce::Point<float>(cx, cy);
    const float R = juce::jmin(b.getWidth(), b.getHeight()) * 0.45f * scale;

    g.setGradientFill(juce::ColourGradient(col.withAlpha(0.5f + lev * 0.5f), c.x, c.y, col.withAlpha(0.f), c.x + R, c.y, true));
    g.fillEllipse(c.x - R, c.y - R, R * 2, R * 2);

    for (auto& s : stars) {
        s.angle += s.speed * 0.002f * (1.f + std::abs(l2) * 4.f);
        float a = s.angle + s.radius * 7.f + rot;               // spiral arms
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
    addAndMakeVisible(*s); addAndMakeVisible(*l);
    atts.push_back(std::make_unique<SA>(proc.apvts, id, *s));
    (sampler ? sKnobs : knobs).push_back(std::move(s));
    (sampler ? sLabels : labels).push_back(std::move(l));
}

MultiverseXEditor::MultiverseXEditor(MultiverseXProcessor& p)
    : AudioProcessorEditor(&p), proc(p), galaxy(p), wave(p),
      keys(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
    setSize(900, 900);
    addAndMakeVisible(galaxy); addAndMakeVisible(wave);
    for (int i = 0; i < kNumEngines; ++i) engineBox.addItem(kEngines[i].name, i + 1);
    addAndMakeVisible(engineBox);
    engineAtt = std::make_unique<CA>(p.apvts, "engine", engineBox);
    engineBox.onChange = [this] { proc.applyEngine(engineBox.getSelectedItemIndex()); };
    prev.onClick = [this] { proc.applyEngine((engineBox.getSelectedItemIndex() + kNumEngines - 1) % kNumEngines); };
    next.onClick = [this] { proc.applyEngine((engineBox.getSelectedItemIndex() + 1) % kNumEngines); };
    rnd.onClick  = [this] { proc.randomize(); };
    for (auto* b : {&prev, &next, &rnd, &loadBtn}) addAndMakeVisible(*b);

    const std::pair<const char*, const char*> defs[] = {
        {"cutoff","Cutoff"},{"res","Resonance"},{"drive","Drive"},{"detune","Detune"},
        {"atk","Attack"},{"dec","Decay"},{"sus","Sustain"},{"rel","Release"},
        {"lfo1Rate","LFO1 Rate"},{"lfo1Cut","LFO1>Cut"},{"lfo2Rate","LFO2 Rate"},{"lfo2Pitch","LFO2>Pitch"},
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

void MultiverseXEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0a0620));
    g.setColour(juce::Colour(0xffb06bff));
    g.setFont(juce::Font(28.f, juce::Font::italic | juce::Font::bold));
    g.drawText("MultiverseX  -  Hazel Bandz", 10, 6, 520, 36, juce::Justification::left);
}

void MultiverseXEditor::resized() {
    auto r = getLocalBounds().reduced(10);
    r.removeFromTop(40);
    auto top = r.removeFromTop(30);
    prev.setBounds(top.removeFromLeft(70)); next.setBounds(top.removeFromLeft(70).translated(6, 0));
    rnd.setBounds(top.removeFromRight(90)); engineBox.setBounds(top.reduced(90, 0));
    keys.setBounds(r.removeFromBottom(90));
    galaxy.setBounds(r.removeFromTop(250).reduced(0, 4));

    auto grid = r.removeFromTop(190);
    const int cols = 8, w = grid.getWidth() / cols, h = grid.getHeight() / 2;
    for (size_t i = 0; i < knobs.size(); ++i) {
        juce::Rectangle<int> cell(grid.getX() + (int)(i % cols) * w, grid.getY() + (int)(i / cols) * h, w, h);
        labels[i]->setBounds(cell.removeFromBottom(18)); knobs[i]->setBounds(cell.reduced(6));
    }

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
