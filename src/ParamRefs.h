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
    struct Lfo { P shape, tempoSync, rate, division, retrigger, phase, unipolar; } lfo[2];
    struct Mod { P enabled, source, dest, amount; } mod[wf::numModSlots];
    P masterVolume, polyphony, pitchBendRange;

    struct Fx
    {
        P distEnabled, distMode, distDrive, distOversample, distOutput, distMix;
        P eqEnabled, eqLowGain, eqLowFreq, eqMidGain, eqMidFreq, eqMidQ, eqHighGain, eqHighFreq;
        P chorusEnabled, chorusRate, chorusDepth, chorusFeedback, chorusDelay, chorusMix;
        P delayEnabled, delayTempoSync, delayTime, delayDivision, delayFeedback, delayLowpass, delayPingPong, delayMix;
        P reverbEnabled, reverbSize, reverbDamping, reverbWidth, reverbPredelay, reverbMix;
    } fx;

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

        for (int i = 0; i < 2; ++i)
        {
            const auto l = ParamID::lfo (i);
            lfo[i] = { get (l.shape), get (l.tempoSync), get (l.rate), get (l.division),
                       get (l.retrigger), get (l.phase), get (l.unipolar) };
        }

        for (int i = 0; i < wf::numModSlots; ++i)
        {
            const auto m = ParamID::modSlot (i);
            mod[i] = { get (m.enabled), get (m.source), get (m.dest), get (m.amount) };
        }

        masterVolume = get (ParamID::masterVolume);
        polyphony = get (ParamID::polyphony);
        pitchBendRange = get (ParamID::pitchBendRange);

        {
            namespace F = ParamID::Fx;
            fx.distEnabled = get (F::distEnabled);       fx.distMode = get (F::distMode);
            fx.distDrive = get (F::distDrive);           fx.distOversample = get (F::distOversample);
            fx.distOutput = get (F::distOutput);         fx.distMix = get (F::distMix);

            fx.eqEnabled = get (F::eqEnabled);
            fx.eqLowGain = get (F::eqLowGain);           fx.eqLowFreq = get (F::eqLowFreq);
            fx.eqMidGain = get (F::eqMidGain);           fx.eqMidFreq = get (F::eqMidFreq);
            fx.eqMidQ = get (F::eqMidQ);
            fx.eqHighGain = get (F::eqHighGain);         fx.eqHighFreq = get (F::eqHighFreq);

            fx.chorusEnabled = get (F::chorusEnabled);   fx.chorusRate = get (F::chorusRate);
            fx.chorusDepth = get (F::chorusDepth);       fx.chorusFeedback = get (F::chorusFeedback);
            fx.chorusDelay = get (F::chorusDelay);       fx.chorusMix = get (F::chorusMix);

            fx.delayEnabled = get (F::delayEnabled);     fx.delayTempoSync = get (F::delayTempoSync);
            fx.delayTime = get (F::delayTime);           fx.delayDivision = get (F::delayDivision);
            fx.delayFeedback = get (F::delayFeedback);   fx.delayLowpass = get (F::delayLowpass);
            fx.delayPingPong = get (F::delayPingPong);   fx.delayMix = get (F::delayMix);

            fx.reverbEnabled = get (F::reverbEnabled);   fx.reverbSize = get (F::reverbSize);
            fx.reverbDamping = get (F::reverbDamping);   fx.reverbWidth = get (F::reverbWidth);
            fx.reverbPredelay = get (F::reverbPredelay); fx.reverbMix = get (F::reverbMix);
        }
    }

    wf::SynthParams snapshot (const wf::Wavetable* tableA, const wf::Wavetable* tableB, float bpm) const
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

        for (int i = 0; i < 2; ++i)
            s.lfo[i] = { n (lfo[i].shape), b (lfo[i].tempoSync), f (lfo[i].rate), n (lfo[i].division),
                         b (lfo[i].retrigger), f (lfo[i].phase), b (lfo[i].unipolar) };

        for (int i = 0; i < wf::numModSlots; ++i)
            s.modSlots[i] = { b (mod[i].enabled), n (mod[i].source), n (mod[i].dest), f (mod[i].amount) };

        s.bpm = bpm;
        s.global.masterGain = juce::Decibels::decibelsToGain (f (masterVolume), -60.0f);
        s.global.polyphony = n (polyphony);
        s.global.pitchBendRange = n (pitchBendRange);

        auto& d = s.fx.distortion;
        d.enabled = b (fx.distEnabled); d.mode = n (fx.distMode); d.driveDb = f (fx.distDrive);
        d.oversample = b (fx.distOversample); d.outputDb = f (fx.distOutput); d.mix = f (fx.distMix);

        auto& e = s.fx.eq;
        e.enabled = b (fx.eqEnabled);
        e.lowGainDb = f (fx.eqLowGain);   e.lowFreqHz = f (fx.eqLowFreq);
        e.midGainDb = f (fx.eqMidGain);   e.midFreqHz = f (fx.eqMidFreq);   e.midQ = f (fx.eqMidQ);
        e.highGainDb = f (fx.eqHighGain); e.highFreqHz = f (fx.eqHighFreq);

        auto& c = s.fx.chorus;
        c.enabled = b (fx.chorusEnabled); c.rateHz = f (fx.chorusRate); c.depth = f (fx.chorusDepth);
        c.feedback = f (fx.chorusFeedback); c.delayMs = f (fx.chorusDelay); c.mix = f (fx.chorusMix);

        auto& dl = s.fx.delay;
        dl.enabled = b (fx.delayEnabled); dl.tempoSync = b (fx.delayTempoSync); dl.timeMs = f (fx.delayTime);
        dl.division = n (fx.delayDivision); dl.feedback = f (fx.delayFeedback); dl.lowpassHz = f (fx.delayLowpass);
        dl.pingPong = b (fx.delayPingPong); dl.mix = f (fx.delayMix);

        auto& r = s.fx.reverb;
        r.enabled = b (fx.reverbEnabled); r.size = f (fx.reverbSize); r.damping = f (fx.reverbDamping);
        r.width = f (fx.reverbWidth); r.predelayMs = f (fx.reverbPredelay); r.mix = f (fx.reverbMix);

        return s;
    }
};
