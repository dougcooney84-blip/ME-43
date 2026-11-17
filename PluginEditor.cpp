#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
struct ControlDefinition
{
    const char* parameterId;
    const char* label;
    const char* suffix;
};

constexpr std::array<ControlDefinition, 8> controlDefinitions {{
    { "wavePosition", "WT POSITION", "" },
    { "filterCutoff", "CUTOFF", " Hz" },
    { "filterResonance", "RESONANCE", " Q" },
    { "masterGain", "MASTER", " dB" },
    { "attack", "ATTACK", " s" },
    { "decay", "DECAY", " s" },
    { "sustain", "SUSTAIN", "" },
    { "release", "RELEASE", " s" }
}};

// Dark stream palette
const juce::Colour bodyTop { 8, 7, 9 };
const juce::Colour bodyBottom { 22, 13, 15 };
const juce::Colour panelTop { 38, 29, 33 };
const juce::Colour panelBottom { 22, 17, 20 };
const juce::Colour edge { 78, 62, 66 };
const juce::Colour screenBg { 7, 15, 15 };
const juce::Colour screenGrid { 36, 92, 86 };
const juce::Colour cream { 240, 228, 210 };
const juce::Colour creamDim { 160, 148, 134 };
const juce::Colour teal { 52, 190, 166 };
const juce::Colour coral { 236, 92, 70 };
const juce::Colour pink { 208, 160, 206 };
const juce::Colour amber { 232, 172, 92 };

constexpr int kWidth = 820;
constexpr int kHeight = 580;

juce::Rectangle<int> oscPanel()    { return { 18, 92, 318, 252 }; }
juce::Rectangle<int> filterPanel() { return { 348, 92, 236, 252 }; }
juce::Rectangle<int> outputPanel() { return { 596, 92, 206, 252 }; }
juce::Rectangle<int> envPanel()    { return { 18, 356, 784, 206 }; }

juce::Rectangle<int> topScreen(juce::Rectangle<int> panel)
{
    return panel.withTrimmedTop(38).withHeight(60).reduced(14, 0);
}

juce::Rectangle<int> envScreen()
{
    return envPanel().withTrimmedTop(40).withTrimmedLeft(16).withWidth(360).withTrimmedBottom(14);
}

class RetroLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        const auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                 static_cast<float>(width), static_cast<float>(height))
                              .reduced(4.0f);
        const auto radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
        const auto centre = area.getCentre();
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const auto angle = startAngle + sliderPos * (endAngle - startAngle);

        auto polar = [&](float a, float r)
        {
            return juce::Point<float>(centre.x + r * std::sin(a), centre.y - r * std::cos(a));
        };

        // tick marks
        for (int i = 0; i <= 10; ++i)
        {
            const auto a = startAngle + (endAngle - startAngle) * static_cast<float>(i) / 10.0f;
            const bool major = (i % 5 == 0);
            g.setColour(cream.withAlpha(major ? 0.75f : 0.40f));
            g.drawLine(juce::Line<float>(polar(a, radius * 0.88f), polar(a, radius)),
                       major ? 1.8f : 1.0f);
        }

        // value track
        const auto arcRadius = radius * 0.78f;
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            startAngle, endAngle, true);
        g.setColour(juce::Colour(52, 40, 44));
        g.strokePath(track, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            startAngle, angle, true);
        g.setColour(accent);
        g.strokePath(value, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // knob body
        const auto bodyRadius = radius * 0.62f;
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius + 3.0f,
                      bodyRadius * 2.0f, bodyRadius * 2.0f);

        g.setGradientFill(juce::ColourGradient(juce::Colour(84, 72, 74),
                                               centre.x - bodyRadius * 0.4f,
                                               centre.y - bodyRadius * 0.5f,
                                               juce::Colour(20, 16, 18),
                                               centre.x + bodyRadius,
                                               centre.y + bodyRadius, true));
        g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius,
                      bodyRadius * 2.0f, bodyRadius * 2.0f);
        g.setColour(juce::Colour(8, 6, 7));
        g.drawEllipse(centre.x - bodyRadius, centre.y - bodyRadius,
                      bodyRadius * 2.0f, bodyRadius * 2.0f, 1.5f);

        // pointer
        g.setColour(cream);
        g.drawLine(juce::Line<float>(polar(angle, bodyRadius * 0.25f),
                                     polar(angle, bodyRadius * 0.92f)), 2.4f);
    }
};

