#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace wf
{

// Sub oscillator: sine / triangle / saw / square, one or two octaves below.
// Saw and square use PolyBLEP; at sub frequencies that is plenty.
class SubOscillator
{
public:
    enum Shape { sine = 0, triangle, saw, square };

    void setSampleRate (double sr) noexcept { sampleRate = sr; }
    void reset() noexcept { phase = 0.0f; }

    void update (float hz, int shapeIn) noexcept
    {
        inc = (float) (hz / sampleRate);
        shape = shapeIn;
    }

    inline float process() noexcept
    {
        float out;
        switch (shape)
        {
            case sine:     out = std::sin (phase * juce::MathConstants<float>::twoPi); break;
            case triangle: out = 4.0f * std::abs (phase - 0.5f) - 1.0f; break;
            case saw:      out = 2.0f * phase - 1.0f - polyBlep (phase); break;
            default:
            {
                out = phase < 0.5f ? 1.0f : -1.0f;
                out += polyBlep (phase);
                float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                out -= polyBlep (p2);
                break;
            }
        }
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        return out;
    }

private:
    inline float polyBlep (float t) const noexcept
    {
        if (t < inc)       { t /= inc; return t + t - t * t - 1.0f; }
        if (t > 1.0f - inc){ t = (t - 1.0f) / inc; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    double sampleRate = 44100.0;
    float phase = 0.0f, inc = 0.0f;
    int shape = sine;
};

} // namespace wf
