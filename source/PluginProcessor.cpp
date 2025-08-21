#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    // ---- Lane 1 (¼ note) — working skeleton ----
    params.push_back(std::make_unique<AudioParameterBool>(
        "lane1.enabled", "Lane 1 Enabled", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.mix", "Lane 1 Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        "lane1.phaseDeg", "Lane 1 Phase (deg)",
        NormalisableRange<float>(0.0f, 360.0f, 0.0f, 1.0f), 0.0f));

    // 4 curvature knobs (RiseA/FallA/RiseB/FallB) — in skeleton we use RiseA/FallA
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

    // ---- Random lane (rate + crossfade + mix + on/off) ----
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

    // TODO: Add lanes 2..8 + Rise/Fall bars + markers stored in ValueTree (non-automatable)

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
    engine.setSampleRate(sampleRate);
}

void PinkELFOntsAudioProcessor::updateTransportInfo()
{
    if (auto *ph = getPlayHead())
        ph->getCurrentPosition(posInfo);
}

void PinkELFOntsAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    updateTransportInfo();

    // MIDI retrig
    const int retrigMode = (int)*apvts.getRawParameterValue("global.retrig");
    for (const auto metadata : midi)
    {
        const auto &m = metadata.getMessage();
        if (m.isNoteOn())
        {
            if (retrigMode == 1 /* Every Note */)
                engine.noteOnRetrig();
            else if (retrigMode == 2 /* First Note */)
                engine.noteOnRetrig(); // refine with legato later
        }
    }

    // Transport → engine
    engine.setTransport(posInfo.bpm > 0.0 ? posInfo.bpm : 120.0,
                        posInfo.ppqPosition, posInfo.isPlaying);

    // Read global params
    const float depth = *apvts.getRawParameterValue("global.depth");
    const float nudge = *apvts.getRawParameterValue("global.phaseNudgeDeg");
    const int range = (int)*apvts.getRawParameterValue("global.range");
    engine.setGlobal(depth, nudge, retrigMode, range == 1);

    // Lane 1
    engine.setLane1(
        apvts.getRawParameterValue("lane1.enabled")->load(),
        apvts.getRawParameterValue("lane1.mix")->load(),
        apvts.getRawParameterValue("lane1.phaseDeg")->load(),
        apvts.getRawParameterValue("lane1.curve.riseA")->load(),
        apvts.getRawParameterValue("lane1.curve.fallA")->load(),
        apvts.getRawParameterValue("lane1.curve.riseB")->load(),
        apvts.getRawParameterValue("lane1.curve.fallB")->load());

    // Random
    engine.setRandom(
        apvts.getRawParameterValue("random.enabled")->load(),
        (int)apvts.getRawParameterValue("random.rate")->load(),
        apvts.getRawParameterValue("random.crossfadeMs")->load(),
        apvts.getRawParameterValue("random.mix")->load());

    // Render mono CV to L
    auto *out = buffer.getWritePointer(0);
    engine.render(out, buffer.getNumSamples());

    // If host gave us stereo, mirror to R
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor *PinkELFOntsAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this); // replaced by our custom editor below
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
