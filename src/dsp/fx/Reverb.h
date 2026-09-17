#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "FxParams.h"
#include "DelayLine.h"

namespace wf
{

// juce::Reverb (Freeverb-style, light) with a pre-delay on the wet path.
// The reverb runs 100% wet; dry/wet mixing is done here with a smoothed mix
// so it behaves like the other units. Room parameters are pushed to the
// reverb only when they change.
class Reverb
{
public:
    static constexpr float maxPredelayMs = 250.0f;

    void prepare (double sampleRateIn, int maxBlockSize)
    {
        sampleRate = sampleRateIn;
        maxBlock = juce::jmax (1, maxBlockSize);
        reverb.setSampleRate (sampleRate);
        const int maxSamples = (int) std::ceil (maxPredelayMs * 0.001 * sampleRate);
        for (auto& line : predelay)
            line.prepare (maxSamples);
        dry.setSize (2, maxBlock);
        predelaySamples.reset (sampleRate, 0.05);
        mix.reset (sampleRate, 0.02);
        last = {};
        last.size = -1.0f;   // force the first update through
        reset();
    }

    void reset() noexcept
    {
        reverb.reset();
        for (auto& line : predelay) line.clear();
        predelaySamples.setCurrentAndTargetValue (predelaySamples.getTargetValue());
        mix.setCurrentAndTargetValue (mix.getTargetValue());
    }

    void update (const ReverbParams& p) noexcept
    {
        predelaySamples.setTargetValue ((float) (juce::jlimit (0.0f, maxPredelayMs, p.predelayMs) * 0.001 * sampleRate));
        mix.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mix));

        if (p.size == last.size && p.damping == last.damping && p.width == last.width)
            return;
        last = p;

        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, p.size);
        rp.damping = juce::jlimit (0.0f, 1.0f, p.damping);
        rp.width = juce::jlimit (0.0f, 1.0f, p.width);
        rp.wetLevel = 0.33f;   // juce::Reverb scales wet by 3, so this is ~unity
        rp.dryLevel = 0.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
    }

    // `buffer` must be stereo and no longer than maxBlockSize.
    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        numSamples = juce::jmin (numSamples, maxBlock);
        float* l = buffer.getWritePointer (0);
        float* r = buffer.getWritePointer (1);
        for (int ch = 0; ch < 2; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float d = predelaySamples.getNextValue();
            const float inL = l[i], inR = r[i];
            if (d >= 1.0f)
            {
                l[i] = predelay[0].read (d);
                r[i] = predelay[1].read (d);
            }
            predelay[0].push (inL);
            predelay[1].push (inR);
        }

        reverb.processStereo (l, r, numSamples);

        const float* dl = dry.getReadPointer (0);
        const float* dr = dry.getReadPointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = mix.getNextValue();
            l[i] = dl[i] * (1.0f - m) + l[i] * m;
            r[i] = dr[i] * (1.0f - m) + r[i] * m;
        }
    }

private:
    double sampleRate = 44100.0;
    int maxBlock = 512;
    juce::Reverb reverb;
    DelayLine predelay[2];
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> predelaySamples { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix { 0.25f };
    ReverbParams last;
};

} // namespace wf
