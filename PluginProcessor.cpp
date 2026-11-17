#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
constexpr int wavetableSize = 2048;
constexpr int wavetableFrameCount = 4;
constexpr int maximumHarmonic = 32;
constexpr double nyquistSafetyFactor = 0.9;

class WavetableSet
{
public:
    WavetableSet()
    {
        for (int harmonicLimit = 1; harmonicLimit <= maximumHarmonic; ++harmonicLimit)
        {
            auto& band = bands[static_cast<size_t>(harmonicLimit - 1)];
            for (int frame = 0; frame < wavetableFrameCount; ++frame)
            {
                auto& table = band[static_cast<size_t>(frame)];
                float peak = 0.0f;

                for (int sample = 0; sample < wavetableSize; ++sample)
                {
                    const auto phase = juce::MathConstants<double>::twoPi
                                       * static_cast<double>(sample) / wavetableSize;
                    double value = 0.0;

                    for (int harmonic = 1; harmonic <= harmonicLimit; ++harmonic)
                    {
                        const auto weight = getHarmonicWeight(frame, harmonic);
                        value += weight * std::sin(phase * harmonic);
                    }

                    table[static_cast<size_t>(sample)] = static_cast<float>(value);
                    peak = juce::jmax(peak, std::abs(table[static_cast<size_t>(sample)]));
                }

                if (peak > 0.0f)
                    for (auto& value : table)
                        value /= peak;
            }
        }
    }

    float getSample(double phase, float position, double frequency, double sampleRate) const noexcept
    {
        const auto framePosition = juce::jlimit(0.0f, 1.0f, position)
                                   * static_cast<float>(wavetableFrameCount - 1);
        const auto firstFrame = juce::jmin(static_cast<int>(framePosition),
                                           wavetableFrameCount - 1);
        const auto secondFrame = juce::jmin(firstFrame + 1, wavetableFrameCount - 1);
        const auto frameBlend = framePosition - static_cast<float>(firstFrame);
        const auto samplePosition = phase * wavetableSize;
        const auto firstSample = static_cast<int>(samplePosition) % wavetableSize;
        const auto secondSample = (firstSample + 1) % wavetableSize;
        const auto sampleBlend = static_cast<float>(samplePosition - firstSample);

        const auto harmonicLimit = juce::jlimit(
            1.0, static_cast<double>(maximumHarmonic + 1),
            nyquistSafetyFactor * sampleRate * 0.5 / juce::jmax(1.0, frequency));
        // A partial begins fading in only after it has moved below the Nyquist guard.
        const auto bandPosition = juce::jlimit(
            0.0, static_cast<double>(maximumHarmonic - 1), harmonicLimit - 2.0);
        const auto firstBand = juce::jlimit(
            0, maximumHarmonic - 1, static_cast<int>(std::floor(bandPosition)));
        const auto secondBand = juce::jmin(firstBand + 1, maximumHarmonic - 1);
        const auto rawBandBlend = static_cast<float>(bandPosition - firstBand);
        const auto bandBlend = rawBandBlend * rawBandBlend * (3.0f - 2.0f * rawBandBlend);

        const auto firstBandValue = getBandFrameSample(
            bands[static_cast<size_t>(firstBand)], firstFrame, secondFrame,
            firstSample, secondSample, sampleBlend, frameBlend);
        const auto secondBandValue = getBandFrameSample(
            bands[static_cast<size_t>(secondBand)], firstFrame, secondFrame,
            firstSample, secondSample, sampleBlend, frameBlend);
        return firstBandValue + (secondBandValue - firstBandValue) * bandBlend;
    }

private:
    using Frame = std::array<float, wavetableSize>;
    using Band = std::array<Frame, wavetableFrameCount>;
    std::array<Band, maximumHarmonic> bands {};

    static float getBandFrameSample(const Band& band, int firstFrame, int secondFrame,
                                    int firstSample, int secondSample, float sampleBlend,
                                    float frameBlend) noexcept
    {
        const auto firstValue = interpolateSample(band[static_cast<size_t>(firstFrame)],
                                                   firstSample, secondSample, sampleBlend);
        const auto secondValue = interpolateSample(band[static_cast<size_t>(secondFrame)],
                                                    firstSample, secondSample, sampleBlend);
        return firstValue + (secondValue - firstValue) * frameBlend;
    }

