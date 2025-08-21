#include "PluginEditor.h"
#include "PluginProcessor.h"

static void configSlider(juce::Slider &s, double min, double max, const juce::String &suffix = {})
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    s.setRange(min, max, 0.0);
    s.setTextValueSuffix(suffix);
}

PinkELFOntsAudioProcessorEditor::PinkELFOntsAudioProcessorEditor(PinkELFOntsAudioProcessor &p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(760, 420);

    // ===== Global =====
    configSlider(depthSlider, 0.0, 1.0, " depth");
    addAndMakeVisible(depthSlider);
    depthAtt = std::make_unique<SliderAtt>(processor.apvts, "global.depth", depthSlider);

    configSlider(phaseNudgeSlider, -30.0, 30.0, " °");
    addAndMakeVisible(phaseNudgeSlider);
    phaseNudgeAtt = std::make_unique<SliderAtt>(processor.apvts, "global.phaseNudgeDeg", phaseNudgeSlider);

    retrigBox.addItemList(juce::StringArray{"Continuous", "Every Note", "First Note Only"}, 1);
    addAndMakeVisible(retrigBox);
    retrigAtt = std::make_unique<ComboAtt>(processor.apvts, "global.retrig", retrigBox);

    rangeBox.addItemList(juce::StringArray{"0..1", "-1..1"}, 1);
    addAndMakeVisible(rangeBox);
    rangeAtt = std::make_unique<ComboAtt>(processor.apvts, "global.range", rangeBox);

    // ===== Lane 1 =====
    addAndMakeVisible(lane1Enabled);
    lane1OnAtt = std::make_unique<ButtonAtt>(processor.apvts, "lane1.enabled", lane1Enabled);

    configSlider(lane1Mix, 0.0, 1.0, " mix");
    addAndMakeVisible(lane1Mix);
    lane1MixAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.mix", lane1Mix);

    configSlider(lane1PhaseDeg, 0.0, 360.0, " °");
    addAndMakeVisible(lane1PhaseDeg);
    lane1PhaseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.phaseDeg", lane1PhaseDeg);

    configSlider(lane1RiseA, 0.25, 4.0, " γ");
    addAndMakeVisible(lane1RiseA);
    lane1RiseAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseA", lane1RiseA);

    configSlider(lane1FallA, 0.25, 4.0, " γ");
    addAndMakeVisible(lane1FallA);
    lane1FallAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallA", lane1FallA);

    configSlider(lane1RiseB, 0.25, 4.0, " γ");
    addAndMakeVisible(lane1RiseB);
    lane1RiseBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseB", lane1RiseB);

    configSlider(lane1FallB, 0.25, 4.0, " γ");
    addAndMakeVisible(lane1FallB);
    lane1FallBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallB", lane1FallB);

    // ===== Random lane =====
    addAndMakeVisible(randomEnabled);
    randomOnAtt = std::make_unique<ButtonAtt>(processor.apvts, "random.enabled", randomEnabled);

    randomRate.addItemList(juce::StringArray{"1/4", "1/8", "1/16"}, 1);
    addAndMakeVisible(randomRate);
    randomRateAtt = std::make_unique<ComboAtt>(processor.apvts, "random.rate", randomRate);

    configSlider(randomXfadeMs, 5.0, 80.0, " ms");
    addAndMakeVisible(randomXfadeMs);
    randomXfadeAtt = std::make_unique<SliderAtt>(processor.apvts, "random.crossfadeMs", randomXfadeMs);

    configSlider(randomMix, 0.0, 1.0, " mix");
    addAndMakeVisible(randomMix);
    randomMixAtt = std::make_unique<SliderAtt>(processor.apvts, "random.mix", randomMix);
}

void PinkELFOntsAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::hotpink);
    g.setFont(juce::Font(16.0f));
    g.drawText("pink eLFOnts — skeleton (Lane 1 + Random wired)", getLocalBounds().removeFromTop(24), juce::Justification::centred);

    g.setColour(juce::Colours::grey);
    g.drawText("(Output scope and draggable pattern editors come next)", getLocalBounds().removeFromTop(48), juce::Justification::centred);
}

void PinkELFOntsAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(10);

    // Global row
    auto top = r.removeFromTop(28);
    depthSlider.setBounds(top.removeFromLeft(220));
    top.removeFromLeft(8);
    phaseNudgeSlider.setBounds(top.removeFromLeft(220));
    top.removeFromLeft(8);
    retrigBox.setBounds(top.removeFromLeft(150));
    top.removeFromLeft(8);
    rangeBox.setBounds(top.removeFromLeft(120));

    r.removeFromTop(8);

    // Lane 1 block
    auto lane = r.removeFromTop(120);
    lane1Enabled.setBounds(lane.removeFromTop(24));
    lane.removeFromTop(4);
    lane1Mix.setBounds(lane.removeFromTop(24));
    lane.removeFromTop(4);
    lane1PhaseDeg.setBounds(lane.removeFromTop(24));
    lane.removeFromTop(4);

    auto curves = lane.removeFromTop(24);
    lane1RiseA.setBounds(curves.removeFromLeft(180));
    curves.removeFromLeft(6);
    lane1FallA.setBounds(curves.removeFromLeft(180));
    curves.removeFromLeft(6);
    lane1RiseB.setBounds(curves.removeFromLeft(180));
    curves.removeFromLeft(6);
    lane1FallB.setBounds(curves);

    r.removeFromTop(8);

    // Random lane block
    auto randRow1 = r.removeFromTop(24);
    randomEnabled.setBounds(randRow1.removeFromLeft(160));
    randRow1.removeFromLeft(6);
    randomRate.setBounds(randRow1.removeFromLeft(120));
    randRow1.removeFromLeft(6);
    randomXfadeMs.setBounds(randRow1.removeFromLeft(240));
    randRow1.removeFromLeft(6);
    randomMix.setBounds(randRow1);
}
