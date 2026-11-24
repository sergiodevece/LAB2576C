#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Compressor.h"
#include "PluginParameters.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace vitaplug::lab2576c
{
class Processor final : public juce::AudioProcessor
{
public:
    Processor();

    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& proposed) const override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LAB2576C"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int size) override;
    juce::AudioProcessorValueTreeState& parameters() noexcept { return state; }
    MeterLevels meterLevels() const noexcept;
    std::uint32_t meterGeneration() const noexcept { return meterUpdates.load(std::memory_order_acquire); }
    int editorWidth() const noexcept;
    void setEditorWidth(int width) noexcept;

private:
    Settings readSettings(bool hostBypass) const noexcept;
    void process(juce::AudioBuffer<float>&, bool hostBypass) noexcept;
    void publishMeters(MeterLevels) noexcept;

    juce::AudioProcessorValueTreeState state;
    std::array<const std::atomic<float>*, knobCount> controls {};
    const std::atomic<float>* vcaMode = nullptr;
    const std::atomic<float>* bypass = nullptr;
    Compressor compressor;
    std::atomic<float> inputMeter { 0.0f };
    std::atomic<float> outputMeter { 0.0f };
    std::atomic<float> reductionMeter { 0.0f };
    std::atomic<std::uint32_t> meterUpdates { 0 };
    std::atomic<int> savedEditorWidth { defaultEditorWidth };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};
}
