#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// ===== Helpers for squaring by intensity ===================================
static inline float squareByIntensity(float x /*0..1*/, float amp /*0..1*/)
{
    // Piecewise: below 0.5 = linear gain up to 1.0; above 0.5 = pre-gain → soft clip
    if (amp <= 0.5f)
    {
        const float g = juce::jmap(amp, 0.0f, 0.5f, 0.0f, 1.0f); // 0..1
        return juce::jlimit(0.0f, 1.0f, x * g);
    }

    // 0.5..1.0 -> 1..maxPreGain
    const float maxPreGain = 8.0f;                   // tweak hardness of “square”
    const float t = (amp - 0.5f) * 2.0f;             // 0..1
    const float g = juce::jmap(t, 1.0f, maxPreGain); // 1..8
    const float pre = x * g;
    return juce::jlimit(0.0f, 1.0f, pre);
}

// Map ph01 ∈ [0..1] to a slope between v0..v1, with curvature.
// slopeAmt01: 0→rise 0..1, 0.5→flat 1..1, 1→fall 1..0
// curve01: 0 concave, 0.5 linear (exactly!), 1 convex
static inline float outputSlopeGain(float ph01, float slopeAmt01, float curve01)
{
    const float b = 2.0f * (slopeAmt01 - 0.5f);    // [-1..1]
    const float v0 = (b < 0.0f ? 1.0f + b : 1.0f); // start level
    const float v1 = (b > 0.0f ? 1.0f - b : 1.0f); // end level

    // Ensure curve01 == 0.5 maps to p == 1 (perfectly linear).
    float p;
    if (curve01 <= 0.5f)
        p = juce::jmap(curve01, 0.0f, 0.5f, 0.25f, 1.0f); // concave → linear
    else
        p = juce::jmap(curve01, 0.5f, 1.0f, 1.0f, 4.0f); // linear → convex

    const float t = std::pow(juce::jlimit(0.0f, 1.0f, ph01), p);
    return juce::jlimit(0.0f, 1.0f, v0 + (v1 - v0) * t);
}

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

    // --- Output slope / curve ----------------------------------------------
    // Outer (slope amount): 0 = full rise (0→1), 0.5 = flat, 1 = full fall (1→0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "output.slope", "Output Slope",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f),
        0.5f));

    // Inner (curve): 0 = fully concave, 0.5 = linear, 1 = fully convex
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "output.slopeCurve", "Output Slope Curve",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f),
        0.5f));

    // --- Output rate (global time scale for the slope/curve) -----------------
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "output.rate", "Output Rate",
        juce::StringArray{"1/4", "1/2", "1 bar", "2 bars", "4 bars"},
        0 /* default = 1/4 */));

    // ---- Lane 1 (¼ note) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane1.enabled", "Lane 1 Enabled", true));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.mix", "Lane 1 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.phaseDeg", "Lane 1 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // NEW: intensity A/B (amplitude per half), default 0.5
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.intensityA", "Lane 1 Intensity A",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.intensityB", "Lane 1 Intensity B",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));

    // NEW: inner curvature for intensity A/B
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curv.intensityA", "Lane 1 Curv Intensity A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.curv.intensityB", "Lane 1 Curv Intensity B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // Existing time (rise/fall) and their curves — keep
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

    // Invert — keep
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

    // NEW: intensity A/B + their inner curvature
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane2.intensityA", "Lane 2 Intensity A",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane2.intensityB", "Lane 2 Intensity B",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane2.curv.intensityA", "Lane 2 Curv Intensity A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane2.curv.intensityB", "Lane 2 Curv Intensity B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

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
        "lane3.enabled", "Lane 3 Enabled", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.mix", "Lane 3 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.phaseDeg", "Lane 3 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // NEW: intensity A/B + their inner curvature
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.intensityA", "Lane 3 Intensity A",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.intensityB", "Lane 3 Intensity B",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curv.intensityA", "Lane 3 Curv Intensity A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane3.curv.intensityB", "Lane 3 Curv Intensity B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

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

    // Invert
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

    // NEW: intensity A/B + their inner curvature
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane4.intensityA", "Lane 4 Intensity A",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane4.intensityB", "Lane 4 Intensity B",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane4.curv.intensityA", "Lane 4 Curv Intensity A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane4.curv.intensityB", "Lane 4 Curv Intensity B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // Length (A/B) + Curvature + Invert (loop style)
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
        "lane5.enabled", "Lane 5 Enabled", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.mix", "Lane 5 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.phaseDeg", "Lane 5 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // NEW: intensity A/B + their inner curvature
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.intensityA", "Lane 5 Intensity A",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.intensityB", "Lane 5 Intensity B",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curv.intensityA", "Lane 5 Curv Intensity A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane5.curv.intensityB", "Lane 5 Curv Intensity B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

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

    // Curvature (inner rings)
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

    // Invert
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

    // NEW: intensity A/B + their inner curvature
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane6.intensityA", "Lane 6 Intensity A",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane6.intensityB", "Lane 6 Intensity B",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane6.curv.intensityA", "Lane 6 Curv Intensity A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane6.curv.intensityB", "Lane 6 Curv Intensity B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // Length (A/B) + Curvature + Invert (loop style)
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
    outputSlopePhase01 = 0.0;
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

    // Lengths (driven by Time A/B outers via attachments)
    s.riseA = p.getRawParameterValue("lane1.curve.riseA")->load();
    s.fallA = p.getRawParameterValue("lane1.curve.fallA")->load();
    s.riseB = p.getRawParameterValue("lane1.curve.riseB")->load();
    s.fallB = p.getRawParameterValue("lane1.curve.fallB")->load();

    // Curvatures [-1..1]  (Time inner → curvRise*, Intensity inner → curvFall*)
    s.curvRiseA = p.getRawParameterValue("lane1.curv.riseA")->load();
    s.curvFallA = p.getRawParameterValue("lane1.curv.fallA")->load();
    s.curvRiseB = p.getRawParameterValue("lane1.curv.riseB")->load();
    s.curvFallB = p.getRawParameterValue("lane1.curv.fallB")->load();

    // Invert (keep your existing behavior)
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane1.invertA")->load()));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane1.invertB")->load()));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane2Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths (driven by Time A/B outers via attachments)
    s.riseA = p.getRawParameterValue("lane2.curve.riseA")->load();
    s.fallA = p.getRawParameterValue("lane2.curve.fallA")->load();
    s.riseB = p.getRawParameterValue("lane2.curve.riseB")->load();
    s.fallB = p.getRawParameterValue("lane2.curve.fallB")->load();

    // Curvatures [-1..1]  (Time inner → curvRise*, Intensity inner → curvFall*)
    s.curvRiseA = p.getRawParameterValue("lane2.curv.riseA")->load();
    s.curvFallA = p.getRawParameterValue("lane2.curv.fallA")->load();
    s.curvRiseB = p.getRawParameterValue("lane2.curv.riseB")->load();
    s.curvFallB = p.getRawParameterValue("lane2.curv.fallB")->load();

    // Invert (keep your existing behavior)
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane2.invertA")->load()));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane2.invertB")->load()));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane3Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths (driven by Time A/B outers via attachments)
    s.riseA = p.getRawParameterValue("lane3.curve.riseA")->load();
    s.fallA = p.getRawParameterValue("lane3.curve.fallA")->load();
    s.riseB = p.getRawParameterValue("lane3.curve.riseB")->load();
    s.fallB = p.getRawParameterValue("lane3.curve.fallB")->load();

    // Curvatures [-1..1]  (Time inner → curvRise*, Intensity inner → curvFall*)
    s.curvRiseA = p.getRawParameterValue("lane3.curv.riseA")->load();
    s.curvFallA = p.getRawParameterValue("lane3.curv.fallA")->load();
    s.curvRiseB = p.getRawParameterValue("lane3.curv.riseB")->load();
    s.curvFallB = p.getRawParameterValue("lane3.curv.fallB")->load();

    // Invert (keep your existing behavior)
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane3.invertA")->load()));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane3.invertB")->load()));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane4Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths (driven by Time A/B outers via attachments)
    s.riseA = p.getRawParameterValue("lane4.curve.riseA")->load();
    s.fallA = p.getRawParameterValue("lane4.curve.fallA")->load();
    s.riseB = p.getRawParameterValue("lane4.curve.riseB")->load();
    s.fallB = p.getRawParameterValue("lane4.curve.fallB")->load();

    // Curvatures [-1..1]  (Time inner → curvRise*, Intensity inner → curvFall*)
    s.curvRiseA = p.getRawParameterValue("lane4.curv.riseA")->load();
    s.curvFallA = p.getRawParameterValue("lane4.curv.fallA")->load();
    s.curvRiseB = p.getRawParameterValue("lane4.curv.riseB")->load();
    s.curvFallB = p.getRawParameterValue("lane4.curv.fallB")->load();

    // Invert (keep your existing behavior)
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane4.invertA")->load()));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane4.invertB")->load()));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane5Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths (driven by Time A/B outers via attachments)
    s.riseA = p.getRawParameterValue("lane5.curve.riseA")->load();
    s.fallA = p.getRawParameterValue("lane5.curve.fallA")->load();
    s.riseB = p.getRawParameterValue("lane5.curve.riseB")->load();
    s.fallB = p.getRawParameterValue("lane5.curve.fallB")->load();

    // Curvatures [-1..1]  (Time inner → curvRise*, Intensity inner → curvFall*)
    s.curvRiseA = p.getRawParameterValue("lane5.curv.riseA")->load();
    s.curvFallA = p.getRawParameterValue("lane5.curv.fallA")->load();
    s.curvRiseB = p.getRawParameterValue("lane5.curv.riseB")->load();
    s.curvFallB = p.getRawParameterValue("lane5.curv.fallB")->load();

    // Invert (keep your existing behavior)
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane5.invertA")->load()));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane5.invertB")->load()));

    return s;
}

