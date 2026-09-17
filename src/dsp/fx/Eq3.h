#pragma once

#include <juce_core/juce_core.h>
#include "FxParams.h"
#include <cmath>

namespace wf
{

// Three-band EQ: low shelf, peaking mid, high shelf. RBJ biquads in
// transposed direct form II, coefficients recomputed only when a parameter
// actually changes (no heap use, unlike juce::dsp::IIR::Coefficients).
class Eq3
{
public:
    void prepare (double sampleRateIn) noexcept
    {
        sampleRate = sampleRateIn;
        last = {};
        last.lowGainDb = 1.0e9f;   // force the first update to recompute
        reset();
    }

    void reset() noexcept
    {
        for (auto& band : bands)
            for (auto& s : band.state)
                s = {};
    }

    void update (const EqParams& p) noexcept
    {
        if (p.lowGainDb == last.lowGainDb && p.lowFreqHz == last.lowFreqHz
            && p.midGainDb == last.midGainDb && p.midFreqHz == last.midFreqHz && p.midQ == last.midQ
            && p.highGainDb == last.highGainDb && p.highFreqHz == last.highFreqHz)
            return;
        last = p;

        bands[0].active = std::abs (p.lowGainDb) > 0.01f;
        bands[1].active = std::abs (p.midGainDb) > 0.01f;
        bands[2].active = std::abs (p.highGainDb) > 0.01f;
        if (bands[0].active) bands[0].coeffs = lowShelf  (p.lowFreqHz,  p.lowGainDb);
        if (bands[1].active) bands[1].coeffs = peak      (p.midFreqHz,  p.midGainDb, p.midQ);
        if (bands[2].active) bands[2].coeffs = highShelf (p.highFreqHz, p.highGainDb);
    }

    void process (float* l, float* r, int numSamples) noexcept
    {
        for (auto& band : bands)
        {
            if (! band.active)
                continue;
            const auto& c = band.coeffs;
            auto& sl = band.state[0];
            auto& sr = band.state[1];
            for (int i = 0; i < numSamples; ++i)
            {
                l[i] = tick (c, sl, l[i]);
                r[i] = tick (c, sr, r[i]);
            }
        }
    }

private:
    struct Coeffs { float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f; };
    struct State  { float z1 = 0.0f, z2 = 0.0f; };
    struct Band   { Coeffs coeffs; State state[2]; bool active = false; };

    static inline float tick (const Coeffs& c, State& s, float x) noexcept
    {
        const float y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    static Coeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        Coeffs c;
        c.b0 = (float) (b0 / a0); c.b1 = (float) (b1 / a0); c.b2 = (float) (b2 / a0);
        c.a1 = (float) (a1 / a0); c.a2 = (float) (a2 / a0);
        return c;
    }

    double omega (float freqHz) const noexcept
    {
        const double f = juce::jlimit (10.0, sampleRate * 0.45, (double) freqHz);
        return juce::MathConstants<double>::twoPi * f / sampleRate;
    }

    Coeffs lowShelf (float freqHz, float gainDb) const noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = omega (freqHz), cs = std::cos (w);
        const double alpha = std::sin (w) / std::sqrt (2.0);     // shelf slope S = 1
        const double k = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1) - (A - 1) * cs + k),
                          2 * A * ((A - 1) - (A + 1) * cs),
                          A * ((A + 1) - (A - 1) * cs - k),
                          (A + 1) + (A - 1) * cs + k,
                          -2 * ((A - 1) + (A + 1) * cs),
                          (A + 1) + (A - 1) * cs - k);
    }

    Coeffs highShelf (float freqHz, float gainDb) const noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = omega (freqHz), cs = std::cos (w);
        const double alpha = std::sin (w) / std::sqrt (2.0);
        const double k = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1) + (A - 1) * cs + k),
                          -2 * A * ((A - 1) + (A + 1) * cs),
                          A * ((A + 1) + (A - 1) * cs - k),
                          (A + 1) - (A - 1) * cs + k,
                          2 * ((A - 1) - (A + 1) * cs),
                          (A + 1) - (A - 1) * cs - k);
    }

    Coeffs peak (float freqHz, float gainDb, float q) const noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = omega (freqHz), cs = std::cos (w);
        const double alpha = std::sin (w) / (2.0 * juce::jmax (0.05, (double) q));
        return normalise (1 + alpha * A, -2 * cs, 1 - alpha * A,
                          1 + alpha / A, -2 * cs, 1 - alpha / A);
    }

    double sampleRate = 44100.0;
    Band bands[3];
    EqParams last;
};

} // namespace wf
