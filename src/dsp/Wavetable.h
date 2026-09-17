#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace wf
{

// A Serum-compatible wavetable: up to 256 frames of 2048 samples, plus a set
// of band-limited mip levels derived once at load time (see docs/ER.md, B layer).
//
// Mip level k keeps harmonics 1..maxHarmonic(k) and is stored at
// mipLength(k) samples per frame (2048, 1024, 512, 512, ... - never shorter
// than 512 so linear interpolation stays accurate). The oscillator picks the
// first level whose harmonic content cannot alias at the note's frequency.
// Every frame is stored with one extra sample (a copy of sample 0) so the
// reader can interpolate without a wrap check.
class Wavetable
{
public:
    static constexpr int frameSize    = 2048;
    static constexpr int maxFrames    = 256;
    static constexpr int numMipLevels = 10;
    static constexpr int minMipLength = 512;

    struct MipLevel
    {
        int length      = 0;   // samples per frame at this level
        int maxHarmonic = 0;   // highest harmonic kept
        std::vector<float> data; // numFrames * (length + 1)

        const float* frame (int index) const noexcept
        {
            return data.data() + (size_t) index * (size_t) (length + 1);
        }
    };

    // `frames` holds numFrames * frameSize samples (frame-major). `sourceId`
    // is what gets saved in the plugin state: "builtin:<name>" or a file path.
    Wavetable (juce::String name, juce::String sourceId, std::vector<float> frames);

    const juce::String& getName() const noexcept     { return name; }
    const juce::String& getSourceId() const noexcept { return sourceId; }
    int getNumFrames() const noexcept                { return numFrames; }

    const MipLevel& getMip (int level) const noexcept { return mips[(size_t) level]; }

    // Unfiltered frame (2048 samples) for the 3D/2D display.
    const float* getRawFrame (int index) const noexcept
    {
        return raw.data() + (size_t) index * (size_t) frameSize;
    }

    static int mipLength (int level) noexcept      { return juce::jmax (minMipLength, frameSize >> level); }
    static int mipMaxHarmonic (int level) noexcept { return juce::jmax (1, (frameSize >> (level + 1)) - 1); }

private:
    void buildMips();

    juce::String name, sourceId;
    int numFrames = 0;
    std::vector<float> raw;
    std::array<MipLevel, numMipLevels> mips;

    JUCE_DECLARE_NON_COPYABLE (Wavetable)
};

} // namespace wf
