#pragma once

#include "Wavetable.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace wf
{

// Pure frame operations used by the wavetable editor (message thread only).
// A frame is Wavetable::frameSize (2048) samples; harmonics are indexed by
// harmonic number (index 0 = DC, unused) as sine amplitudes and phases.
namespace WaveEditOps
{
    constexpr int N = Wavetable::frameSize;
    constexpr int numHarmonics = 256;   // what the harmonic editor exposes

    struct Harmonics
    {
        std::array<float, numHarmonics + 1> magnitude {};   // >= 0
        std::array<float, numHarmonics + 1> phase {};       // radians
    };

    inline juce::dsp::FFT& fft()
    {
        static juce::dsp::FFT f (11);
        return f;
    }

    // frame -> magnitude/phase of harmonics 1..numHarmonics
    inline void analyse (const float* frame, Harmonics& h)
    {
        std::vector<float> spec ((size_t) N * 2, 0.0f);
        std::copy_n (frame, N, spec.begin());
        fft().performRealOnlyForwardTransform (spec.data(), true);
        for (int k = 1; k <= numHarmonics; ++k)
        {
            const float re = spec[(size_t) (2 * k)], im = spec[(size_t) (2 * k + 1)];
            h.magnitude[(size_t) k] = 2.0f * std::sqrt (re * re + im * im) / (float) N;   // sine amplitude
            h.phase[(size_t) k] = std::atan2 (im, re);
        }
    }

    // harmonics -> frame (harmonics above numHarmonics are dropped)
    inline void synthesise (const Harmonics& h, float* frame)
    {
        std::vector<float> spec ((size_t) N * 2, 0.0f);
        for (int k = 1; k <= numHarmonics; ++k)
        {
            const float a = h.magnitude[(size_t) k] * (float) N * 0.5f;
            spec[(size_t) (2 * k)]     = a * std::cos (h.phase[(size_t) k]);
            spec[(size_t) (2 * k + 1)] = a * std::sin (h.phase[(size_t) k]);
        }
        fft().performRealOnlyInverseTransform (spec.data());
        std::copy_n (spec.begin(), N, frame);
    }

    inline float peak (const float* frame)
    {
        float p = 0.0f;
        for (int i = 0; i < N; ++i) p = juce::jmax (p, std::abs (frame[i]));
        return p;
    }

    inline void normalise (float* frame)
    {
        const float p = peak (frame);
        if (p > 1.0e-6f)
            for (int i = 0; i < N; ++i) frame[i] /= p;
    }

    inline void invert (float* frame)  { for (int i = 0; i < N; ++i) frame[i] = -frame[i]; }
    inline void reverse (float* frame) { std::reverse (frame, frame + N); }

    inline void removeDc (float* frame)
    {
        double sum = 0.0;
        for (int i = 0; i < N; ++i) sum += frame[i];
        const float dc = (float) (sum / N);
        for (int i = 0; i < N; ++i) frame[i] -= dc;
    }

    // Circular moving average, `radius` samples each side, applied `passes` times.
    inline void smooth (float* frame, int radius = 8, int passes = 2)
    {
        std::vector<float> tmp ((size_t) N);
        for (int pass = 0; pass < passes; ++pass)
        {
            for (int i = 0; i < N; ++i)
            {
                float s = 0.0f;
                for (int d = -radius; d <= radius; ++d)
                    s += frame[(i + d + N) % N];
                tmp[(size_t) i] = s / (float) (2 * radius + 1);
            }
            std::copy (tmp.begin(), tmp.end(), frame);
        }
    }

    // Linear cross-fade between frame `from` and frame `to` for every frame
    // in between (frames is frame-major, numFrames x N).
    inline void morph (float* frames, int numFrames, int from, int to)
    {
        from = juce::jlimit (0, numFrames - 1, from);
        to   = juce::jlimit (0, numFrames - 1, to);
        if (to - from < 2)
            return;
        const float* a = frames + (size_t) from * N;
        const float* b = frames + (size_t) to * N;
        for (int f = from + 1; f < to; ++f)
        {
            const float t = (float) (f - from) / (float) (to - from);
            float* dst = frames + (size_t) f * N;
            for (int i = 0; i < N; ++i)
                dst[i] = a[i] + (b[i] - a[i]) * t;
        }
    }
}

} // namespace wf