    static double getHarmonicWeight(int frame, int harmonic) noexcept
    {
        switch (frame)
        {
            case 0: // Pure sine.
                return harmonic == 1 ? 1.0 : 0.0;
            case 1: // Soft triangle with alternating odd harmonics.
                if ((harmonic % 2) == 0)
                    return 0.0;
                return ((harmonic % 4) == 1 ? 1.0 : -1.0)
                       / static_cast<double>(harmonic * harmonic);
            case 2: // Saw with a gentle, band-limited harmonic roll-off.
                return (harmonic % 2 == 0 ? -1.0 : 1.0)
                       / static_cast<double>(harmonic);
            default: // Hollow square.
                return (harmonic % 2) == 0 ? 0.0 : 1.0 / static_cast<double>(harmonic);
        }
    }

    static float interpolateSample(const Frame& frame, int first, int second,
                                   float blend) noexcept
    {
        const auto a = frame[static_cast<size_t>(first)];
        const auto b = frame[static_cast<size_t>(second)];
        return a + (b - a) * blend;
    }
};

const WavetableSet& getWavetableSet()
{
    static const WavetableSet tables;
    return tables;
}

} // namespace

WavetableVoice::WavetableVoice(std::atomic<float>* wavePosition, std::atomic<float>* attack,
                               std::atomic<float>* decay, std::atomic<float>* sustain,
                               std::atomic<float>* release, std::atomic<float>* filterCutoff,
                               std::atomic<float>* filterResonance)
    : wavePositionParameter(wavePosition),
      attackParameter(attack),
      decayParameter(decay),
      sustainParameter(sustain),
      releaseParameter(release),
      filterCutoffParameter(filterCutoff),
      filterResonanceParameter(filterResonance)
{
    (void) getWavetableSet();
    lowPassFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
}

bool WavetableVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<WavetableSound*>(sound) != nullptr;
}

void WavetableVoice::startNote(int midiNoteNumber, float noteVelocity, juce::SynthesiserSound*,
                               int pitchWheelPosition)
{
    noteNumber = midiNoteNumber;
    pitchWheel = pitchWheelPosition;
    phase = 0.0;
    velocity = juce::jlimit(0.0f, 1.0f, noteVelocity);
    updateSoundParameters();
    amplitudeEnvelope.reset();
    amplitudeEnvelope.noteOn();
    lowPassFilter.reset();
    updateFrequency();
}

void WavetableVoice::stopNote(float, bool allowTailOff)
{
    if (!allowTailOff)
    {
        amplitudeEnvelope.reset();
        clearCurrentNote();
        velocity = 0.0f;
        return;
    }

    updateSoundParameters();
    amplitudeEnvelope.noteOff();
}

void WavetableVoice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchWheel = newPitchWheelValue;
    updateFrequency();
}

void WavetableVoice::controllerMoved(int, int) {}

void WavetableVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample,
                                     int numSamples)
{
    if (!isVoiceActive())
        return;

    updateSoundParameters();
    const auto wavePosition = wavePositionParameter->load(std::memory_order_relaxed);
    auto* left = outputBuffer.getWritePointer(0);
    auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1) : nullptr;
    const auto& tables = getWavetableSet();

    for (int sample = startSample; sample < startSample + numSamples; ++sample)
    {
        const auto envelope = amplitudeEnvelope.getNextSample();
        if (!amplitudeEnvelope.isActive())
        {
            clearCurrentNote();
            break;
        }

        const auto frequency = phaseIncrement * sampleRate;
        const auto oscillatorSample = tables.getSample(phase, wavePosition, frequency, sampleRate);
        const auto filteredSample = lowPassFilter.processSample(
            0, oscillatorSample * envelope * velocity);
        left[sample] += filteredSample;
        if (right != nullptr)
            right[sample] += filteredSample;

        phase += phaseIncrement;
        if (phase >= 1.0)
            phase -= std::floor(phase);
    }
}

void WavetableVoice::setCurrentPlaybackSampleRate(double newRate)
{
    sampleRate = juce::jmax(1.0, newRate);
    amplitudeEnvelope.setSampleRate(sampleRate);
    lowPassFilter.prepare(juce::dsp::ProcessSpec { sampleRate, 512, 1 });
    updateFrequency();
    updateSoundParameters();
}

void WavetableVoice::updateFrequency() noexcept
{
    const auto bendSemitones = (static_cast<double>(pitchWheel) - 8192.0) / 8192.0 * 2.0;
    const auto frequency = juce::MidiMessage::getMidiNoteInHertz(noteNumber)
                           * std::pow(2.0, bendSemitones / 12.0);
    phaseIncrement = frequency / sampleRate;
}

