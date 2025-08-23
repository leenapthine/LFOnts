#pragma once
#include <JuceHeader.h>
#include "SynthEngine.h" // still present; we just don't use it for the carrier
#include "LFOShape.h"    // <— single source of truth for the LFO math

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

    // Helper so the editor (or others) can sample the current lane1 at any phase (0..1)
    float evalLane1(float ph01) const;

private:
    // Build the current Lane 1 shape from APVTS
    LFO::Shape makeLane1Shape() const;

    // Tempo utility
    double getCurrentBpm() const;

    // Map final shape to unipolar 0..1 for amplitude (respects global.range)
    float toUnipolar01(float v) const;

    // --- audio/LFO state ---
    double sampleRateHz = 44100.0;
    double lane1Phase01 = 0.0; // 0..1 phase
    double carrierPhase = 0.0; // 0..1 phase for the audio carrier

    // You can expose these later as params if you want
    float carrierHz = 1000.0f;  // sine carrier for EF to “see”
    float beatsPerCycle = 1.0f; // “1/4” = one cycle per quarter note

    // Retrig book-keeping (if you want to get fancy later)
    juce::AudioPlayHead *playHead = nullptr;
    juce::AudioPlayHead::CurrentPositionInfo posInfo{};

    // Your existing synth engine (unused for the carrier render here)
    SynthEngine engine{44100.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkELFOntsAudioProcessor)
};
