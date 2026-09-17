#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <chrono>

// Lock-free audio-thread instrumentation used to tell "our DSP produced a
// discontinuity" apart from "the host starved us". Compiled in only when
// WAVEFORGE_DIAGNOSTICS=1; all methods are no-ops otherwise.
//
// The audio thread only updates atomics. A message-thread timer (in the
// processor) snapshots them and appends a line to %TEMP%/WaveForge_diag.log.
class Diagnostics
{
public:
#if WAVEFORGE_DIAGNOSTICS
    void blockStart (int numSamples) noexcept
    {
        const auto now = clock::now();
        if (lastBlockEnd != clock::time_point {})
        {
            const auto gapUs = (float) std::chrono::duration_cast<std::chrono::microseconds> (now - lastBlockEnd).count();
            updateMax (maxGapUs, gapUs);
        }
        blockStartTime = now;
        ++blocks;
        updateMax (maxBlock, (float) numSamples);
        updateMin (minBlock, (float) numSamples);
    }

    // Call after the buffer is final. Detects jumps between consecutive
    // samples, including across the block boundary.
    void blockEnd (const juce::AudioBuffer<float>& buf, int numSamples) noexcept
    {
        if (numSamples > 0 && buf.getNumChannels() > 0)
        {
            const float* s = buf.getReadPointer (0);
            float prev = lastSample.load (std::memory_order_relaxed);
            float localMax = 0.0f;
            int   localJumps = 0;
            for (int i = 0; i < numSamples; ++i)
            {
                const float d = std::abs (s[i] - prev);
                if (d > localMax) localMax = d;
                if (d > jumpThreshold) ++localJumps;
                prev = s[i];
            }
            lastSample.store (prev, std::memory_order_relaxed);
            updateMax (maxDelta, localMax);
            jumps.fetch_add (localJumps, std::memory_order_relaxed);
            updateMax (peak, buf.getMagnitude (0, numSamples));
        }

        const auto now = clock::now();
        const auto durUs = (float) std::chrono::duration_cast<std::chrono::microseconds> (now - blockStartTime).count();
        updateMax (maxDurUs, durUs);
        lastBlockEnd = now;
    }

    // Message thread: format + reset the per-interval maxima.
    juce::String snapshotAndReset (double sampleRate)
    {
        const auto b = blocks.exchange (0);
        juce::String line;
        line << "blocks=" << (int) b
             << " blk[min=" << (int) minBlock.exchange (1e9f) << " max=" << (int) maxBlock.exchange (0) << "]"
             << " maxDelta=" << juce::String (maxDelta.exchange (0), 4)
             << " jumps=" << (int) jumps.exchange (0)
             << " peak=" << juce::String (peak.exchange (0), 3)
             << " maxProcUs=" << (int) maxDurUs.exchange (0)
             << " maxGapUs=" << (int) maxGapUs.exchange (0)
             << " (block@" << (int) sampleRate << "Hz)";
        return line;
    }

    // Anything above this between two consecutive samples is not a sine at
    // musical frequencies (440 Hz full-scale moves ~0.06/sample at 44.1 kHz).
    static constexpr float jumpThreshold = 0.15f;

private:
    using clock = std::chrono::steady_clock;

    static void updateMax (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
    static void updateMin (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v < cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }

    clock::time_point blockStartTime {}, lastBlockEnd {};
    std::atomic<int>   blocks { 0 }, jumps { 0 };
    std::atomic<float> minBlock { 1e9f }, maxBlock { 0 }, maxDelta { 0 }, peak { 0 },
                       maxDurUs { 0 }, maxGapUs { 0 }, lastSample { 0 };
#else
    void blockStart (int) noexcept {}
    void blockEnd (const juce::AudioBuffer<float>&, int) noexcept {}
    juce::String snapshotAndReset (double) { return {}; }
#endif
};
