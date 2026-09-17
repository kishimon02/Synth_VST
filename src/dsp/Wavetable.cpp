#include "Wavetable.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace wf
{

Wavetable::Wavetable (juce::String nameIn, juce::String sourceIdIn, std::vector<float> frames)
    : name (std::move (nameIn)), sourceId (std::move (sourceIdIn)), raw (std::move (frames))
{
    numFrames = juce::jlimit (1, maxFrames, (int) (raw.size() / (size_t) frameSize));
    raw.resize ((size_t) numFrames * (size_t) frameSize, 0.0f);
    buildMips();
}

// Spectral resampling: FFT each 2048-sample frame once, then for every mip
// level keep bins 1..maxHarmonic and inverse-transform at that level's length.
// juce::dsp::FFT::performRealOnlyInverseTransform only reads the first
// (size/2 + 1) complex bins and scales by 1/size, so a spectrum computed at
// size N must be multiplied by M/N to keep the same amplitude at size M.
void Wavetable::buildMips()
{
    constexpr int fullOrder = 11; // 2^11 == 2048
    static_assert ((1 << fullOrder) == frameSize);

    juce::dsp::FFT forward (fullOrder);
    std::vector<float> spectrum ((size_t) frameSize * 2);

    // One inverse FFT object per distinct level length.
    std::array<std::unique_ptr<juce::dsp::FFT>, numMipLevels> inverse;
    for (int level = 0; level < numMipLevels; ++level)
    {
        const int len = mipLength (level);
        int order = 0;
        while ((1 << order) < len) ++order;
        inverse[(size_t) level] = std::make_unique<juce::dsp::FFT> (order);

        auto& mip = mips[(size_t) level];
        mip.length      = len;
        mip.maxHarmonic = mipMaxHarmonic (level);
        mip.data.assign ((size_t) numFrames * (size_t) (len + 1), 0.0f);
    }

    std::vector<float> work;

    for (int f = 0; f < numFrames; ++f)
    {
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        std::copy_n (getRawFrame (f), frameSize, spectrum.begin());
        forward.performRealOnlyForwardTransform (spectrum.data(), true);
        // spectrum[2k], spectrum[2k+1] = re, im of bin k (k = 0 .. 1024)

        for (int level = 0; level < numMipLevels; ++level)
        {
            auto& mip = mips[(size_t) level];
            const int len = mip.length;
            const float scale = (float) len / (float) frameSize;
            const int lastBin = juce::jmin (mip.maxHarmonic, len / 2 - 1, frameSize / 2 - 1);

            work.assign ((size_t) len * 2, 0.0f);
            for (int k = 1; k <= lastBin; ++k) // bin 0 (DC) intentionally dropped
            {
                work[(size_t) (2 * k)]     = spectrum[(size_t) (2 * k)]     * scale;
                work[(size_t) (2 * k + 1)] = spectrum[(size_t) (2 * k + 1)] * scale;
            }

            inverse[(size_t) level]->performRealOnlyInverseTransform (work.data());

            float* dst = mip.data.data() + (size_t) f * (size_t) (len + 1);
            std::copy_n (work.begin(), len, dst);
            dst[len] = dst[0]; // wrap sample
        }
    }
}

} // namespace wf
