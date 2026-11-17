#include "../Source/PluginProcessor.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
int failures = 0;

void expect(bool condition, const std::string& description)
{
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << description << '\n';
}

void setParameter(StageASynthAudioProcessor& processor, const juce::String& parameterId,
                  float value)
{
    if (auto* parameter = processor.parameters.getParameter(parameterId))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    else
        expect(false, "parameter exists: " + parameterId.toStdString());
}

float getParameter(const StageASynthAudioProcessor& processor, const juce::String& parameterId)
{
    const auto* value = processor.parameters.getRawParameterValue(parameterId);
    return value != nullptr ? value->load() : std::numeric_limits<float>::quiet_NaN();
}

void prepare(StageASynthAudioProcessor& processor)
{
    processor.prepareToPlay(sampleRate, 512);
}

juce::AudioBuffer<float> render(StageASynthAudioProcessor& processor, int sampleCount,
                                juce::MidiBuffer midi = {})
{
    juce::AudioBuffer<float> buffer(2, sampleCount);
    processor.processBlock(buffer, midi);
    return buffer;
}

juce::MidiBuffer withMessage(const juce::MidiMessage& message)
{
    juce::MidiBuffer midi;
    midi.addEvent(message, 0);
    return midi;
}

double rms(const juce::AudioBuffer<float>& buffer, int channel = 0)
{
    double sum = 0.0;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = buffer.getSample(channel, sample);
        sum += static_cast<double>(value) * value;
    }

    return std::sqrt(sum / juce::jmax(1, buffer.getNumSamples()));
}

bool isFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(buffer.getSample(channel, sample)))
                return false;

    return true;
}

double estimateFrequency(const juce::AudioBuffer<float>& buffer, double rate)
{
    std::vector<double> crossings;
    for (int sample = 1; sample < buffer.getNumSamples(); ++sample)
    {
        const auto previous = static_cast<double>(buffer.getSample(0, sample - 1));
        const auto current = static_cast<double>(buffer.getSample(0, sample));
        if (previous <= 0.0 && current > 0.0)
        {
            const auto fraction = -previous / (current - previous);
            crossings.push_back(static_cast<double>(sample - 1) + fraction);
        }
    }

    if (crossings.size() < 2)
        return 0.0;

    return static_cast<double>(crossings.size() - 1) * rate
           / (crossings.back() - crossings.front());
}

double measureToneAmplitude(const juce::AudioBuffer<float>& buffer, double frequency, double rate)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi * frequency * sample / rate;
        const auto value = static_cast<double>(buffer.getSample(0, sample));
        real += value * std::cos(phase);
        imaginary -= value * std::sin(phase);
    }

    return 2.0 * std::sqrt(real * real + imaginary * imaginary)
           / static_cast<double>(buffer.getNumSamples());
}

void setSustainedSineParameters(StageASynthAudioProcessor& processor)
{
    setParameter(processor, "masterGain", 0.0f);
    setParameter(processor, "wavePosition", 0.0f);
    setParameter(processor, "attack", 0.001f);
    setParameter(processor, "decay", 0.001f);
    setParameter(processor, "sustain", 1.0f);
    setParameter(processor, "release", 0.05f);
    setParameter(processor, "filterCutoff", 20000.0f);
    setParameter(processor, "filterResonance", 0.707f);
}

void testNoteOnOffAndReleaseCompletion()
{
    StageASynthAudioProcessor processor;
    setSustainedSineParameters(processor);
    prepare(processor);

    auto onset = render(processor, 512, withMessage(juce::MidiMessage::noteOn(1, 69, 1.0f)));
    expect(rms(onset) > 0.05, "note-on produces audible output");
    expect(processor.getActiveVoiceCountForTesting() == 1, "note-on activates one voice");

    auto release = render(processor, 512, withMessage(juce::MidiMessage::noteOff(1, 69)));
    expect(rms(release) > 0.001, "note-off enters a non-zero release tail");
    expect(processor.getActiveVoiceCountForTesting() == 1, "voice remains active during release");

    render(processor, 6144);
    const auto settled = render(processor, 512);
    expect(rms(settled) < 1.0e-7, "released note becomes silent after its tail");
    expect(processor.getActiveVoiceCountForTesting() == 0, "voice is cleared after release completes");
}

void testPitchBend()
{
    StageASynthAudioProcessor processor;
    setSustainedSineParameters(processor);
    prepare(processor);

    juce::MidiBuffer note;
    note.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
    render(processor, 512, note);

    const auto before = render(processor, 16384);
    const auto beforeFrequency = estimateFrequency(before, sampleRate);
    const auto bent = render(processor, 512, withMessage(juce::MidiMessage::pitchWheel(1, 16383)));
    expect(isFinite(bent), "pitch bend keeps output finite");
    const auto after = render(processor, 16384);
    const auto afterFrequency = estimateFrequency(after, sampleRate);
    const auto ratio = afterFrequency / beforeFrequency;
    expect(beforeFrequency > 435.0 && beforeFrequency < 445.0,
           "A4 note renders near 440 Hz");
    expect(ratio > 1.11 && ratio < 1.14, "maximum pitch bend raises pitch by about two semitones");
}

