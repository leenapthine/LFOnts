#pragma once
#include <JuceHeader.h>
#include "LFOShape.h" // LFO math (returns 0..1 for our shape)

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
    float evalMixed(float ph01) const;

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

    // Helpers so the editor (or others) can sample current lanes at any phase (0..1)
    float evalLane1(float ph01) const;
    float evalLane2Triplet(float ph01) const; // A, B, B across 3 triangles
    float evalLane3(float ph01) const;        // 1/8
    float evalLane4Triplet(float ph01) const; // A, B, B across 3 triangles
    float evalLane5(float ph01) const;        // 1/16
    float evalLane6Triplet(float ph01) const; // A, B, B across 3 triangles

private:
    // Build shapes from APVTS
    LFO::Shape makeLane1Shape() const;
    LFO::Shape makeLane2Shape() const;
    LFO::Shape makeLane3Shape() const;
    LFO::Shape makeLane4Shape() const;
    LFO::Shape makeLane5Shape() const;
    LFO::Shape makeLane6Shape() const;

    // Tempo utility
    double getCurrentBpm() const;

    // Unipolar clamp (we ignore global.range now)
    static inline float toUnipolar01(float v) { return juce::jlimit(0.0f, 1.0f, v); }

    // --- audio/LFO state ---
    double sampleRateHz = 44100.0;
    double lane1Phase01 = 0.0; // 0..1 phase (2 triangles per full cycle)
    double lane2Phase01 = 0.0; // 0..1 phase (3 triangles per full cycle, lasts 2 beats)
    double lane3Phase01 = 0.0; // 0..1 phase (2 triangles per full cycle)
    double lane4Phase01 = 0.0; // 0..1 phase (3 triangles per full cycle, lasts 2 beats)
    double lane5Phase01 = 0.0; // 0..1 phase (2 triangles per full cycle)
    double lane6Phase01 = 0.0; // 0..1 phase (3 triangles per full cycle, lasts 2 beats)
    double carrierPhase = 0.0; // 0..1 phase for the audio carrier (for EF)

    // Carrier for EF visualization
    float carrierHz = 1000.0f;

    // Transport/book-keeping
    juce::AudioPlayHead *playHead = nullptr;
    juce::AudioPlayHead::CurrentPositionInfo posInfo{};
};