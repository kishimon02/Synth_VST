#pragma once

#include "Wavetable.h"
#include <cmath>

namespace wf
{

// Reads one phase from a wavetable with linear interpolation between samples
// and between adjacent frames. All per-note work (mip selection, increments)
// happens in setFrequency()/setPosition(); process() is branch-light.
class WavetableReader
{
public:
    void setSampleRate (double sr) noexcept
    {
        sampleRate = sr;
        // Level k is safe while maxHarmonic(k) * f < Nyquist.
        for (int k = 0; k < Wavetable::numMipLevels; ++k)
            maxFreqForLevel[(size_t) k] = (float) (sr * 0.5 / (double) (Wavetable::mipMaxHarmonic (k) + 1));
    }

    void setTable (const Wavetable* t) noexcept
    {
        table = t;
        numFrames = t != nullptr ? t->getNumFrames() : 0;
        setPosition (position01);
    }

    void resetPhase (float phase01) noexcept { phase = phase01 - std::floor (phase01); }

    void setFrequency (float hz) noexcept
    {
        phaseInc = (float) (hz / sampleRate);
        int k = Wavetable::numMipLevels - 1;
        for (int i = 0; i < Wavetable::numMipLevels; ++i)
            if (hz <= maxFreqForLevel[(size_t) i]) { k = i; break; }
        level = k;
    }

    void setPosition (float pos01) noexcept
    {
        position01 = pos01;
        const float maxPos = (float) juce::jmax (0, numFrames - 1);
        const float p = juce::jlimit (0.0f, maxPos, pos01 * maxPos);
        frameA = (int) p;
        frameB = juce::jmin (frameA + 1, juce::jmax (0, numFrames - 1));
        frameMix = p - (float) frameA;
    }

    inline float process() noexcept
    {
        if (table == nullptr)
            return 0.0f;

        const auto& mip = table->getMip (level);
        const float idx = phase * (float) mip.length;
        const int i0 = (int) idx;
        const float t = idx - (float) i0;

        const float* a = mip.frame (frameA);
        const float* b = mip.frame (frameB);
        const float sa = a[i0] + (a[i0 + 1] - a[i0]) * t;
        const float sb = b[i0] + (b[i0 + 1] - b[i0]) * t;

        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;

        return sa + (sb - sa) * frameMix;
    }

private:
    const Wavetable* table = nullptr;
    double sampleRate = 44100.0;
    std::array<float, Wavetable::numMipLevels> maxFreqForLevel {};
    int numFrames = 0, level = 0, frameA = 0, frameB = 0;
    float frameMix = 0.0f, position01 = 0.0f, phase = 0.0f, phaseInc = 0.0f;
};

// One oscillator slot (OSC A or B): up to 8 unison readers with detune,
// stereo spread and blend. Output is stereo.
class UnisonOscillator
{
public:
    static constexpr int maxUnison = 8;

    void setSampleRate (double sr) noexcept
    {
        for (auto& r : readers) r.setSampleRate (sr);
    }

    void setTable (const Wavetable* t) noexcept
    {
        for (auto& r : readers) r.setTable (t);
    }

    // Called at note-on. `randomPhase` spreads the unison readers too.
    void start (int unisonVoices, float phase01, bool randomPhase, juce::Random& rng) noexcept
    {
        numVoices = juce::jlimit (1, maxUnison, unisonVoices);
        for (int i = 0; i < numVoices; ++i)
            readers[(size_t) i].resetPhase (randomPhase ? rng.nextFloat() : phase01 + (float) i * 0.13f);
    }

    // Control-rate update. detuneCents applies +-detune across the stack,
    // width 0..1 pans the stack, blend 0..1 scales the outer voices' level.
    void update (float baseHz, float position01, float detuneCents, float width, float blend) noexcept
    {
        gainNorm = 0.0f;
        for (int i = 0; i < numVoices; ++i)
        {
            const float offset = numVoices > 1 ? ((float) i / (float) (numVoices - 1)) * 2.0f - 1.0f : 0.0f; // -1..1
            const float ratio = std::exp2 (offset * detuneCents / 1200.0f);
            auto& r = readers[(size_t) i];
            r.setFrequency (baseHz * ratio);
            r.setPosition (position01);

            const float w = numVoices > 1 ? (1.0f - (1.0f - blend) * std::abs (offset)) : 1.0f;
            const float pan = offset * width;                     // -1..1
            gainL[(size_t) i] = w * std::sqrt (0.5f * (1.0f - pan));
            gainR[(size_t) i] = w * std::sqrt (0.5f * (1.0f + pan));
            gainNorm += w * w;
        }
        gainNorm = gainNorm > 0.0f ? 1.0f / std::sqrt (gainNorm) : 1.0f;
    }

    inline void process (float& outL, float& outR) noexcept
    {
        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < numVoices; ++i)
        {
            const float s = readers[(size_t) i].process();
            l += s * gainL[(size_t) i];
            r += s * gainR[(size_t) i];
        }
        outL = l * gainNorm;
        outR = r * gainNorm;
    }

private:
    std::array<WavetableReader, maxUnison> readers;
    std::array<float, maxUnison> gainL {}, gainR {};
    int numVoices = 1;
    float gainNorm = 1.0f;
};

} // namespace wf
