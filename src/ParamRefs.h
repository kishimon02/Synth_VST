#pragma once

#include "Params.h"
#include "dsp/SynthParams.h"

// Cached raw-value pointers for every parameter, bound once from the APVTS.
// snapshot() turns them into a plain wf::SynthParams for the audio thread.
struct ParamRefs
{
    using P = std::atomic<float>*;

    struct Osc
    {
        P enabled, wtPos, octave, semi, fine, level, pan, phase, randomPhase,
          unisonVoices, unisonDetune, unisonBlend, unisonWidth;
    };

    Osc osc[2];
    P subEnabled, subShape, subOctave, subLevel, subDirect;
    P noiseEnabled, noiseType, noiseLevel;
    P filterEnabled, filterType, filterCutoff, filterResonance, filterDrive, filterKeyTrack,
      filterRouteA, filterRouteB, filterRouteSub, filterRouteNoise, filterEnv2Amount;
    struct Env { P attack, decay, sustain, release; } env[2];
    P masterVolume, polyphony, pitchBendRange;

    void bind (juce::AudioProcessorValueTreeState& apvts)
    {
        auto get = [&apvts] (const juce::String& id)
        {
            auto* p = apvts.getRawParameterValue (id);
            jassert (p != nullptr); // typo in a parameter id
            return p;
        };

        for (int i = 0; i < 2; ++i)
        {
            const auto o = ParamID::osc (i);
            osc[i] = { get (o.enabled), get (o.wtPos), get (o.octave), get (o.semi), get (o.fine),
                       get (o.level), get (o.pan), get (o.phase), get (o.randomPhase),
                       get (o.unisonVoices), get (o.unisonDetune), get (o.unisonBlend), get (o.unisonWidth) };
        }
        subEnabled = get (ParamID::subEnabled); subShape = get (ParamID::subShape);
        subOctave = get (ParamID::subOctave);   subLevel = get (ParamID::subLevel);
        subDirect = get (ParamID::subDirect);

        noiseEnabled = get (ParamID::noiseEnabled); noiseType = get (ParamID::noiseType);
        noiseLevel = get (ParamID::noiseLevel);

        filterEnabled = get (ParamID::filterEnabled);     filterType = get (ParamID::filterType);
        filterCutoff = get (ParamID::filterCutoff);       filterResonance = get (ParamID::filterResonance);
        filterDrive = get (ParamID::filterDrive);         filterKeyTrack = get (ParamID::filterKeyTrack);
        filterRouteA = get (ParamID::filterRouteA);       filterRouteB = get (ParamID::filterRouteB);
        filterRouteSub = get (ParamID::filterRouteSub);   filterRouteNoise = get (ParamID::filterRouteNoise);
        filterEnv2Amount = get (ParamID::filterEnv2Amount);

        for (int i = 0; i < 2; ++i)
        {
            const auto e = ParamID::env (i);
            env[i] = { get (e.attack), get (e.decay), get (e.sustain), get (e.release) };
        }

        masterVolume = get (ParamID::masterVolume);
        polyphony = get (ParamID::polyphony);
        pitchBendRange = get (ParamID::pitchBendRange);
    }

    wf::SynthParams snapshot (const wf::Wavetable* tableA, const wf::Wavetable* tableB) const
    {
        wf::SynthParams s;
        auto f = [] (P p) { return p->load (std::memory_order_relaxed); };
        auto b = [&f] (P p) { return f (p) >= 0.5f; };
        auto n = [&f] (P p) { return (int) std::lround (f (p)); };

        for (int i = 0; i < 2; ++i)
        {
            const auto& o = osc[i];
            auto& d = s.osc[i];
            d.enabled = b (o.enabled);
            d.table = i == 0 ? tableA : tableB;
            d.wtPosition = f (o.wtPos);
            d.octave = n (o.octave);
            d.semitone = n (o.semi);
            d.fineCents = f (o.fine);
            d.level = f (o.level);
            d.pan = f (o.pan);
            d.phase = f (o.phase);
            d.randomPhase = b (o.randomPhase);
            d.unisonVoices = n (o.unisonVoices);
            d.unisonDetune = f (o.unisonDetune);
            d.unisonBlend = f (o.unisonBlend);
            d.unisonWidth = f (o.unisonWidth);
        }

        s.sub = { b (subEnabled), n (subShape), n (subOctave), f (subLevel), b (subDirect) };
        s.noise = { b (noiseEnabled), n (noiseType), f (noiseLevel) };

        s.filter.enabled = b (filterEnabled);
        s.filter.type = n (filterType);
        s.filter.cutoffHz = f (filterCutoff);
        s.filter.resonance = f (filterResonance);
        s.filter.drive = f (filterDrive);
        s.filter.keyTrack = f (filterKeyTrack);
        s.filter.routeA = b (filterRouteA);
        s.filter.routeB = b (filterRouteB);
        s.filter.routeSub = b (filterRouteSub);
        s.filter.routeNoise = b (filterRouteNoise);
        s.filter.env2Amount = f (filterEnv2Amount);

        for (int i = 0; i < 2; ++i)
            s.env[i] = { f (env[i].attack), f (env[i].decay), f (env[i].sustain), f (env[i].release) };

        s.global.masterGain = juce::Decibels::decibelsToGain (f (masterVolume), -60.0f);
        s.global.polyphony = n (polyphony);
        s.global.pitchBendRange = n (pitchBendRange);
        return s;
    }
};
