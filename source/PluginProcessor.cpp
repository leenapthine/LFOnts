#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// ===== Parameter layout =====
PinkELFOntsAudioProcessor::APVTS::ParameterLayout
PinkELFOntsAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // ---- Global ----
    params.push_back(std::make_unique<AudioParameterFloat>(
        "global.depth", "Global Depth",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "global.phaseNudgeDeg", "Phase Nudge (deg)",
        NormalisableRange<float>(-30.0f, 30.0f, 0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<AudioParameterChoice>(
        "global.retrig", "Retrig Mode",
        StringArray{"Continuous", "Every Note", "First Note Only"}, 0));

    // ---- Lane 1 (¼ note) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane1.enabled", "Lane 1 Enabled", true)); // default ON

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.mix", "Lane 1 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.phaseDeg", "Lane 1 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // Length (A/B rise/fall)
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curve.riseA", "Lane 1 Rise A",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curve.fallA", "Lane 1 Fall A",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curve.riseB", "Lane 1 Rise B",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curve.fallB", "Lane 1 Fall B",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));

    // Curvature (inner rings)  [-1..1]
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curv.riseA", "Lane 1 Curv Rise A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curv.fallA", "Lane 1 Curv Fall A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curv.riseB", "Lane 1 Curv Rise B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curv.fallB", "Lane 1 Curv Fall B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // Invert knobs [-1..1], we’ll use |value| in DSP
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.invertA", "Lane 1 Invert A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.invertB", "Lane 1 Invert B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // ---- Lane 2 (¼ triplet — three triangles per 2 beats) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane2.enabled", "Lane 2 Enabled", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane2.mix", "Lane 2 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane2.phaseDeg", "Lane 2 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // Length (A/B) + Curvature + Invert (mirrors lane 1)
    for (auto id : {std::pair{"riseA", "Rise A"}, std::pair{"fallA", "Fall A"},
                    std::pair{"riseB", "Rise B"}, std::pair{"fallB", "Fall B"}})
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
            "lane2.curve." + juce::String(id.first), "Lane 2 " + juce::String(id.second),
            NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    }
    for (auto id : {std::pair{"riseA", "Curv Rise A"}, std::pair{"fallA", "Curv Fall A"},
                    std::pair{"riseB", "Curv Rise B"}, std::pair{"fallB", "Curv Fall B"}})
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
            "lane2.curv." + juce::String(id.first), "Lane 2 " + juce::String(id.second),
            NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    }
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane2.invertA", "Lane 2 Invert A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane2.invertB", "Lane 2 Invert B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // ---- Lane 3 (1/8 note) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane3.enabled", "Lane 3 Enabled", false)); // default Off

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.mix", "Lane 3 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.phaseDeg", "Lane 3 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // Length (A/B rise/fall)
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curve.riseA", "Lane 3 Rise A",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curve.fallA", "Lane 3 Fall A",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curve.riseB", "Lane 3 Rise B",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curve.fallB", "Lane 3 Fall B",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));

    // Curvature (inner rings)  [-1..1]
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curv.riseA", "Lane 3 Curv Rise A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curv.fallA", "Lane 3 Curv Fall A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curv.riseB", "Lane 3 Curv Rise B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curv.fallB", "Lane 3 Curv Fall B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // Invert knobs [-1..1], we’ll use |value| in DSP
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.invertA", "Lane 3 Invert A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.invertB", "Lane 3 Invert B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // ---- Lane 4 (1/8 triplet — three triangles per 2 beats) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane4.enabled", "Lane 4 Enabled", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane4.mix", "Lane 4 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane4.phaseDeg", "Lane 4 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // Length (A/B) + Curvature + Invert (mirrors lane 1)
    for (auto id : {std::pair{"riseA", "Rise A"}, std::pair{"fallA", "Fall A"},
                    std::pair{"riseB", "Rise B"}, std::pair{"fallB", "Fall B"}})
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
            "lane4.curve." + juce::String(id.first), "Lane 4 " + juce::String(id.second),
            NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    }
    for (auto id : {std::pair{"riseA", "Curv Rise A"}, std::pair{"fallA", "Curv Fall A"},
                    std::pair{"riseB", "Curv Rise B"}, std::pair{"fallB", "Curv Fall B"}})
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
            "lane4.curv." + juce::String(id.first), "Lane 4 " + juce::String(id.second),
            NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    }
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane4.invertA", "Lane 4 Invert A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane4.invertB", "Lane 4 Invert B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // ---- Lane 5 (1/16 note) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane5.enabled", "Lane 5 Enabled", false)); // default Off

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.mix", "Lane 5 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.phaseDeg", "Lane 5 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // Length (A/B rise/fall)
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curve.riseA", "Lane 5 Rise A",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curve.fallA", "Lane 5 Fall A",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curve.riseB", "Lane 5 Rise B",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curve.fallB", "Lane 5 Fall B",
        NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));

    // Curvature (inner rings)  [-1..1]
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curv.riseA", "Lane 5 Curv Rise A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curv.fallA", "Lane 5 Curv Fall A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curv.riseB", "Lane 5 Curv Rise B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curv.fallB", "Lane 5 Curv Fall B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // Invert knobs [-1..1], we’ll use |value| in DSP
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.invertA", "Lane 5 Invert A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.invertB", "Lane 5 Invert B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // ---- Lane 6 (1/16 triplet — three triangles per 2 beats) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane6.enabled", "Lane 6 Enabled", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane6.mix", "Lane 6 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane6.phaseDeg", "Lane 6 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // Length (A/B) + Curvature + Invert (mirrors lane 1)
    for (auto id : {std::pair{"riseA", "Rise A"}, std::pair{"fallA", "Fall A"},
                    std::pair{"riseB", "Rise B"}, std::pair{"fallB", "Fall B"}})
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
            "lane6.curve." + juce::String(id.first), "Lane 6 " + juce::String(id.second),
            NormalisableRange<float>(0.25f, 4.0f, 0.0f, 1.0f), 1.0f));
    }
    for (auto id : {std::pair{"riseA", "Curv Rise A"}, std::pair{"fallA", "Curv Fall A"},
                    std::pair{"riseB", "Curv Rise B"}, std::pair{"fallB", "Curv Fall B"}})
    {
        params.push_back(std::make_unique<AudioParameterFloat>(
            "lane6.curv." + juce::String(id.first), "Lane 6 " + juce::String(id.second),
            NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    }
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane6.invertA", "Lane 6 Invert A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane6.invertB", "Lane 6 Invert B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // ---- Random lane (placeholders) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "random.enabled", "Random Enabled", false));
    params.push_back(std::make_unique<AudioParameterChoice>(
        "random.rate", "Random Rate",
        StringArray{"1/4", "1/8", "1/16"}, 0));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "random.crossfadeMs", "Random Crossfade (ms)",
        NormalisableRange<float>(5.0f, 80.0f, 0.0f, 1.0f), 20.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "random.mix", "Random Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));

    return {params.begin(), params.end()};
}

// ===== Boilerplate =====
PinkELFOntsAudioProcessor::PinkELFOntsAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::mono(), true))
{
    playHead = getPlayHead();
}

void PinkELFOntsAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRateHz = sampleRate;
    lane1Phase01 = 0.0;
    lane2Phase01 = 0.0;
    lane3Phase01 = 0.0;
    lane4Phase01 = 0.0;
    lane5Phase01 = 0.0;
    lane6Phase01 = 0.0;
    carrierPhase = 0.0;
}

void PinkELFOntsAudioProcessor::updateTransportInfo()
{
    if (auto *ph = getPlayHead())
        ph->getCurrentPosition(posInfo);
}

// ==================== LFO helpers ====================

LFO::Shape PinkELFOntsAudioProcessor::makeLane1Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths
    s.riseA = (float)*p.getRawParameterValue("lane1.curve.riseA");
    s.fallA = (float)*p.getRawParameterValue("lane1.curve.fallA");
    s.riseB = (float)*p.getRawParameterValue("lane1.curve.riseB");
    s.fallB = (float)*p.getRawParameterValue("lane1.curve.fallB");

    // Curvatures [-1..1]
    s.curvRiseA = (float)*p.getRawParameterValue("lane1.curv.riseA");
    s.curvFallA = (float)*p.getRawParameterValue("lane1.curv.fallA");
    s.curvRiseB = (float)*p.getRawParameterValue("lane1.curv.riseB");
    s.curvFallB = (float)*p.getRawParameterValue("lane1.curv.fallB");

    // Invert: take absolute value — center (0) = no inversion; ends = full inversion
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane1.invertA")));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane1.invertB")));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane2Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths
    s.riseA = (float)*p.getRawParameterValue("lane2.curve.riseA");
    s.fallA = (float)*p.getRawParameterValue("lane2.curve.fallA");
    s.riseB = (float)*p.getRawParameterValue("lane2.curve.riseB");
    s.fallB = (float)*p.getRawParameterValue("lane2.curve.fallB");

    // Curvatures [-1..1]
    s.curvRiseA = (float)*p.getRawParameterValue("lane2.curv.riseA");
    s.curvFallA = (float)*p.getRawParameterValue("lane2.curv.fallA");
    s.curvRiseB = (float)*p.getRawParameterValue("lane2.curv.riseB");
    s.curvFallB = (float)*p.getRawParameterValue("lane2.curv.fallB");

    // Invert is processed as |value|
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane2.invertA")));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane2.invertB")));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane3Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths
    s.riseA = (float)*p.getRawParameterValue("lane3.curve.riseA");
    s.fallA = (float)*p.getRawParameterValue("lane3.curve.fallA");
    s.riseB = (float)*p.getRawParameterValue("lane3.curve.riseB");
    s.fallB = (float)*p.getRawParameterValue("lane3.curve.fallB");

    // Curvatures [-1..1]
    s.curvRiseA = (float)*p.getRawParameterValue("lane3.curv.riseA");
    s.curvFallA = (float)*p.getRawParameterValue("lane3.curv.fallA");
    s.curvRiseB = (float)*p.getRawParameterValue("lane3.curv.riseB");
    s.curvFallB = (float)*p.getRawParameterValue("lane3.curv.fallB");

    // Invert: take absolute value — center (0) = no inversion; ends = full inversion
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane3.invertA")));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane3.invertB")));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane4Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths
    s.riseA = (float)*p.getRawParameterValue("lane4.curve.riseA");
    s.fallA = (float)*p.getRawParameterValue("lane4.curve.fallA");
    s.riseB = (float)*p.getRawParameterValue("lane4.curve.riseB");
    s.fallB = (float)*p.getRawParameterValue("lane4.curve.fallB");

    // Curvatures [-1..1]
    s.curvRiseA = (float)*p.getRawParameterValue("lane4.curv.riseA");
    s.curvFallA = (float)*p.getRawParameterValue("lane4.curv.fallA");
    s.curvRiseB = (float)*p.getRawParameterValue("lane4.curv.riseB");
    s.curvFallB = (float)*p.getRawParameterValue("lane4.curv.fallB");

    // Invert is processed as |value|
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane4.invertA")));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane4.invertB")));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane5Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths
    s.riseA = (float)*p.getRawParameterValue("lane5.curve.riseA");
    s.fallA = (float)*p.getRawParameterValue("lane5.curve.fallA");
    s.riseB = (float)*p.getRawParameterValue("lane5.curve.riseB");
    s.fallB = (float)*p.getRawParameterValue("lane5.curve.fallB");

    // Curvatures [-1..1]
    s.curvRiseA = (float)*p.getRawParameterValue("lane5.curv.riseA");
    s.curvFallA = (float)*p.getRawParameterValue("lane5.curv.fallA");
    s.curvRiseB = (float)*p.getRawParameterValue("lane5.curv.riseB");
    s.curvFallB = (float)*p.getRawParameterValue("lane5.curv.fallB");

    // Invert: take absolute value — center (0) = no inversion; ends = full inversion
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane5.invertA")));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane5.invertB")));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane6Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths
    s.riseA = (float)*p.getRawParameterValue("lane6.curve.riseA");
    s.fallA = (float)*p.getRawParameterValue("lane6.curve.fallA");
    s.riseB = (float)*p.getRawParameterValue("lane6.curve.riseB");
    s.fallB = (float)*p.getRawParameterValue("lane6.curve.fallB");

    // Curvatures [-1..1]
    s.curvRiseA = (float)*p.getRawParameterValue("lane6.curv.riseA");
    s.curvFallA = (float)*p.getRawParameterValue("lane6.curv.fallA");
    s.curvRiseB = (float)*p.getRawParameterValue("lane6.curv.riseB");
    s.curvFallB = (float)*p.getRawParameterValue("lane6.curv.fallB");

    // Invert is processed as |value|
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane6.invertA")));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs((float)*p.getRawParameterValue("lane6.invertB")));

    return s;
}

