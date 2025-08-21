#include "SynthEngine.h"

// Triangle helper: standard 0..1 triangle from phase p in [0,1)
static inline float tri01(double p)
{
    // 0..0.5 rising 0->1, 0.5..1 falling 1->0
    return (float)(p < 0.5 ? (p * 2.0) : (2.0 - p * 2.0));
}

// Apply per-edge curvature to a 0..1 triangle using rise/fall gammas (skeleton uses riseA/fallA)
static inline float triBendy01(double p, float riseGamma, float fallGamma)
{
    if (p < 0.5)
    {
        // normalized t on rise edge
        const float t = (float)(p * 2.0);
        return SynthEngine::powCurveRise(t, riseGamma);
    }
    else
    {
        const float t = (float)((p - 0.5) * 2.0); // 0..1 on fall
        return SynthEngine::powCurveFall(t, fallGamma);
    }
}

// Convert beats-per-cycle for our named divisions (skeleton only uses 1/4 = 1 beat)
static inline double beatsPerCycle_Lane1() { return 1.0; } // quarter note = 1 beat

void SynthEngine::render(float *out, int numSamples)
{
    if (out == nullptr || numSamples <= 0)
        return;

    const double bpsPerSample = beatsPerSample(transport.bpm); // beats advanced per sample if phase increment = 1 beat per cycle
    const double lane1Beats = beatsPerCycle_Lane1();
    const double inc1 = (bpsPerSample / lane1Beats); // since phase is 0..1 per cycle

    // Handle retrig (simple): zero phase at next sample
    if (retrigRequested)
    {
        lane1.phase = 0.0;
        retrigRequested = false;
    }

    // Precompute global phase nudge
    const double nudge01 = (globalPhaseNudgeDeg / 360.0);

    for (int i = 0; i < numSamples; ++i)
    {
        float sum = 0.0f;

        // ----- Lane 1 -----
        if (lane1.enabled)
        {
            // advance phase in beat-locked way
            lane1.phase += inc1;
            if (lane1.phase >= 1.0)
                lane1.phase -= 1.0;

            double p = lane1.phase + (lane1.phaseDeg / 360.0) + nudge01;
            p -= std::floor(p); // wrap

            // Skeleton: use riseA/fallA for both edges (we’ll split by A/B when markers/tiles are added)
            float l1 = triBendy01(p, lane1.riseA, lane1.fallA);
            sum += l1 * lane1.mix;
        }

        // ----- Random lane (skeleton) -----
        if (random.enabled)
        {
            // For now, random just mirrors lane1 shape and applies crossfade when rate ticks would change.
            // (Full implementation will select among enabled lanes at 1/4, 1/8, 1/16 and crossfade.)
            float r = sum; // placeholder: same as current sum; independent weighting:
            sum += r * random.mix;
        }

        // Global depth & range
        float y = sum * globalDepth; // 0..1-ish sum; we’ll normalize/limit in the full combiner
        y = clamp01(y);
        if (outBipolar)
            y = (y * 2.0f - 1.0f); // -1..1

        out[i] = y;
    }
}
