#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace vitaplug::lab2576c
{
namespace
{
juce::String parameterText(Unit unit, float value)
{
    switch (unit)
    {
        case Unit::db:      return juce::String(value, 1) + " dB";
        case Unit::ratio:   return juce::String(value, 1) + ":1";
        case Unit::ms:      return juce::String(value, value < 1.0f ? 2 : 1) + " ms";
        case Unit::hz:      return value < 1.0f ? juce::String("OFF") : juce::String(value, 0) + " Hz";
        case Unit::percent: return juce::String(value, 0) + "%";
    }

    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& spec : knobs)
    {
        juce::NormalisableRange<float> range(spec.minimum, spec.maximum, spec.step);
        range.setSkewForCentre(spec.centre);
        const auto attributes = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([unit = spec.unit](float value, int) { return parameterText(unit, value); })
            .withValueFromStringFunction([](const juce::String& value)
            {
                return value.trim().replaceCharacter(',', '.').getFloatValue();
            });
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { spec.id, 1 },
                                                               juce::String::fromUTF8(spec.hostLabel),
                                                               range, spec.initial, attributes));
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { vcaModeId, 1 }, "VCA Detector",
                                                            juce::StringArray { "Feed-forward", "Feedback" }, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { bypassId, 1 }, "Bypass", false));
    return layout;
}
}

Processor::Processor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "LAB2576C", makeParameters())
{
    for (std::size_t i = 0; i < controls.size(); ++i)
        controls[i] = state.getRawParameterValue(knobs[i].id);

    vcaMode = state.getRawParameterValue(vcaModeId);
    bypass = state.getRawParameterValue(bypassId);
}

void Processor::prepareToPlay(double sampleRate, int)
{
    compressor.prepare(sampleRate, readSettings(false));
    publishMeters({});
}

void Processor::releaseResources() {}

void Processor::reset()
{
    prepareToPlay(getSampleRate(), getBlockSize());
}

bool Processor::isBusesLayoutSupported(const BusesLayout& proposed) const
{
    const auto layout = proposed.getMainInputChannelSet();
    return layout == proposed.getMainOutputChannelSet()
        && (layout == juce::AudioChannelSet::mono() || layout == juce::AudioChannelSet::stereo());
}

Settings Processor::readSettings(bool hostBypass) const noexcept
{
    const auto get = [this](Knob knob) { return controls[knob]->load(std::memory_order_relaxed); };
    Settings result;
    result.inputDb = get(input);
    result.thresholdDb = get(threshold);
    result.ratio = get(ratio);
    result.attackMs = get(attack);
    result.releaseMs = get(release);
    result.kneeDb = get(knee);
    result.morph = get(morph) / 100.0f;
    result.scHighPassHz = get(hpf);
    result.scTiltDb = get(tilt);
    result.stereoLink = get(link) / 100.0f;
    result.mix = get(mix) / 100.0f;
    result.outputDb = get(output);
    result.vcaFeedback = vcaMode->load(std::memory_order_relaxed) >= 0.5f;
    result.bypass = hostBypass || bypass->load(std::memory_order_relaxed) >= 0.5f;
    return result;
}

void Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process(buffer, false);
}

void Processor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process(buffer, true);
}

void Processor::process(juce::AudioBuffer<float>& buffer, bool hostBypass) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    const auto inputChannels = getTotalNumInputChannels();
    for (int channel = inputChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (inputChannels == 0 || buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    auto* right = inputChannels > 1 && buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    compressor.process(buffer.getWritePointer(0), right, static_cast<std::size_t>(buffer.getNumSamples()),
                       readSettings(hostBypass));
    publishMeters(compressor.meterLevels());
}

void Processor::publishMeters(MeterLevels levels) noexcept
{
    inputMeter.store(levels.inputPeak, std::memory_order_relaxed);
    outputMeter.store(levels.outputPeak, std::memory_order_relaxed);
    reductionMeter.store(levels.reductionDb, std::memory_order_relaxed);
    meterUpdates.fetch_add(1, std::memory_order_release);
}

juce::AudioProcessorParameter* Processor::getBypassParameter() const
{
    return state.getParameter(bypassId);
}

MeterLevels Processor::meterLevels() const noexcept
{
    return { inputMeter.load(std::memory_order_relaxed), outputMeter.load(std::memory_order_relaxed),
             reductionMeter.load(std::memory_order_relaxed) };
}

juce::AudioProcessorEditor* Processor::createEditor()
{
    return new Editor(*this);
}

void Processor::getStateInformation(juce::MemoryBlock& result)
{
    if (const auto xml = state.copyState().createXml())
    {
        xml->setAttribute("editorWidth", editorWidth());
        copyXmlToBinary(*xml, result);
    }
}

void Processor::setStateInformation(const void* data, int size)
{
    if (const auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(state.state.getType().toString()))
    {
        setEditorWidth(xml->getIntAttribute("editorWidth", defaultEditorWidth));
        auto restored = juce::ValueTree::fromXml(*xml);
        restored.removeProperty("editorWidth", nullptr);
        state.replaceState(restored);
    }
}

int Processor::editorWidth() const noexcept
{
    return savedEditorWidth.load(std::memory_order_relaxed);
}

void Processor::setEditorWidth(int width) noexcept
{
    savedEditorWidth.store(juce::jlimit(minimumEditorWidth, maximumEditorWidth, width), std::memory_order_relaxed);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vitaplug::lab2576c::Processor();
}