LFO::Shape PinkELFOntsAudioProcessor::makeLane6Shape() const
{
    LFO::Shape s;
    auto &p = apvts;

    // Lengths (driven by Time A/B outers via attachments)
    s.riseA = p.getRawParameterValue("lane6.curve.riseA")->load();
    s.fallA = p.getRawParameterValue("lane6.curve.fallA")->load();
    s.riseB = p.getRawParameterValue("lane6.curve.riseB")->load();
    s.fallB = p.getRawParameterValue("lane6.curve.fallB")->load();

    // Curvatures [-1..1]  (Time inner → curvRise*, Intensity inner → curvFall*)
    s.curvRiseA = p.getRawParameterValue("lane6.curv.riseA")->load();
    s.curvFallA = p.getRawParameterValue("lane6.curv.fallA")->load();
    s.curvRiseB = p.getRawParameterValue("lane6.curv.riseB")->load();
    s.curvFallB = p.getRawParameterValue("lane6.curv.fallB")->load();

    // Invert (keep your existing behavior)
    s.invertA = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane6.invertA")->load()));
    s.invertB = juce::jlimit(0.0f, 1.0f, std::abs(p.getRawParameterValue("lane6.invertB")->load()));

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

    const float lanePhaseDeg = apvts.getRawParameterValue("lane1.phaseDeg")->load();
    const float globalNudge = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    const float v = LFO::evalCycle(ph01, s);      // 0..1 base
    const int half = halfIndexFromPhase(ph01);    // 0=A, 1=B
    const float amp = laneHalfIntensity(1, half); // lane1.intensityA/B 0..1
    return squareByIntensity(v, amp);
}

