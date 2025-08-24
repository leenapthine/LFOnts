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
    constexpr int kLaneH = 430; // room for full-size top row + one dual row
    constexpr int kKnob = 100;  // full knob
    constexpr int kDual = 88;   // dual knob size
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

    // --- Output card (global) ----------------------------------------------
    addAndMakeVisible(depthK);
    configSlider(depthK.slider, 0.0, 1.0, "");
    depthAtt = std::make_unique<SliderAtt>(processor.apvts, "global.depth", depthK.slider);

    addAndMakeVisible(phaseNudgeK);
    configSlider(phaseNudgeK.slider, -30.0, 30.0, "°");
    phaseNudgeAtt = std::make_unique<SliderAtt>(processor.apvts, "global.phaseNudgeDeg", phaseNudgeK.slider);

    // Switch matrix in Output card (attach L1 + Random; others are dummies)
    addAndMakeVisible(switches);
    switches.setAlwaysOnTop(true);
    lane1OnAtt = std::make_unique<ButtonAtt>(processor.apvts, "lane1.enabled", switches.l1);
    randomOnAtt = std::make_unique<ButtonAtt>(processor.apvts, "random.enabled", switches.random);

    // --- Lane 1 controls ----------------------------------------------------
    addAndMakeVisible(mixK);
    configSlider(mixK.slider, 0.0, 1.0, "");
    mixAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.mix", mixK.slider);

    addAndMakeVisible(phaseK);
    configSlider(phaseK.slider, 0.0, 360.0, "°");
    phaseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.phaseDeg", phaseK.slider);

    addAndMakeVisible(invertAK);
    configSlider(invertAK.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertBK);
    configSlider(invertBK.slider, -1.0, 1.0, "");

    // Duals (length + curve)
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

    // ---- Double-click reset behaviour -------------------------------------
    invertAK.slider.setDoubleClickReturnValue(true, 0.0);
    invertBK.slider.setDoubleClickReturnValue(true, 0.0);

    for (auto *s : {&riseA.curve, &fallA.curve, &riseB.curve, &fallB.curve})
        s->setDoubleClickReturnValue(true, 0.0);

    phaseK.slider.setDoubleClickReturnValue(true, 0.0);

    // Attach to APVTS
    // (lengths)
    riseAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseA", riseA.length);
    fallAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallA", fallA.length);
    riseBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseB", riseB.length);
    fallBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallB", fallB.length);
    // (curves)
    riseACurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.riseA", riseA.curve);
    fallACurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.fallA", fallA.curve);
    riseBCurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.riseB", riseB.curve);
    fallBCurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.fallB", fallB.curve);
    // (invert A/B)
    invertAAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.invertA", invertAK.slider);
    invertBAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.invertB", invertBK.slider);

    // --- Random tab ---------------------------------------------------------
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

    // Update scope when any knob changes
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
    chainOnValue(phaseK.slider, upd);

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

    // Output section layout
    {
        auto r = outputArea.reduced(16, 32);

        // Row A: Depth | Phase Nudge
        auto rowA = r.removeFromTop(kKnob + 8);
        depthK.setBounds(rowA.removeFromLeft(kKnob));
        rowA.removeFromLeft(kGap);
        phaseNudgeK.setBounds(rowA.removeFromLeft(kKnob));

        r.removeFromTop(kGap);

        // Row B: Switch matrix
        switches.setBounds(r.removeFromTop(80)); // ~2 rows of pill toggles
    }

    // --- Tab content --------------------------------------------------------
    const int tab = laneTabs.getCurrentTabIndex();
    auto content = laneTabs.getBounds().reduced(16, 16).withTrimmedTop(tabH + 6);

    const bool laneVisible = (tab == 0);
    const bool randomVisible = (tab == 1);

    // Lane 1 widgets
    mixK.setVisible(laneVisible);
    phaseK.setVisible(laneVisible);
    invertAK.setVisible(laneVisible);
    invertBK.setVisible(laneVisible);
    riseA.setVisible(laneVisible);
    fallA.setVisible(laneVisible);
    riseB.setVisible(laneVisible);
    fallB.setVisible(laneVisible);
    lane1Scope2.setVisible(laneVisible);

    // Random widgets
    randomRate.setVisible(randomVisible);
    randomXfadeK.setVisible(randomVisible);
    randomMixK.setVisible(randomVisible);
    randomScope3.setVisible(randomVisible);

    if (laneVisible)
    {
        // Left controls / Right scope
        auto r = content;

        // Fixed 4-column grid: top knobs over dual knobs
        const int cols = 4;
        const int colW = kDual; // match dual width so columns line up cleanly
        const int colGap = kGap;
        const int gridW = cols * colW + (cols - 1) * colGap;

        const int controlsW = gridW + 4;
        auto controls = r.removeFromLeft(controlsW);
        auto scope = r.reduced(8, 6);
        lane1Scope2.setBounds(scope);

        // Row 0: Mix | Phase | Invert A | Invert B
        auto row0 = controls.removeFromTop(kKnob);
        auto placeTop = [&](Knob &k)
        {
            k.setBounds(row0.removeFromLeft(colW));
            row0.removeFromLeft(colGap);
        };
        placeTop(mixK);
        placeTop(phaseK);
        placeTop(invertAK);
        placeTop(invertBK);

        controls.removeFromTop(kGap);

        // Row 1: Rise A | Fall A | Rise B | Fall B
        auto rowDual = controls.removeFromTop(kDual);
        auto placeDual = [&](DualKnob &dk)
        {
            dk.setBounds(rowDual.removeFromLeft(colW));
            rowDual.removeFromLeft(colGap);
        };
        placeDual(riseA);
        placeDual(fallA);
        placeDual(riseB);
        placeDual(fallB);
    }
    else
    {
        // Random (placeholder layout)
        auto r = content;
        const int controlsW = kKnob * 3 + kGap * 2 + 120;
        auto controls = r.removeFromLeft(controlsW);
        auto scope = r.reduced(8, 6);
        randomScope3.setBounds(scope);

        auto row = controls.removeFromTop(kKnob + 8);
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
    LFO::Shape s;
    s.riseA = (float)riseA.length.getValue();
    s.fallA = (float)fallA.length.getValue();
    s.riseB = (float)riseB.length.getValue();
    s.fallB = (float)fallB.length.getValue();

    s.curvRiseA = (float)riseA.curve.getValue();
    s.curvFallA = (float)fallA.curve.getValue();
    s.curvRiseB = (float)riseB.curve.getValue();
    s.curvFallB = (float)fallB.curve.getValue();

    // Map UI [-1..1] -> shape [0..1]; center (0) = no inversion
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs((float)invertAK.slider.getValue()));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs((float)invertBK.slider.getValue()));

    lane1Scope2.setNumTriangles(2); // two triangles per lane-1 cycle
    lane1Scope2.setFromShape(s, (float)phaseK.slider.getValue());
}
