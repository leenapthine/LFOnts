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
            }
        }
    }

    // ---- timing ----
    const double bpm = getCurrentBpm();

    // A "cycle" = 2 beats for both lanes.
    //   Lane 1: 2 triangles (each = 1 beat)
    //   Lane 2: 3 triangles (each = 2/3 beat = quarter-triplet)
    const double beatsPerCycle = 2.0;
    const double cyclesPerSec = (bpm / 60.0) / beatsPerCycle;
    const double dPhiCycle = cyclesPerSec / sampleRateHz;

    // carrier (EF/preview tone)
    const double dPhiCar = (double)carrierHz / sampleRateHz;

    // Params
    const float depth = (float)*apvts.getRawParameterValue("global.depth");
    const bool lane1On = (*apvts.getRawParameterValue("lane1.enabled") > 0.5f);
    const bool lane2On = (*apvts.getRawParameterValue("lane2.enabled") > 0.5f);
    const float mix1 = (float)*apvts.getRawParameterValue("lane1.mix");
    const float mix2 = (float)*apvts.getRawParameterValue("lane2.mix");

    if (depth <= 0.0f || (!lane1On && !lane2On) || (mix1 <= 0.0f && mix2 <= 0.0f))
        return;

    auto *ch0 = buffer.getWritePointer(0);

    for (int n = 0; n < numSamples; ++n)
    {
        // LFOs (0..1)
        const float y1 = lane1On ? evalLane1((float)lane1Phase01) : 0.0f;
        const float y2 = lane2On ? evalLane2Triplet((float)lane2Phase01) : 0.0f;

        lane1Phase01 += dPhiCycle;
        lane2Phase01 += dPhiCycle;
        if (lane1Phase01 >= 1.0)
            lane1Phase01 -= 1.0;
        if (lane2Phase01 >= 1.0)
            lane2Phase01 -= 1.0;

        // Mix lanes, unipolar 0..1
        float amp01 = (y1 * mix1 + y2 * mix2);
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
