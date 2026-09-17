#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace wf
{

// The last stage before the host. Allocation free, lock free, no branches that
// depend on the host being well behaved.
//
// Two jobs:
//  * NaN / Inf trap. A single bad sample entering a feedback path (delay,
//    reverb, resonant filter) poisons every block after it, which in a DAW
//    shows up as sudden full-scale noise. When one is seen the block is
//    silenced and `tripped()` tells the processor to reset the DSP.
//  * Soft clip. Below 0 dBFS the samples are passed through untouched (bit for
//    bit), above it the curve bends instead of running away, so a runaway
//    level is merely loud rather than a burst of square-wave noise.
class OutputGuard
{
public:
    // Returns true when the block had to be silenced (caller should reset).
    bool process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        bool bad = false;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = d[i];
                if (! std::isfinite (x))
                {
                    bad = true;
                    break;
                }
                d[i] = softClip (x);
            }
            if (bad)
                break;
        }

        if (bad)
        {
            buffer.clear();
            trips.fetch_add (1, std::memory_order_relaxed);
        }
        return bad;
    }

    // How many blocks have been silenced since the plugin was loaded. The
    // editor shows this so a glitch is visible instead of being guessed at.
    int tripCount() const noexcept { return trips.load (std::memory_order_relaxed); }

private:
    // Identity below 1.0, then a quadratic knee that flattens out at 2.0 (+6 dB):
    // continuous in value and slope at 1.0, so nothing audible changes there.
    static float softClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 1.0f)
            return x;
        const float y = a < 3.0f ? a - (a - 1.0f) * (a - 1.0f) * 0.25f : 2.0f;
        return x < 0.0f ? -y : y;
    }

    std::atomic<int> trips { 0 };
};

} // namespace wf
