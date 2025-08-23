#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "LookAndFeel.h"

namespace
{
    PinkLookAndFeel gPinkLAF;
}

// Layout
namespace
{
    constexpr int kPad = 14;
    constexpr int kGap = 10;
    constexpr int kRowH = 44;
    constexpr int kCardH = 220;
    constexpr int kLaneH = 430;  // room for full-size top row + one dual row
    constexpr int kKnob = 100;   // full knob
    constexpr int kDual = 88;    // dual knob size
    constexpr int kToggleW = 70; // "On" pill
}

static void configSlider(juce::Slider &s, double min, double max, const juce::String &suf = {})
{
    s.setRange(min, max, 0.0);
    s.setTextValueSuffix(suf);
}

static void chainOnValue(juce::Slider &s, std::function<void()> extra)
{
    auto prev = s.onValueChange;
    s.onValueChange = [prev, extra]
    { if (prev) prev(); if (extra) extra(); };
}

PinkELFOntsAudioProcessorEditor::PinkELFOntsAudioProcessorEditor(PinkELFOntsAudioProcessor &p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&gPinkLAF);
    setSize(980, 620);

    // --- Top bar ------------------------------------------------------------
    title.setText("pink eLFOnts", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    title.setColour(juce::Label::textColourId, juce::Colour(0xFFFF4FA3));
    title.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    addAndMakeVisible(title);

    retrigBox.addItemList(juce::StringArray{"Continuous", "Every Note", "First Note Only"}, 1);
    addAndMakeVisible(retrigBox);
    retrigAtt = std::make_unique<ComboAtt>(processor.apvts, "global.retrig", retrigBox);

    rangeBox.addItemList(juce::StringArray{"0..1", "-1..1"}, 1);
    addAndMakeVisible(rangeBox);
    rangeAtt = std::make_unique<ComboAtt>(processor.apvts, "global.range", rangeBox);

    // --- Sections -----------------------------------------------------------
    addAndMakeVisible(secOutput);
    addAndMakeVisible(secLane1);
    secLane1.title = {};
    secRandom.setVisible(false);

    // --- Tabs ---------------------------------------------------------------
    addAndMakeVisible(laneTabs);
    laneTabs.addTab("Lane 1 (1/4)", juce::Colours::transparentBlack, nullptr, false);
    laneTabs.addTab("Random", juce::Colours::transparentBlack, nullptr, false);
    laneTabs.getTabbedButtonBar().setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colour(0xFFE6EBF2));
    laneTabs.getTabbedButtonBar().addChangeListener(this);

    // --- Global knobs -------------------------------------------------------
    addAndMakeVisible(depthK);
    configSlider(depthK.slider, 0.0, 1.0, "");
    depthAtt = std::make_unique<SliderAtt>(processor.apvts, "global.depth", depthK.slider);

    addAndMakeVisible(phaseNudgeK);
    configSlider(phaseNudgeK.slider, -30.0, 30.0, "°");
    phaseNudgeAtt = std::make_unique<SliderAtt>(processor.apvts, "global.phaseNudgeDeg", phaseNudgeK.slider);

    // --- Lane 1 (top row) ---------------------------------------------------
    lane1Enabled.setButtonText("On");
    addAndMakeVisible(lane1Enabled);
    lane1OnAtt = std::make_unique<ButtonAtt>(processor.apvts, "lane1.enabled", lane1Enabled);

    addAndMakeVisible(mixK);
    configSlider(mixK.slider, 0.0, 1.0, "");
    mixAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.mix", mixK.slider);

    addAndMakeVisible(phaseK);
    configSlider(phaseK.slider, 0.0, 360.0, "°");
    phaseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.phaseDeg", phaseK.slider);

    addAndMakeVisible(invertAK);
    configSlider(invertAK.slider, -1.0, 1.0, "");
    invertAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.invertA", invertAK.slider);

    addAndMakeVisible(invertBK);
    configSlider(invertBK.slider, -1.0, 1.0, "");
    invertBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.invertB", invertBK.slider);

    // Duals
    addAndMakeVisible(riseA);
    configSlider(riseA.length, 0.25, 4.0, "");
    configSlider(riseA.curve, -1.0, 1.0, "");

    addAndMakeVisible(fallA);
    configSlider(fallA.length, 0.25, 4.0, "");
    configSlider(fallA.curve, -1.0, 1.0, "");

    addAndMakeVisible(riseB);
    configSlider(riseB.length, 0.25, 4.0, "");
    configSlider(riseB.curve, -1.0, 1.0, "");

    addAndMakeVisible(fallB);
    configSlider(fallB.length, 0.25, 4.0, "");
    configSlider(fallB.curve, -1.0, 1.0, "");

    // Attach to APVTS
    riseAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseA", riseA.length);
    fallAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallA", fallA.length);
    riseBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseB", riseB.length);
    fallBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallB", fallB.length);

    riseACurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.riseA", riseA.curve);
    fallACurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.fallA", fallA.curve);
    riseBCurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.riseB", riseB.curve);
    fallBCurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.fallB", fallB.curve);

    // --- Random -------------------------------------------------------------
    addAndMakeVisible(randomEnabled);
    randomOnAtt = std::make_unique<ButtonAtt>(processor.apvts, "random.enabled", randomEnabled);

    randomRate.addItemList(juce::StringArray{"1/4", "1/8", "1/16"}, 1);
    addAndMakeVisible(randomRate);
    randomRateAtt = std::make_unique<ComboAtt>(processor.apvts, "random.rate", randomRate);

    addAndMakeVisible(randomXfadeK);
    configSlider(randomXfadeK.slider, 5.0, 80.0, "");
    randomXfadeAtt = std::make_unique<SliderAtt>(processor.apvts, "random.crossfadeMs", randomXfadeK.slider);

    addAndMakeVisible(randomMixK);
    configSlider(randomMixK.slider, 0.0, 1.0, "");
    randomMixAtt = std::make_unique<SliderAtt>(processor.apvts, "random.mix", randomMixK.slider);

    // --- Scopes -------------------------------------------------------------
    addAndMakeVisible(lane1Scope2);
    addAndMakeVisible(randomScope3);

    auto upd = [this]
    { updateLane1Scope(); };
    chainOnValue(riseA.length, upd);
    chainOnValue(riseA.curve, upd);
    chainOnValue(fallA.length, upd);
    chainOnValue(fallA.curve, upd);
    chainOnValue(riseB.length, upd);
    chainOnValue(riseB.curve, upd);
    chainOnValue(fallB.length, upd);
    chainOnValue(fallB.curve, upd);
    chainOnValue(invertAK.slider, upd);
    chainOnValue(invertBK.slider, upd);

    laneTabs.setCurrentTabIndex(0, juce::NotificationType::dontSendNotification);
    updateLane1Scope();
    resized();
}

