#include "Params.h"

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using Range  = juce::NormalisableRange<float>;

    constexpr int version = 1;

    juce::ParameterID id (const juce::String& s) { return { s, version }; }

    std::unique_ptr<juce::AudioParameterFloat> flt (const juce::String& pid, const juce::String& name,
                                                    Range range, float def, const juce::String& label = {})
    {
        return std::make_unique<juce::AudioParameterFloat> (id (pid), name, range, def,
                                                            juce::AudioParameterFloatAttributes().withLabel (label));
    }
    std::unique_ptr<juce::AudioParameterInt> integer (const juce::String& pid, const juce::String& name,
                                                      int lo, int hi, int def, const juce::String& label = {})
    {
        return std::make_unique<juce::AudioParameterInt> (id (pid), name, lo, hi, def,
                                                          juce::AudioParameterIntAttributes().withLabel (label));
    }
    std::unique_ptr<juce::AudioParameterBool> boolean (const juce::String& pid, const juce::String& name, bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (id (pid), name, def);
    }
    std::unique_ptr<juce::AudioParameterChoice> choice (const juce::String& pid, const juce::String& name,
                                                        const juce::StringArray& items, int def)
    {
        return std::make_unique<juce::AudioParameterChoice> (id (pid), name, items, def);
    }

    Range skewed (float lo, float hi, float centre)
    {
        Range r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
    Range timeMs()   { return skewed (1.0f, 10000.0f, 300.0f); }
    Range unit()     { return Range (0.0f, 1.0f, 0.001f); }
}

juce::AudioProcessorValueTreeState::ParameterLayout Params::createLayout()
{
    Layout layout;

    // ---- GLOBAL
    layout.add (flt (ParamID::masterVolume, "Master Volume", Range (-60.0f, 6.0f, 0.1f), -6.0f, "dB"));
    layout.add (integer (ParamID::polyphony, "Polyphony", 1, 16, 8));
    layout.add (integer (ParamID::pitchBendRange, "Pitch Bend Range", 1, 24, 2, "st"));

    // ---- OSC A / B
    for (int i = 0; i < 2; ++i)
    {
        const auto o = ParamID::osc (i);
        const auto n = ParamID::oscPrefix (i);
        layout.add (boolean (o.enabled, n + "On", i == 0));
        layout.add (flt (o.wtPos, n + "WT Position", unit(), 0.0f));
        layout.add (integer (o.octave, n + "Octave", -3, 3, 0));
        layout.add (integer (o.semi, n + "Semitone", -12, 12, 0));
        layout.add (flt (o.fine, n + "Fine", Range (-100.0f, 100.0f, 1.0f), 0.0f, "ct"));
        layout.add (flt (o.level, n + "Level", unit(), 0.75f));
        layout.add (flt (o.pan, n + "Pan", Range (-1.0f, 1.0f, 0.01f), 0.0f));
        layout.add (flt (o.phase, n + "Phase", unit(), 0.0f));
        layout.add (boolean (o.randomPhase, n + "Random Phase", true));
        layout.add (integer (o.unisonVoices, n + "Unison Voices", 1, 8, 1));
        layout.add (flt (o.unisonDetune, n + "Unison Detune", Range (0.0f, 100.0f, 0.1f), 10.0f, "ct"));
        layout.add (flt (o.unisonBlend, n + "Unison Blend", unit(), 0.75f));
        layout.add (flt (o.unisonWidth, n + "Unison Width", unit(), 0.5f));
    }

    // ---- SUB
    layout.add (boolean (ParamID::subEnabled, "Sub On", false));
    layout.add (choice (ParamID::subShape, "Sub Shape", { "Sine", "Triangle", "Saw", "Square" }, 0));
    layout.add (integer (ParamID::subOctave, "Sub Octave", -2, 0, -1));
    layout.add (flt (ParamID::subLevel, "Sub Level", unit(), 0.5f));
    layout.add (boolean (ParamID::subDirect, "Sub Direct Out", false));

    // ---- NOISE
    layout.add (boolean (ParamID::noiseEnabled, "Noise On", false));
    layout.add (choice (ParamID::noiseType, "Noise Type", { "White", "Pink" }, 0));
    layout.add (flt (ParamID::noiseLevel, "Noise Level", unit(), 0.25f));

    // ---- FILTER
    layout.add (boolean (ParamID::filterEnabled, "Filter On", true));
    layout.add (choice (ParamID::filterType, "Filter Type", { "LP 12", "LP 24", "HP 12", "HP 24", "BP 12" }, 1));
    layout.add (flt (ParamID::filterCutoff, "Filter Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, "Hz"));
    layout.add (flt (ParamID::filterResonance, "Filter Resonance", unit(), 0.0f));
    layout.add (flt (ParamID::filterDrive, "Filter Drive", skewed (1.0f, 10.0f, 3.0f), 1.0f));
    layout.add (flt (ParamID::filterKeyTrack, "Filter Key Track", unit(), 0.0f));
    layout.add (boolean (ParamID::filterRouteA, "Filter <- OSC A", true));
    layout.add (boolean (ParamID::filterRouteB, "Filter <- OSC B", true));
    layout.add (boolean (ParamID::filterRouteSub, "Filter <- Sub", true));
    layout.add (boolean (ParamID::filterRouteNoise, "Filter <- Noise", true));
    layout.add (flt (ParamID::filterEnv2Amount, "Filter Env2 Amount", Range (-96.0f, 96.0f, 0.1f), 0.0f, "st"));

    // ---- ENV 1 (amp) / ENV 2 (mod)
    for (int i = 0; i < 2; ++i)
    {
        const auto e = ParamID::env (i);
        const juce::String n = i == 0 ? "Env1 (Amp) " : "Env2 (Mod) ";
        layout.add (flt (e.attack,  n + "Attack",  timeMs(), 5.0f, "ms"));
        layout.add (flt (e.decay,   n + "Decay",   timeMs(), i == 0 ? 200.0f : 300.0f, "ms"));
        layout.add (flt (e.sustain, n + "Sustain", unit(), i == 0 ? 0.8f : 0.0f));
        layout.add (flt (e.release, n + "Release", timeMs(), i == 0 ? 150.0f : 200.0f, "ms"));
    }

    return layout;
}
