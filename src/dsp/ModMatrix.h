#pragma once

#include <juce_core/juce_core.h>

namespace wf
{

// Modulation sources and destinations (docs/ER.md layer A: MOD_SLOT).
//
// A slot contributes `amount * sourceValue` to its destination. `amount` is
// -1..1 and the per-destination full-scale range below says what 1.0 means,
// so a slot's contribution is always expressed in the destination's own units.

namespace ModSource
{
    enum Id
    {
        none = 0, env1, env2, lfo1, lfo2, velocity, modWheel, aftertouch, keyTrack, randomPerNote,
        count
    };

    inline juce::StringArray names()
    {
        return { "None", "Env 1", "Env 2", "LFO 1", "LFO 2", "Velocity",
                 "Mod Wheel", "Aftertouch", "Key Track", "Random" };
    }
}

namespace ModDest
{
    enum Id
    {
        none = 0,
        oscAWtPos, oscBWtPos,
        oscAPitch, oscBPitch,
        oscALevel, oscBLevel,
        oscAPan,   oscBPan,
        oscADetune, oscBDetune,
        subLevel, noiseLevel,
        filterCutoff, filterResonance, filterDrive,
        ampLevel,
        count
    };

    inline juce::StringArray names()
    {
        return { "None",
                 "OSC A WT Pos", "OSC B WT Pos",
                 "OSC A Pitch", "OSC B Pitch",
                 "OSC A Level", "OSC B Level",
                 "OSC A Pan", "OSC B Pan",
                 "OSC A Detune", "OSC B Detune",
                 "Sub Level", "Noise Level",
                 "Filter Cutoff", "Filter Reso", "Filter Drive",
                 "Amp Level" };
    }

    // What an amount of 1.0 adds, in the destination's own unit.
    inline float fullScale (int dest) noexcept
    {
        switch (dest)
        {
            case oscAPitch:
            case oscBPitch:        return 48.0f;   // semitones
            case oscADetune:
            case oscBDetune:       return 100.0f;  // cents
            case filterCutoff:     return 96.0f;   // semitones
            case filterDrive:      return 9.0f;    // drive units (1..10)
            default:               return 1.0f;    // normalised 0..1 controls
        }
    }
}

inline constexpr int numModSlots = 8;

struct ModSlotParams
{
    bool  enabled = false;
    int   source  = ModSource::none;
    int   dest    = ModDest::none;
    float amount  = 0.0f;   // -1..1
};

// Accumulates every enabled slot into a per-destination offset array.
// `sources` is indexed by ModSource::Id, `out` by ModDest::Id.
inline void accumulateModulation (const ModSlotParams* slots,
                                  const float* sources,
                                  float* out) noexcept
{
    for (int i = 0; i < ModDest::count; ++i)
        out[i] = 0.0f;

    for (int i = 0; i < numModSlots; ++i)
    {
        const auto& s = slots[i];
        if (! s.enabled || s.dest == ModDest::none || s.source == ModSource::none)
            continue;
        out[s.dest] += s.amount * sources[s.source] * ModDest::fullScale (s.dest);
    }
}

} // namespace wf