float PinkELFOntsAudioProcessor::evalLane2Triplet(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane2Shape();

    const float lanePhaseDeg = apvts.getRawParameterValue("lane2.phaseDeg")->load();
    const float globalNudge = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    // Piecewise map 0..1 into three triangles: A (0..1), B (0..1), B (0..1)
    float v = 0.0f;
    int half = 0; // 0=A, 1=B
    if (ph01 < 2.0f / 3.0f)
    {
        const float u = ph01 * 1.5f; // 0..1 over first 2/3
        v = LFO::evalCycle(u, s);    // does A then B across 0..1
        half = (u < 0.5f ? 0 : 1);   // A in first half, B in second
    }
    else
    {
        const float u = (ph01 - 2.0f / 3.0f) * 3.0f; // 0..1 over last 1/3
        const float b = 0.5f + 0.5f * u;             // force eval of B half
        v = LFO::evalCycle(b, s);
        half = 1; // third triangle is B
    }

    const float amp = laneHalfIntensity(2, half);
    return squareByIntensity(v, amp);
}

float PinkELFOntsAudioProcessor::evalLane3(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane3Shape();

    const float lanePhaseDeg = apvts.getRawParameterValue("lane3.phaseDeg")->load();
    const float globalNudge = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    const float v = LFO::evalCycle(ph01, s);
    const int half = halfIndexFromPhase(ph01);
    const float amp = laneHalfIntensity(3, half);
    return squareByIntensity(v, amp);
}

