#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs. Kept in one place so DSP, UI, and the mod-matrix all agree.
// Mirrors docs/ER.md layer A. LFO / MOD_SLOT arrive in Phase 2, FX in Phase 3.
namespace ParamID
{
    // GLOBAL
    inline constexpr auto masterVolume   = "master_volume_db";
    inline constexpr auto polyphony      = "polyphony";
    inline constexpr auto pitchBendRange = "pitch_bend_range";

    // OSCILLATOR (A = 0, B = 1)
    struct OscIDs
    {
        juce::String enabled, wtPos, octave, semi, fine, level, pan, phase, randomPhase,
                     unisonVoices, unisonDetune, unisonBlend, unisonWidth;
    };
    inline OscIDs osc (int index)
    {
        const juce::String p = index == 0 ? "osc_a_" : "osc_b_";
        return { p + "enabled", p + "wt_pos", p + "octave", p + "semi", p + "fine", p + "level", p + "pan",
                 p + "phase", p + "random_phase", p + "unison_voices", p + "unison_detune",
                 p + "unison_blend", p + "unison_width" };
    }
    inline juce::String oscPrefix (int index) { return index == 0 ? "OSC A " : "OSC B "; }

    // SUB_OSC
    inline constexpr auto subEnabled = "sub_enabled";
    inline constexpr auto subShape   = "sub_shape";
    inline constexpr auto subOctave  = "sub_octave";
    inline constexpr auto subLevel   = "sub_level";
    inline constexpr auto subDirect  = "sub_direct";

    // NOISE_OSC
    inline constexpr auto noiseEnabled = "noise_enabled";
    inline constexpr auto noiseType    = "noise_type";
    inline constexpr auto noiseLevel   = "noise_level";

    // FILTER
    inline constexpr auto filterEnabled    = "filter_enabled";
    inline constexpr auto filterType       = "filter_type";
    inline constexpr auto filterCutoff     = "filter_cutoff";
    inline constexpr auto filterResonance  = "filter_resonance";
    inline constexpr auto filterDrive      = "filter_drive";
    inline constexpr auto filterKeyTrack   = "filter_keytrack";
    inline constexpr auto filterRouteA     = "filter_route_a";
    inline constexpr auto filterRouteB     = "filter_route_b";
    inline constexpr auto filterRouteSub   = "filter_route_sub";
    inline constexpr auto filterRouteNoise = "filter_route_noise";
    inline constexpr auto filterEnv2Amount = "filter_env2_amount";

    // ENVELOPE (1 = amp, 2 = mod)
    struct EnvIDs { juce::String attack, decay, sustain, release; };
    inline EnvIDs env (int index)
    {
        const juce::String p = "env" + juce::String (index + 1) + "_";
        return { p + "attack", p + "decay", p + "sustain", p + "release" };
    }

    // LFO (1, 2)
    struct LfoIDs { juce::String shape, tempoSync, rate, division, retrigger, phase, unipolar; };
    inline LfoIDs lfo (int index)
    {
        const juce::String p = "lfo" + juce::String (index + 1) + "_";
        return { p + "shape", p + "tempo_sync", p + "rate", p + "division",
                 p + "retrigger", p + "phase", p + "unipolar" };
    }

    // MOD_SLOT (0..7)
    struct ModSlotIDs { juce::String enabled, source, dest, amount; };
    inline ModSlotIDs modSlot (int index)
    {
        const juce::String p = "mod" + juce::String (index + 1) + "_";
        return { p + "enabled", p + "source", p + "dest", p + "amount" };
    }

    // Non-parameter state stored as properties on the APVTS ValueTree.
    inline const juce::Identifier oscTableProperty (int index) { return index == 0 ? "osc_a_table" : "osc_b_table"; }
}

namespace Params
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
