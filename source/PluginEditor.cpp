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

    // Slope / Curve
    addAndMakeVisible(slopeK);

    // Outer (slope amount): 0..1, default 0.5 (flat)
    configSlider(slopeK.length, 0.0, 1.0, "");
    slopeK.length.setDoubleClickReturnValue(true, 0.5);

    // Inner (curve): 0..1, default 0.5 (linear)
    configSlider(slopeK.curve, 0.0, 1.0, "");
    slopeK.curve.setDoubleClickReturnValue(true, 0.5);

    // Attach
    slopeLenAtt = std::make_unique<SliderAtt>(processor.apvts, "output.slope", slopeK.length);
    slopeCurveAtt = std::make_unique<SliderAtt>(processor.apvts, "output.slopeCurve", slopeK.curve);

    // --- LANE 1 controls ----------------------------------------------------
    addAndMakeVisible(phaseK1);
    configSlider(phaseK1.slider, 0.0, 360.0, "°");
    phase1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.phaseDeg", phaseK1.slider);

    addAndMakeVisible(invertA1);
    configSlider(invertA1.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB1);
    configSlider(invertB1.slider, -1.0, 1.0, "");

    addAndMakeVisible(timeA1);
    configSlider(timeA1.length, 0.25, 4.0, "");
    configSlider(timeA1.curve, -1.0, 1.0, "");

    addAndMakeVisible(timeB1);
    configSlider(timeB1.length, 0.25, 4.0, "");
    configSlider(timeB1.curve, -1.0, 1.0, "");

    addAndMakeVisible(intensityA1);
    configSlider(intensityA1.length, 0.0, 1.0, "");
    configSlider(intensityA1.curve, -1.0, 1.0, "");

    addAndMakeVisible(intensityB1);
    configSlider(intensityB1.length, 0.0, 1.0, "");
    configSlider(intensityB1.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA1.slider, &invertB1.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&timeA1.curve, &timeB1.curve, &intensityA1.curve, &intensityB1.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK1.slider.setDoubleClickReturnValue(true, 0.0);

    // --- Attach to APVTS (lane 1) ----------------------------------------------
    // Phase / invert
    phase1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.phaseDeg", phaseK1.slider);
    invertA1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.invertA", invertA1.slider);
    invertB1Att = std::make_unique<SliderAtt>(processor.apvts, "lane1.invertB", invertB1.slider);

    // Time A replaces (riseA + fallA): drive both lengths + both curvature params
    timeA1LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.riseA", timeA1.length);
    timeA1LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallA", timeA1.length); // mirror
    timeA1CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.riseA", timeA1.curve);

    // Time B replaces (riseB + fallB)
    timeB1LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1riseB", timeB1.length);
    timeB1LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curve.fallB", timeB1.length); // mirror
    timeB1CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.fallA", timeB1.curve);

    // Intensities: outer length = amplitude per half; inner curve drives only the FALL edge
    intensityA1LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.intensityA", intensityA1.length);
    intensityA1CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.riseB", intensityA1.curve);

    intensityB1LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.intensityB", intensityB1.length);
    intensityB1CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane1.curv.fallB", intensityB1.curve);

    // --- LANE 2 controls ----------------------------------------------------
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

    addAndMakeVisible(timeA2);
    configSlider(timeA2.length, 0.25, 4.0, "");
    configSlider(timeA2.curve, -1.0, 1.0, "");
    addAndMakeVisible(timeB2);
    configSlider(timeB2.length, 0.25, 4.0, "");
    configSlider(timeB2.curve, -1.0, 1.0, "");

    addAndMakeVisible(intensityA2);
    configSlider(intensityA2.length, 0.0, 1.0, "");
    configSlider(intensityA2.curve, -1.0, 1.0, "");
    addAndMakeVisible(intensityB2);
    configSlider(intensityB2.length, 0.0, 1.0, "");
    configSlider(intensityB2.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA2.slider, &invertB2.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&timeA2.curve, &timeB2.curve, &intensityA2.curve, &intensityB2.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK2.slider.setDoubleClickReturnValue(true, 0.0);

    // --- Attach to APVTS (lane 2) ------------------------------------------
    timeA2LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.riseA", timeA2.length);
    timeA2LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curve.fallA", timeA2.length); // mirror length
    timeA2CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curv.riseA", timeA2.curve);

    timeB2LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.riseB", timeB2.length);
    timeB2LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curve.fallB", timeB2.length); // mirror length
    timeB2CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curv.fallA", timeB2.curve);

    intensityA2LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.intensityA", intensityA2.length);
    intensityA2CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curv.riseB", intensityA2.curve);

    intensityB2LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.intensityB", intensityB2.length);
    intensityB2CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane2.curv.fallB", intensityB2.curve);

    // --- LANE 3 controls ----------------------------------------------------
    addAndMakeVisible(phaseK3);
    configSlider(phaseK3.slider, 0.0, 360.0, "°");
    if (paramExists("lane3.phaseDeg"))
        phase3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.phaseDeg", phaseK3.slider);

    addAndMakeVisible(invertA3);
    configSlider(invertA3.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB3);
    configSlider(invertB3.slider, -1.0, 1.0, "");
    if (paramExists("lane3.invertA"))
        invertA3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.invertA", invertA3.slider);
    if (paramExists("lane3.invertB"))
        invertB3Att = std::make_unique<SliderAtt>(processor.apvts, "lane3.invertB", invertB3.slider);

    addAndMakeVisible(timeA3);
    configSlider(timeA3.length, 0.25, 4.0, "");
    configSlider(timeA3.curve, -1.0, 1.0, "");
    addAndMakeVisible(timeB3);
    configSlider(timeB3.length, 0.25, 4.0, "");
    configSlider(timeB3.curve, -1.0, 1.0, "");

    addAndMakeVisible(intensityA3);
    configSlider(intensityA3.length, 0.0, 1.0, "");
    configSlider(intensityA3.curve, -1.0, 1.0, "");
    addAndMakeVisible(intensityB3);
    configSlider(intensityB3.length, 0.0, 1.0, "");
    configSlider(intensityB3.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA3.slider, &invertB3.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&timeA3.curve, &timeB3.curve, &intensityA3.curve, &intensityB3.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK3.slider.setDoubleClickReturnValue(true, 0.0);

    // --- Attach to APVTS (lane 3) ------------------------------------------
    timeA3LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.riseA", timeA3.length);
    timeA3LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curve.fallA", timeA3.length);
    timeA3CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curv.riseA", timeA3.curve);

    timeB3LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.riseB", timeB3.length);
    timeB3LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curve.fallB", timeB3.length);
    timeB3CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curv.fallA", timeB3.curve);

    intensityA3LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.intensityA", intensityA3.length);
    intensityA3CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curv.riseB", intensityA3.curve);

    intensityB3LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.intensityB", intensityB3.length);
    intensityB3CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane3.curv.fallB", intensityB3.curve);

    // --- LANE 4 controls ----------------------------------------------------
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

    addAndMakeVisible(timeA4);
    configSlider(timeA4.length, 0.25, 4.0, "");
    configSlider(timeA4.curve, -1.0, 1.0, "");
    addAndMakeVisible(timeB4);
    configSlider(timeB4.length, 0.25, 4.0, "");
    configSlider(timeB4.curve, -1.0, 1.0, "");

    addAndMakeVisible(intensityA4);
    configSlider(intensityA4.length, 0.0, 1.0, "");
    configSlider(intensityA4.curve, -1.0, 1.0, "");
    addAndMakeVisible(intensityB4);
    configSlider(intensityB4.length, 0.0, 1.0, "");
    configSlider(intensityB4.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA4.slider, &invertB4.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&timeA4.curve, &timeB4.curve, &intensityA4.curve, &intensityB4.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK4.slider.setDoubleClickReturnValue(true, 0.0);

    // --- Attach to APVTS (lane 4) ------------------------------------------
    timeA4LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.riseA", timeA4.length);
    timeA4LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curve.fallA", timeA4.length);
    timeA4CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curv.riseA", timeA4.curve);

    timeB4LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.riseB", timeB4.length);
    timeB4LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curve.fallB", timeB4.length);
    timeB4CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curv.fallA", timeB4.curve);

    intensityA4LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.intensityA", intensityA4.length);
    intensityA4CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curv.riseB", intensityA4.curve);

    intensityB4LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.intensityB", intensityB4.length);
    intensityB4CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane4.curv.fallB", intensityB4.curve);

    // --- LANE 5 controls ----------------------------------------------------
    addAndMakeVisible(phaseK5);
    configSlider(phaseK5.slider, 0.0, 360.0, "°");
    if (paramExists("lane5.phaseDeg"))
        phase5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.phaseDeg", phaseK5.slider);

    addAndMakeVisible(invertA5);
    configSlider(invertA5.slider, -1.0, 1.0, "");
    addAndMakeVisible(invertB5);
    configSlider(invertB5.slider, -1.0, 1.0, "");
    if (paramExists("lane5.invertA"))
        invertA5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.invertA", invertA5.slider);
    if (paramExists("lane5.invertB"))
        invertB5Att = std::make_unique<SliderAtt>(processor.apvts, "lane5.invertB", invertB5.slider);

    addAndMakeVisible(timeA5);
    configSlider(timeA5.length, 0.25, 4.0, "");
    configSlider(timeA5.curve, -1.0, 1.0, "");
    addAndMakeVisible(timeB5);
    configSlider(timeB5.length, 0.25, 4.0, "");
    configSlider(timeB5.curve, -1.0, 1.0, "");

    addAndMakeVisible(intensityA5);
    configSlider(intensityA5.length, 0.0, 1.0, "");
    configSlider(intensityA5.curve, -1.0, 1.0, "");
    addAndMakeVisible(intensityB5);
    configSlider(intensityB5.length, 0.0, 1.0, "");
    configSlider(intensityB5.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA5.slider, &invertB5.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&timeA5.curve, &timeB5.curve, &intensityA5.curve, &intensityB5.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK5.slider.setDoubleClickReturnValue(true, 0.0);

    // --- Attach to APVTS (lane 5) ------------------------------------------
    timeA5LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.riseA", timeA5.length);
    timeA5LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curve.fallA", timeA5.length);
    timeA5CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curv.riseA", timeA5.curve);

    timeB5LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.riseB", timeB5.length);
    timeB5LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curve.fallB", timeB5.length);
    timeB5CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curv.fallA", timeB5.curve);

    intensityA5LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.intensityA", intensityA5.length);
    intensityA5CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curv.riseB", intensityA5.curve);

    intensityB5LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.intensityB", intensityB5.length);
    intensityB5CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane5.curv.fallB", intensityB5.curve);

    // --- LANE 6 controls ----------------------------------------------------
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

    addAndMakeVisible(timeA6);
    configSlider(timeA6.length, 0.25, 4.0, "");
    configSlider(timeA6.curve, -1.0, 1.0, "");
    addAndMakeVisible(timeB6);
    configSlider(timeB6.length, 0.25, 4.0, "");
    configSlider(timeB6.curve, -1.0, 1.0, "");

    addAndMakeVisible(intensityA6);
    configSlider(intensityA6.length, 0.0, 1.0, "");
    configSlider(intensityA6.curve, -1.0, 1.0, "");
    addAndMakeVisible(intensityB6);
    configSlider(intensityB6.length, 0.0, 1.0, "");
    configSlider(intensityB6.curve, -1.0, 1.0, "");

    // Double-click resets
    for (auto *s : {&invertA6.slider, &invertB6.slider})
        s->setDoubleClickReturnValue(true, 0.0);
    for (auto *s : {&timeA6.curve, &timeB6.curve, &intensityA6.curve, &intensityB6.curve})
        s->setDoubleClickReturnValue(true, 0.0);
    phaseK6.slider.setDoubleClickReturnValue(true, 0.0);

    // --- Attach to APVTS (lane 6) ------------------------------------------
    timeA6LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.riseA", timeA6.length);
    timeA6LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curve.fallA", timeA6.length);
    timeA6CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curv.riseA", timeA6.curve);

    timeB6LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.riseB", timeB6.length);
    timeB6LenFallAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curve.fallB", timeB6.length);
    timeB6CurveRiseAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curv.fallA", timeB6.curve);

    intensityA6LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.intensityA", intensityA6.length);
    intensityA6CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curv.riseB", intensityA6.curve);

    intensityB6LenAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.intensityB", intensityB6.length);
    intensityB6CurveAtt = std::make_unique<SliderAtt>(processor.apvts, "lane6.curv.fallB", intensityB6.curve);

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
    { updateLane1Scope(); updateOutputMixScope(); };
    auto upd2 = [this]
    { updateLane2Scope();  updateOutputMixScope(); };
    auto upd3 = [this]
    { updateLane3Scope(); updateOutputMixScope(); };
    auto upd4 = [this]
    { updateLane4Scope();  updateOutputMixScope(); };
    auto upd5 = [this]
    { updateLane5Scope(); updateOutputMixScope(); };
    auto upd6 = [this]
    { updateLane6Scope(); updateOutputMixScope(); };

    // Lane 1 chain
    chainOnValue(timeA1.length, upd1);
    chainOnValue(timeA1.curve, upd1);
    chainOnValue(timeB1.length, upd1);
    chainOnValue(timeB1.curve, upd1);
    chainOnValue(intensityA1.length, upd1);
    chainOnValue(intensityA1.curve, upd1);
    chainOnValue(intensityB1.length, upd1);
    chainOnValue(intensityB1.curve, upd1);
    chainOnValue(invertA1.slider, upd1);
    chainOnValue(invertB1.slider, upd1);
    chainOnValue(phaseK1.slider, upd1);

    // Lane 2 chain
    chainOnValue(timeA2.length, upd2);
    chainOnValue(timeA2.curve, upd2);
    chainOnValue(timeB2.length, upd2);
    chainOnValue(timeB2.curve, upd2);
    chainOnValue(intensityA2.length, upd2);
    chainOnValue(intensityA2.curve, upd2);
    chainOnValue(intensityB2.length, upd2);
    chainOnValue(intensityB2.curve, upd2);
    chainOnValue(invertA2.slider, upd2);
    chainOnValue(invertB2.slider, upd2);
    chainOnValue(phaseK2.slider, upd2);

    // Lane 3 chain
    chainOnValue(timeA3.length, upd3);
    chainOnValue(timeA3.curve, upd3);
    chainOnValue(timeB3.length, upd3);
    chainOnValue(timeB3.curve, upd3);
    chainOnValue(intensityA3.length, upd3);
    chainOnValue(intensityA3.curve, upd3);
    chainOnValue(intensityB3.length, upd3);
    chainOnValue(intensityB3.curve, upd3);
    chainOnValue(invertA3.slider, upd3);
    chainOnValue(invertB3.slider, upd3);
    chainOnValue(phaseK3.slider, upd3);

    // Lane 4 chain
    chainOnValue(timeA4.length, upd4);
    chainOnValue(timeA4.curve, upd4);
    chainOnValue(timeB4.length, upd4);
    chainOnValue(timeB4.curve, upd4);
    chainOnValue(intensityA4.length, upd4);
    chainOnValue(intensityA4.curve, upd4);
    chainOnValue(intensityB4.length, upd4);
    chainOnValue(intensityB4.curve, upd4);
    chainOnValue(invertA4.slider, upd4);
    chainOnValue(invertB4.slider, upd4);
    chainOnValue(phaseK4.slider, upd4);

    // Lane 5 chain
    chainOnValue(timeA5.length, upd5);
    chainOnValue(timeA5.curve, upd5);
    chainOnValue(timeB5.length, upd5);
    chainOnValue(timeB5.curve, upd5);
    chainOnValue(intensityA5.length, upd5);
    chainOnValue(intensityA5.curve, upd5);
    chainOnValue(intensityB5.length, upd5);
    chainOnValue(intensityB5.curve, upd5);
    chainOnValue(invertA5.slider, upd5);
    chainOnValue(invertB5.slider, upd5);
    chainOnValue(phaseK5.slider, upd5);

    // Lane 6 chain
    chainOnValue(timeA6.length, upd6);
    chainOnValue(timeA6.curve, upd6);
    chainOnValue(timeB6.length, upd6);
    chainOnValue(timeB6.curve, upd6);
    chainOnValue(intensityA6.length, upd6);
    chainOnValue(intensityA6.curve, upd6);
    chainOnValue(intensityB6.length, upd6);
    chainOnValue(intensityB6.curve, upd6);
    chainOnValue(invertA6.slider, upd6);
    chainOnValue(invertB6.slider, upd6);
    chainOnValue(phaseK6.slider, upd6);

    // depth / phase nudge / slope affect the mixed scope
    chainOnValue(depthK.slider, [this]
                 { updateOutputMixScope(); });
    chainOnValue(phaseNudgeK.slider, [this]
                 { updateOutputMixScope(); });
    chainOnValue(slopeK.length, [this]
                 { updateOutputMixScope(); });
    chainOnValue(slopeK.curve, [this]
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

    // ===== Output section layout (top row nudged; scope aspect preserved) =====
    {
        auto r = outputArea.reduced(16, 18); // inner padding

        constexpr int knobW = kKnob, knobH = kKnob;
        constexpr int dual = kDual;
        constexpr int gap = kGap;
        constexpr float kScopeAspect = 2.40f;
        constexpr int topNudgePx = 12; // push Depth/Phase down a bit

        // Left column wide enough for big dual knob
        const int leftColW = std::max(2 * knobW + gap, dual) + 16;
        auto leftCol = r.removeFromLeft(leftColW);

        // --- Anchor Slope / Curve at the bottom (unchanged position)
        auto slopeRow = leftCol.removeFromBottom(dual);
        slopeK.setBounds(slopeRow.withSizeKeepingCentre(dual, dual));

        // --- Top row: Depth | Phase Nudge (moved slightly lower)
        leftCol.removeFromTop(topNudgePx); // extra headroom for labels
        auto rowTop = leftCol.removeFromTop(knobH);
        depthK.setBounds(rowTop.removeFromLeft(knobW));
        rowTop.removeFromLeft(gap);
        phaseNudgeK.setBounds(rowTop.removeFromLeft(knobW));

        // --- Scope on the right, fixed aspect ratio & vertically centered
        r.removeFromLeft(gap * 2); // breathing room
        const int availW = std::max(0, r.getWidth());
        const int availH = std::max(0, r.getHeight());
        int scopeH = std::min(availH, (int)std::floor(availW / kScopeAspect));
        int scopeW = (int)std::floor(scopeH * kScopeAspect);
        auto scopeArea = juce::Rectangle<int>(0, 0, scopeW, scopeH).withCentre(r.getCentre());
        outputMixScope.setBounds(scopeArea);
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
    phaseK1.setVisible(lane1Visible);
    invertA1.setVisible(lane1Visible);
    invertB1.setVisible(lane1Visible);
    timeA1.setVisible(lane1Visible);
    timeB1.setVisible(lane1Visible);
    intensityA1.setVisible(lane1Visible);
    intensityB1.setVisible(lane1Visible);
    lane1Scope2.setVisible(lane1Visible);

    // LANE 2 controls visibility
    phaseK2.setVisible(lane2Visible);
    invertA2.setVisible(lane2Visible);
    invertB2.setVisible(lane2Visible);
    timeA2.setVisible(lane2Visible);
    timeB2.setVisible(lane2Visible);
    intensityA2.setVisible(lane2Visible);
    intensityB2.setVisible(lane2Visible);
    lane2Scope3.setVisible(lane2Visible);

    // LANE 3 controls visibility
    phaseK3.setVisible(lane3Visible);
    invertA3.setVisible(lane3Visible);
    invertB3.setVisible(lane3Visible);
    timeA3.setVisible(lane3Visible);
    timeB3.setVisible(lane3Visible);
    intensityA3.setVisible(lane3Visible);
    intensityB3.setVisible(lane3Visible);
    lane3Scope2.setVisible(lane3Visible);

    // LANE 4 controls visibility
    phaseK4.setVisible(lane4Visible);
    invertA4.setVisible(lane4Visible);
    invertB4.setVisible(lane4Visible);
    timeA4.setVisible(lane4Visible);
    timeB4.setVisible(lane4Visible);
    intensityA4.setVisible(lane4Visible);
    intensityB4.setVisible(lane4Visible);
    lane4Scope3.setVisible(lane4Visible);

    // LANE 5 controls visibility
    phaseK5.setVisible(lane5Visible);
    invertA5.setVisible(lane5Visible);
    invertB5.setVisible(lane5Visible);
    timeA5.setVisible(lane5Visible);
    timeB5.setVisible(lane5Visible);
    intensityA5.setVisible(lane5Visible);
    intensityB5.setVisible(lane5Visible);
    lane5Scope2.setVisible(lane5Visible);

    // LANE 6 controls visibility
    phaseK6.setVisible(lane6Visible);
    invertA6.setVisible(lane6Visible);
    invertB6.setVisible(lane6Visible);
    timeA6.setVisible(lane6Visible);
    timeB6.setVisible(lane6Visible);
    intensityA6.setVisible(lane6Visible);
    intensityB6.setVisible(lane6Visible);
    lane6Scope3.setVisible(lane6Visible);

    // Random (placeholders)
    randomRate.setVisible(randomVisible);
    randomXfadeK.setVisible(randomVisible);
    randomMixK.setVisible(randomVisible);
    randomScope3.setVisible(randomVisible);

    auto layoutLane = [&]()
    {
        auto r = content;

        // Left controls / Right scope
        const int cols = 4;
        const int colW = kDual;
        const int colGap = kGap;
        const int gridW = cols * colW + (cols - 1) * colGap;

        const int controlsW = gridW + 4;
        auto controls = r.removeFromLeft(controlsW);
        auto scope = r.reduced(8, 6);

        // Pick the scope for the visible lane
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

        // (If you show the global/output scope here, keep this; otherwise remove)
        addAndMakeVisible(outputMixScope);
        outputMixScope.setEvaluator([this](float ph01)
                                    { return processor.evalMixed(ph01); });

        // draw the output slope/curve as a soft green overlay
        outputMixScope.setOverlayEvaluator(
            [this](float ph01)
            { return processor.evalSlopeOnly(ph01); },
            juce::Colour::fromFloatRGBA(0.55f, 0.95f, 0.75f, 0.70f) // soft green, semi-transparent
        );

        // ---------------- Row 0: Phase | Invert A | Invert B (centered) ----------
        auto row0 = controls.removeFromTop(kKnob);
        auto placeTop = [&](Knob &k)
        {
            k.setBounds(row0.removeFromLeft(colW));
            row0.removeFromLeft(colGap);
        };

        // Leave one slot empty to center 3 knobs in a 4-slot row
        row0.removeFromLeft(colW + colGap);

        if (lane1Visible)
        {
            placeTop(phaseK1);
            placeTop(invertA1);
            placeTop(invertB1);
        }
        else if (lane2Visible)
        {
            placeTop(phaseK2);
            placeTop(invertA2);
            placeTop(invertB2);
        }
        else if (lane3Visible)
        {
            placeTop(phaseK3);
            placeTop(invertA3);
            placeTop(invertB3);
        }
        else if (lane4Visible)
        {
            placeTop(phaseK4);
            placeTop(invertA4);
            placeTop(invertB4);
        }
        else if (lane5Visible)
        {
            placeTop(phaseK5);
            placeTop(invertA5);
            placeTop(invertB5);
        }
        else if (lane6Visible)
        {
            placeTop(phaseK6);
            placeTop(invertA6);
            placeTop(invertB6);
        }

        controls.removeFromTop(kGap);

        // ------ Row 1: Time A | Time B | Intensity A | Intensity B ----------
        auto rowDual = controls.removeFromTop(kDual);
        auto placeDual = [&](DualKnob &dk)
        {
            dk.setBounds(rowDual.removeFromLeft(colW));
            rowDual.removeFromLeft(colGap);
        };

        if (lane1Visible)
        {
            placeDual(timeA1);
            placeDual(timeB1);
            placeDual(intensityA1);
            placeDual(intensityB1);
        }
        else if (lane2Visible)
        {
            placeDual(timeA2);
            placeDual(timeB2);
            placeDual(intensityA2);
            placeDual(intensityB2);
        }
        else if (lane3Visible)
        {
            placeDual(timeA3);
            placeDual(timeB3);
            placeDual(intensityA3);
            placeDual(intensityB3);
        }
        else if (lane4Visible)
        {
            placeDual(timeA4);
            placeDual(timeB4);
            placeDual(intensityA4);
            placeDual(intensityB4);
        }
        else if (lane5Visible)
        {
            placeDual(timeA5);
            placeDual(timeB5);
            placeDual(intensityA5);
            placeDual(intensityB5);
        }
        else if (lane6Visible)
        {
            placeDual(timeA6);
            placeDual(timeB6);
            placeDual(intensityA6);
            placeDual(intensityB6);
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