void testHighRegisterWavetableSuppressesFoldback()
{
    StageASynthAudioProcessor processor;
    setSustainedSineParameters(processor);
    setParameter(processor, "wavePosition", 0.667f);
    prepare(processor);

    constexpr int midiNote = 96;
    const auto fundamental = juce::MidiMessage::getMidiNoteInHertz(midiNote);
    render(processor, 512, withMessage(juce::MidiMessage::noteOn(1, midiNote, 1.0f)));
    const auto audio = render(processor, 8192);

    const auto fundamentalAmplitude = measureToneAmplitude(audio, fundamental, sampleRate);
    const auto twelfthHarmonicAlias = sampleRate - 12.0 * fundamental;
    const auto aliasAmplitude = measureToneAmplitude(audio, twelfthHarmonicAlias, sampleRate);
    expect(isFinite(audio), "high-register wavetable output remains finite");
    expect(fundamentalAmplitude > 0.05, "high-register bright wavetable remains audible");
    expect(aliasAmplitude < fundamentalAmplitude * 0.02,
           "harmonics above the pitch-aware table limit do not fold back strongly");

    StageASynthAudioProcessor topNoteProcessor;
    setSustainedSineParameters(topNoteProcessor);
    setParameter(topNoteProcessor, "wavePosition", 1.0f);
    prepare(topNoteProcessor);
    const auto topNote = render(topNoteProcessor, 1024,
                                withMessage(juce::MidiMessage::noteOn(1, 127, 1.0f)));
    expect(isFinite(topNote), "bright wavetable remains stable at the top MIDI note");
}

void testEightVoiceAllocationAndStealing()
{
    StageASynthAudioProcessor processor;
    setSustainedSineParameters(processor);
    prepare(processor);

    juce::MidiBuffer firstEight;
    for (int note = 48; note < 56; ++note)
        firstEight.addEvent(juce::MidiMessage::noteOn(1, note, 0.8f), 0);

    const auto eightVoiceAudio = render(processor, 512, firstEight);
    expect(processor.getActiveVoiceCountForTesting() == 8, "all eight voices allocate");
    expect(rms(eightVoiceAudio) > 0.01, "eight allocated voices render audio");

    const auto ninth = render(processor, 512, withMessage(juce::MidiMessage::noteOn(1, 72, 1.0f)));
    expect(processor.getActiveVoiceCountForTesting() == 8, "voice count remains capped at eight");
    expect(processor.isNotePlayingForTesting(72), "ninth note takes a voice through stealing");

    auto remainingOriginalNotes = 0;
    for (int note = 48; note < 56; ++note)
        if (processor.isNotePlayingForTesting(note))
            ++remainingOriginalNotes;
    expect(remainingOriginalNotes == 7, "stealing replaces one of the original eight notes");
    expect(isFinite(ninth) && rms(ninth) > 0.001, "stolen-voice block remains finite and audible");
}

void testGainSmoothing()
{
    StageASynthAudioProcessor changing;
    StageASynthAudioProcessor reference;
    setSustainedSineParameters(changing);
    setSustainedSineParameters(reference);
    prepare(changing);
    prepare(reference);

    render(changing, 512, withMessage(juce::MidiMessage::noteOn(1, 69, 1.0f)));
    render(reference, 512, withMessage(juce::MidiMessage::noteOn(1, 69, 1.0f)));
    render(changing, 4096);
    render(reference, 4096);
    setParameter(changing, "masterGain", -60.0f);

    std::vector<double> gainRatios;
    for (int block = 0; block < 10; ++block)
    {
        const auto changedAudio = render(changing, 128);
        const auto referenceAudio = render(reference, 128);
        gainRatios.push_back(rms(changedAudio) / rms(referenceAudio));
    }

    expect(gainRatios.front() > 0.65, "gain change does not jump immediately to its target");
    expect(gainRatios[3] < gainRatios.front() * 0.7,
           "gain decreases progressively during the smoothing ramp");
    expect(gainRatios.back() < 0.002, "gain reaches the -60 dB target after the ramp");
}

void testParameterStateSaveRestore()
{
    StageASynthAudioProcessor source;
    setParameter(source, "masterGain", -7.25f);
    setParameter(source, "wavePosition", 0.73f);
    setParameter(source, "attack", 0.42f);
    setParameter(source, "decay", 1.12f);
    setParameter(source, "sustain", 0.38f);
    setParameter(source, "release", 2.4f);
    setParameter(source, "filterCutoff", 8432.0f);
    setParameter(source, "filterResonance", 2.3f);

    juce::MemoryBlock state;
    source.getStateInformation(state);
    expect(state.getSize() > 0, "parameter state serializes to a non-empty block");

    StageASynthAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const juce::StringArray ids {
        "masterGain", "wavePosition", "attack", "decay",
        "sustain", "release", "filterCutoff", "filterResonance"
    };
    for (const auto& id : ids)
        expect(std::abs(getParameter(source, id) - getParameter(restored, id)) < 0.02f,
               "saved parameter restores: " + id.toStdString());
}

