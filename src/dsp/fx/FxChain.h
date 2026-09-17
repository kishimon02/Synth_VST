#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "FxParams.h"
#include "Distortion.h"
#include "Eq3.h"
#include "Chorus.h"
#include "Delay.h"
#include "Reverb.h"

namespace wf
{

// Master FX chain, fixed order: Distortion -> EQ -> Chorus -> Delay -> Reverb.
// A disabled unit is skipped entirely (no processing, no state update) and
// reset when it is switched back on, so it never plays a stale tail.
// Mono buffers are widened to stereo internally; blocks larger than the
// prepared size are processed in chunks.
class FxChain
{
public:
    enum Unit { distortion = 0, eq, chorus, delay, reverb, numUnits };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void process (juce::AudioBuffer<float>& buffer, const FxParams& p, float bpm);

    // True when a unit with a tail (delay / reverb) is on.
    static bool hasTail (const FxParams& p) noexcept { return p.delay.enabled || p.reverb.enabled; }

private:
    void processStereo (juce::AudioBuffer<float>& stereo, int numSamples, const FxParams& p, float bpm);
    bool unitTurnedOn (Unit u, bool enabled) noexcept;

    Distortion distortionUnit;
    Eq3        eqUnit;
    Chorus     chorusUnit;
    Delay      delayUnit;
    Reverb     reverbUnit;

    juce::AudioBuffer<float> scratch;   // stereo copy for mono hosts
    int maxBlock = 512;
    bool wasEnabled[numUnits] {};
};

} // namespace wf
