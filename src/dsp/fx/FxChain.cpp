#include "FxChain.h"

namespace wf
{

void FxChain::prepare (double sampleRate, int maxBlockSize)
{
    maxBlock = juce::jmax (1, maxBlockSize);
    distortionUnit.prepare (sampleRate, maxBlock);
    eqUnit.prepare (sampleRate);
    chorusUnit.prepare (sampleRate, maxBlock);
    delayUnit.prepare (sampleRate, maxBlock);
    reverbUnit.prepare (sampleRate, maxBlock);
    scratch.setSize (2, maxBlock);
    reset();
}

void FxChain::reset()
{
    distortionUnit.reset();
    eqUnit.reset();
    chorusUnit.reset();
    delayUnit.reset();
    reverbUnit.reset();
    for (auto& w : wasEnabled) w = false;
}

bool FxChain::unitTurnedOn (Unit u, bool enabled) noexcept
{
    const bool turnedOn = enabled && ! wasEnabled[u];
    wasEnabled[u] = enabled;
    return turnedOn;
}

void FxChain::process (juce::AudioBuffer<float>& buffer, const FxParams& p, float bpm)
{
    const int numChannels = buffer.getNumChannels();
    const int total = buffer.getNumSamples();
    if (numChannels < 1 || total == 0)
        return;

    // Fast path: nothing enabled -> the buffer is left untouched, and the
    // "was enabled" flags are cleared so re-enabling resets the units.
    if (! (p.distortion.enabled || p.eq.enabled || p.chorus.enabled || p.delay.enabled || p.reverb.enabled))
    {
        for (auto& w : wasEnabled) w = false;
        return;
    }

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);

        if (numChannels >= 2)
        {
            float* ptrs[2] { buffer.getWritePointer (0, start), buffer.getWritePointer (1, start) };
            juce::AudioBuffer<float> view (ptrs, 2, n);
            processStereo (view, n, p, bpm);
        }
        else
        {
            scratch.copyFrom (0, 0, buffer, 0, start, n);
            scratch.copyFrom (1, 0, buffer, 0, start, n);
            processStereo (scratch, n, p, bpm);
            buffer.copyFrom (0, start, scratch, 0, 0, n);
            buffer.addFrom  (0, start, scratch, 1, 0, n);
            buffer.applyGain (0, start, n, 0.5f);
        }
    }
}

void FxChain::processStereo (juce::AudioBuffer<float>& stereo, int numSamples, const FxParams& p, float bpm)
{
    float* l = stereo.getWritePointer (0);
    float* r = stereo.getWritePointer (1);

    // Each unit: update first so a reset on switch-on snaps the smoothed
    // values to their targets instead of gliding from stale ones.
    if (p.distortion.enabled)
    {
        distortionUnit.update (p.distortion);
        if (unitTurnedOn (distortion, true)) distortionUnit.reset();
        distortionUnit.process (stereo, numSamples);
    }
    else unitTurnedOn (distortion, false);

    if (p.eq.enabled)
    {
        eqUnit.update (p.eq);
        if (unitTurnedOn (eq, true)) eqUnit.reset();
        eqUnit.process (l, r, numSamples);
    }
    else unitTurnedOn (eq, false);

    if (p.chorus.enabled)
    {
        chorusUnit.update (p.chorus);
        if (unitTurnedOn (chorus, true)) chorusUnit.reset();
        chorusUnit.process (stereo, numSamples);
    }
    else unitTurnedOn (chorus, false);

    if (p.delay.enabled)
    {
        delayUnit.update (p.delay, bpm);
        if (unitTurnedOn (delay, true)) delayUnit.reset();
        delayUnit.process (l, r, numSamples);
    }
    else unitTurnedOn (delay, false);

    if (p.reverb.enabled)
    {
        reverbUnit.update (p.reverb);
        if (unitTurnedOn (reverb, true)) reverbUnit.reset();
        reverbUnit.process (stereo, numSamples);
    }
    else unitTurnedOn (reverb, false);
}

} // namespace wf
