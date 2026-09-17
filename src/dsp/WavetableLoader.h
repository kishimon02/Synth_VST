#pragma once

#include "Wavetable.h"
#include <memory>

namespace wf
{

// Creates wavetables: generated built-ins (no file IO) and Serum-style .wav
// files (2048 samples per frame, optional "clm " chunk giving the frame size).
class WavetableLoader
{
public:
    static const juce::StringArray& builtinNames();

    // Returns nullptr if `name` is unknown.
    static std::shared_ptr<Wavetable> createBuiltin (const juce::String& name);

    // Returns nullptr and fills `error` on failure.
    static std::shared_ptr<Wavetable> loadFile (const juce::File& file, juce::String& error);

    // Resolves a sourceId saved in the plugin state ("builtin:Name" or a path).
    static std::shared_ptr<Wavetable> fromSourceId (const juce::String& sourceId, juce::String& error);

    static juce::String builtinSourceId (const juce::String& name) { return "builtin:" + name; }

private:
    // Fills `out` (2048 samples) from harmonic amplitudes (index 1 = fundamental),
    // normalised to peak 1.0.
    static void synthesizeFrame (const std::vector<float>& harmonicAmps, float* out);
};

} // namespace wf
