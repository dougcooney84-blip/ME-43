#pragma once

#include "PluginProcessor.h"

#include <array>

class StageASynthAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit StageASynthAudioProcessorEditor(StageASynthAudioProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    StageASynthAudioProcessor& processor;
    std::array<Control, 8> controls;
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StageASynthAudioProcessorEditor)
};
