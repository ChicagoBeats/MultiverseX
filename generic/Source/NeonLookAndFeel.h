#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Neon sci-fi look. `accent` follows the selected engine's colour.
class NeonLookAndFeel : public juce::LookAndFeel_V4 {
public:
    juce::Colour accent{0xff9b5cff};

    NeonLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff0a0620));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1438));
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d0a22));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0a0620));
        setColour(juce::PopupMenu::textColourId, juce::Colours::white);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff4a2a9a));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::Label::textColourId, juce::Colour(0xffc9c2ff));
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0d0a22));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                          float start, float end, juce::Slider&) override {
        auto b = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(4.f);
        const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float angle = start + pos * (end - start), arcR = r - 2.f;

        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2e2c44), c.x, c.y - r, juce::Colour(0xff07060f), c.x, c.y + r, false));
        g.fillEllipse(c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f);
        g.setColour(juce::Colour(0xff000000).withAlpha(0.6f));
        g.drawEllipse(c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f, 1.5f);

        juce::Path track; track.addCentredArc(c.x, c.y, arcR, arcR, 0.f, start, end, true);
        g.setColour(accent.withAlpha(0.18f)); g.strokePath(track, juce::PathStrokeType(3.f));

        juce::Path val; val.addCentredArc(c.x, c.y, arcR, arcR, 0.f, start, angle, true);
        g.setColour(accent.withAlpha(0.15f)); g.strokePath(val, juce::PathStrokeType(9.f));
        g.setColour(accent.withAlpha(0.35f)); g.strokePath(val, juce::PathStrokeType(6.f));
        g.setColour(accent.brighter(0.4f));   g.strokePath(val, juce::PathStrokeType(3.f));

        const float len = r * 0.62f;
        g.setColour(juce::Colours::white);
        g.drawLine(c.x + std::sin(angle) * len * 0.35f, c.y - std::cos(angle) * len * 0.35f,
                   c.x + std::sin(angle) * len,          c.y - std::cos(angle) * len, 2.5f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float mn, float mx,
                          juce::Slider::SliderStyle style, juce::Slider& s) override {
        if (style != juce::Slider::LinearHorizontal) {
            LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, mn, mx, style, s);
            return;
        }
        const float cy = (float)y + (float)h * 0.5f;
        g.setColour(juce::Colour(0xff1c1840));
        g.fillRoundedRectangle((float)x, cy - 2.f, (float)w, 4.f, 2.f);
        const float x0 = s.getMinimum() < 0.0 ? (float)x + (float)w * 0.5f : (float)x;   // bipolar sliders fill from the centre
        const float a = juce::jmin(x0, pos), len = std::abs(pos - x0);
        g.setColour(accent.withAlpha(0.25f)); g.fillRoundedRectangle(a, cy - 4.f, len, 8.f, 4.f);
        g.setColour(accent);                  g.fillRoundedRectangle(a, cy - 2.f, len, 4.f, 2.f);
        g.setColour(accent.withAlpha(0.3f));  g.fillEllipse(pos - 9.f, cy - 9.f, 18.f, 18.f);
        g.setColour(juce::Colours::white);    g.fillEllipse(pos - 5.f, cy - 5.f, 10.f, 10.f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool hi, bool down) override {
        auto r = b.getLocalBounds().toFloat().reduced(1.f);
        auto c = bg; if (hi) c = c.brighter(0.15f); if (down) c = c.brighter(0.3f);
        g.setGradientFill(juce::ColourGradient(c.brighter(0.15f), 0.f, r.getY(), c.darker(0.4f), 0.f, r.getBottom(), false));
        g.fillRoundedRectangle(r, 5.f);
        g.setColour(accent.withAlpha(hi ? 1.f : 0.75f));
        g.drawRoundedRectangle(r, 5.f, 1.5f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hi, bool) override {
        auto r = b.getLocalBounds().toFloat();
        const float s = 14.f;
        juce::Rectangle<float> box(r.getX() + 2.f, r.getCentreY() - s * 0.5f, s, s);
        g.setColour(juce::Colour(0xff0d0a22)); g.fillRoundedRectangle(box, 3.f);
        if (b.getToggleState()) {
            g.setColour(accent.withAlpha(0.35f)); g.fillRoundedRectangle(box.expanded(3.f), 5.f);
            g.setColour(accent);                  g.fillRoundedRectangle(box.reduced(3.f), 2.f);
        }
        g.setColour(accent.withAlpha(hi ? 1.f : 0.7f)); g.drawRoundedRectangle(box, 3.f, 1.2f);
        g.setColour(b.findColour(juce::ToggleButton::textColourId));
        g.setFont(juce::Font(13.f));
        g.drawText(b.getButtonText(), r.withTrimmedLeft(s + 8.f), juce::Justification::centredLeft);
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override {
        auto r = juce::Rectangle<float>(0.f, 0.f, (float)w, (float)h).reduced(1.f);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId)); g.fillRoundedRectangle(r, 5.f);
        g.setColour(accent.withAlpha(0.8f)); g.drawRoundedRectangle(r, 5.f, 1.5f);
        juce::Path a; const float ax = (float)w - 14.f, ay = (float)h * 0.5f;
        a.addTriangle(ax - 4.f, ay - 2.f, ax + 4.f, ay - 2.f, ax, ay + 3.f);
        g.setColour(juce::Colours::white); g.fillPath(a);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(14.f); }
};
