#pragma once
#include <JuceHeader.h>

namespace LFO
{
    // Map |c| in [0..1] to an exponent >= 1 (bigger = stronger effect)
    inline float expoFromAmount(float a)
    {
        a = juce::jlimit(0.0f, 1.0f, a);
        return juce::jmap(a, 0.0f, 1.0f, 1.0f, 5.0f); // 1..5 feels nicely dramatic
    }

    // shape01: t∈[0,1], c∈[-1,1]
    //   c < 0 → concave (ease-in):   slow start, then faster  =>  t^e (e>=1)
    //   c > 0 → convex (ease-out):   fast start, then slower  =>  1 - (1-t)^e (e>=1)
    inline float shape01(float t, float c)
    {
        t = juce::jlimit(0.0f, 1.0f, t);
        const float e = expoFromAmount(std::abs(c));

        if (c >= 0.0f) // convex (fast start)
            return 1.0f - std::pow(1.0f - t, e);
        else // concave (slow start)
            return std::pow(t, e);
    }

    struct Shape
    {
        // relative edge lengths
        float riseA = 1.0f, fallA = 1.0f, riseB = 1.0f, fallB = 1.0f;

        // curvature per edge in [-1..1]
        float curvRiseA = 0.0f, curvFallA = 0.0f, curvRiseB = 0.0f, curvFallB = 0.0f;

        // invert blend in [0..1] (0 = normal, 1 = fully inverted around 0.5)
        float invertA = 0.0f, invertB = 0.0f;
    };

    // One triangle half (upward), returns 0..1 BEFORE inversion blend.
    inline float evalHalf(float ph01,
                          float rise, float fall,
                          float curvRise, float curvFall,
                          float invertAmt01)
    {
        const float split = juce::jlimit(0.05f, 0.95f, rise / juce::jmax(0.0001f, rise + fall));
        float y01 = 0.0f;

        if (ph01 < split)
        {
            const float t = ph01 / split; // 0..1 along RISE edge
            y01 = shape01(t, curvRise);   // 0..1 (concave/convex by sign)
        }
        else
        {
            const float t = (ph01 - split) / (1.0f - split); // 0..1 along FALL edge
            y01 = 1.0f - shape01(t, curvFall);               // drop 1→0 with same semantics
        }

        // Invert around 0.5 (blend to mirrored peak)
        const float inv = juce::jlimit(0.0f, 1.0f, invertAmt01);
        y01 = juce::jmap(inv, y01, 1.0f - y01);

        return juce::jlimit(0.0f, 1.0f, y01);
    }

    // Full cycle: A-half then B-half — BOTH positive triangles (EF sees two matching peaks).
    inline float evalCycle(float ph01, const Shape &s)
    {
        const float y01 = (ph01 < 0.5f)
                              ? evalHalf(ph01 * 2.0f, s.riseA, s.fallA, s.curvRiseA, s.curvFallA, s.invertA)
                              : evalHalf((ph01 - 0.5f) * 2.0f, s.riseB, s.fallB, s.curvRiseB, s.curvFallB, s.invertB);

        return juce::jlimit(0.0f, 1.0f, y01);
    }
} // namespace LFO