void WavetableVoice::updateSoundParameters() noexcept
{
    amplitudeEnvelope.setParameters({
        attackParameter->load(std::memory_order_relaxed),
        decayParameter->load(std::memory_order_relaxed),
        sustainParameter->load(std::memory_order_relaxed),
        releaseParameter->load(std::memory_order_relaxed)
    });

    const auto maximumCutoff = juce::jmax(20.0f, static_cast<float>(sampleRate * 0.45));
    const auto cutoff = juce::jlimit(20.0f, maximumCutoff,
                                     filterCutoffParameter->load(std::memory_order_relaxed));
    lowPassFilter.setCutoffFrequency(cutoff);
    lowPassFilter.setResonance(filterResonanceParameter->load(std::memory_order_relaxed));
}

juce::AudioProcessorValueTreeState::ParameterLayout
StageASynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "masterGain", 1 }, "Master Gain",
        juce::NormalisableRange<float> { -60.0f, 6.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wavePosition", 1 }, "Wavetable Position",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "attack", 1 }, "Attack",
        juce::NormalisableRange<float> { 0.001f, 5.0f, 0.001f }, 0.01f,
        juce::AudioParameterFloatAttributes().withLabel("s")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "decay", 1 }, "Decay",
        juce::NormalisableRange<float> { 0.001f, 5.0f, 0.001f }, 0.18f,
        juce::AudioParameterFloatAttributes().withLabel("s")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "sustain", 1 }, "Sustain",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.75f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "release", 1 }, "Release",
        juce::NormalisableRange<float> { 0.005f, 10.0f, 0.001f }, 0.25f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    juce::NormalisableRange<float> cutoffRange { 20.0f, 20000.0f, 1.0f };
    cutoffRange.setSkewForCentre(1000.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterCutoff", 1 }, "Filter Cutoff", cutoffRange, 12000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterResonance", 1 }, "Filter Resonance",
        juce::NormalisableRange<float> { 0.1f, 10.0f, 0.01f }, 0.707f,
        juce::AudioParameterFloatAttributes().withLabel("Q")));
    return layout;
}

StageASynthAudioProcessor::StageASynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    constexpr int voiceCount = 8;
    for (int voice = 0; voice < voiceCount; ++voice)
    {
        synthesiser.addVoice(new WavetableVoice(
            parameters.getRawParameterValue("wavePosition"),
            parameters.getRawParameterValue("attack"),
            parameters.getRawParameterValue("decay"),
            parameters.getRawParameterValue("sustain"),
            parameters.getRawParameterValue("release"),
            parameters.getRawParameterValue("filterCutoff"),
            parameters.getRawParameterValue("filterResonance")));
    }

    synthesiser.addSound(new WavetableSound());
    synthesiser.setNoteStealingEnabled(true);
}

void StageASynthAudioProcessor::prepareToPlay(double sampleRate, int)
{
    synthesiser.setCurrentPlaybackSampleRate(sampleRate);
    outputGain.reset(sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue("masterGain")->load()));
}

void StageASynthAudioProcessor::releaseResources() {}

bool StageASynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void StageASynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    synthesiser.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    const auto gainDb = parameters.getRawParameterValue("masterGain")->load();
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(gainDb));
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto gain = outputGain.getNextValue();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);
    }
}

juce::AudioProcessorEditor* StageASynthAudioProcessor::createEditor()
{
    return new StageASynthAudioProcessorEditor(*this);
}

void StageASynthAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (const auto state = parameters.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destinationData);
}

void StageASynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes < 9)
        return;

    const auto* bytes = static_cast<const juce::uint8*>(data);
    const auto xmlLength = juce::ByteOrder::littleEndianInt(bytes + 4);
    if (xmlLength == 0
        || xmlLength != static_cast<juce::uint32>(sizeInBytes - 9)
        || bytes[sizeInBytes - 1] != 0)
        return;

    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

#if defined(STAGE_A_SYNTH_TESTING)
int StageASynthAudioProcessor::getActiveVoiceCountForTesting() const noexcept
{
    auto activeVoices = 0;
    for (auto voiceIndex = 0; voiceIndex < synthesiser.getNumVoices(); ++voiceIndex)
        if (const auto* voice = synthesiser.getVoice(voiceIndex); voice != nullptr && voice->isVoiceActive())
            ++activeVoices;

    return activeVoices;
}

bool StageASynthAudioProcessor::isNotePlayingForTesting(int midiNoteNumber) const noexcept
{
    for (auto voiceIndex = 0; voiceIndex < synthesiser.getNumVoices(); ++voiceIndex)
        if (const auto* voice = synthesiser.getVoice(voiceIndex);
            voice != nullptr && voice->isVoiceActive()
            && voice->getCurrentlyPlayingNote() == midiNoteNumber)
            return true;

    return false;
}
#endif

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StageASynthAudioProcessor();
}
