#pragma once

#include <juce_dsp/juce_dsp.h>

namespace wf
{

// Per-voice stereo filter: TPT state-variable (Cytomic) core, 12 or 24 dB
// (two cascaded stages), with a soft-clip drive stage in front.
class VoiceFilter
{
public:
    enum Type { lp12 = 0, lp24, hp12, hp24, bp12 };

    void prepare (double sampleRate) noexcept
    {
        juce::dsp::ProcessSpec spec { sampleRate, 32, 2 };
        for (auto& s : stages) s.prepare (spec);
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : stages) s.reset();
    }

    // Control-rate update. cutoff in Hz, resonance 0..1, drive 1..10.
    void update (int typeIn, float cutoffHz, float resonance01, float driveIn) noexcept
    {
        type = typeIn;
        drive = driveIn;
        twoStage = (type == lp24 || type == hp24);

        using F = juce::dsp::StateVariableTPTFilterType;
        const F t = (type == hp12 || type == hp24) ? F::highpass
                  : (type == bp12)                 ? F::bandpass
                                                   : F::lowpass;
        // Q 0.5 (flat) .. ~10 (self-oscillation-ish)
        const float q = 0.5f + resonance01 * resonance01 * 9.5f;
        const float fc = juce::jlimit (20.0f, 20000.0f, cutoffHz);
        for (int i = 0; i < (twoStage ? 2 : 1); ++i)
        {
            stages[(size_t) i].setType (t);
            stages[(size_t) i].setCutoffFrequency (fc);
            stages[(size_t) i].setResonance (q);
        }
    }

    inline void process (float& l, float& r) noexcept
    {
        if (drive > 1.001f)
        {
            l = softClip (l * drive) / drive;
            r = softClip (r * drive) / drive;
        }
        l = stages[0].processSample (0, l);
        r = stages[0].processSample (1, r);
        if (twoStage)
        {
            l = stages[1].processSample (0, l);
            r = stages[1].processSample (1, r);
        }
    }

private:
    static inline float softClip (float x) noexcept
    {
        // Cheap tanh-like curve, monotonic, |y| < 1.
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> stages;
    int type = lp12;
    bool twoStage = false;
    float drive = 1.0f;
};

} // namespace wf