// A, B, B across the unit phase (third triangle mirrors B)
float PinkELFOntsAudioProcessor::evalLane4Triplet(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane4Shape();

    const float lanePhaseDeg = apvts.getRawParameterValue("lane4.phaseDeg")->load();
    const float globalNudge = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    float v = 0.0f;
    int half = 0;
    if (ph01 < 2.0f / 3.0f)
    {
        const float u = ph01 * 1.5f;
        v = LFO::evalCycle(u, s);
        half = (u < 0.5f ? 0 : 1);
    }
    else
    {
        const float u = (ph01 - 2.0f / 3.0f) * 3.0f;
        const float b = 0.5f + 0.5f * u;
        v = LFO::evalCycle(b, s);
        half = 1;
    }

    const float amp = laneHalfIntensity(4, half);
    return squareByIntensity(v, amp);
}

float PinkELFOntsAudioProcessor::evalLane5(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane5Shape();

    const float lanePhaseDeg = apvts.getRawParameterValue("lane5.phaseDeg")->load();
    const float globalNudge = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    const float v = LFO::evalCycle(ph01, s);
    const int half = halfIndexFromPhase(ph01);
    const float amp = laneHalfIntensity(5, half);
    return squareByIntensity(v, amp);
}

// A, B, B across the unit phase (third triangle mirrors B)
float PinkELFOntsAudioProcessor::evalLane6Triplet(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane6Shape();

    const float lanePhaseDeg = apvts.getRawParameterValue("lane6.phaseDeg")->load();
    const float globalNudge = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);

    float v = 0.0f;
    int half = 0;
    if (ph01 < 2.0f / 3.0f)
    {
        const float u = ph01 * 1.5f;
        v = LFO::evalCycle(u, s);
        half = (u < 0.5f ? 0 : 1);
    }
    else
    {
        const float u = (ph01 - 2.0f / 3.0f) * 3.0f;
        const float b = 0.5f + 0.5f * u;
        v = LFO::evalCycle(b, s);
        half = 1;
    }

    const float amp = laneHalfIntensity(6, half);
    return squareByIntensity(v, amp);
}

float PinkELFOntsAudioProcessor::evalMixed(float ph01) const
{
    // Phase nudge: wrap (not clamp) so modulation keeps moving around the cycle
    const float nudgeDeg = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float base = std::fmod(ph01 + nudgeDeg / 360.0f + 1.0f, 1.0f); // 0..1

    auto laneMix = [&](int laneIdx, float v)
    {
        const juce::String baseId = "lane" + juce::String(laneIdx);
        const auto *en = apvts.getParameter(baseId + ".enabled");
        const auto *mix = apvts.getParameter(baseId + ".mix");
        if (!en || !mix)
            return 0.0f;

        const bool enabled = en->getValue() > 0.5f;
        const float m = mix->getValue(); // 0..1
        if (!enabled || m <= 0.0f)
            return 0.0f;
        return m * v;
    };

    auto wrap01 = [](float x)
    { return x - std::floor(x); };

    // Evaluate each lane at the same wrapped, nudged phase
    float sum = 0.0f;
    sum += laneMix(1, evalLane1(wrap01(base * 1.0f)));        // L1  1/4
    sum += laneMix(2, evalLane2Triplet(wrap01(base * 1.0f))); // L2  1/4T
    sum += laneMix(3, evalLane3(wrap01(base * 2.0f)));        // L3  1/8
    sum += laneMix(4, evalLane4Triplet(wrap01(base * 2.0f))); // L4  1/8T
    sum += laneMix(5, evalLane5(wrap01(base * 4.0f)));        // L5  1/16
    sum += laneMix(6, evalLane6Triplet(wrap01(base * 4.0f))); // L6  1/16T

    // Global depth
    const float depth = apvts.getRawParameterValue("global.depth")->load(); // 0..1

    // Output slope/curve (use the SAME wrapped phase so overlay == DSP)
    const float slopeAmt = apvts.getRawParameterValue("output.slope")->load();        // 0..1
    const float slopeCurve = apvts.getRawParameterValue("output.slopeCurve")->load(); // 0..1
    const float slopeGain = outputSlopeGain(base, slopeAmt, slopeCurve);              // 0..1

    const float out = juce::jlimit(0.0f, 1.0f, sum * depth * slopeGain);
    return out;
}

