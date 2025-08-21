#pragma once
#include <cmath>
#include <random>

struct TransportInfo
{
    double bpm{120.0};
    double ppq{0.0};
    bool playing{false};
};

class SynthEngine
{
public:
    explicit SynthEngine(double sr) : sampleRate(sr) {}

    void setSampleRate(double sr) { sampleRate = sr; }
    void setTransport(double bpm, double ppq, bool isPlaying)
    {
        transport.bpm = bpm;
        transport.ppq = ppq;
        transport.playing = isPlaying;
        // keep beat-locked; we recompute increments in render()
    }

    // Global
    void setGlobal(float depth01, float phaseNudgeDeg, int retrigMode, bool bipolarOut)
    {
        globalDepth = depth01;
        globalPhaseNudgeDeg = phaseNudgeDeg;
        globalRetrig = retrigMode;
        outBipolar = bipolarOut;
    }

    // Lane 1 (¼ note) – skeleton
    void setLane1(bool enabled, float mix01, float phaseDeg,
                  float riseA, float fallA, float riseB, float fallB)
    {
        lane1.enabled = enabled;
        lane1.mix = mix01;
        lane1.phaseDeg = phaseDeg;
        lane1.riseA = riseA;
        lane1.fallA = fallA;
        lane1.riseB = riseB;
        lane1.fallB = fallB;
    }

    // Random lane – skeleton (selects from enabled lanes; here effectively lane1)
    void setRandom(bool enabled, int rateIndex, float crossfadeMs, float mix01)
    {
        random.enabled = enabled;
        random.rateIndex = rateIndex;
        random.crossfadeMs = crossfadeMs;
        random.mix = mix01;
    }

    // MIDI retrig
    void noteOnRetrig() { retrigRequested = true; }

    // Render CV into out[0..numSamples)
    void render(float *out, int numSamples);

private:
    // Helpers
    inline double beatsPerSample(double bpm) const { return (bpm / 60.0) / sampleRate; } // beats advanced per sample

    static inline float powCurveRise(float t, float gamma) { return std::pow(t, gamma); }
    static inline float powCurveFall(float t, float gamma) { return 1.0f - std::pow(t, gamma); }
    static inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
    static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

    struct Lane
    {
        bool enabled{false};
        float mix{0.5f};
        float phaseDeg{0.0f};
        // 4 curvature knobs
        float riseA{1.0f}, fallA{1.0f}, riseB{1.0f}, fallB{1.0f};

        // phase accumulator in [0,1)
        double phase{0.0};
    };

    struct RandomLane
    {
        bool enabled{false};
        int rateIndex{0}; // 0=1/4, 1=1/8, 2=1/16
        float crossfadeMs{20.0f};
        float mix{0.5f};

        // xfade state
        float xfadePos{1.0f}; // 0..1; 1 means no active fade
        float prevVal{0.0f};
    };

    // State
    double sampleRate{44100.0};
    TransportInfo transport{};

    // Globals
    float globalDepth{1.0f};
    float globalPhaseNudgeDeg{0.0f};
    int globalRetrig{0};
    bool outBipolar{false};

    bool retrigRequested{false};

    // Lanes
    Lane lane1{};
    RandomLane random{};
};
