#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "LookAndFeel.h"

// Global L&F (lives for editor lifetime)
namespace
{
    PinkLookAndFeel gPinkLAF;
}

// Layout constants
namespace
{
    constexpr int kPad = 14;
    constexpr int kGap = 10;
    constexpr int kRowH = 44;
    constexpr int kCardH = 220; // Output card
    constexpr int kLaneH = 320; // Lane card (tall enough for two knob rows)
    constexpr int kKnob = 100;  // Knob square
}

// Helper: set slider range/suffix (Knob owns style)
static void configSlider(juce::Slider &s, double min, double max, const juce::String &suffix = {})
{
    s.setRange(min, max, 0.0);
    s.setTextValueSuffix(suffix);
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

    // --- Sections (cards) ---------------------------------------------------
    addAndMakeVisible(secOutput);
    addAndMakeVisible(secLane1);
    secLane1.title = {};
    secRandom.setVisible(false); // Random lives under tabs now

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

    // --- Lane 1 -------------------------------------------------------------
    addAndMakeVisible(lane1Enabled);
    lane1OnAtt = std::make_unique<ButtonAtt>(processor.apvts, "lane1.enabled", lane1Enabled);

    addAndMakeVisible(lane1MixK);
    configSlider(lane1MixK.slider, 0.0, 1.0, "");
    lane1MixAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.mix", lane1MixK.slider);

    addAndMakeVisible(lane1PhaseDegK);
    configSlider(lane1PhaseDegK.slider, 0.0, 360.0, "°");
    lane1PhaseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.phaseDeg", lane1PhaseDegK.slider);

    addAndMakeVisible(lane1RiseAK);
    configSlider(lane1RiseAK.slider, 0.25, 4.0, "");
    lane1RiseAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseA", lane1RiseAK.slider);

    addAndMakeVisible(lane1FallAK);
    configSlider(lane1FallAK.slider, 0.25, 4.0, "");
    lane1FallAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallA", lane1FallAK.slider);

    addAndMakeVisible(lane1RiseBK);
    configSlider(lane1RiseBK.slider, 0.25, 4.0, "");
    lane1RiseBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseB", lane1RiseBK.slider);

    addAndMakeVisible(lane1FallBK);
    configSlider(lane1FallBK.slider, 0.25, 4.0, "");
    lane1FallBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallB", lane1FallBK.slider);

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

    laneTabs.setCurrentTabIndex(0, juce::NotificationType::dontSendNotification);
    resized();
}

PinkELFOntsAudioProcessorEditor::~PinkELFOntsAudioProcessorEditor()
{
    laneTabs.getTabbedButtonBar().removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void PinkELFOntsAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xFF0B0D10)); // background; Section draws its own card
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

    // --- Section cards ------------------------------------------------------
    auto row1 = bounds.removeFromTop(kCardH);
    auto outputArea = row1.removeFromLeft(int(row1.getWidth() * 0.58f));
    secOutput.setBounds(outputArea);

    bounds.removeFromTop(kGap);
    auto laneCardArea = bounds.removeFromTop(kLaneH);
    secLane1.setBounds(laneCardArea);

    // Place the tab strip inside the Lane card (title line removed -> tighter top pad)
    laneTabs.setBounds(laneCardArea.reduced(12, 12));
    const int tabH = laneTabs.getTabbedButtonBar().getHeight();

    // --- Output section (Depth + Phase) ------------------------------------
    {
        auto r = outputArea.reduced(16, 32);
        auto row = r.removeFromTop(kKnob + 8);
        depthK.setBounds(row.removeFromLeft(kKnob));
        row.removeFromLeft(kGap);
        phaseNudgeK.setBounds(row.removeFromLeft(kKnob));
        // (Remaining area reserved for the output scope)
    }

    // --- Active tab content layout -----------------------------------------
    const int tab = laneTabs.getCurrentTabIndex(); // 0 = Lane 1, 1 = Random
    auto content = laneTabs.getBounds()
                       .reduced(16, 16)
                       .withTrimmedTop(tabH + 6);

    const bool laneVisible = (tab == 0);
    const bool randomVisible = (tab == 1);

    // Show/hide per-page widgets
    lane1Enabled.setVisible(laneVisible);
    lane1MixK.setVisible(laneVisible);
    lane1PhaseDegK.setVisible(laneVisible);
    lane1RiseAK.setVisible(laneVisible);
    lane1FallAK.setVisible(laneVisible);
    lane1RiseBK.setVisible(laneVisible);
    lane1FallBK.setVisible(laneVisible);

    randomEnabled.setVisible(randomVisible);
    randomRate.setVisible(randomVisible);
    randomXfadeK.setVisible(randomVisible);
    randomMixK.setVisible(randomVisible);

    if (laneVisible)
    {
        auto r = content;

        // Row A: compact toggle + Mix + Phase
        auto rowA = r.removeFromTop(kKnob + 8);
        auto toggleSlot = rowA.removeFromLeft(70);
        lane1Enabled.setBounds(toggleSlot.withSizeKeepingCentre(60, 24));
        rowA.removeFromLeft(kGap);
        lane1MixK.setBounds(rowA.removeFromLeft(kKnob));
        rowA.removeFromLeft(kGap);
        lane1PhaseDegK.setBounds(rowA.removeFromLeft(kKnob));

        r.removeFromTop(kGap);

        // Row B: Rise/Fall A/B
        auto rowB = r.removeFromTop(kKnob + 8);
        lane1RiseAK.setBounds(rowB.removeFromLeft(kKnob));
        rowB.removeFromLeft(kGap);
        lane1FallAK.setBounds(rowB.removeFromLeft(kKnob));
        rowB.removeFromLeft(kGap);
        lane1RiseBK.setBounds(rowB.removeFromLeft(kKnob));
        rowB.removeFromLeft(kGap);
        lane1FallBK.setBounds(rowB.removeFromLeft(kKnob));
    }
    else if (randomVisible)
    {
        auto r = content;
        auto row = r.removeFromTop(kKnob + 8);
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
        resized(); // relayout on tab change
}
