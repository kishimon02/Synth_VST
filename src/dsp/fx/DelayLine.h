#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

namespace wf
{

// Minimal mono delay line with linear interpolation. Memory is allocated in
// prepare() only. Call read() then push() once per sample: an impulse pushed
// at sample n comes out of read (d) at sample n + d exactly.
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        buffer.assign ((size_t) juce::jmax (4, maxDelaySamples + 2), 0.0f);
        size = (int) buffer.size();
        writePos = 0;
    }

    void clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    int getMaxDelay() const noexcept { return size - 2; }

    float read (float delaySamples) const noexcept
    {
        delaySamples = juce::jlimit (1.0f, (float) (size - 2), delaySamples);
        const int   di = (int) delaySamples;
        const float frac = delaySamples - (float) di;

        int i0 = writePos - di;      if (i0 < 0) i0 += size;
        int i1 = i0 - 1;             if (i1 < 0) i1 += size;
        const float a = buffer[(size_t) i0];
        const float b = buffer[(size_t) i1];
        return a + frac * (b - a);
    }

    void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        if (++writePos >= size)
            writePos = 0;
    }

private:
    std::vector<float> buffer;
    int size = 0, writePos = 0;
};

} // namespace wf
