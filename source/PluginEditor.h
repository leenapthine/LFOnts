#pragma once
#include <JuceHeader.h>
class PinkELFOntsAudioProcessor;

class PinkELFOntsAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    explicit PinkELFOntsAudioProcessorEditor(PinkELFOntsAudioProcessor &);
    ~PinkELFOntsAudioProcessorEditor() override = default;

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    PinkELFOntsAudioProcessor &processor;

    // Global controls
    juce::Slider depthSlider, phaseNudgeSlider;
    juce::ComboBox retrigBox, rangeBox;

    // Lane 1 controls (¼ note)
    juce::ToggleButton lane1Enabled{"Lane 1 (1/4) On"};
    juce::Slider lane1Mix, lane1PhaseDeg, lane1RiseA, lane1FallA, lane1RiseB, lane1FallB;

    // Random lane controls
    juce::ToggleButton randomEnabled{"Random On"};
    juce::ComboBox randomRate;
    juce::Slider randomXfadeMs, randomMix;

    // Attachments
    using SliderAtt = APVTS::SliderAttachment;
    using ButtonAtt = APVTS::ButtonAttachment;
    using ComboAtt = APVTS::ComboBoxAttachment;

    std::unique_ptr<SliderAtt> depthAtt, phaseNudgeAtt;
    std::unique_ptr<ComboAtt> retrigAtt, rangeAtt;

    std::unique_ptr<ButtonAtt> lane1OnAtt;
    std::unique_ptr<SliderAtt> lane1MixAtt, lane1PhaseAtt, lane1RiseAAtt, lane1FallAAtt, lane1RiseBAtt, lane1FallBAtt;

    std::unique_ptr<ButtonAtt> randomOnAtt;
    std::unique_ptr<ComboAtt> randomRateAtt;
    std::unique_ptr<SliderAtt> randomXfadeAtt, randomMixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkELFOntsAudioProcessorEditor)
};
