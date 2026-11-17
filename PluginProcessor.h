#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class WavetableVoice final : public juce::SynthesiserVoice
{
public:
    WavetableVoice(std::atomic<float>* wavePosition, std::atomic<float>* attack,
                   std::atomic<float>* decay, std::atomic<float>* sustain,
                   std::atomic<float>* release, std::atomic<float>* filterCutoff,
                   std::atomic<float>* filterResonance);

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound,
                   int pitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample,
                         int numSamples) override;
    void setCurrentPlaybackSampleRate(double newRate) override;

private:
    void updateFrequency() noexcept;
    void updateSoundParameters() noexcept;

    double sampleRate = 44100.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float velocity = 0.0f;
    int noteNumber = 0;
    int pitchWheel = 8192;
    juce::ADSR amplitudeEnvelope;
    juce::dsp::StateVariableTPTFilter<float> lowPassFilter;

    std::atomic<float>* wavePositionParameter;
    std::atomic<float>* attackParameter;
    std::atomic<float>* decayParameter;
    std::atomic<float>* sustainParameter;
    std::atomic<float>* releaseParameter;
    std::atomic<float>* filterCutoffParameter;
    std::atomic<float>* filterResonanceParameter;
};

class WavetableSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class StageASynthAudioProcessor final : public juce::AudioProcessor
{
public:
    StageASynthAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ME - 43"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

#if defined(STAGE_A_SYNTH_TESTING)
    int getActiveVoiceCountForTesting() const noexcept;
    bool isNotePlayingForTesting(int midiNoteNumber) const noexcept;
#endif

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::Synthesiser synthesiser;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StageASynthAudioProcessor)
};