double PinkELFOntsAudioProcessor::getCurrentBpm() const
{
    if (auto *ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pi;
        if (ph->getCurrentPosition(pi) && pi.bpm > 1.0)
            return pi.bpm;
    }
    return 120.0; // fallback
}

float PinkELFOntsAudioProcessor::evalLane1(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane1Shape();

    const float lanePhaseDeg = (float)*apvts.getRawParameterValue("lane1.phaseDeg");
    const float globalNudge = (float)*apvts.getRawParameterValue("global.phaseNudgeDeg");
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);
    return LFO::evalCycle(ph01, s); // unipolar 0..1
}

// A, B, B across the unit phase (third triangle mirrors B)
float PinkELFOntsAudioProcessor::evalLane2Triplet(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane2Shape();

    const float lanePhaseDeg = (float)*apvts.getRawParameterValue("lane2.phaseDeg");
    const float globalNudge = (float)*apvts.getRawParameterValue("global.phaseNudgeDeg");
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    if (ph01 < 2.0f / 3.0f)
    {
        // Map 0..2/3 -> 0..1 and let evalCycle do A then B
        float u = ph01 * 1.5f; // 0..1
        return LFO::evalCycle(u, s);
    }
    else
    {
        // Third triangle: replicate B over full [0..1) by sampling second half of evalCycle
        float u = (ph01 - 2.0f / 3.0f) * 3.0f; // 0..1
        float v = 0.5f + 0.5f * u;             // second half (B)
        return LFO::evalCycle(v, s);
    }
}