RetroLookAndFeel& getRetroLookAndFeel()
{
    static RetroLookAndFeel instance;
    return instance;
}

void drawFlow(juce::Graphics& g, int width, float alpha)
{
    const juce::Colour colours[] { teal, coral, pink };
    for (int i = 0; i < 26; ++i)
    {
        juce::Path p;
        const float offset = static_cast<float>(i) * 34.0f - 300.0f;
        for (int x = 0; x <= width; x += 6)
        {
            const float fx = static_cast<float>(x);
            const float fi = static_cast<float>(i);
            const float y = offset + fx * 0.32f
                          + 16.0f * std::sin(fx * 0.011f + fi * 0.6f)
                          + 9.0f * std::sin(fx * 0.027f - fi * 0.4f);
            if (x == 0)
                p.startNewSubPath(fx, y);
            else
                p.lineTo(fx, y);
        }
        g.setColour(colours[i % 3].withAlpha(alpha));
        g.strokePath(p, juce::PathStrokeType(1.4f));
    }
}

void drawScrew(juce::Graphics& g, juce::Point<float> p)
{
    g.setColour(juce::Colour(10, 8, 9));
    g.fillEllipse(p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
    g.setColour(edge.brighter(0.3f));
    g.drawEllipse(p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, 1.0f);
    g.drawLine(p.x - 2.5f, p.y - 1.0f, p.x + 2.5f, p.y + 1.0f, 1.0f);
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
               juce::Colour accent)
{
    const auto r = area.toFloat();
    g.setGradientFill(juce::ColourGradient::vertical(panelTop.withAlpha(0.93f), r.getY(),
                                                     panelBottom.withAlpha(0.93f), r.getBottom()));
    g.fillRoundedRectangle(r, 8.0f);

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawLine(r.getX() + 8.0f, r.getY() + 1.5f, r.getRight() - 8.0f, r.getY() + 1.5f, 1.0f);
    g.setColour(edge);
    g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);

    const float inset = 9.0f;
    drawScrew(g, { r.getX() + inset, r.getY() + inset });
    drawScrew(g, { r.getRight() - inset, r.getY() + inset });
    drawScrew(g, { r.getX() + inset, r.getBottom() - inset });
    drawScrew(g, { r.getRight() - inset, r.getBottom() - inset });

    g.setColour(accent);
    g.fillEllipse(r.getX() + 24.0f, r.getY() + 16.0f, 7.0f, 7.0f);
    g.setColour(cream);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText(title, area.getX() + 38, area.getY() + 8, area.getWidth() - 70, 22,
               juce::Justification::centredLeft);

    g.setColour(edge.withAlpha(0.8f));
    g.drawLine(r.getX() + 20.0f, r.getY() + 33.0f, r.getRight() - 20.0f, r.getY() + 33.0f, 1.0f);
}

void drawScreen(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto r = area.toFloat();
    g.setColour(screenBg);
    g.fillRoundedRectangle(r, 5.0f);

    g.setColour(screenGrid.withAlpha(0.35f));
    for (int i = 1; i < 4; ++i)
    {
        const float gx = r.getX() + r.getWidth() * static_cast<float>(i) / 4.0f;
        g.drawLine(gx, r.getY() + 4.0f, gx, r.getBottom() - 4.0f, 1.0f);
    }
    g.drawLine(r.getX() + 4.0f, r.getCentreY(), r.getRight() - 4.0f, r.getCentreY(), 1.0f);

    g.setColour(edge);
    g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.3f);
}

float shapeSample(int shape, float t)
{
    switch (shape)
    {
        case 0:  return std::sin(juce::MathConstants<float>::twoPi * t);
        case 1:  return 1.0f - 4.0f * std::abs(t - 0.5f);
        case 2:  return 2.0f * t - 1.0f;
        default: return t < 0.5f ? 1.0f : -1.0f;
    }
}

void drawWaveDisplay(juce::Graphics& g, juce::Rectangle<int> area, float position)
{
    drawScreen(g, area);
    const auto r = area.toFloat().reduced(8.0f, 8.0f);

    const float pos = juce::jlimit(0.0f, 1.0f, position) * 3.0f;
    const int shapeIndex = juce::jmin(static_cast<int>(pos), 2);
    const float frac = pos - static_cast<float>(shapeIndex);

    juce::Path path;
    const int points = juce::jmax(2, static_cast<int>(r.getWidth()));
    for (int i = 0; i <= points; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(points);
        float t = 2.0f * u;
        t -= std::floor(t);
        const float s = (1.0f - frac) * shapeSample(shapeIndex, t)
                      + frac * shapeSample(shapeIndex + 1, t);
        const float px = r.getX() + u * r.getWidth();
        const float py = r.getCentreY() - s * r.getHeight() * 0.5f;
        if (i == 0)
            path.startNewSubPath(px, py);
        else
            path.lineTo(px, py);
    }
    g.setColour(coral.withAlpha(0.25f));
    g.strokePath(path, juce::PathStrokeType(5.0f));
    g.setColour(coral);
    g.strokePath(path, juce::PathStrokeType(1.8f));
}