float PinkELFOntsAudioProcessor::evalSlopeOnly(float ph01) const
{
    // match the same phase nudge applied to the mixed scope
    const float nudgeDeg = apvts.getRawParameterValue("global.phaseNudgeDeg")->load();
    const float t = std::fmod(ph01 + nudgeDeg / 360.0f + 1.0f, 1.0f);

    const float slopeAmt01 = apvts.getRawParameterValue("output.slope")->load();   // 0..1 (0.5=flat)
    const float curve01 = apvts.getRawParameterValue("output.slopeCurve")->load(); // 0..1 (0.5=linear)

    return outputSlopeGain(t, slopeAmt01, curve01); // uses your static inline defined above
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
                lane1Phase01 = lane2Phase01 = lane3Phase01 = 0.0;
                lane4Phase01 = lane5Phase01 = lane6Phase01 = 0.0;
                outputSlopePhase01 = 0.0;
            }
        }
    }

    // ---- timing helpers ----
    const double bpm = getCurrentBpm();
    auto dPhiForBeats = [&](double beatsPerCycle)
    {
        const double cyclesPerSec = (bpm / 60.0) / beatsPerCycle;
        return cyclesPerSec / sampleRateHz;
    };

    // Lane lengths in beats per full LFO cycle
    const double d1 = dPhiForBeats(2.0); // L1 1/4
    const double d2 = dPhiForBeats(2.0); // L2 1/4T
    const double d3 = dPhiForBeats(1.0); // L3 1/8
    const double d4 = dPhiForBeats(1.0); // L4 1/8T
    const double d5 = dPhiForBeats(0.5); // L5 1/16
    const double d6 = dPhiForBeats(0.5); // L6 1/16T

    // Global output-rate (beats per slope cycle)
    const int rateIdx = (int)apvts.getRawParameterValue("output.rate")->load();
    const double beatsPerSlopeCycle[5] = {1.0, 2.0, 4.0, 8.0, 16.0}; // 1/4, 1/2, 1 bar, 2 bars, 4 bars
    const double dSlope = dPhiForBeats(beatsPerSlopeCycle[juce::jlimit(0, 4, rateIdx)]);

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

    // Slope/curve params (block-rate read is fine)
    const float slopeAmt = apvts.getRawParameterValue("output.slope")->load();        // 0..1
    const float slopeCurve = apvts.getRawParameterValue("output.slopeCurve")->load(); // 0..1

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

        // advance phases
        lane1Phase01 += d1;
        if (lane1Phase01 >= 1.0)
            lane1Phase01 -= 1.0;
        lane2Phase01 += d2;
        if (lane2Phase01 >= 1.0)
            lane2Phase01 -= 1.0;
        lane3Phase01 += d3;
        if (lane3Phase01 >= 1.0)
            lane3Phase01 -= 1.0;
        lane4Phase01 += d4;
        if (lane4Phase01 >= 1.0)
            lane4Phase01 -= 1.0;
        lane5Phase01 += d5;
        if (lane5Phase01 >= 1.0)
            lane5Phase01 -= 1.0;
        lane6Phase01 += d6;
        if (lane6Phase01 >= 1.0)
            lane6Phase01 -= 1.0;

        // advance global slope phase by chosen rate
        outputSlopePhase01 += dSlope;
        if (outputSlopePhase01 >= 1.0)
            outputSlopePhase01 -= 1.0;

        // --- mix lanes, then apply depth & slope, then clamp ---
        float amp01 = (y1 * mix1 + y2 * mix2 + y3 * mix3 + y4 * mix4 + y5 * mix5 + y6 * mix6);

        amp01 *= depth;

        const float slopeGain = outputSlopeGain((float)outputSlopePhase01, slopeAmt, slopeCurve); // 0..1
        amp01 *= slopeGain;

        amp01 = juce::jlimit(0.0f, 1.0f, amp01);

        // carrier out
        const float car = std::sin(float(juce::MathConstants<double>::twoPi * carrierPhase));
        carrierPhase += dPhiCar;
        if (carrierPhase >= 1.0)
            carrierPhase -= 1.0;

        ch0[n] = car * amp01; // mono
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