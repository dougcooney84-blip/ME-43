#include "PluginEditor.h"

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

const juce::Colour ink { 17, 20, 28 };
const juce::Colour panel { 27, 32, 43 };
const juce::Colour panelHighlight { 34, 40, 52 };
const juce::Colour border { 56, 63, 77 };
const juce::Colour warmWhite { 244, 239, 230 };
const juce::Colour mutedText { 157, 166, 177 };
const juce::Colour mint { 111, 213, 188 };
const juce::Colour amber { 245, 183, 113 };
const juce::Colour coral { 240, 143, 126 };
}

StageASynthAudioProcessorEditor::StageASynthAudioProcessorEditor(
    StageASynthAudioProcessor& audioProcessor)
    : AudioProcessorEditor(&audioProcessor), processor(audioProcessor)
{
    setSize(760, 520);

    titleLabel.setText("ME - 43", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, warmWhite);
    addAndMakeVisible(titleLabel);

    for (size_t index = 0; index < controls.size(); ++index)
    {
        auto& control = controls[index];
        const auto& definition = controlDefinitions[index];

        control.label.setText(definition.label, juce::dontSendNotification);
        control.label.setJustificationType(juce::Justification::centred);
        control.label.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        control.label.setColour(juce::Label::textColourId, mutedText);
        addAndMakeVisible(control.label);

        control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 104, 23);
        control.slider.setTextValueSuffix(definition.suffix);
        control.slider.setNumDecimalPlacesToDisplay(index == 1 ? 0 : 2);
        const auto accent = index == 0 ? mint : (index == 3 ? amber : coral);
        control.slider.setColour(juce::Slider::rotarySliderFillColourId,
                                 accent);
        control.slider.setColour(juce::Slider::rotarySliderOutlineColourId,
                                 border);
        control.slider.setColour(juce::Slider::thumbColourId,
                                 warmWhite);
        control.slider.setColour(juce::Slider::textBoxTextColourId, warmWhite);
        control.slider.setColour(juce::Slider::textBoxBackgroundColourId, panelHighlight);
        control.slider.setColour(juce::Slider::textBoxOutlineColourId, border);
        addAndMakeVisible(control.slider);

        control.attachment =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.parameters, definition.parameterId, control.slider);
    }
}

void StageASynthAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    juce::ColourGradient background(juce::Colour { 30, 35, 47 }, 0.0f, 0.0f,
                                    ink, static_cast<float>(getWidth()),
                                    static_cast<float>(getHeight()), false);
    graphics.setGradientFill(background);
    graphics.fillRect(getLocalBounds());

    graphics.setColour(mint.withAlpha(0.045f));
    graphics.fillEllipse(static_cast<float>(getWidth() - 245), -105.0f, 300.0f, 230.0f);

    const auto bounds = getLocalBounds().toFloat();
    graphics.setColour(border.withAlpha(0.72f));
    graphics.drawRoundedRectangle(bounds.reduced(10.0f), 16.0f, 1.0f);

    graphics.setColour(mutedText);
    graphics.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    graphics.drawText("HOUSE + TECHNO  /  WAVETABLE INSTRUMENT",
                      juce::Rectangle<int>(30, 56, 420, 18),
                      juce::Justification::centredLeft);

    const juce::Rectangle<float> badge(static_cast<float>(getWidth() - 226),
                                       28.0f, 188.0f, 28.0f);
    graphics.setColour(panelHighlight);
    graphics.fillRoundedRectangle(badge, 14.0f);
    graphics.setColour(amber);
    graphics.fillEllipse(badge.getX() + 11.0f, badge.getY() + 10.0f, 8.0f, 8.0f);
    graphics.setColour(warmWhite);
    graphics.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    graphics.drawText("STAGE B  /  8 VOICES",
                      badge.withTrimmedLeft(27.0f).toNearestInt(),
                      juce::Justification::centredLeft);

    const auto contentX = 26.0f;
    const auto contentWidth = static_cast<float>(getWidth()) - 52.0f;
    const juce::Rectangle<float> soundPanel(contentX, 96.0f, contentWidth, 193.0f);
    const juce::Rectangle<float> envelopePanel(contentX, 302.0f, contentWidth, 190.0f);

    graphics.setColour(panel.withAlpha(0.98f));
    graphics.fillRoundedRectangle(soundPanel, 14.0f);
    graphics.fillRoundedRectangle(envelopePanel, 14.0f);
    graphics.setColour(border.withAlpha(0.78f));
    graphics.drawRoundedRectangle(soundPanel, 14.0f, 1.0f);
    graphics.drawRoundedRectangle(envelopePanel, 14.0f, 1.0f);

    graphics.setColour(mint);
    graphics.fillRoundedRectangle(
        juce::Rectangle<float>(contentX + 1.0f, 112.0f, 3.0f, 15.0f), 1.5f);
    graphics.setColour(amber);
    graphics.fillRoundedRectangle(
        juce::Rectangle<float>(contentX + 1.0f, 318.0f, 3.0f, 15.0f), 1.5f);

    graphics.setColour(warmWhite);
    graphics.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    graphics.drawText("OSCILLATOR  /  FILTER  /  OUTPUT", 38, 108, 340, 22,
                      juce::Justification::centredLeft);
    graphics.drawText("AMPLITUDE ENVELOPE", 38, 314, 280, 22,
                      juce::Justification::centredLeft);

    graphics.setColour(border.withAlpha(0.62f));
    graphics.drawLine(38.0f, 132.0f, static_cast<float>(getWidth() - 38), 132.0f, 1.0f);
    graphics.drawLine(38.0f, 338.0f, static_cast<float>(getWidth() - 38), 338.0f, 1.0f);
}

void StageASynthAudioProcessorEditor::resized()
{
    titleLabel.setBounds(30, 19, 310, 36);

    const auto margin = 34;
    const auto gap = 12;
    const auto columnWidth = (getWidth() - margin * 2 - gap * 3) / 4;
    constexpr int labelTop[] { 135, 341 };
    for (size_t index = 0; index < controls.size(); ++index)
    {
        const auto row = index / 4;
        const auto column = index % 4;
        const auto x = margin + static_cast<int>(column) * (columnWidth + gap);
        auto& control = controls[index];
        control.label.setBounds(x, labelTop[row], columnWidth, 19);
        control.slider.setBounds(x, labelTop[row] + 19, columnWidth, 132);
    }
}
