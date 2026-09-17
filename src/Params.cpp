#include "Params.h"

#include "dsp/Lfo.h"
#include "dsp/ModMatrix.h"
#include "dsp/fx/Distortion.h"
#include "dsp/Voice.h"

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using Range  = juce::NormalisableRange<float>;

    constexpr int version = 1;

    juce::ParameterID id (const juce::String& s) { return { s, version }; }

    // Display precision that follows the magnitude, so 20000 Hz does not
    // become "20000.000000" and 0.25 does not become "0".
    juce::String formatValue (float v, int)
    {
        const float a = std::abs (v);
        if (a >= 1000.0f) return juce::String (juce::roundToInt (v));
        if (a >= 100.0f)  return juce::String (v, 1);
        if (a >= 10.0f)   return juce::String (v, 2);
        return juce::String (v, 3);
    }

    std::unique_ptr<juce::AudioParameterFloat> flt (const juce::String& pid, const juce::String& name,
                                                    Range range, float def, const juce::String& label = {})
    {
        return std::make_unique<juce::AudioParameterFloat> (id (pid), name, range, def,
                                                            juce::AudioParameterFloatAttributes()
                                                                .withLabel (label)
                                                                .withStringFromValueFunction (formatValue));
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
    layout.add (choice (ParamID::oscAWarpMode, "OSC A Warp Mode", wf::Voice::warpModeNames(), 0));
    layout.add (flt (ParamID::oscAWarpAmount, "OSC A Warp Amount", unit(), 0.0f));

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

    // ---- LFO 1 / 2
    for (int i = 0; i < 2; ++i)
    {
        const auto l = ParamID::lfo (i);
        const juce::String n = "LFO " + juce::String (i + 1) + " ";
        layout.add (choice (l.shape, n + "Shape", { "Sine", "Triangle", "Saw", "Square", "S&H" }, 0));
        layout.add (boolean (l.tempoSync, n + "Tempo Sync", false));
        layout.add (flt (l.rate, n + "Rate", skewed (0.01f, 40.0f, 2.0f), i == 0 ? 2.0f : 0.5f, "Hz"));
        layout.add (choice (l.division, n + "Division", wf::Lfo::divisionNames(), 5));
        layout.add (boolean (l.retrigger, n + "Retrigger", true));
        layout.add (flt (l.phase, n + "Phase", unit(), 0.0f));
        layout.add (boolean (l.unipolar, n + "Unipolar", false));
    }

    // ---- MOD MATRIX (8 slots)
    const auto sourceNames = wf::ModSource::names();
    const auto destNames   = wf::ModDest::names();
    for (int i = 0; i < wf::numModSlots; ++i)
    {
        const auto m = ParamID::modSlot (i);
        const juce::String n = "Mod " + juce::String (i + 1) + " ";
        layout.add (boolean (m.enabled, n + "On", false));
        layout.add (choice (m.source, n + "Source", sourceNames, 0));
        layout.add (choice (m.dest, n + "Dest", destNames, 0));
        layout.add (flt (m.amount, n + "Amount", Range (-1.0f, 1.0f, 0.001f), 0.0f));
    }

    // ---- FX
    {
        using namespace ParamID::Fx;
        layout.add (boolean (distEnabled, "Dist On", false));
        layout.add (choice (distMode, "Dist Mode", wf::Distortion::modeNames(), 0));
        layout.add (flt (distDrive, "Dist Drive", Range (0.0f, 40.0f, 0.1f), 12.0f, "dB"));
        layout.add (boolean (distOversample, "Dist Oversample 2x", true));
        layout.add (flt (distOutput, "Dist Output", Range (-24.0f, 6.0f, 0.1f), 0.0f, "dB"));
        layout.add (flt (distMix, "Dist Mix", unit(), 1.0f));

        layout.add (boolean (eqEnabled, "EQ On", false));
        layout.add (flt (eqLowGain,  "EQ Low Gain",  Range (-18.0f, 18.0f, 0.1f), 0.0f, "dB"));
        layout.add (flt (eqLowFreq,  "EQ Low Freq",  skewed (30.0f, 600.0f, 120.0f), 120.0f, "Hz"));
        layout.add (flt (eqMidGain,  "EQ Mid Gain",  Range (-18.0f, 18.0f, 0.1f), 0.0f, "dB"));
        layout.add (flt (eqMidFreq,  "EQ Mid Freq",  skewed (200.0f, 8000.0f, 1000.0f), 1000.0f, "Hz"));
        layout.add (flt (eqMidQ,     "EQ Mid Q",     skewed (0.3f, 8.0f, 1.0f), 1.0f));
        layout.add (flt (eqHighGain, "EQ High Gain", Range (-18.0f, 18.0f, 0.1f), 0.0f, "dB"));
        layout.add (flt (eqHighFreq, "EQ High Freq", skewed (1500.0f, 16000.0f, 6000.0f), 6000.0f, "Hz"));

        layout.add (boolean (chorusEnabled, "Chorus On", false));
        layout.add (flt (chorusRate,     "Chorus Rate",     skewed (0.05f, 10.0f, 1.0f), 0.8f, "Hz"));
        layout.add (flt (chorusDepth,    "Chorus Depth",    unit(), 0.3f));
        layout.add (flt (chorusFeedback, "Chorus Feedback", Range (-0.95f, 0.95f, 0.01f), 0.0f));
        layout.add (flt (chorusDelay,    "Chorus Delay",    Range (1.0f, 50.0f, 0.1f), 7.0f, "ms"));
        layout.add (flt (chorusMix,      "Chorus Mix",      unit(), 0.5f));

        layout.add (boolean (delayEnabled, "Delay On", false));
        layout.add (boolean (delayTempoSync, "Delay Tempo Sync", true));
        layout.add (flt (delayTime, "Delay Time", skewed (1.0f, 4000.0f, 300.0f), 375.0f, "ms"));
        layout.add (choice (delayDivision, "Delay Division", wf::Lfo::divisionNames(), 6));
        layout.add (flt (delayFeedback, "Delay Feedback", Range (0.0f, 0.95f, 0.01f), 0.4f));
        layout.add (flt (delayLowpass,  "Delay Lowpass",  skewed (200.0f, 20000.0f, 4000.0f), 6000.0f, "Hz"));
        layout.add (boolean (delayPingPong, "Delay Ping Pong", false));
        layout.add (flt (delayMix, "Delay Mix", unit(), 0.3f));

        layout.add (boolean (reverbEnabled, "Reverb On", false));
        layout.add (flt (reverbSize,     "Reverb Size",     unit(), 0.6f));
        layout.add (flt (reverbDamping,  "Reverb Damping",  unit(), 0.5f));
        layout.add (flt (reverbWidth,    "Reverb Width",    unit(), 1.0f));
        layout.add (flt (reverbPredelay, "Reverb Predelay", Range (0.0f, 250.0f, 1.0f), 10.0f, "ms"));
        layout.add (flt (reverbMix,      "Reverb Mix",      unit(), 0.25f));
    }

    return layout;
}