float PinkELFOntsAudioProcessor::evalLane3(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane3Shape();

    const float lanePhaseDeg = (float)*apvts.getRawParameterValue("lane3.phaseDeg");
    const float globalNudge = (float)*apvts.getRawParameterValue("global.phaseNudgeDeg");
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);
    return LFO::evalCycle(ph01, s); // unipolar 0..1
}

// A, B, B across the unit phase (third triangle mirrors B)
float PinkELFOntsAudioProcessor::evalLane4Triplet(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane4Shape();

    const float lanePhaseDeg = (float)*apvts.getRawParameterValue("lane4.phaseDeg");
    const float globalNudge = (float)*apvts.getRawParameterValue("global.phaseNudgeDeg");
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    if (ph01 < 2.0f / 3.0f)
    {
        // Map 0..2/3 -> 0..1 and let evalCycle do A then B
        float u = ph01 * 1.5f; // 0..1
        return LFO::evalCycle(u, s);
    }
    else
    {
        // Third triangle: replicate B over full [0..1) by sampling second half of evalCycle
        float u = (ph01 - 2.0f / 3.0f) * 3.0f; // 0..1
        float v = 0.5f + 0.5f * u;             // second half (B)
        return LFO::evalCycle(v, s);
    }
}

float PinkELFOntsAudioProcessor::evalLane5(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane5Shape();

    const float lanePhaseDeg = (float)*apvts.getRawParameterValue("lane5.phaseDeg");
    const float globalNudge = (float)*apvts.getRawParameterValue("global.phaseNudgeDeg");
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);
    return LFO::evalCycle(ph01, s); // unipolar 0..1
}

