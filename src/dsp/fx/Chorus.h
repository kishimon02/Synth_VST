#pragma once

#include <juce_dsp/juce_dsp.h>
#include "FxParams.h"

namespace wf
{

// Thin wrapper over juce::dsp::Chorus (allocation-free after prepare, mix
// smoothed internally). Blocks longer than the prepared size are handled by
// the chain, which never hands us more than maxBlockSize samples.
class Chorus
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        chorus.prepare ({ sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize), 2 });
        reset();
    }

    void reset() noexcept { chorus.reset(); }

    void update (const ChorusParams& p) noexcept
    {
        chorus.setRate (juce::jlimit (0.01f, 20.0f, p.rateHz));
        chorus.setDepth (juce::jlimit (0.0f, 1.0f, p.depth));
        chorus.setFeedback (juce::jlimit (-0.95f, 0.95f, p.feedback));
        chorus.setCentreDelay (juce::jlimit (1.0f, 50.0f, p.delayMs));
        chorus.setMix (juce::jlimit (0.0f, 1.0f, p.mix));
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, 0, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorus.process (ctx);
    }

private:
    juce::dsp::Chorus<float> chorus;
};

} // namespace wf
