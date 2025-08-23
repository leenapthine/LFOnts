#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LFOShape.h"
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

    params.push_back(std::make_unique<AudioParameterChoice>(
        "global.range", "Output Range",
        StringArray{"0..1", "-1..1"}, 0));

    // ---- Lane 1 (¼ note) ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane1.enabled", "Lane 1 Enabled", true)); // enable by default so you see triangles

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

    // Invert knobs [-1..1], 0 = normal
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.invertA", "Lane 1 Invert A",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.invertB", "Lane 1 Invert B",
        NormalisableRange<float>(-1.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    // ---- Random lane (unchanged placeholders) ----
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
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::mono(), true))
{
    playHead = getPlayHead();
}

void PinkELFOntsAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRateHz = sampleRate;
    lane1Phase01 = 0.0;
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

    // Invert: **use absolute value** so 0 = no inversion (center), ends = fully inverted
    const float invA = std::abs((float)*p.getRawParameterValue("lane1.invertA"));
    const float invB = std::abs((float)*p.getRawParameterValue("lane1.invertB"));
    s.invertA = juce::jlimit(0.0f, 1.0f, invA);
    s.invertB = juce::jlimit(0.0f, 1.0f, invB);

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

float PinkELFOntsAudioProcessor::toUnipolar01(float v) const
{
    // Respect global range ("0..1" vs "-1..1")
    const int range = (int)*apvts.getRawParameterValue("global.range"); // 0 => "0..1", 1 => "-1..1"
    if (range == 0)
        return juce::jlimit(0.0f, 1.0f, v);
    else
        return juce::jlimit(0.0f, 1.0f, 0.5f * (v + 1.0f));
}

float PinkELFOntsAudioProcessor::evalLane1(float ph01) const
{
    LFO::Shape s = const_cast<PinkELFOntsAudioProcessor *>(this)->makeLane1Shape();

    // Lane phase + global nudge
    const float lanePhaseDeg = (float)*apvts.getRawParameterValue("lane1.phaseDeg");
    const float globalNudge = (float)*apvts.getRawParameterValue("global.phaseNudgeDeg");
    const float phaseAdd01 = (lanePhaseDeg + globalNudge) / 360.0f;

    ph01 = std::fmod(ph01 + phaseAdd01 + 1.0f, 1.0f);
    return LFO::evalCycle(ph01, s); // [-1..1]
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
                lane1Phase01 = 0.0;
        }
    }

    // ---- timing ----
    const double bpm = getCurrentBpm();

    // **Two triangles per full cycle** → full cycle spans TWO quarter notes (2 beats).
    // Each triangle lasts one quarter note as requested.
    const double beatsPerCycleLane1 = 2.0; // Fix #3
    const double cyclesPerSec = (bpm / 60.0) / beatsPerCycleLane1;
    const double dPhiLFO = cyclesPerSec / sampleRateHz;

    // carrier just for EF visualization (host-visible)
    const double dPhiCar = (double)carrierHz / sampleRateHz;

    const float depth = (float)*apvts.getRawParameterValue("global.depth");
    const bool laneOn = (*apvts.getRawParameterValue("lane1.enabled") > 0.5f);
    const float mix = (float)*apvts.getRawParameterValue("lane1.mix");

    if (!laneOn || depth <= 0.0f || mix <= 0.0f)
        return;

    auto *ch0 = buffer.getWritePointer(0);

    for (int n = 0; n < numSamples; ++n)
    {
        const float y = evalLane1((float)lane1Phase01); // [-1..1]
        lane1Phase01 += dPhiLFO;
        if (lane1Phase01 >= 1.0)
            lane1Phase01 -= 1.0;

        const float amp01 = toUnipolar01(y) * depth * mix; // 0..1

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

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new PinkELFOntsAudioProcessor();
}