void expectDefaultParameters(const StageASynthAudioProcessor& processor,
                             const std::string& description)
{
    const juce::StringArray ids {
        "masterGain", "wavePosition", "attack", "decay",
        "sustain", "release", "filterCutoff", "filterResonance"
    };
    constexpr std::array<float, 8> defaults {
        0.0f, 0.0f, 0.01f, 0.18f, 0.75f, 0.25f, 12000.0f, 0.707f
    };

    for (int index = 0; index < ids.size(); ++index)
    {
        const auto value = getParameter(processor, ids[index]);
        expect(std::isfinite(value)
                   && std::abs(value - defaults[static_cast<size_t>(index)]) < 0.02f,
               description + ": default remains valid for " + ids[index].toStdString());
    }
}

void expectProcessorUsableAfterInvalidState(StageASynthAudioProcessor& processor,
                                            const std::string& description)
{
    expectDefaultParameters(processor, description);
    prepare(processor);
    const auto audio = render(processor, 1024,
                              withMessage(juce::MidiMessage::noteOn(1, 69, 1.0f)));
    expect(isFinite(audio), description + ": processor output remains finite");
    expect(rms(audio) > 0.01, description + ": processor still renders a note");
}

void testInvalidStateDoesNotReplaceDefaults()
{
    StageASynthAudioProcessor truncatedStateProcessor;
    StageASynthAudioProcessor stateSource;
    setParameter(stateSource, "wavePosition", 0.73f);
    juce::MemoryBlock truncatedState;
    stateSource.getStateInformation(truncatedState);
    expect(truncatedState.getSize() > 1, "serialized state has bytes to truncate");
    truncatedState.setSize(truncatedState.getSize() - 1);
    truncatedStateProcessor.setStateInformation(
        truncatedState.getData(), static_cast<int>(truncatedState.getSize()));
    expectProcessorUsableAfterInvalidState(truncatedStateProcessor, "truncated binary state");

    StageASynthAudioProcessor unrelatedStateProcessor;
    juce::XmlElement unrelatedXml("UNRELATED_STATE");
    unrelatedXml.setAttribute("wavePosition", 0.73);
    juce::MemoryBlock unrelatedState;
    juce::AudioProcessor::copyXmlToBinary(unrelatedXml, unrelatedState);
    unrelatedStateProcessor.setStateInformation(
        unrelatedState.getData(), static_cast<int>(unrelatedState.getSize()));
    expectProcessorUsableAfterInvalidState(unrelatedStateProcessor, "unrelated XML root");
}

juce::AudioBuffer<float> renderReferenceAudio()
{
    StageASynthAudioProcessor processor;
    setSustainedSineParameters(processor);
    prepare(processor);
    return render(processor, 2048,
                  withMessage(juce::MidiMessage::noteOn(1, 69, 0.8f)));
}

void testDeterministicReferenceRender()
{
    const auto first = renderReferenceAudio();
    const auto second = renderReferenceAudio();
    expect(isFinite(first), "reference render contains only finite samples");
    expect(rms(first) > 0.1, "reference render is audibly non-silent");

    constexpr std::array<int, 5> referenceIndices { 128, 256, 512, 1024, 2047 };
    constexpr std::array<float, 5> referenceSamples {
        0.707667f, 0.664450f, -0.749690f, 0.531373f, -0.800699f
    };
    for (size_t index = 0; index < referenceIndices.size(); ++index)
        expect(std::abs(first.getSample(0, referenceIndices[index]) - referenceSamples[index]) < 0.003f,
               "reference audio landmark remains stable at sample "
                   + std::to_string(referenceIndices[index]));

    auto maximumDifference = 0.0f;
    auto channelsMatch = true;
    for (int sample = 0; sample < first.getNumSamples(); ++sample)
    {
        maximumDifference = juce::jmax(maximumDifference,
            std::abs(first.getSample(0, sample) - second.getSample(0, sample)));
        channelsMatch = channelsMatch
                        && std::abs(first.getSample(0, sample) - first.getSample(1, sample)) < 1.0e-6f;
    }
    expect(channelsMatch, "stereo reference channels match");
    expect(maximumDifference < 1.0e-6f, "reference render repeats deterministically");
}
} // namespace

int main()
{
    testNoteOnOffAndReleaseCompletion();
    testPitchBend();
    testHighRegisterWavetableSuppressesFoldback();
    testEightVoiceAllocationAndStealing();
    testGainSmoothing();
    testParameterStateSaveRestore();
    testInvalidStateDoesNotReplaceDefaults();
    testDeterministicReferenceRender();

    if (failures != 0)
    {
        std::cerr << failures << " regression check(s) failed.\n";
        return 1;
    }

    std::cout << "All Stage A synth regression checks passed.\n";
    return 0;
}
