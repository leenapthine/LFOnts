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

    // --- Mixer card (top-right) ------------------------------------------------
    addAndMakeVisible(secMixer);

    // Build 8 columns: label, fader (0..1), mute (laneX.enabled)
    for (int i = 0; i < 8; ++i)
    {
        // Label "L1..L8"
        mixerLbl[i].setText("L" + juce::String(i + 1), juce::dontSendNotification);
        mixerLbl[i].setJustificationType(juce::Justification::centred);
        mixerLbl[i].setColour(juce::Label::textColourId, juce::Colour(0xFFE6EBF2));
        addAndMakeVisible(mixerLbl[i]);

        // Fader
        auto &f = mixerFader[i];
        f.setSliderStyle(juce::Slider::LinearVertical);
        f.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        f.setRange(0.0, 1.0, 0.0);
        f.setDoubleClickReturnValue(true, 1.0);
        addAndMakeVisible(f);

        // Attach to laneX.mix if present (X=1..8)
        const juce::String mixId = "lane" + juce::String(i + 1) + ".mix";
        if (processor.apvts.getParameter(mixId) != nullptr)
            mixerFaderAtt[i] = std::make_unique<SliderAtt>(processor.apvts, mixId, f);

        // Mute/enable dot
        auto &m = mixerOn[i];
        m.setButtonText({});
        m.setClickingTogglesState(true);
        addAndMakeVisible(m);

        const juce::String enId = "lane" + juce::String(i + 1) + ".enabled";
        if (processor.apvts.getParameter(enId) != nullptr)
            mixerOnAtt[i] = std::make_unique<ButtonAtt>(processor.apvts, enId, m);
    }

    // --- Tabs ---------------------------------------------------------------
    addAndMakeVisible(laneTabs);
    laneTabs.addTab("Lane 1 (1/4)", juce::Colours::transparentBlack, nullptr, false);
    laneTabs.addTab("Lane 2 (1/4T)", juce::Colours::transparentBlack, nullptr, false);
    laneTabs.addTab("Lane 3 (1/8)", juce::Colours::transparentBlack, nullptr, false);
    laneTabs.addTab("Lane 4 (1/8T)", juce::Colours::transparentBlack, nullptr, false);
    laneTabs.addTab("Lane 5 (1/16)", juce::Colours::transparentBlack, nullptr, false);
    laneTabs.addTab("Lane 6 (1/16T)", juce::Colours::transparentBlack, nullptr, false);
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

    // --- LANE 3 controls ----------------------------------------------------
    addAndMakeVisible(mixK3);
    configSlider(mixK3.slider, 0.0, 1.0, "");
    mix3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.mix", mixK3.slider);

    addAndMakeVisible(phaseK3);
    configSlider(phaseK3.slider, 0.0, 360.0, "°");
    phase3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.phaseDeg", phaseK3.slider);

    addAndMakeVisible(invertA3);
    configSlider(invertA3.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB3);
    configSlider(invertB3.slider, -1.0, 1.0, "");

    addAndMakeVisible(riseA3);
    configSlider(riseA3.length, 0.25, 4.0, "");
    configSlider(riseA3.curve, -1.0, 1.0, "");

    addAndMakeVisible(fallA3);
    configSlider(fallA3.length, 0.25, 4.0, "");
    configSlider(fallA3.curve, -1.0, 1.0, "");

    addAndMakeVisible(riseB3);
    configSlider(riseB3.length, 0.25, 4.0, "");
    configSlider(riseB3.curve, -1.0, 1.0, "");

    addAndMakeVisible(fallB3);
    configSlider(fallB3.length, 0.25, 4.0, "");
    configSlider(fallB3.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA3.slider, &invertB3.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&riseA3.curve, &fallA3.curve, &riseB3.curve, &fallB3.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK3.slider.setDoubleClickReturnValue(true, 0.0);

    // Attach to APVTS (lane 3)
    riseA3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.curve.riseA", riseA3.length);
    fallA3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.curve.fallA", fallA3.length);
    riseB3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.curve.riseB", riseB3.length);
    fallB3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.curve.fallB", fallB3.length);
    riseA3CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curv.riseA", riseA3.curve);
    fallA3CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curv.fallA", fallA3.curve);
    riseB3CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curv.riseB", riseB3.curve);
    fallB3CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curv.fallB", fallB3.curve);
    invertA3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.invertA", invertA3.slider);
    invertB3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.invertB", invertB3.slider);

    // --- LANE 4 controls (own knobs & attachments) -------------------------
    addAndMakeVisible(mixK4);
    configSlider(mixK4.slider, 0.0, 1.0, "");
    if (paramExists("lane4.mix"))
        mix4Att = std::make_unique<SliderAtt>(processor.apvts, "lane4.mix", mixK4.slider);

    addAndMakeVisible(phaseK4);
    configSlider(phaseK4.slider, 0.0, 360.0, "°");
    if (paramExists("lane4.phaseDeg"))
        phase4Att = std::make_unique<SliderAtt>(processor.apvts, "lane4.phaseDeg", phaseK4.slider);

    addAndMakeVisible(invertA4);
    configSlider(invertA4.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB4);
    configSlider(invertB4.slider, -1.0, 1.0, "");
    if (paramExists("lane4.invertA"))
        invertA4Att = std::make_unique<SliderAtt>(processor.apvts, "lane4.invertA", invertA4.slider);
    if (paramExists("lane4.invertB"))
        invertB4Att = std::make_unique<SliderAtt>(processor.apvts, "lane4.invertB", invertB4.slider);

    addAndMakeVisible(riseA4);
    configSlider(riseA4.length, 0.25, 4.0, "");
    configSlider(riseA4.curve, -1.0, 1.0, "");
    addAndMakeVisible(fallA4);
    configSlider(fallA4.length, 0.25, 4.0, "");
    configSlider(fallA4.curve, -1.0, 1.0, "");
    addAndMakeVisible(riseB4);
    configSlider(riseB4.length, 0.25, 4.0, "");
    configSlider(riseB4.curve, -1.0, 1.0, "");
    addAndMakeVisible(fallB4);
    configSlider(fallB4.length, 0.25, 4.0, "");
    configSlider(fallB4.curve, -1.0, 1.0, "");

    if (paramExists("lane4.curve.riseA"))
        riseA4Att = std::make_unique<SliderAtt>(processor.apvts, "lane4.curve.riseA", riseA4.length);
    if (paramExists("lane4.curve.fallA"))
        fallA4Att = std::make_unique<SliderAtt>(processor.apvts, "lane4.curve.fallA", fallA4.length);
    if (paramExists("lane4.curve.riseB"))
        riseB4Att = std::make_unique<SliderAtt>(processor.apvts, "lane4.curve.riseB", riseB4.length);
    if (paramExists("lane4.curve.fallB"))
        fallB4Att = std::make_unique<SliderAtt>(processor.apvts, "lane4.curve.fallB", fallB4.length);

    if (paramExists("lane4.curv.riseA"))
        riseA4CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curv.riseA", riseA4.curve);
    if (paramExists("lane4.curv.fallA"))
        fallA4CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curv.fallA", fallA4.curve);
    if (paramExists("lane4.curv.riseB"))
        riseB4CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curv.riseB", riseB4.curve);
    if (paramExists("lane4.curv.fallB"))
        fallB4CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curv.fallB", fallB4.curve);

    // ---- Double-click reset behaviour (lane 4)
    for (auto *s : {&invertA4.slider, &invertB4.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&riseA4.curve, &fallA4.curve, &riseB4.curve, &fallB4.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK4.slider.setDoubleClickReturnValue(true, 0.0);

    // --- LANE 5 controls ----------------------------------------------------
    addAndMakeVisible(mixK5);
    configSlider(mixK5.slider, 0.0, 1.0, "");
    mix5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.mix", mixK5.slider);

    addAndMakeVisible(phaseK5);
    configSlider(phaseK5.slider, 0.0, 360.0, "°");
    phase5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.phaseDeg", phaseK5.slider);

    addAndMakeVisible(invertA5);
    configSlider(invertA5.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB5);
    configSlider(invertB5.slider, -1.0, 1.0, "");

    addAndMakeVisible(riseA5);
    configSlider(riseA5.length, 0.25, 4.0, "");
    configSlider(riseA5.curve, -1.0, 1.0, "");

    addAndMakeVisible(fallA5);
    configSlider(fallA5.length, 0.25, 4.0, "");
    configSlider(fallA5.curve, -1.0, 1.0, "");

    addAndMakeVisible(riseB5);
    configSlider(riseB5.length, 0.25, 4.0, "");
    configSlider(riseB5.curve, -1.0, 1.0, "");

    addAndMakeVisible(fallB5);
    configSlider(fallB5.length, 0.25, 4.0, "");
    configSlider(fallB5.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA5.slider, &invertB5.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&riseA5.curve, &fallA5.curve, &riseB5.curve, &fallB5.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK5.slider.setDoubleClickReturnValue(true, 0.0);

    // Attach to APVTS (lane 5)
    riseA5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.curve.riseA", riseA5.length);
    fallA5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.curve.fallA", fallA5.length);
    riseB5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.curve.riseB", riseB5.length);
    fallB5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.curve.fallB", fallB5.length);
    riseA5CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curv.riseA", riseA5.curve);
    fallA5CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curv.fallA", fallA5.curve);
    riseB5CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curv.riseB", riseB5.curve);
    fallB5CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curv.fallB", fallB5.curve);
    invertA5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.invertA", invertA5.slider);
    invertB5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.invertB", invertB5.slider);

    // --- LANE 6 controls (own knobs & attachments) -------------------------
    addAndMakeVisible(mixK6);
    configSlider(mixK6.slider, 0.0, 1.0, "");
    if (paramExists("lane6.mix"))
        mix6Att = std::make_unique<SliderAtt>(processor.apvts, "lane6.mix", mixK6.slider);

    addAndMakeVisible(phaseK6);
    configSlider(phaseK6.slider, 0.0, 360.0, "°");
    if (paramExists("lane6.phaseDeg"))
        phase6Att = std::make_unique<SliderAtt>(processor.apvts, "lane6.phaseDeg", phaseK6.slider);

    addAndMakeVisible(invertA6);
    configSlider(invertA6.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB6);
    configSlider(invertB6.slider, -1.0, 1.0, "");
    if (paramExists("lane6.invertA"))
        invertA6Att = std::make_unique<SliderAtt>(processor.apvts, "lane6.invertA", invertA6.slider);
    if (paramExists("lane6.invertB"))
        invertB6Att = std::make_unique<SliderAtt>(processor.apvts, "lane6.invertB", invertB6.slider);

    addAndMakeVisible(riseA6);
    configSlider(riseA6.length, 0.25, 4.0, "");
    configSlider(riseA6.curve, -1.0, 1.0, "");
    addAndMakeVisible(fallA6);
    configSlider(fallA6.length, 0.25, 4.0, "");
    configSlider(fallA6.curve, -1.0, 1.0, "");
    addAndMakeVisible(riseB6);
    configSlider(riseB6.length, 0.25, 4.0, "");
    configSlider(riseB6.curve, -1.0, 1.0, "");
    addAndMakeVisible(fallB6);
    configSlider(fallB6.length, 0.25, 4.0, "");
    configSlider(fallB6.curve, -1.0, 1.0, "");

    if (paramExists("lane6.curve.riseA"))
        riseA6Att = std::make_unique<SliderAtt>(processor.apvts, "lane6.curve.riseA", riseA6.length);
    if (paramExists("lane6.curve.fallA"))
        fallA6Att = std::make_unique<SliderAtt>(processor.apvts, "lane6.curve.fallA", fallA6.length);
    if (paramExists("lane6.curve.riseB"))
        riseB6Att = std::make_unique<SliderAtt>(processor.apvts, "lane6.curve.riseB", riseB6.length);
    if (paramExists("lane6.curve.fallB"))
        fallB6Att = std::make_unique<SliderAtt>(processor.apvts, "lane6.curve.fallB", fallB6.length);

    if (paramExists("lane6.curv.riseA"))
        riseA6CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curv.riseA", riseA6.curve);
    if (paramExists("lane6.curv.fallA"))
        fallA6CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curv.fallA", fallA6.curve);
    if (paramExists("lane6.curv.riseB"))
        riseB6CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curv.riseB", riseB6.curve);
    if (paramExists("lane6.curv.fallB"))
        fallB6CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curv.fallB", fallB6.curve);

    // ---- Double-click reset behaviour (lane 6)
    for (auto *s : {&invertA6.slider, &invertB6.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&riseA6.curve, &fallA6.curve, &riseB6.curve, &fallB6.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK6.slider.setDoubleClickReturnValue(true, 0.0);

    // --- Scopes -------------------------------------------------------------
    addAndMakeVisible(lane1Scope2);
    addAndMakeVisible(lane2Scope3);
    addAndMakeVisible(lane3Scope2);
    addAndMakeVisible(lane4Scope3);
    addAndMakeVisible(lane5Scope2);
    addAndMakeVisible(lane6Scope3);
    addAndMakeVisible(randomScope3);

    // **Drive scopes from processor (DSP truth) so curvature/invert apply**
    lane1Scope2.setEvaluator([this](float ph)
                             { return processor.evalLane1(ph); });
    lane2Scope3.setEvaluator([this](float ph)
                             { return processor.evalLane2Triplet(ph); });
    lane3Scope2.setEvaluator([this](float ph)
                             { return processor.evalLane3(ph); });
    lane4Scope3.setEvaluator([this](float ph)
                             { return processor.evalLane4Triplet(ph); });
    lane5Scope2.setEvaluator([this](float ph)
                             { return processor.evalLane5(ph); });
    lane6Scope3.setEvaluator([this](float ph)
                             { return processor.evalLane6Triplet(ph); });

    // Update scopes when any relevant knob changes
    auto upd1 = [this]
    { updateLane1Scope(); };
    auto upd2 = [this]
    { updateLane2Scope(); };
    auto upd3 = [this]
    { updateLane3Scope(); };
    auto upd4 = [this]
    { updateLane4Scope(); };
    auto upd5 = [this]
    { updateLane5Scope(); };
    auto upd6 = [this]
    { updateLane6Scope(); };

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

    // Lane 3 chain
    chainOnValue(riseA3.length, upd3);
    chainOnValue(riseA3.curve, upd3);
    chainOnValue(fallA3.length, upd3);
    chainOnValue(fallA3.curve, upd3);
    chainOnValue(riseB3.length, upd3);
    chainOnValue(riseB3.curve, upd3);
    chainOnValue(fallB3.length, upd3);
    chainOnValue(fallB3.curve, upd3);
    chainOnValue(invertA3.slider, upd3);
    chainOnValue(invertB3.slider, upd3);
    chainOnValue(phaseK3.slider, upd3);

    // Lane 4 chain
    chainOnValue(riseA4.length, upd4);
    chainOnValue(riseA4.curve, upd4);
    chainOnValue(fallA4.length, upd4);
    chainOnValue(fallA4.curve, upd4);
    chainOnValue(riseB4.length, upd4);
    chainOnValue(riseB4.curve, upd4);
    chainOnValue(fallB4.length, upd4);
    chainOnValue(fallB4.curve, upd4);
    chainOnValue(invertA4.slider, upd4);
    chainOnValue(invertB4.slider, upd4);
    chainOnValue(phaseK4.slider, upd4);

    // Lane 5 chain
    chainOnValue(riseA5.length, upd5);
    chainOnValue(riseA5.curve, upd5);
    chainOnValue(fallA5.length, upd5);
    chainOnValue(fallA5.curve, upd5);
    chainOnValue(riseB5.length, upd5);
    chainOnValue(riseB5.curve, upd5);
    chainOnValue(fallB5.length, upd5);
    chainOnValue(fallB5.curve, upd5);
    chainOnValue(invertA5.slider, upd5);
    chainOnValue(invertB5.slider, upd5);
    chainOnValue(phaseK5.slider, upd5);

    // Lane 6 chain
    chainOnValue(riseA6.length, upd6);
    chainOnValue(riseA6.curve, upd6);
    chainOnValue(fallA6.length, upd6);
    chainOnValue(fallA6.curve, upd6);
    chainOnValue(riseB6.length, upd6);
    chainOnValue(riseB6.curve, upd6);
    chainOnValue(fallB6.length, upd6);
    chainOnValue(fallB6.curve, upd6);
    chainOnValue(invertA6.slider, upd6);
    chainOnValue(invertB6.slider, upd6);
    chainOnValue(phaseK6.slider, upd6);

    // depth / phase nudge affect the mixed scope
    chainOnValue(depthK.slider, [this]
                 { updateOutputMixScope(); });
    chainOnValue(phaseNudgeK.slider, [this]
                 { updateOutputMixScope(); });

    // mixer faders + on/off toggles affect the mixed scope
    for (int i = 0; i < 8; ++i)
    {
        mixerFader[i].onValueChange = [this]
        { updateOutputMixScope(); };
        mixerOn[i].onClick = [this]
        { updateOutputMixScope(); };
    }

    laneTabs.setCurrentTabIndex(0, juce::NotificationType::dontSendNotification);
    updateLane1Scope();
    updateLane2Scope();
    updateLane3Scope();
    updateLane4Scope();
    updateLane5Scope();
    updateLane6Scope();
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
    // Right: Mixer card (remainder of the top row)
    auto row1 = bounds.removeFromTop(kCardH);
    auto outputArea = row1.removeFromLeft(int(row1.getWidth() * 0.58f));
    secOutput.setBounds(outputArea);

    auto mixerArea = row1; // whatever remains
    secMixer.setBounds(mixerArea);

    // Layout: labels, faders, mute dots
    auto m = mixerArea.reduced(16, 32);
    m.removeFromTop(4); // breathing room under section header
    // Row for labels
    auto labelsRow = m.removeFromTop(24);
    m.removeFromTop(8);

    // Compute columns
    const int cols = 8;
    const int gapX = 16;
    const int colW = (m.getWidth() - gapX * (cols - 1)) / cols;
    const int muteH = 22;
    const int faderH = juce::jmin(160, m.getHeight() - muteH - 12);

    auto cursorLabels = labelsRow;
    auto cursorCols = m;

    for (int i = 0; i < cols; ++i)
    {
        auto labelCol = cursorLabels.removeFromLeft(colW);
        mixerLbl[i].setBounds(labelCol);

        auto col = cursorCols.removeFromLeft(colW);
        if (i < cols - 1)
        {
            cursorLabels.removeFromLeft(gapX);
            cursorCols.removeFromLeft(gapX);
        }

        // Fader centered in column, above mute row
        auto faderRect = col.withTrimmedBottom(muteH + 8)
                             .withSizeKeepingCentre(colW, faderH)
                             .reduced(10, 4);
        mixerFader[i].setBounds(faderRect);

        // Mute dot at base
        auto muteArea = col.removeFromBottom(muteH);
        mixerOn[i].setBounds(muteArea.withSizeKeepingCentre(18, 18));
    }

    auto laneCardArea = bounds.removeFromTop(kLaneH);
    secLane.setBounds(laneCardArea);

    laneTabs.setBounds(laneCardArea.reduced(12, 12));
    const int tabH = laneTabs.getTabbedButtonBar().getHeight();

    // Output section layout
    {
        auto r = outputArea.reduced(16, 32); // inner content of the card
        const int knobW = kKnob, knobH = kKnob;
        const int y = r.getCentreY() - knobH / 2; // vertical center inside the card

        // scope takes the remaining right side, vertically centered around the knobs
        const int left = phaseNudgeK.getRight() + kGap * 3;
        auto scopeArea = juce::Rectangle<int>(left, r.getY(), r.getRight() - left, kKnob + 20)
                             .withCentre({(left + r.getRight()) / 2, r.getCentreY()});
        outputMixScope.setBounds(scopeArea);

        // left-aligned, vertically centered
        depthK.setBounds(r.getX(), y, knobW, knobH);
        phaseNudgeK.setBounds(r.getX() + knobW + kGap, y, knobW, knobH);

        r.removeFromTop(10);
    }

    // --- Tab content --------------------------------------------------------
    const int tab = laneTabs.getCurrentTabIndex();
    auto content = laneTabs.getBounds().reduced(16, 16).withTrimmedTop(tabH + 6);

    const bool lane1Visible = (tab == 0);
    const bool lane2Visible = (tab == 1);
    const bool lane3Visible = (tab == 2);
    const bool lane4Visible = (tab == 3);
    const bool lane5Visible = (tab == 4);
    const bool lane6Visible = (tab == 5);
    const bool randomVisible = (tab == 6);

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

    // LANE 3 controls visibility
    mixK3.setVisible(lane3Visible);
    phaseK3.setVisible(lane3Visible);
    invertA3.setVisible(lane3Visible);
    invertB3.setVisible(lane3Visible);
    riseA3.setVisible(lane3Visible);
    fallA3.setVisible(lane3Visible);
    riseB3.setVisible(lane3Visible);
    fallB3.setVisible(lane3Visible);
    lane3Scope2.setVisible(lane3Visible);

    // LANE 4 controls visibility
    mixK4.setVisible(lane4Visible);
    phaseK4.setVisible(lane4Visible);
    invertA4.setVisible(lane4Visible);
    invertB4.setVisible(lane4Visible);
    riseA4.setVisible(lane4Visible);
    fallA4.setVisible(lane4Visible);
    riseB4.setVisible(lane4Visible);
    fallB4.setVisible(lane4Visible);
    lane4Scope3.setVisible(lane4Visible);

    // LANE 5 controls visibility
    mixK5.setVisible(lane5Visible);
    phaseK5.setVisible(lane5Visible);
    invertA5.setVisible(lane5Visible);
    invertB5.setVisible(lane5Visible);
    riseA5.setVisible(lane5Visible);
    fallA5.setVisible(lane5Visible);
    riseB5.setVisible(lane5Visible);
    fallB5.setVisible(lane5Visible);
    lane5Scope2.setVisible(lane5Visible);

    // LANE 6 controls visibility
    mixK6.setVisible(lane6Visible);
    phaseK6.setVisible(lane6Visible);
    invertA6.setVisible(lane6Visible);
    invertB6.setVisible(lane6Visible);
    riseA6.setVisible(lane6Visible);
    fallA6.setVisible(lane6Visible);
    riseB6.setVisible(lane6Visible);
    fallB6.setVisible(lane6Visible);
    lane6Scope3.setVisible(lane6Visible);

    // Random (placeholders)
    randomRate.setVisible(randomVisible);
    randomXfadeK.setVisible(randomVisible);
    randomMixK.setVisible(randomVisible);
    randomScope3.setVisible(randomVisible);

    auto layoutLane = [&]()
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

        juce::Component *scopeView = nullptr;

        if (lane1Visible)
            scopeView = &lane1Scope2;
        else if (lane2Visible)
            scopeView = &lane2Scope3;
        else if (lane3Visible)
            scopeView = &lane3Scope2;
        else if (lane4Visible)
            scopeView = &lane4Scope3;
        else if (lane5Visible)
            scopeView = &lane5Scope2;
        else if (lane6Visible)
            scopeView = &lane6Scope3;
        else if (randomVisible)
            scopeView = &randomScope3;

        if (scopeView != nullptr)
            scopeView->setBounds(scope);

        addAndMakeVisible(outputMixScope);
        outputMixScope.setEvaluator([this](float ph01)
                                    {
                                        return processor.evalMixed(ph01); // coming in step C
                                    });

        // Row 0: Mix | Phase | Invert A | Invert B
        auto row0 = controls.removeFromTop(kKnob);
        auto placeTop = [&](Knob &k)
        {
            k.setBounds(row0.removeFromLeft(colW));
            row0.removeFromLeft(colGap);
        };

        if (lane1Visible)
        {
            placeTop(mixK1);
            placeTop(phaseK1);
            placeTop(invertA1);
            placeTop(invertB1);
        }
        else if (lane2Visible)
        {
            placeTop(mixK2);
            placeTop(phaseK2);
            placeTop(invertA2);
            placeTop(invertB2);
        }
        else if (lane3Visible)
        {
            placeTop(mixK3);
            placeTop(phaseK3);
            placeTop(invertA3);
            placeTop(invertB3);
        }
        else if (lane4Visible)
        {
            placeTop(mixK4);
            placeTop(phaseK4);
            placeTop(invertA4);
            placeTop(invertB4);
        }
        else if (lane5Visible)
        {
            placeTop(mixK5);
            placeTop(phaseK5);
            placeTop(invertA5);
            placeTop(invertB5);
        }
        else if (lane6Visible)
        {
            placeTop(mixK6);
            placeTop(phaseK6);
            placeTop(invertA6);
            placeTop(invertB6);
        }

        controls.removeFromTop(kGap);

        // Row 1: Rise A | Fall A | Rise B | Fall B
        auto rowDual = controls.removeFromTop(kDual);
        auto placeDual = [&](DualKnob &dk)
        {
            dk.setBounds(rowDual.removeFromLeft(colW));
            rowDual.removeFromLeft(colGap);
        };

        if (lane1Visible)
        {
            placeDual(riseA1);
            placeDual(fallA1);
            placeDual(riseB1);
            placeDual(fallB1);
        }
        else if (lane2Visible)
        {
            placeDual(riseA2);
            placeDual(fallA2);
            placeDual(riseB2);
            placeDual(fallB2);
        }
        else if (lane3Visible)
        {
            placeDual(riseA3);
            placeDual(fallA3);
            placeDual(riseB3);
            placeDual(fallB3);
        }
        else if (lane4Visible)
        {
            placeDual(riseA4);
            placeDual(fallA4);
            placeDual(riseB4);
            placeDual(fallB4);
        }
        else if (lane5Visible)
        {
            placeDual(riseA5);
            placeDual(fallA5);
            placeDual(riseB5);
            placeDual(fallB5);
        }
        else if (lane6Visible)
        {
            placeDual(riseA6);
            placeDual(fallA6);
            placeDual(riseB6);
            placeDual(fallB6);
        }
    };

    if (lane1Visible || lane2Visible || lane3Visible || lane4Visible || lane5Visible || lane6Visible)
        layoutLane();
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

void PinkELFOntsAudioProcessorEditor::updateLane3Scope()
{
    // evaluator pulls from APVTS in processor; just repaint
    lane3Scope2.repaint();
}

void PinkELFOntsAudioProcessorEditor::updateLane4Scope()
{
    // evaluator pulls from APVTS in processor; just repaint
    lane4Scope3.repaint();
}

void PinkELFOntsAudioProcessorEditor::updateLane5Scope()
{
    // evaluator pulls from APVTS in processor; just repaint
    lane5Scope2.repaint();
}

void PinkELFOntsAudioProcessorEditor::updateLane6Scope()
{
    // evaluator pulls from APVTS in processor; just repaint
    lane6Scope3.repaint();
}

void PinkELFOntsAudioProcessorEditor::updateOutputMixScope()
{
    outputMixScope.repaint();
}
