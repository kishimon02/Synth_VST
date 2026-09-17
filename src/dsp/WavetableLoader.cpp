#include "WavetableLoader.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace wf
{

namespace
{
    constexpr int N = Wavetable::frameSize;
    constexpr int numHarmonics = 512;

    // Classic shapes as harmonic amplitude arrays (index = harmonic number).
    std::vector<float> sineHarmonics()
    {
        std::vector<float> h ((size_t) numHarmonics + 1, 0.0f);
        h[1] = 1.0f;
        return h;
    }
    std::vector<float> triangleHarmonics()
    {
        std::vector<float> h ((size_t) numHarmonics + 1, 0.0f);
        for (int k = 1; k <= numHarmonics; k += 2)
            h[(size_t) k] = (((k - 1) / 2) % 2 == 0 ? 1.0f : -1.0f) / (float) (k * k);
        return h;
    }
    std::vector<float> sawHarmonics()
    {
        std::vector<float> h ((size_t) numHarmonics + 1, 0.0f);
        for (int k = 1; k <= numHarmonics; ++k)
            h[(size_t) k] = 1.0f / (float) k;
        return h;
    }
    std::vector<float> squareHarmonics()
    {
        std::vector<float> h ((size_t) numHarmonics + 1, 0.0f);
        for (int k = 1; k <= numHarmonics; k += 2)
            h[(size_t) k] = 1.0f / (float) k;
        return h;
    }
    std::vector<float> pulseHarmonics (float width)
    {
        // Pulse wave with duty cycle `width` (0..0.5): a_k = 2 sin(pi k w) / (pi k)
        std::vector<float> h ((size_t) numHarmonics + 1, 0.0f);
        for (int k = 1; k <= numHarmonics; ++k)
            h[(size_t) k] = 2.0f * std::sin (juce::MathConstants<float>::pi * (float) k * width)
                            / (juce::MathConstants<float>::pi * (float) k);
        return h;
    }
    std::vector<float> lerp (const std::vector<float>& a, const std::vector<float>& b, float t)
    {
        std::vector<float> r (a.size());
        for (size_t i = 0; i < a.size(); ++i)
            r[i] = a[i] + (b[i] - a[i]) * t;
        return r;
    }

    std::shared_ptr<Wavetable> make (const juce::String& name, std::vector<float> frames)
    {
        return std::make_shared<Wavetable> (name, WavetableLoader::builtinSourceId (name), std::move (frames));
    }

    // Reads the frame size out of Serum's "clm " chunk ("<!>2048 ...") if present.
    int readClmFrameSize (const juce::File& file)
    {
        juce::MemoryBlock block;
        if (! file.loadFileAsData (block) || block.getSize() < 12)
            return 0;

        const auto* bytes = static_cast<const char*> (block.getData());
        const size_t size = block.getSize();
        if (std::memcmp (bytes, "RIFF", 4) != 0 || std::memcmp (bytes + 8, "WAVE", 4) != 0)
            return 0;

        size_t pos = 12;
        while (pos + 8 <= size)
        {
            const juce::uint32 chunkSize = juce::ByteOrder::littleEndianInt (bytes + pos + 4);
            if (std::memcmp (bytes + pos, "clm ", 4) == 0)
            {
                const size_t avail = juce::jmin ((size_t) chunkSize, size - pos - 8);
                const juce::String text (juce::CharPointer_UTF8 (bytes + pos + 8),
                                         juce::CharPointer_UTF8 (bytes + pos + 8 + avail));
                const auto marker = text.indexOf ("<!>");
                if (marker >= 0)
                    return text.substring (marker + 3).trimStart().getIntValue();
                return 0;
            }
            pos += 8 + chunkSize + (chunkSize & 1);
        }
        return 0;
    }
}

void WavetableLoader::synthesizeFrame (const std::vector<float>& amps, float* out)
{
    juce::dsp::FFT fft (11);
    std::vector<float> spec ((size_t) N * 2, 0.0f);
    const int last = juce::jmin ((int) amps.size() - 1, N / 2 - 1);
    for (int k = 1; k <= last; ++k)
    {
        // Sine phase: -j * A * N/2  ->  re = 0, im = -A * N / 2  (inverse divides by N)
        spec[(size_t) (2 * k + 1)] = -amps[(size_t) k] * (float) N * 0.5f;
    }
    fft.performRealOnlyInverseTransform (spec.data());

    float peak = 1e-9f;
    for (int i = 0; i < N; ++i) peak = juce::jmax (peak, std::abs (spec[(size_t) i]));
    for (int i = 0; i < N; ++i) out[i] = spec[(size_t) i] / peak;
}

const juce::StringArray& WavetableLoader::builtinNames()
{
    static const juce::StringArray names { "Basic Shapes", "Sine", "Harmonics", "PWM" };
    return names;
}

std::shared_ptr<Wavetable> WavetableLoader::createBuiltin (const juce::String& name)
{
    if (name == "Sine")
    {
        std::vector<float> frames ((size_t) N);
        synthesizeFrame (sineHarmonics(), frames.data());
        return make (name, std::move (frames));
    }

    if (name == "Basic Shapes")
    {
        // 16 frames morphing sine -> triangle -> saw -> square (Serum-style).
        constexpr int frames = 16;
        std::vector<float> data ((size_t) frames * N);
        const auto sine = sineHarmonics(), tri = triangleHarmonics(), saw = sawHarmonics(), sq = squareHarmonics();
        for (int f = 0; f < frames; ++f)
        {
            const float t = (float) f / (float) (frames - 1) * 3.0f; // 0..3
            std::vector<float> h;
            if (t < 1.0f)      h = lerp (sine, tri, t);
            else if (t < 2.0f) h = lerp (tri, saw, t - 1.0f);
            else               h = lerp (saw, sq, t - 2.0f);
            synthesizeFrame (h, data.data() + (size_t) f * N);
        }
        return make (name, std::move (data));
    }

    if (name == "Harmonics")
    {
        // Frame f contains harmonics 1..(f+1) at equal amplitude.
        constexpr int frames = 32;
        std::vector<float> data ((size_t) frames * N);
        for (int f = 0; f < frames; ++f)
        {
            std::vector<float> h ((size_t) numHarmonics + 1, 0.0f);
            for (int k = 1; k <= f + 1; ++k) h[(size_t) k] = 1.0f;
            synthesizeFrame (h, data.data() + (size_t) f * N);
        }
        return make (name, std::move (data));
    }

    if (name == "PWM")
    {
        constexpr int frames = 32;
        std::vector<float> data ((size_t) frames * N);
        for (int f = 0; f < frames; ++f)
        {
            const float width = 0.5f - 0.47f * (float) f / (float) (frames - 1); // 50% -> 3%
            synthesizeFrame (pulseHarmonics (width), data.data() + (size_t) f * N);
        }
        return make (name, std::move (data));
    }

    return nullptr;
}

std::shared_ptr<Wavetable> WavetableLoader::loadFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Not a readable audio file: " + file.getFileName();
        return nullptr;
    }

    const auto totalSamples = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) N * 4096);
    if (totalSamples <= 0)
    {
        error = "Empty audio file";
        return nullptr;
    }

    juce::AudioBuffer<float> audio (1, totalSamples);
    reader->read (&audio, 0, totalSamples, 0, true, false);
    const float* src = audio.getReadPointer (0);

    int frameLen = readClmFrameSize (file);
    if (frameLen <= 0)
        frameLen = totalSamples >= N ? N : totalSamples; // single-cycle files become one frame

    int numFrames = juce::jmax (1, totalSamples / frameLen);
    const int step = juce::jmax (1, (numFrames + Wavetable::maxFrames - 1) / Wavetable::maxFrames);
    const int outFrames = juce::jmin (Wavetable::maxFrames, (numFrames + step - 1) / step);

    std::vector<float> frames ((size_t) outFrames * N, 0.0f);
    for (int f = 0; f < outFrames; ++f)
    {
        const float* in = src + (size_t) (f * step) * (size_t) frameLen;
        float* out = frames.data() + (size_t) f * N;
        if (frameLen == N)
        {
            std::copy_n (in, N, out);
        }
        else
        {
            // Resample the cycle to 2048 samples (linear, wrap-around).
            for (int i = 0; i < N; ++i)
            {
                const float pos = (float) i * (float) frameLen / (float) N;
                const int i0 = (int) pos;
                const float t = pos - (float) i0;
                const float a = in[i0 % frameLen], b = in[(i0 + 1) % frameLen];
                out[i] = a + (b - a) * t;
            }
        }
    }

    return std::make_shared<Wavetable> (file.getFileNameWithoutExtension(), file.getFullPathName(), std::move (frames));
}

std::shared_ptr<Wavetable> WavetableLoader::fromSourceId (const juce::String& sourceId, juce::String& error)
{
    if (sourceId.startsWith ("builtin:"))
    {
        auto t = createBuiltin (sourceId.fromFirstOccurrenceOf ("builtin:", false, false));
        if (t == nullptr) error = "Unknown built-in wavetable: " + sourceId;
        return t;
    }
    const juce::File file (sourceId);
    if (! file.existsAsFile())
    {
        error = "Wavetable file not found: " + sourceId;
        return nullptr;
    }
    return loadFile (file, error);
}

} // namespace wf
