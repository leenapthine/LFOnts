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
    constexpr int kLaneH = 520; // room for full-size top row + one dual row
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

bool PinkELFOntsAudioProcessorEditor::paramExists(const juce::String &id) const
{
    return processor.apvts.getParameter(id) != nullptr;
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
    addAndMakeVisible(secLane);
    secLane.title = {};
    // Random card hidden for now (kept for parity in code)

    // --- Tabs ---------------------------------------------------------------
    addAndMakeVisible(laneTabs);
    laneTabs.addTab("Lane 1 (1/4)", juce::Colours::transparentBlack, nullptr, false);
    laneTabs.addTab("Lane 2 (1/4T)", juce::Colours::transparentBlack, nullptr, false);
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

    // Switch matrix in Output card (attach L1 + L2 + Random)
    addAndMakeVisible(switches);
    switches.setAlwaysOnTop(true);
    lane1OnAtt = std::make_unique<ButtonAtt>(processor.apvts, "lane1.enabled", switches.l1);
    lane2OnAtt = std::make_unique<ButtonAtt>(processor.apvts, "lane2.enabled", switches.l2); // NEW
    randomOnAtt = std::make_unique<ButtonAtt>(processor.apvts, "random.enabled", switches.random);

    // --- LANE 1 controls ----------------------------------------------------
    addAndMakeVisible(mixK1);
    configSlider(mixK1.slider, 0.0, 1.0, "");
    mix1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.mix", mixK1.slider);

    addAndMakeVisible(phaseK1);
    configSlider(phaseK1.slider, 0.0, 360.0, "°");
    phase1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.phaseDeg", phaseK1.slider);

    addAndMakeVisible(invertA1);
    configSlider(invertA1.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB1);
    configSlider(invertB1.slider, -1.0, 1.0, "");

    addAndMakeVisible(riseA1);
    configSlider(riseA1.length, 0.25, 4.0, "");
    configSlider(riseA1.curve, -1.0, 1.0, "");

    addAndMakeVisible(fallA1);
    configSlider(fallA1.length, 0.25, 4.0, "");
    configSlider(fallA1.curve, -1.0, 1.0, "");

    addAndMakeVisible(riseB1);
    configSlider(riseB1.length, 0.25, 4.0, "");
    configSlider(riseB1.curve, -1.0, 1.0, "");

    addAndMakeVisible(fallB1);
    configSlider(fallB1.length, 0.25, 4.0, "");
    configSlider(fallB1.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA1.slider, &invertB1.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&riseA1.curve, &fallA1.curve, &riseB1.curve, &fallB1.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK1.slider.setDoubleClickReturnValue(true, 0.0);

    // Attach to APVTS (lane 1)
    riseA1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseA", riseA1.length);
    fallA1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallA", fallA1.length);
    riseB1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.riseB", riseB1.length);
    fallB1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallB", fallB1.length);
    riseA1CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.riseA", riseA1.curve);
    fallA1CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.fallA", fallA1.curve);
    riseB1CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.riseB", riseB1.curve);
    fallB1CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.fallB", fallB1.curve);
    invertA1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.invertA", invertA1.slider);
    invertB1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.invertB", invertB1.slider);

    // --- LANE 2 controls (own knobs & attachments) -------------------------
    addAndMakeVisible(mixK2);
    configSlider(mixK2.slider, 0.0, 1.0, "");
    if (paramExists("lane2.mix"))
        mix2Att = std::make_unique<SliderAtt>(processor.apvts, "lane2.mix", mixK2.slider);

    addAndMakeVisible(phaseK2);
    configSlider(phaseK2.slider, 0.0, 360.0, "°");
    if (paramExists("lane2.phaseDeg"))
        phase2Att = std::make_unique<SliderAtt>(processor.apvts, "lane2.phaseDeg", phaseK2.slider);

    addAndMakeVisible(invertA2);
    configSlider(invertA2.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB2);
    configSlider(invertB2.slider, -1.0, 1.0, "");
    if (paramExists("lane2.invertA"))
        invertA2Att = std::make_unique<SliderAtt>(processor.apvts, "lane2.invertA", invertA2.slider);
    if (paramExists("lane2.invertB"))
        invertB2Att = std::make_unique<SliderAtt>(processor.apvts, "lane2.invertB", invertB2.slider);

    addAndMakeVisible(riseA2);
    configSlider(riseA2.length, 0.25, 4.0, "");
    configSlider(riseA2.curve, -1.0, 1.0, "");
    addAndMakeVisible(fallA2);
    configSlider(fallA2.length, 0.25, 4.0, "");
    configSlider(fallA2.curve, -1.0, 1.0, "");
    addAndMakeVisible(riseB2);
    configSlider(riseB2.length, 0.25, 4.0, "");
    configSlider(riseB2.curve, -1.0, 1.0, "");
    addAndMakeVisible(fallB2);
    configSlider(fallB2.length, 0.25, 4.0, "");
    configSlider(fallB2.curve, -1.0, 1.0, "");

    if (paramExists("lane2.curve.riseA"))
        riseA2Att = std::make_unique<SliderAtt>(processor.apvts, "lane2.curve.riseA", riseA2.length);
    if (paramExists("lane2.curve.fallA"))
        fallA2Att = std::make_unique<SliderAtt>(processor.apvts, "lane2.curve.fallA", fallA2.length);
    if (paramExists("lane2.curve.riseB"))
        riseB2Att = std::make_unique<SliderAtt>(processor.apvts, "lane2.curve.riseB", riseB2.length);
    if (paramExists("lane2.curve.fallB"))
        fallB2Att = std::make_unique<SliderAtt>(processor.apvts, "lane2.curve.fallB", fallB2.length);

    if (paramExists("lane2.curv.riseA"))
        riseA2CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curv.riseA", riseA2.curve);
    if (paramExists("lane2.curv.fallA"))
        fallA2CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curv.fallA", fallA2.curve);
    if (paramExists("lane2.curv.riseB"))
        riseB2CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curv.riseB", riseB2.curve);
    if (paramExists("lane2.curv.fallB"))
        fallB2CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curv.fallB", fallB2.curve);

    // ---- Double-click reset behaviour (lane 2)
    for (auto *s : {&invertA2.slider, &invertB2.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&riseA2.curve, &fallA2.curve, &riseB2.curve, &fallB2.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK2.slider.setDoubleClickReturnValue(true, 0.0);

    // --- Scopes -------------------------------------------------------------
    addAndMakeVisible(lane1Scope2);
    addAndMakeVisible(lane2Scope3);
    addAndMakeVisible(randomScope3);

    // **Drive scopes from processor (DSP truth) so curvature/invert apply**
    lane1Scope2.setEvaluator([this](float ph)
                             { return processor.evalLane1(ph); });
    lane2Scope3.setEvaluator([this](float ph)
                             { return processor.evalLane2Triplet(ph); });

    // Update scopes when any relevant knob changes
    auto upd1 = [this]
    { updateLane1Scope(); };
    auto upd2 = [this]
    { updateLane2Scope(); };

    // Lane 1 chain
    chainOnValue(riseA1.length, upd1);
    chainOnValue(riseA1.curve, upd1);
    chainOnValue(fallA1.length, upd1);
    chainOnValue(fallA1.curve, upd1);
    chainOnValue(riseB1.length, upd1);
    chainOnValue(riseB1.curve, upd1);
    chainOnValue(fallB1.length, upd1);
    chainOnValue(fallB1.curve, upd1);
    chainOnValue(invertA1.slider, upd1);
    chainOnValue(invertB1.slider, upd1);
    chainOnValue(phaseK1.slider, upd1);

    // Lane 2 chain
    chainOnValue(riseA2.length, upd2);
    chainOnValue(riseA2.curve, upd2);
    chainOnValue(fallA2.length, upd2);
    chainOnValue(fallA2.curve, upd2);
    chainOnValue(riseB2.length, upd2);
    chainOnValue(riseB2.curve, upd2);
    chainOnValue(fallB2.length, upd2);
    chainOnValue(fallB2.curve, upd2);
    chainOnValue(invertA2.slider, upd2);
    chainOnValue(invertB2.slider, upd2);
    chainOnValue(phaseK2.slider, upd2);

    laneTabs.setCurrentTabIndex(0, juce::NotificationType::dontSendNotification);
    updateLane1Scope();
    updateLane2Scope();
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
    secLane.setBounds(laneCardArea);

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

    const bool lane1Visible = (tab == 0);
    const bool lane2Visible = (tab == 1);
    const bool randomVisible = (tab == 2);

    // LANE 1 controls visibility
    mixK1.setVisible(lane1Visible);
    phaseK1.setVisible(lane1Visible);
    invertA1.setVisible(lane1Visible);
    invertB1.setVisible(lane1Visible);
    riseA1.setVisible(lane1Visible);
    fallA1.setVisible(lane1Visible);
    riseB1.setVisible(lane1Visible);
    fallB1.setVisible(lane1Visible);
    lane1Scope2.setVisible(lane1Visible);

    // LANE 2 controls visibility
    mixK2.setVisible(lane2Visible);
    phaseK2.setVisible(lane2Visible);
    invertA2.setVisible(lane2Visible);
    invertB2.setVisible(lane2Visible);
    riseA2.setVisible(lane2Visible);
    fallA2.setVisible(lane2Visible);
    riseB2.setVisible(lane2Visible);
    fallB2.setVisible(lane2Visible);
    lane2Scope3.setVisible(lane2Visible);

    // Random (placeholders)
    randomRate.setVisible(randomVisible);
    randomXfadeK.setVisible(randomVisible);
    randomMixK.setVisible(randomVisible);
    randomScope3.setVisible(randomVisible);

    auto layoutLane = [&](bool isLane1)
    {
        auto r = content;

        // Left controls / Right scope (identical layout on both tabs)
        const int cols = 4;
        const int colW = kDual;
        const int colGap = kGap;
        const int gridW = cols * colW + (cols - 1) * colGap;

        const int controlsW = gridW + 4;
        auto controls = r.removeFromLeft(controlsW);
        auto scope = r.reduced(8, 6);

        auto &scopeView = isLane1 ? static_cast<juce::Component &>(lane1Scope2)
                                  : static_cast<juce::Component &>(lane2Scope3);
        scopeView.setBounds(scope);

        // Row 0: Mix | Phase | Invert A | Invert B
        auto row0 = controls.removeFromTop(kKnob);
        auto placeTop = [&](Knob &k)
        {
            k.setBounds(row0.removeFromLeft(colW));
            row0.removeFromLeft(colGap);
        };
        if (isLane1)
        {
            placeTop(mixK1);
            placeTop(phaseK1);
            placeTop(invertA1);
            placeTop(invertB1);
        }
        else
        {
            placeTop(mixK2);
            placeTop(phaseK2);
            placeTop(invertA2);
            placeTop(invertB2);
        }

        controls.removeFromTop(kGap);

        // Row 1: Rise A | Fall A | Rise B | Fall B
        auto rowDual = controls.removeFromTop(kDual);
        auto placeDual = [&](DualKnob &dk)
        {
            dk.setBounds(rowDual.removeFromLeft(colW));
            rowDual.removeFromLeft(colGap);
        };
        if (isLane1)
        {
            placeDual(riseA1);
            placeDual(fallA1);
            placeDual(riseB1);
            placeDual(fallB1);
        }
        else
        {
            placeDual(riseA2);
            placeDual(fallA2);
            placeDual(riseB2);
            placeDual(fallB2);
        }
    };

    if (lane1Visible)
        layoutLane(true);
    else if (lane2Visible)
        layoutLane(false);
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
    // evaluator pulls from APVTS in processor; just repaint
    lane1Scope2.repaint();
}

void PinkELFOntsAudioProcessorEditor::updateLane2Scope()
{
    // evaluator pulls from APVTS in processor; just repaint
    lane2Scope3.repaint();
}
