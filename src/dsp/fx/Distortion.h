#pragma once

#include <juce_dsp/juce_dsp.h>
#include "FxParams.h"

namespace wf
{

// Waveshaper with three curves and optional 2x oversampling. The oversampler
// (polyphase IIR half-band, low latency) is allocated in prepare() only. A DC
// blocker after the shaper removes the offset the fold curve can introduce.
class Distortion
{
public:
    enum Mode { soft = 0, hard, fold, numModes };
    static juce::StringArray modeNames() { return { "Soft", "Hard", "Fold" }; }

    void prepare (double sampleRate, int maxBlockSize)
    {
        maxBlock = juce::jmax (1, maxBlockSize);
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
        oversampler->initProcessing ((size_t) maxBlock);
        dry.setSize (2, maxBlock);
        // 5 Hz: low enough that a clipped 440 Hz square only tilts by ~3 %
        dcCoeff = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
        mix.reset (sampleRate, 0.02);
        reset();
    }

    void reset() noexcept
    {
        if (oversampler != nullptr)
            oversampler->reset();
        for (auto& s : dc) s = {};
        mix.setCurrentAndTargetValue (mix.getTargetValue());
    }

    void update (const DistortionParams& p) noexcept
    {
        mode = p.mode;
        gain = juce::Decibels::decibelsToGain (p.driveDb);
        outGain = juce::Decibels::decibelsToGain (p.outputDb);
        useOversampling = p.oversample;
        mix.setTargetValue (p.mix);
    }

    // `buffer` must be stereo and no longer than maxBlockSize.
    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        numSamples = juce::jmin (numSamples, maxBlock);
        for (int ch = 0; ch < 2; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, 0, (size_t) numSamples);
        if (useOversampling && oversampler != nullptr)
        {
            auto up = oversampler->processSamplesUp (block);
            shapeBlock (up);
            oversampler->processSamplesDown (block);
        }
        else
        {
            shapeBlock (block);
        }

        float* l = buffer.getWritePointer (0);
        float* r = buffer.getWritePointer (1);
        const float* dl = dry.getReadPointer (0);
        const float* dr = dry.getReadPointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = mix.getNextValue();
            l[i] = dl[i] * (1.0f - m) + dcBlock (dc[0], l[i]) * outGain * m;
            r[i] = dr[i] * (1.0f - m) + dcBlock (dc[1], r[i]) * outGain * m;
        }
    }

    static float shape (int mode, float x) noexcept
    {
        switch (mode)
        {
            case hard: return juce::jlimit (-1.0f, 1.0f, x);
            case fold:
            {
                float t = std::fmod (x + 1.0f, 4.0f);
                if (t < 0.0f) t += 4.0f;
                return t < 2.0f ? t - 1.0f : 3.0f - t;
            }
            case soft:
            default:
            {
                // Rational tanh approximation, |y| < 1, flat beyond +-3
                const float c = juce::jlimit (-3.0f, 3.0f, x);
                const float c2 = c * c;
                return c * (27.0f + c2) / (27.0f + 9.0f * c2);
            }
        }
    }

private:
    struct DcState { float xPrev = 0.0f, yPrev = 0.0f; };

    // y[n] = x[n] - x[n-1] + R y[n-1]
    inline float dcBlock (DcState& s, float x) const noexcept
    {
        const float y = x - s.xPrev + dcCoeff * s.yPrev;
        s.xPrev = x;
        s.yPrev = y;
        return y;
    }

    void shapeBlock (juce::dsp::AudioBlock<float>& block) noexcept
    {
        const int n = (int) block.getNumSamples();
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            float* s = block.getChannelPointer (ch);
            for (int i = 0; i < n; ++i)
                s[i] = shape (mode, s[i] * gain);
        }
    }

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> dry;
    DcState dc[2];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix { 1.0f };
    int mode = soft, maxBlock = 512;
    float gain = 1.0f, outGain = 1.0f, dcCoeff = 0.999f;
    bool useOversampling = true;
};

} // namespace wf