void drawFilterDisplay(juce::Graphics& g, juce::Rectangle<int> area, float cutoff, float q)
{
    drawScreen(g, area);
    const auto r = area.toFloat().reduced(6.0f, 6.0f);
    const float fc = juce::jmax(20.0f, cutoff);
    const float qq = juce::jmax(0.1f, q);

    juce::Path curve;
    const int points = juce::jmax(2, static_cast<int>(r.getWidth()));
    for (int i = 0; i <= points; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(points);
        const float f = 20.0f * std::pow(1000.0f, u);
        const float ratio = f / fc;
        const float a = 1.0f - ratio * ratio;
        const float b = ratio / qq;
        const float mag = 1.0f / std::sqrt(a * a + b * b);
        const float db = juce::jlimit(-30.0f, 20.0f, 20.0f * std::log10(mag));
        const float norm = (db + 30.0f) / 50.0f;
        const float px = r.getX() + u * r.getWidth();
        const float py = r.getBottom() - norm * r.getHeight();
        if (i == 0)
            curve.startNewSubPath(px, py);
        else
            curve.lineTo(px, py);
    }

    juce::Path fill(curve);
    fill.lineTo(r.getRight(), r.getBottom());
    fill.lineTo(r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setColour(teal.withAlpha(0.18f));
    g.fillPath(fill);
    g.setColour(teal);
    g.strokePath(curve, juce::PathStrokeType(1.8f));
}

void drawEnvelopeDisplay(juce::Graphics& g, juce::Rectangle<int> area,
                         float attackN, float decayN, float sustain, float releaseN)
{
    drawScreen(g, area);
    const auto r = area.toFloat().reduced(12.0f, 14.0f);
    const float maxWidth = r.getWidth();

    const float aw = maxWidth * (0.04f + 0.19f * attackN);
    const float dw = maxWidth * (0.04f + 0.19f * decayN);
    const float hw = maxWidth * 0.15f;
    const float rw = maxWidth * (0.04f + 0.19f * releaseN);

    const float yBottom = r.getBottom();
    const float yTop = r.getY();
    const float yS = yBottom - juce::jlimit(0.0f, 1.0f, sustain) * (yBottom - yTop);

    const float x0 = r.getX();
    const float x1 = x0 + aw;
    const float x2 = x1 + dw;
    const float x3 = x2 + hw;
    const float x4 = x3 + rw;

    juce::Path line;
    line.startNewSubPath(x0, yBottom);
    line.lineTo(x1, yTop);
    line.lineTo(x2, yS);
    line.lineTo(x3, yS);
    line.lineTo(x4, yBottom);

    juce::Path fill(line);
    fill.closeSubPath();
    g.setColour(teal.withAlpha(0.18f));
    g.fillPath(fill);

    g.setColour(coral.withAlpha(0.25f));
    g.strokePath(line, juce::PathStrokeType(5.0f));
    g.setColour(coral);
    g.strokePath(line, juce::PathStrokeType(1.8f));

    g.setColour(cream);
    for (float px : { x1, x2, x3 })
        g.fillEllipse(px - 2.5f, (px == x1 ? yTop : yS) - 2.5f, 5.0f, 5.0f);
}
} // namespace