// A, B, B across the unit phase (third triangle mirrors B)
float PinkELFOntsAudioProcessor::evalLane6Triplet(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane6Shape();

    const float lanePhaseDeg = (float)*apvts.getRawParameterValue("lane6.phaseDeg");
    const float globalNudge = (float)*apvts.getRawParameterValue("global.phaseNudgeDeg");
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    if (ph01 < 2.0f / 3.0f)
    {
        // Map 0..2/3 -> 0..1 and let evalCycle do A then B
        float u = ph01 * 1.5f; // 0..1
        return LFO::evalCycle(u, s);
    }
    else
    {
        // Third triangle: replicate B over full [0..1) by sampling second half of evalCycle
        float u = (ph01 - 2.0f / 3.0f) * 3.0f; // 0..1
        float v = 0.5f + 0.5f * u;             // second half (B)
        return LFO::evalCycle(v, s);
    }
}

float PinkELFOntsAudioProcessor::evalMixed(float ph01) const
{
    // Apply phase nudge (deg -> 0..1)
    const float nudgeDeg = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float nudge = juce::jlimit(0.0f, 1.0f, ph01 + nudgeDeg / 360.0f);

    auto laneMix = [&](int laneIdx, float v)
    {
        const juce::String base = "lane" + juce::String(laneIdx);
        const auto *en = apvts.getParameter(base + ".enabled");
        const auto *mix = apvts.getParameter(base + ".mix");
        if (!en || !mix)
            return 0.0f;

        const bool enabled = en->getValue() > 0.5f;
        const float m = mix->getValue(); // 0..1
        if (!enabled || m <= 0.0f)
            return 0.0f;
        return m * v; // simple linear
    };

    // Evaluate each lane’s shape at the nudged phase
    float sum = 0.0f;
    sum += laneMix(1, evalLane1(nudge));
    sum += laneMix(2, evalLane2Triplet(nudge));
    sum += laneMix(3, evalLane3(nudge));
    sum += laneMix(4, evalLane4Triplet(nudge));
    sum += laneMix(5, evalLane5(nudge));
    sum += laneMix(6, evalLane6Triplet(nudge));
    // lanes 7–8 when you add them:
    // sum += laneMix(7, evalLane7(...));
    // sum += laneMix(8, evalLane8(...));

    // depth scales the whole thing
    const float depth = apvts.getRawParameterValue("global.depth")->load(); // 0..1
    const float out = juce::jlimit(0.0f, 1.0f, sum * depth);

    return out;
}

// ==================== lifecycle / audio ====================

void PinkELFOntsAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChans = buffer.getNumChannels();

    buffer.clear();
    updateTransportInfo();

    // --- retrig from MIDI ---
    const int retrigMode = (int)*apvts.getRawParameterValue("global.retrig");
    for (const auto metadata : midi)
    {
        const auto &m = metadata.getMessage();
        if (m.isNoteOn())
        {
            if (retrigMode == 1 /* Every Note */ || retrigMode == 2 /* First Note (basic) */)
            {
                lane1Phase01 = 0.0;
                lane2Phase01 = 0.0;
                lane3Phase01 = 0.0;
                lane4Phase01 = 0.0;
                lane5Phase01 = 0.0;
                lane6Phase01 = 0.0;
            }
        }
    }

    // ---- timing ----
    const double bpm = getCurrentBpm();

    auto dPhiForBeats = [&](double beatsPerCycle)
    {
        const double cyclesPerSec = (bpm / 60.0) / beatsPerCycle;
        return cyclesPerSec / sampleRateHz;
    };

    // Lane lengths in beats per full LFO cycle:
    // L1: 2.0 beats  (¼: two triangles over two beats)
    // L2: 2.0 beats  (¼T: three triangles over two beats)
    // L3: 1.0 beats  (⅛: one triangle per half-beat)
    // L4: 1.0 beats  (⅛T: three triangles per one beat)
    // L5: 0.25 beats (1/16: one triangle per quarter-beat)
    // L6: 0.5 beats  (1/16T: three triangles per half-beat)
    const double d1 = dPhiForBeats(2.0);
    const double d2 = dPhiForBeats(2.0);
    const double d3 = dPhiForBeats(1.0);
    const double d4 = dPhiForBeats(1.0);
    const double d5 = dPhiForBeats(0.5);
    const double d6 = dPhiForBeats(0.5);

    // carrier (EF/preview tone)
    const double dPhiCar = (double)carrierHz / sampleRateHz;

    // Params
    const float depth = (float)*apvts.getRawParameterValue("global.depth");
    const bool lane1On = (*apvts.getRawParameterValue("lane1.enabled") > 0.5f);
    const bool lane2On = (*apvts.getRawParameterValue("lane2.enabled") > 0.5f);
    const bool lane3On = (*apvts.getRawParameterValue("lane3.enabled") > 0.5f);
    const bool lane4On = (*apvts.getRawParameterValue("lane4.enabled") > 0.5f);
    const bool lane5On = (*apvts.getRawParameterValue("lane5.enabled") > 0.5f);
    const bool lane6On = (*apvts.getRawParameterValue("lane6.enabled") > 0.5f);
    const float mix1 = (float)*apvts.getRawParameterValue("lane1.mix");
    const float mix2 = (float)*apvts.getRawParameterValue("lane2.mix");
    const float mix3 = (float)*apvts.getRawParameterValue("lane3.mix");
    const float mix4 = (float)*apvts.getRawParameterValue("lane4.mix");
    const float mix5 = (float)*apvts.getRawParameterValue("lane5.mix");
    const float mix6 = (float)*apvts.getRawParameterValue("lane6.mix");

    if (depth <= 0.0f || (!lane1On && !lane2On && !lane3On && !lane4On && !lane5On && !lane6On) || (mix1 <= 0.0f && mix2 <= 0.0f && mix3 <= 0.0f && mix4 <= 0.0f && mix5 <= 0.0f && mix6 <= 0.0f))
        return;

    auto *ch0 = buffer.getWritePointer(0);

    for (int n = 0; n < numSamples; ++n)
    {
        // LFOs (0..1)
        const float y1 = lane1On ? evalLane1((float)lane1Phase01) : 0.0f;
        const float y2 = lane2On ? evalLane2Triplet((float)lane2Phase01) : 0.0f;
        const float y3 = lane3On ? evalLane3((float)lane3Phase01) : 0.0f;
        const float y4 = lane4On ? evalLane4Triplet((float)lane4Phase01) : 0.0f;
        const float y5 = lane5On ? evalLane5((float)lane5Phase01) : 0.0f;
        const float y6 = lane6On ? evalLane6Triplet((float)lane6Phase01) : 0.0f;

        lane1Phase01 += d1;
        lane2Phase01 += d2;
        lane3Phase01 += d3;
        lane4Phase01 += d4;
        lane5Phase01 += d5;
        lane6Phase01 += d6;
        if (lane1Phase01 >= 1.0)
            lane1Phase01 -= 1.0;
        if (lane2Phase01 >= 1.0)
            lane2Phase01 -= 1.0;
        if (lane3Phase01 >= 1.0)
            lane3Phase01 -= 1.0;
        if (lane4Phase01 >= 1.0)
            lane4Phase01 -= 1.0;
        if (lane5Phase01 >= 1.0)
            lane5Phase01 -= 1.0;
        if (lane6Phase01 >= 1.0)
            lane6Phase01 -= 1.0;

        // Mix lanes, unipolar 0..1
        float amp01 = (y1 * mix1 + y2 * mix2 + y3 * mix3 + y4 * mix4 + y5 * mix5 + y6 * mix6);
        amp01 = juce::jlimit(0.0f, 1.0f, amp01) * depth;

        const float car = std::sin(float(juce::MathConstants<double>::twoPi * carrierPhase));
        carrierPhase += dPhiCar;
        if (carrierPhase >= 1.0)
            carrierPhase -= 1.0;

        ch0[n] = car * amp01; // mono out
    }

    if (numChans > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor *PinkELFOntsAudioProcessor::createEditor()
{
    return new PinkELFOntsAudioProcessorEditor(*this);
}

void PinkELFOntsAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void PinkELFOntsAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    if (tree.isValid())
        apvts.replaceState(tree);
}

// JUCE factory entry point
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new PinkELFOntsAudioProcessor();
}