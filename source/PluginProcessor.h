#pragma once
#include <JuceHeader.h>
#include "SynthEngine.h"

class PinkELFOntsAudioProcessor : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    PinkELFOntsAudioProcessor();
    ~PinkELFOntsAudioProcessor() override = default;

    // ==== AudioProcessor overrides ====
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override { return true; }
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    // UI
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    // Boilerplate
    const juce::String getName() const override { return "pink eLFOnts"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    // ==== Parameters ====
    APVTS apvts{*this, nullptr, "PARAMS", createParameterLayout()};
    static APVTS::ParameterLayout createParameterLayout();

    // Transport pull
    void updateTransportInfo();

private:
    SynthEngine engine{44100.0};
    juce::AudioPlayHead *playHead = nullptr;
    juce::AudioPlayHead::CurrentPositionInfo posInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkELFOntsAudioProcessor)
};