PinkELFOntsAudioProcessorEditor::~PinkELFOntsAudioProcessorEditor()
{
    laneTabs.getTabbedButtonBar().removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void PinkELFOntsAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xFF0B0D10));
}

void PinkELFOntsAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(kPad);

    // --- Top bar ------------------------------------------------------------
    auto top = bounds.removeFromTop(kRowH);
    title.setBounds(top.removeFromLeft(280));
    top.removeFromRight(kGap);
    rangeBox.setBounds(top.removeFromRight(120));
    top.removeFromRight(kGap);
    retrigBox.setBounds(top.removeFromRight(220));

    bounds.removeFromTop(kGap);

    // --- Cards --------------------------------------------------------------
    auto row1 = bounds.removeFromTop(kCardH);
    auto outputArea = row1.removeFromLeft(int(row1.getWidth() * 0.58f));
    secOutput.setBounds(outputArea);

    bounds.removeFromTop(kGap);
    auto laneCardArea = bounds.removeFromTop(kLaneH);
    secLane1.setBounds(laneCardArea);

    laneTabs.setBounds(laneCardArea.reduced(12, 12));
    const int tabH = laneTabs.getTabbedButtonBar().getHeight();

    // Output section
    {
        auto r = outputArea.reduced(16, 32);
        auto row = r.removeFromTop(kKnob + 8);
        depthK.setBounds(row.removeFromLeft(kKnob));
        row.removeFromLeft(kGap);
        phaseNudgeK.setBounds(row.removeFromLeft(kKnob));
    }

    // --- Tab content --------------------------------------------------------
    const int tab = laneTabs.getCurrentTabIndex();
    auto content = laneTabs.getBounds().reduced(16, 16).withTrimmedTop(tabH + 6);

    const bool laneVisible = (tab == 0);
    const bool randomVisible = (tab == 1);

    mixK.setVisible(laneVisible);
    phaseK.setVisible(laneVisible);
    invertAK.setVisible(laneVisible);
    invertBK.setVisible(laneVisible);
    riseA.setVisible(laneVisible);
    fallA.setVisible(laneVisible);
    riseB.setVisible(laneVisible);
    fallB.setVisible(laneVisible);
    lane1Enabled.setVisible(laneVisible);
    lane1Scope2.setVisible(laneVisible);

    randomEnabled.setVisible(randomVisible);
    randomRate.setVisible(randomVisible);
    randomXfadeK.setVisible(randomVisible);
    randomMixK.setVisible(randomVisible);
    randomScope3.setVisible(randomVisible);

    if (laneVisible)
    {
        // Left controls / Right scope
        auto r = content;

        const int topRowW = kToggleW + kGap + 4 * kKnob + 3 * kGap;
        const int dualRowW = 4 * kDual + 3 * kGap; // four duals on one row
        const int controlsW = std::max(topRowW, dualRowW) + 4;

        auto controls = r.removeFromLeft(controlsW);
        auto scope = r.reduced(8, 6);
        lane1Scope2.setBounds(scope);

        // Row 0: On | Mix | Phase | Invert A | Invert B
        auto row0 = controls.removeFromTop(kKnob);
        lane1Enabled.setBounds(row0.removeFromLeft(kToggleW).withSizeKeepingCentre(kToggleW, 24));
        row0.removeFromLeft(kGap);
        mixK.setBounds(row0.removeFromLeft(kKnob));
        row0.removeFromLeft(kGap);
        phaseK.setBounds(row0.removeFromLeft(kKnob));
        row0.removeFromLeft(kGap);
        invertAK.setBounds(row0.removeFromLeft(kKnob));
        row0.removeFromLeft(kGap);
        invertBK.setBounds(row0.removeFromLeft(kKnob));

        controls.removeFromTop(kGap);

        // Row 1: Rise A | Fall A | Rise B | Fall B  (all dual knobs)
        auto rowDual = controls.removeFromTop(kDual);
        riseA.setBounds(rowDual.removeFromLeft(kDual));
        rowDual.removeFromLeft(kGap);
        fallA.setBounds(rowDual.removeFromLeft(kDual));
        rowDual.removeFromLeft(kGap);
        riseB.setBounds(rowDual.removeFromLeft(kDual));
        rowDual.removeFromLeft(kGap);
        fallB.setBounds(rowDual.removeFromLeft(kDual));
    }
    else
    {
        // Random
        auto r = content;
        const int controlsW = kKnob * 3 + kGap * 3 + 120;
        auto controls = r.removeFromLeft(controlsW);
        auto scope = r.reduced(8, 6);
        randomScope3.setBounds(scope);

        auto row = controls.removeFromTop(kKnob + 8);
        randomEnabled.setBounds(row.removeFromLeft(60).withSizeKeepingCentre(60, 24));
        row.removeFromLeft(kGap);
        randomRate.setBounds(row.removeFromLeft(120));
        row.removeFromLeft(kGap);
        randomXfadeK.setBounds(row.removeFromLeft(kKnob));
        row.removeFromLeft(kGap);
        randomMixK.setBounds(row.removeFromLeft(kKnob));
    }
}

void PinkELFOntsAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster *source)
{
    if (source == &laneTabs.getTabbedButtonBar())
        resized();
}

void PinkELFOntsAudioProcessorEditor::updateLane1Scope()
{
    lane1Scope2.setShape(
        (float)riseA.length.getValue(), (float)fallA.length.getValue(),
        (float)riseB.length.getValue(), (float)fallB.length.getValue(),
        (float)invertAK.slider.getValue(), (float)invertBK.slider.getValue(),
        (float)riseA.curve.getValue(), (float)fallA.curve.getValue(),
        (float)riseB.curve.getValue(), (float)fallB.curve.getValue());
}
