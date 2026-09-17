#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdint>

namespace wf
{

// Low-frequency oscillator, evaluated at control rate (every
// Voice::controlInterval samples) rather than per sample.
//
// Free-run vs retrigger: every voice owns its own Lfo. In free mode a new
// voice copies the engine's running global phase at note-on, so all voices
// stay locked (identical increments from an identical phase never drift).
// In retrigger mode a voice starts from the LFO's phase offset instead.
class Lfo
{
public:
    enum Shape { sine = 0, triangle, saw, square, sampleHold, numShapes };

    // Tempo-sync divisions, in beats per LFO cycle.
    static constexpr int numDivisions = 15;
    static float divisionBeats (int index) noexcept
    {
        static constexpr float beats[numDivisions] =
        {
            32.0f, 16.0f, 8.0f, 4.0f,          // 8 / 4 / 2 / 1 bars (4/4)
            2.0f, 1.0f, 0.5f, 0.25f, 0.125f,   // 1/2 1/4 1/8 1/16 1/32
            2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f,   // 1/4T 1/8T 1/16T
            3.0f, 1.5f, 0.75f                   // 1/2D 1/4D 1/8D
        };
        return beats[juce::jlimit (0, numDivisions - 1, index)];
    }

    static juce::StringArray divisionNames()
    {
        return { "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32",
                 "1/4T", "1/8T", "1/16T", "1/2D", "1/4D", "1/8D" };
    }

    // Hz for a division at the given tempo. Guarded so a zero/absent BPM
    // from the host cannot produce a NaN increment.
    static float syncedRateHz (float bpm, int division) noexcept
    {
        const float safeBpm = bpm > 1.0f ? bpm : 120.0f;
        return (safeBpm / 60.0f) / divisionBeats (division);
    }

    void setSampleRate (double sr) noexcept { sampleRate = sr; }

    void reset (float phase01, uint32_t seed) noexcept
    {
        phase = phase01 - std::floor (phase01);
        rngState = seed != 0 ? seed : 0x9E3779B9u;
        held = nextRandom();
        lastWholeCycle = (int) phase;
    }

    void setRate (float hz) noexcept
    {
        inc = (float) (juce::jlimit (0.0f, 400.0f, hz) / sampleRate);
    }

    float getPhase() const noexcept { return phase; }
    float getValue() const noexcept { return value; }

    // Advances by `numSamples` and returns the new value in -1..1.
    float processControl (int shape, int numSamples) noexcept
    {
        phase += inc * (float) numSamples;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = nextRandom();   // one new S&H value per cycle
        }
        value = evaluate (shape);
        return value;
    }

    // Phase-only advance, used by the engine's free-running master phase.
    static float advancePhase (float phase01, float incPerSample, int numSamples) noexcept
    {
        float p = phase01 + incPerSample * (float) numSamples;
        return p - std::floor (p);
    }

private:
    float evaluate (int shape) const noexcept
    {
        switch (shape)
        {
            case sine:       return std::sin (phase * juce::MathConstants<float>::twoPi);
            case triangle:   return 4.0f * std::abs (phase - 0.5f) - 1.0f;
            case saw:        return 1.0f - 2.0f * phase;          // falling saw, like Serum
            case square:     return phase < 0.5f ? 1.0f : -1.0f;
            case sampleHold:
            default:         return held;
        }
    }

    float nextRandom() noexcept
    {
        rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
        return (float) (rngState & 0xFFFFFF) * (2.0f / 16777216.0f) - 1.0f;
    }

    double sampleRate = 44100.0;
    float phase = 0.0f, inc = 0.0f, value = 0.0f, held = 0.0f;
    int lastWholeCycle = 0;
    uint32_t rngState = 0x12345678u;
};

} // namespace wf
