#pragma once
#include <JuceHeader.h>
#include "LFOShape.h"

class SynthEngine
{
public:
    explicit SynthEngine(double sr = 44100.0) { setSampleRate(sr); }

    // Setup
    void setSampleRate(double sr) { sampleRate = (sr > 0 ? sr : 44100.0); }
    void setTransport(double bpmIn, double ppq, bool playing)
    {
        bpm = (bpmIn > 1.0 ? bpmIn : 120.0);
        ppqPos = ppq;
        isPlaying = playing;
    }

    // Global range/depth etc.
    void setGlobal(float depthIn, float nudgeDegIn, int retrigModeIn, bool bipolarOut)
    {
        depth = juce::jlimit(0.0f, 1.0f, depthIn);
        nudgeDeg = nudgeDegIn;
        retrigMode = retrigModeIn;
        bipolar = bipolarOut;
    }

    // Retrigger from MIDI
    void noteOnRetrig()
    {
        if (retrigMode != 0)
            lane1Phase = 0.0;
    }

    // Lane 1 (two triangles per full cycle)
    void setLane1(bool enabledIn, float mixIn, float phaseDegIn,
                  float riseA, float fallA, float riseB, float fallB,
                  float curvRiseA, float curvFallA, float curvRiseB, float curvFallB,
                  float invertA, float invertB)
    {
        lane1Enabled = enabledIn;
        lane1Mix = juce::jlimit(0.0f, 1.0f, mixIn);
        lane1PhaseDeg = phaseDegIn;

        lane1Shape.riseA = riseA;
        lane1Shape.fallA = fallA;
        lane1Shape.riseB = riseB;
        lane1Shape.fallB = fallB;

        lane1Shape.curvRiseA = curvRiseA;
        lane1Shape.curvFallA = curvFallA;
        lane1Shape.curvRiseB = curvRiseB;
        lane1Shape.curvFallB = curvFallB;

        // IMPORTANT: invert is now in [-1..1] (0 means no inversion)
        lane1Shape.invertA = juce::jlimit(-1.0f, 1.0f, invertA);
        lane1Shape.invertB = juce::jlimit(-1.0f, 1.0f, invertB);
    }

    // Random lane (placeholder to keep your existing calls intact)
    void setRandom(bool, int, float, float) {}

    // Render mono control signal
    void render(float *out, int numSamples)
    {
        if (!out || numSamples <= 0)
            return;

        for (int n = 0; n < numSamples; ++n)
        {
            float sig = 0.0f;

            if (lane1Enabled && lane1Mix > 0.0f && depth > 0.0f)
            {
                const double phaseTotal = lane1Phase + (lane1PhaseDeg + nudgeDeg) / 360.0;
                float v = LFO::evalCycle((float)std::fmod(phaseTotal + 1.0, 1.0),
                                         lane1Shape); // [-1..1]

                // Map to output range, then apply mix & depth
                float u = bipolar ? v : (0.5f * (v + 1.0f)); // [-1..1] or [0..1]
                sig += u * lane1Mix * depth;

                // Advance phase: 2 triangles per cycle, each triangle = 1/4 note
                const double beatsPerTriangle = 1.0;                               // Lane-1 rate = quarter-notes
                const double trianglesPerCycle = 2.0;                              // two triangles in one cycle
                const double beatsPerCycle = beatsPerTriangle * trianglesPerCycle; // = 2.0
                const double cyclesPerSec = (bpm / 60.0) / beatsPerCycle;
                lane1dPhi = cyclesPerSec / sampleRate;

                lane1Phase += lane1dPhi;
                if (lane1Phase >= 1.0)
                    lane1Phase -= 1.0;
            }

            out[n] += sig;
        }
    }

private:
    // Clocking
    double sampleRate = 44100.0, bpm = 120.0, ppqPos = 0.0, lane1Phase = 0.0, lane1dPhi = 0.0;
    bool isPlaying = false;

    // Global
    float depth = 1.0f, nudgeDeg = 0.0f;
    int retrigMode = 0;
    bool bipolar = false;

    // Lane 1
    bool lane1Enabled = false;
    float lane1Mix = 0.5f;
    float lane1PhaseDeg = 0.0f;
    LFO::Shape lane1Shape{};
};
