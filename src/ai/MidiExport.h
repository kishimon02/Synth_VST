#pragma once

#include "MusicContext.h"

namespace ai
{

// Writes suggested notes as a standard MIDI file (one track, tempo + time
// signature, drums on channel 10) so it can be dragged into the DAW.
struct MidiExport
{
    static constexpr int ticksPerBeat = 960;

    static bool write (const std::vector<Note>& notes, float bpm, int timeSigNum, int timeSigDen, bool drums,
                       const juce::File& file, juce::String& error);

    // %TEMP%/WaveForge/<name>.mid (unique per call)
    static juce::File tempFileFor (const juce::String& name);
};

} // namespace ai
