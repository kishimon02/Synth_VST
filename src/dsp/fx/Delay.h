#pragma once

#include <juce_core/juce_core.h>
#include "FxParams.h"
#include "DelayLine.h"
#include "../Lfo.h"

namespace wf
{

// Stereo delay with tempo sync, feedback low-pass and ping-pong mode. The
// delay time is smoothed (50 ms) so changing it glides instead of clicking.
class Delay
{
public:
    static constexpr float maxDelaySeconds = 5.0f;

    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;
        const int maxSamples = (int) std::ceil (maxDelaySeconds * sampleRate);
        for (auto& line : lines)
            line.prepare (maxSamples);
        timeSamples.reset (sampleRate, 0.05);
        mix.reset (sampleRate, 0.02);
        reset();
    }

    void reset() noexcept
    {
        for (auto& line : lines) line.clear();
        lpState[0] = lpState[1] = 0.0f;
        timeSamples.setCurrentAndTargetValue (timeSamples.getTargetValue());
        mix.setCurrentAndTargetValue (mix.getTargetValue());
    }

    void update (const DelayParams& p, float bpm) noexcept
    {
        float seconds = p.tempoSync ? 1.0f / Lfo::syncedRateHz (bpm, p.division)
                                    : p.timeMs * 0.001f;
        seconds = juce::jlimit (0.001f, maxDelaySeconds, seconds);
        timeSamples.setTargetValue ((float) (seconds * sampleRate));

        feedback = juce::jlimit (0.0f, 0.98f, p.feedback);
        pingPong = p.pingPong;
        mix.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mix));

        // one-pole low-pass in the feedback path
        const double fc = juce::jlimit (20.0, sampleRate * 0.45, (double) p.lowpassHz);
        lpCoeff = 1.0f - (float) std::exp (-juce::MathConstants<double>::twoPi * fc / sampleRate);
    }

    float getCurrentDelaySamples() const noexcept { return timeSamples.getCurrentValue(); }

    void process (float* l, float* r, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float d = timeSamples.getNextValue();
            const float m = mix.getNextValue();

            const float outL = lines[0].read (d);
            const float outR = lines[1].read (d);

            // low-passed feedback
            lpState[0] += lpCoeff * (outL - lpState[0]);
            lpState[1] += lpCoeff * (outR - lpState[1]);

            if (pingPong)
            {
                const float mono = 0.5f * (l[i] + r[i]);
                lines[0].push (mono + feedback * lpState[1]);
                lines[1].push (feedback * lpState[0]);
            }
            else
            {
                lines[0].push (l[i] + feedback * lpState[0]);
                lines[1].push (r[i] + feedback * lpState[1]);
            }

            l[i] = l[i] * (1.0f - m) + outL * m;
            r[i] = r[i] * (1.0f - m) + outR * m;
        }
    }

private:
    double sampleRate = 44100.0;
    DelayLine lines[2];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> timeSamples { 16537.5f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix { 0.3f };
    float feedback = 0.4f, lpCoeff = 0.5f;
    float lpState[2] { 0.0f, 0.0f };
    bool pingPong = false;
};

} // namespace wf
