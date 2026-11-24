#pragma once

#include "PluginProcessor.h"

namespace vitaplug::lab2576c
{
class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Editor(Processor&);
    ~Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class Plate;
    void timerCallback() override;

    Processor& ownerProcessor;
    std::unique_ptr<Plate> plate;
    juce::TooltipWindow tooltips { this, 850 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};
}