StageASynthAudioProcessorEditor::StageASynthAudioProcessorEditor(
    StageASynthAudioProcessor& audioProcessor)
    : AudioProcessorEditor(&audioProcessor), processor(audioProcessor)
{
    setSize(kWidth, kHeight);

    titleLabel.setText("ME - 43", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(36.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, cream);
    addAndMakeVisible(titleLabel);

    const juce::Colour accents[] { teal, coral, pink, amber, pink, pink, pink, pink };

    for (size_t index = 0; index < controls.size(); ++index)
    {
        auto& control = controls[index];
        const auto& definition = controlDefinitions[index];

        control.label.setText(definition.label, juce::dontSendNotification);
        control.label.setJustificationType(juce::Justification::centred);
        control.label.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        control.label.setColour(juce::Label::textColourId, creamDim);
        addAndMakeVisible(control.label);

        control.slider.setLookAndFeel(&getRetroLookAndFeel());
        control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
        control.slider.setTextValueSuffix(definition.suffix);
        control.slider.setNumDecimalPlacesToDisplay(index == 1 ? 0 : 2);
        control.slider.setColour(juce::Slider::rotarySliderFillColourId, accents[index]);
        control.slider.setColour(juce::Slider::textBoxTextColourId, cream);
        control.slider.setColour(juce::Slider::textBoxBackgroundColourId, screenBg);
        control.slider.setColour(juce::Slider::textBoxOutlineColourId, edge);
        control.slider.onValueChange = [this]
        {
            repaint(topScreen(oscPanel()));
            repaint(topScreen(filterPanel()));
            repaint(envScreen());
        };
        addAndMakeVisible(control.slider);

        control.attachment =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.parameters, definition.parameterId, control.slider);
    }
}

void StageASynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    // background
    g.setGradientFill(juce::ColourGradient::vertical(bodyTop, 0.0f, bodyBottom,
                                                     static_cast<float>(getHeight())));
    g.fillRect(getLocalBounds());

    drawFlow(g, getWidth(), 0.09f);
    g.saveState();
    g.reduceClipRegion(0, 0, getWidth(), 84);
    drawFlow(g, getWidth(), 0.22f);
    g.restoreState();

    // header
    g.setColour(teal);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText("MULTI ENGINE SYNTHESIZER", 30, 58, 300, 16, juce::Justification::centredLeft);
    g.setColour(cream.withAlpha(0.8f));
    g.drawText("HOUSE + TECHNO / WAVETABLE", getWidth() - 320, 30, 296, 18,
               juce::Justification::centredRight);
    g.setColour(coral.withAlpha(0.6f));
    g.drawLine(18.0f, 80.0f, static_cast<float>(getWidth() - 18), 80.0f, 1.2f);

    // panels
    drawPanel(g, oscPanel(), "OSCILLATOR", teal);
    drawPanel(g, filterPanel(), "FILTER", coral);
    drawPanel(g, outputPanel(), "OUTPUT", amber);
    drawPanel(g, envPanel(), "AMPLITUDE ENVELOPE", pink);

    // displays
    drawWaveDisplay(g, topScreen(oscPanel()),
                    static_cast<float>(controls[0].slider.getValue()));
    drawFilterDisplay(g, topScreen(filterPanel()),
                      static_cast<float>(controls[1].slider.getValue()),
                      static_cast<float>(controls[2].slider.getValue()));

    const auto plate = topScreen(outputPanel());
    drawScreen(g, plate);
    g.setColour(amber);
    g.fillEllipse(static_cast<float>(plate.getX() + 14), static_cast<float>(plate.getCentreY() - 4),
                  8.0f, 8.0f);
    g.setColour(cream);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText("8-VOICE POLY", plate.withTrimmedLeft(30), juce::Justification::centredLeft);

    drawEnvelopeDisplay(g, envScreen(),
                        static_cast<float>(controls[4].slider.valueToProportionOfLength(
                            controls[4].slider.getValue())),
                        static_cast<float>(controls[5].slider.valueToProportionOfLength(
                            controls[5].slider.getValue())),
                        static_cast<float>(controls[6].slider.getValue()),
                        static_cast<float>(controls[7].slider.valueToProportionOfLength(
                            controls[7].slider.getValue())));
}

void StageASynthAudioProcessorEditor::resized()
{
    titleLabel.setBounds(26, 14, 300, 44);

    auto place = [this](size_t index, juce::Rectangle<int> column)
    {
        controls[index].label.setBounds(column.removeFromTop(16));
        controls[index].slider.setBounds(column);
    };

    // Oscillator, filter, output
    place(0, { oscPanel().getCentreX() - 65, 198, 130, 142 });
    place(1, { filterPanel().getX() + 8, 198, 104, 142 });
    place(2, { filterPanel().getX() + 124, 198, 104, 142 });
    place(3, { outputPanel().getCentreX() - 65, 198, 130, 142 });

    // Envelope: attack, decay, sustain, release
    for (size_t i = 0; i < 4; ++i)
        place(4 + i, { 410 + static_cast<int>(i) * 95, 396, 95, 148 });
}
