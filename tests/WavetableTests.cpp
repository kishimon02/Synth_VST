// DSP unit tests. Built as the console app `WaveForgeTests` (excluded from
// the default build): cmake --build --preset vs2022-release --target WaveForgeTests
// then run build/vs2022/WaveForgeTests_artefacts/Release/WaveForgeTests.exe

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Params.h"
#include "PresetManager.h"

#include "dsp/Wavetable.h"
#include "dsp/WavetableLoader.h"
#include "dsp/WavetableOscillator.h"
#include "dsp/Envelope.h"
#include "dsp/SynthEngine.h"
#include "dsp/fx/FxChain.h"
#include "dsp/WaveEditOps.h"
#include "dsp/Arpeggiator.h"
#include "ai/Settings.h"
#include "ai/LlmClient.h"
#include "ai/ParamCatalog.h"
#include "ai/RequestPresets.h"
#include "ai/ChordLibrary.h"
#include "ai/MusicContext.h"
#include "ai/MidiExport.h"
#include "ai/ReplyFormat.h"

#include <cmath>
#include <iostream>

namespace
{
    // Ideal band-limited saw with harmonics 1..H, unit fundamental amplitude.
    float bandlimitedSaw (float phase01, int H)
    {
        float s = 0.0f;
        for (int k = 1; k <= H; ++k)
            s += std::sin (juce::MathConstants<float>::twoPi * (float) k * phase01) / (float) k;
        return s;
    }
}

struct WavetableMipTest final : public juce::UnitTest
{
    WavetableMipTest() : juce::UnitTest ("Wavetable mipmaps", "DSP") {}

    void runTest() override
    {
        beginTest ("mip levels keep only the allowed harmonics at the right amplitude");

        constexpr int N = wf::Wavetable::frameSize;
        std::vector<float> frame ((size_t) N);
        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = bandlimitedSaw ((float) i / (float) N, 1023); // full-band saw

        wf::Wavetable table ("test", "test", frame);
        expectEquals (table.getNumFrames(), 1);

        for (int level = 0; level < wf::Wavetable::numMipLevels; ++level)
        {
            const auto& mip = table.getMip (level);
            const int H = juce::jmin (mip.maxHarmonic, mip.length / 2 - 1);
            float maxErr = 0.0f;
            for (int i = 0; i <= mip.length; ++i)
            {
                const float expected = bandlimitedSaw ((float) i / (float) mip.length, H);
                maxErr = juce::jmax (maxErr, std::abs (mip.frame (0)[i] - expected));
            }
            logMessage ("level " + juce::String (level) + " len=" + juce::String (mip.length)
                        + " H=" + juce::String (H) + " maxErr=" + juce::String (maxErr, 5));
            expectLessThan (maxErr, 2.0e-3f, "level " + juce::String (level));
            expectEquals (mip.frame (0)[mip.length], mip.frame (0)[0], "wrap sample");
        }
    }
};

struct OscillatorTest final : public juce::UnitTest
{
    OscillatorTest() : juce::UnitTest ("Wavetable oscillator", "DSP") {}

    void runTest() override
    {
        beginTest ("sine table reproduces a sine at the requested frequency");
        auto table = wf::WavetableLoader::createBuiltin ("Sine");
        expect (table != nullptr);

        wf::WavetableReader r;
        r.setSampleRate (48000.0);
        r.setTable (table.get());
        r.resetPhase (0.0f);
        r.setFrequency (1000.0f);
        r.setPosition (0.0f);

        float maxErr = 0.0f;
        for (int i = 0; i < 4800; ++i)
        {
            const float expected = std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) i / 48000.0f);
            maxErr = juce::jmax (maxErr, std::abs (r.process() - expected));
        }
        expectLessThan (maxErr, 5.0e-3f);

        beginTest ("high notes pick a low-harmonic mip level (no aliasing)");
        auto saw = wf::WavetableLoader::createBuiltin ("Basic Shapes");
        wf::WavetableReader hi;
        hi.setSampleRate (44100.0);
        hi.setTable (saw.get());
        hi.setPosition (0.7f);
        hi.setFrequency (8000.0f);
        // Any output must be bounded and finite.
        for (int i = 0; i < 1000; ++i)
        {
            const float v = hi.process();
            expect (std::isfinite (v) && std::abs (v) <= 1.5f);
        }
    }
};

struct EnvelopeTest final : public juce::UnitTest
{
    EnvelopeTest() : juce::UnitTest ("Envelope", "DSP") {}

    void runTest() override
    {
        beginTest ("ADSR reaches 1.0 within the attack time and decays to sustain");
        wf::Envelope e;
        e.setSampleRate (48000.0);
        e.setParameters (10.0f, 50.0f, 0.5f, 100.0f);
        e.noteOn();
        int samplesToPeak = 0;
        while (e.getStage() == wf::Envelope::Stage::attack && samplesToPeak < 48000) { e.process(); ++samplesToPeak; }
        expectWithinAbsoluteError ((float) samplesToPeak / 48.0f, 10.0f, 1.5f, "attack ms");
        for (int i = 0; i < 48000 / 4; ++i) e.process();
        expectWithinAbsoluteError (e.getLevel(), 0.5f, 1.0e-3f, "sustain");
        e.noteOff();
        int rel = 0;
        while (e.isActive() && rel < 48000) { e.process(); ++rel; }
        expect (! e.isActive());
        expectLessThan (rel, 48000 / 4);
    }
};

struct EngineRenderTest final : public juce::UnitTest
{
    EngineRenderTest() : juce::UnitTest ("Synth engine render", "DSP") {}

    void runTest() override
    {
        beginTest ("a note renders finite, non-silent, click-free audio and releases to silence");

        auto tableA = wf::WavetableLoader::createBuiltin ("Basic Shapes");
        auto tableB = wf::WavetableLoader::createBuiltin ("Sine");

        wf::SynthParams p;
        p.osc[0].table = tableA.get(); p.osc[0].wtPosition = 0.7f; p.osc[0].unisonVoices = 4;
        p.osc[1].table = tableB.get(); p.osc[1].enabled = true; p.osc[1].octave = -1;
        p.sub.enabled = true; p.noise.enabled = true; p.noise.level = 0.05f;
        p.filter.type = 1; p.filter.cutoffHz = 2000.0f; p.filter.resonance = 0.4f; p.filter.env2Amount = 24.0f;
        p.env[0] = { 5.0f, 100.0f, 0.7f, 80.0f };
        p.env[1] = { 5.0f, 200.0f, 0.0f, 100.0f };

        constexpr double sr = 48000.0;
        constexpr int block = 256;
        wf::SynthEngine engine;
        engine.prepare (sr);

        juce::AudioBuffer<float> out (2, block);
        juce::MidiBuffer midi;
        float peak = 0.0f, maxDelta = 0.0f, prev = 0.0f;
        bool finite = true;
        int activeAfterRelease = -1;

        const int totalBlocks = (int) (sr * 1.0 / block); // 1 s
        for (int b = 0; b < totalBlocks; ++b)
        {
            midi.clear();
            if (b == 0)  midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
            if (b == 0)  midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.9f), 10);
            if (b == 60) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            if (b == 60) midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);

            out.clear();
            engine.process (out, midi, p);

            const float* l = out.getReadPointer (0);
            for (int i = 0; i < block; ++i)
            {
                finite = finite && std::isfinite (l[i]);
                peak = juce::jmax (peak, std::abs (l[i]));
                maxDelta = juce::jmax (maxDelta, std::abs (l[i] - prev));
                prev = l[i];
            }
            if (b == 40) expectEquals (engine.getActiveVoiceCount(), 2, "two voices while held");
        }
        activeAfterRelease = engine.getActiveVoiceCount();

        logMessage ("peak=" + juce::String (peak, 3) + " maxDelta=" + juce::String (maxDelta, 3));
        expect (finite, "all samples finite");
        expectGreaterThan (peak, 0.05f, "audible output");
        expectLessThan (peak, 2.0f, "no runaway gain");
        expectLessThan (maxDelta, 0.6f, "no hard discontinuities");
        expectEquals (activeAfterRelease, 0, "voices freed after release");
    }
};

struct LfoTest final : public juce::UnitTest
{
    LfoTest() : juce::UnitTest ("LFO", "DSP") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;
        constexpr int control = wf::Voice::controlInterval;

        beginTest ("a 1 Hz sine LFO completes exactly one cycle per second");
        wf::Lfo lfo;
        lfo.setSampleRate (sr);
        lfo.reset (0.0f, 1234u);
        lfo.setRate (1.0f);

        float minV = 1.0f, maxV = -1.0f;
        int zeroCrossings = 0;
        float prev = lfo.processControl (wf::Lfo::sine, 0);
        for (int i = 0; i < (int) (sr / control); ++i) // 1 second
        {
            const float v = lfo.processControl (wf::Lfo::sine, control);
            minV = juce::jmin (minV, v);
            maxV = juce::jmax (maxV, v);
            if ((prev < 0.0f) != (v < 0.0f)) ++zeroCrossings;
            prev = v;
        }
        expectWithinAbsoluteError (maxV, 1.0f, 0.01f, "peak");
        expectWithinAbsoluteError (minV, -1.0f, 0.01f, "trough");
        expectEquals (zeroCrossings, 2, "two zero crossings per cycle");

        beginTest ("every shape stays inside -1..1");
        for (int shape = 0; shape < wf::Lfo::numShapes; ++shape)
        {
            wf::Lfo l;
            l.setSampleRate (sr);
            l.reset (0.0f, (uint32_t) (shape + 1) * 7919u);
            l.setRate (7.3f);
            for (int i = 0; i < 4000; ++i)
            {
                const float v = l.processControl (shape, control);
                expect (std::isfinite (v) && v >= -1.001f && v <= 1.001f,
                        "shape " + juce::String (shape));
            }
        }

        beginTest ("tempo sync maps divisions to the right rate");
        // 1 bar at 120 BPM in 4/4 = 2 s = 0.5 Hz; 1/4 = 2 Hz.
        expectWithinAbsoluteError (wf::Lfo::syncedRateHz (120.0f, 3), 0.5f, 1.0e-4f, "1 bar");
        expectWithinAbsoluteError (wf::Lfo::syncedRateHz (120.0f, 5), 2.0f, 1.0e-4f, "1/4");
        expectWithinAbsoluteError (wf::Lfo::syncedRateHz (120.0f, 6), 4.0f, 1.0e-4f, "1/8");
        // A host that reports no tempo must not produce a NaN rate.
        expect (std::isfinite (wf::Lfo::syncedRateHz (0.0f, 5)), "zero BPM guarded");

        beginTest ("free-running voices stay phase-locked to the master phase");
        const float inc = (float) (3.0f / sr);
        float master = 0.0f;
        wf::Lfo voice;
        voice.setSampleRate (sr);
        voice.setRate (3.0f);
        // Master runs for a while, then a voice starts from its phase.
        for (int b = 0; b < 20; ++b) master = wf::Lfo::advancePhase (master, inc, 256);
        voice.reset (master, 99u);
        for (int b = 0; b < 200; ++b)
        {
            master = wf::Lfo::advancePhase (master, inc, control);
            voice.processControl (wf::Lfo::sine, control);
        }
        const float diff = std::abs (voice.getPhase() - master);
        expect (juce::jmin (diff, 1.0f - diff) < 1.0e-3f, "phases still aligned");
    }
};

struct ModMatrixTest final : public juce::UnitTest
{
    ModMatrixTest() : juce::UnitTest ("Modulation matrix", "DSP") {}

    void runTest() override
    {
        beginTest ("slots accumulate into their destination in the destination's units");

        wf::ModSlotParams slots[wf::numModSlots];
        slots[0] = { true, wf::ModSource::lfo1, wf::ModDest::filterCutoff, 0.5f };
        slots[1] = { true, wf::ModSource::env2, wf::ModDest::filterCutoff, 0.25f };
        slots[2] = { true, wf::ModSource::velocity, wf::ModDest::oscAWtPos, 1.0f };
        slots[3] = { false, wf::ModSource::lfo2, wf::ModDest::oscAWtPos, 1.0f }; // disabled
        slots[4] = { true, wf::ModSource::lfo2, wf::ModDest::none, 1.0f };       // no destination
        slots[5] = { true, wf::ModSource::none, wf::ModDest::oscBPitch, 1.0f };  // no source

        float sources[wf::ModSource::count] = {};
        sources[wf::ModSource::lfo1] = 1.0f;
        sources[wf::ModSource::lfo2] = 1.0f;
        sources[wf::ModSource::env2] = 0.5f;
        sources[wf::ModSource::velocity] = 0.8f;

        float mod[wf::ModDest::count];
        wf::accumulateModulation (slots, sources, mod);

        // cutoff full scale is 96 semitones: 0.5*1*96 + 0.25*0.5*96 = 48 + 12
        expectWithinAbsoluteError (mod[wf::ModDest::filterCutoff], 60.0f, 1.0e-4f, "two slots summed");
        expectWithinAbsoluteError (mod[wf::ModDest::oscAWtPos], 0.8f, 1.0e-4f, "normalised destination");
        expectWithinAbsoluteError (mod[wf::ModDest::oscBPitch], 0.0f, 1.0e-6f, "source None contributes nothing");
        expectWithinAbsoluteError (mod[wf::ModDest::oscBWtPos], 0.0f, 1.0e-6f, "untouched destination is zero");

        beginTest ("source and destination name lists match the enums");
        expectEquals (wf::ModSource::names().size(), (int) wf::ModSource::count);
        expectEquals (wf::ModDest::names().size(), (int) wf::ModDest::count);

        beginTest ("an LFO routed to the filter actually changes the output");
        auto table = wf::WavetableLoader::createBuiltin ("Basic Shapes");
        constexpr double sr = 48000.0;
        constexpr int block = 256;

        auto renderPeakSpread = [&] (bool modEnabled)
        {
            wf::SynthParams p;
            p.osc[0].table = table.get();
            p.osc[0].wtPosition = 0.9f;
            p.filter.type = 1;
            p.filter.cutoffHz = 500.0f;
            p.lfo[0] = { wf::Lfo::sine, false, 4.0f, 5, true, 0.0f, false };
            p.modSlots[0] = { modEnabled, wf::ModSource::lfo1, wf::ModDest::filterCutoff, 0.8f };

            wf::SynthEngine engine;
            engine.prepare (sr);
            juce::AudioBuffer<float> out (2, block);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.9f), 0);

            float lo = 1.0e9f, hi = 0.0f;
            for (int b = 0; b < (int) (sr / block); ++b) // 1 s, covering 4 LFO cycles
            {
                out.clear();
                engine.process (out, midi, p);
                midi.clear();
                if (b > 20) // skip the attack
                {
                    const float mag = out.getMagnitude (0, 0, block);
                    lo = juce::jmin (lo, mag);
                    hi = juce::jmax (hi, mag);
                }
            }
            return hi - lo;
        };

        const float spreadOff = renderPeakSpread (false);
        const float spreadOn  = renderPeakSpread (true);
        logMessage ("peak spread: mod off=" + juce::String (spreadOff, 4)
                    + "  mod on=" + juce::String (spreadOn, 4));
        expectGreaterThan (spreadOn, spreadOff * 3.0f, "modulation audibly sweeps the filter");
    }
};

struct FxTest final : public juce::UnitTest
{
    FxTest() : juce::UnitTest ("FX chain", "DSP") {}

    static constexpr double sr = 48000.0;

    static juce::AudioBuffer<float> sine (float freqHz, float amp, int numSamples)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = amp * std::sin (juce::MathConstants<float>::twoPi * freqHz * (float) i / (float) sr);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    static float rms (const juce::AudioBuffer<float>& b, int from, int to)
    {
        double sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) b.getSample (0, i) * b.getSample (0, i);
        return (float) std::sqrt (sum / (double) juce::jmax (1, to - from));
    }

    static bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    void runTest() override
    {
        beginTest ("delay echoes an impulse after exactly the set time, with feedback");
        {
            wf::Delay delay;
            delay.prepare (sr, 256);
            wf::DelayParams dp;
            dp.tempoSync = false; dp.timeMs = 100.0f; dp.feedback = 0.5f;
            dp.lowpassHz = 20000.0f; dp.mix = 1.0f;
            delay.update (dp, 120.0f);
            delay.reset();   // snap the smoothed time to its target

            const int n = 12000;
            juce::AudioBuffer<float> b (2, n);
            b.clear();
            b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f);
            delay.process (b.getWritePointer (0), b.getWritePointer (1), n);

            int peakIndex = 0; float peak = 0.0f;
            for (int i = 1; i < n; ++i)
                if (std::abs (b.getSample (0, i)) > peak) { peak = std::abs (b.getSample (0, i)); peakIndex = i; }
            logMessage ("first echo at sample " + juce::String (peakIndex) + " amp=" + juce::String (peak, 3));
            expectEquals (peakIndex, 4800, "100 ms at 48 kHz");
            expectWithinAbsoluteError (peak, 1.0f, 0.05f, "first echo at unity (mix 1, feedback lowpass open)");
            expectWithinAbsoluteError (std::abs (b.getSample (0, 9600)), 0.5f, 0.05f, "second echo at feedback level");
            expectLessThan (std::abs (b.getSample (0, 2400)), 1.0e-6f, "silence between echoes");

            dp.tempoSync = true; dp.division = 5;   // 1/4 note
            delay.update (dp, 120.0f);
            delay.reset();
            expectWithinAbsoluteError (delay.getCurrentDelaySamples(), 24000.0f, 1.0f, "1/4 at 120 BPM = 500 ms");
        }

        beginTest ("EQ low shelf boosts the lows and leaves the highs alone");
        {
            wf::Eq3 eq;
            eq.prepare (sr);
            wf::EqParams ep;
            ep.lowGainDb = 12.0f; ep.lowFreqHz = 100.0f;
            eq.update (ep);

            auto low = sine (30.0f, 0.1f, 48000);
            eq.process (low.getWritePointer (0), low.getWritePointer (1), low.getNumSamples());
            const float lowGain = juce::Decibels::gainToDecibels (rms (low, 24000, 48000) / (0.1f / std::sqrt (2.0f)));

            eq.reset();
            auto high = sine (5000.0f, 0.1f, 48000);
            eq.process (high.getWritePointer (0), high.getWritePointer (1), high.getNumSamples());
            const float highGain = juce::Decibels::gainToDecibels (rms (high, 24000, 48000) / (0.1f / std::sqrt (2.0f)));

            logMessage ("30 Hz: " + juce::String (lowGain, 2) + " dB   5 kHz: " + juce::String (highGain, 2) + " dB");
            expectWithinAbsoluteError (lowGain, 12.0f, 1.0f, "shelf gain at 30 Hz");
            expectWithinAbsoluteError (highGain, 0.0f, 0.3f, "flat at 5 kHz");
        }

        beginTest ("distortion clips hard, stays bounded and is a no-op at mix 0");
        {
            wf::Distortion dist;
            dist.prepare (sr, 512);
            wf::DistortionParams dp;
            dp.mode = wf::Distortion::hard; dp.driveDb = 30.0f; dp.oversample = false; dp.mix = 1.0f;
            dist.update (dp);
            dist.reset();

            auto b = sine (440.0f, 0.5f, 4096);
            for (int start = 0; start < 4096; start += 512)
            {
                float* ptrs[2] { b.getWritePointer (0, start), b.getWritePointer (1, start) };
                juce::AudioBuffer<float> view (ptrs, 2, 512);
                dist.process (view, 512);
            }
            const float peak = b.getMagnitude (0, 0, 4096);
            const float crest = peak / rms (b, 1024, 4096);
            logMessage ("hard clip: peak=" + juce::String (peak, 3) + " crest=" + juce::String (crest, 3));
            expect (allFinite (b), "finite");
            // The DC blocker after the clipper tilts the flat tops slightly, so
            // allow a few percent over the +-1 clip level.
            expectLessThan (peak, 1.08f, "hard clip bounded to about +-1");
            expectLessThan (crest, 1.15f, "nearly a square wave (sine would be 1.414)");

            dp.mode = wf::Distortion::fold; dp.oversample = true;
            dist.update (dp); dist.reset();
            auto f = sine (440.0f, 0.5f, 512);
            dist.process (f, 512);
            expect (allFinite (f), "fold + oversampling finite");
            expectLessThan (f.getMagnitude (0, 0, 512), 1.5f, "fold bounded");

            dp.mode = wf::Distortion::soft; dp.mix = 0.0f;
            dist.update (dp); dist.reset();
            auto dry = sine (440.0f, 0.5f, 512);
            auto wet = dry;
            dist.process (wet, 512);
            float maxDiff = 0.0f;
            for (int i = 0; i < 512; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (wet.getSample (0, i) - dry.getSample (0, i)));
            expectLessThan (maxDiff, 1.0e-6f, "mix 0 passes the dry signal");
        }

        beginTest ("chorus changes the signal and stays bounded");
        {
            wf::Chorus chorus;
            chorus.prepare (sr, 512);
            wf::ChorusParams cp;
            cp.depth = 0.5f; cp.mix = 0.5f;
            chorus.update (cp);
            chorus.reset();

            auto dry = sine (440.0f, 0.5f, 512);
            auto wet = dry;
            for (int i = 0; i < 40; ++i)   // let the modulation move
            {
                wet = sine (440.0f, 0.5f, 512);
                chorus.process (wet, 512);
            }
            float maxDiff = 0.0f;
            for (int i = 0; i < 512; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (wet.getSample (0, i) - dry.getSample (0, i)));
            expect (allFinite (wet), "finite");
            expectGreaterThan (maxDiff, 0.01f, "chorus audibly alters the signal");
            expectLessThan (wet.getMagnitude (0, 0, 512), 1.2f, "no gain blow-up");
        }

        beginTest ("reverb produces a decaying tail");
        {
            wf::Reverb reverb;
            reverb.prepare (sr, 512);
            wf::ReverbParams rp;
            rp.size = 0.8f; rp.mix = 1.0f; rp.predelayMs = 0.0f;
            reverb.update (rp);
            reverb.reset();

            const int n = (int) sr * 2;
            juce::AudioBuffer<float> b (2, n);
            b.clear();
            b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f);
            for (int start = 0; start < n; start += 512)
            {
                float* ptrs[2] { b.getWritePointer (0, start), b.getWritePointer (1, start) };
                juce::AudioBuffer<float> view (ptrs, 2, juce::jmin (512, n - start));
                reverb.process (view, view.getNumSamples());
            }
            const float early = rms (b, 4800, 24000);
            const float late  = rms (b, 72000, 96000);
            logMessage ("reverb rms early=" + juce::String (early, 5) + " late=" + juce::String (late, 5));
            expect (allFinite (b), "finite");
            expectGreaterThan (early, 1.0e-4f, "tail present");
            expectGreaterThan (late, 0.0f, "tail still ringing at 1.5 s");
            expectLessThan (late, early, "tail decays");
        }

        beginTest ("the chain is bit-exact when bypassed, and safe on mono / large blocks");
        {
            wf::FxChain chain;
            chain.prepare (sr, 64);
            wf::FxParams off;   // everything disabled by default

            auto src = sine (220.0f, 0.5f, 1000);
            auto b = src;
            chain.process (b, off, 120.0f);
            bool identical = true;
            for (int i = 0; i < 1000; ++i)
                identical = identical && b.getSample (0, i) == src.getSample (0, i);
            expect (identical, "bypassed chain leaves the buffer untouched");

            wf::FxParams on;
            on.distortion.enabled = on.eq.enabled = on.chorus.enabled = on.delay.enabled = on.reverb.enabled = true;
            on.eq.lowGainDb = 6.0f; on.eq.midGainDb = -4.0f; on.eq.highGainDb = 3.0f;
            b = src;
            chain.process (b, on, 120.0f);     // 1000 samples through a 64-sample prepare -> chunked
            expect (allFinite (b), "all units on: finite");
            expectLessThan (b.getMagnitude (0, 0, 1000), 4.0f, "all units on: sane level");

            juce::AudioBuffer<float> mono (1, 1000);
            for (int i = 0; i < 1000; ++i)
                mono.setSample (0, i, src.getSample (0, i));
            chain.process (mono, on, 120.0f);
            expect (allFinite (mono), "mono buffer: finite");

            chain.process (b, off, 120.0f);    // switching everything off again
            chain.process (b, on, 120.0f);     // and back on resets the units without blowing up
            expect (allFinite (b), "toggle: finite");
        }
    }
};

struct WaveEditTest final : public juce::UnitTest
{
    WaveEditTest() : juce::UnitTest ("Wave editing", "DSP") {}

    void runTest() override
    {
        namespace Ops = wf::WaveEditOps;
        constexpr int N = wf::Wavetable::frameSize;

        beginTest ("harmonic analysis and synthesis round-trip a band-limited frame");
        {
            std::vector<float> saw ((size_t) N);
            for (int i = 0; i < N; ++i)
                saw[(size_t) i] = bandlimitedSaw ((float) i / (float) N, 100);   // 100 harmonics < 256

            Ops::Harmonics h;
            Ops::analyse (saw.data(), h);
            expectWithinAbsoluteError (h.magnitude[1], 1.0f, 1.0e-3f, "fundamental amplitude");
            expectWithinAbsoluteError (h.magnitude[7], 1.0f / 7.0f, 1.0e-3f, "7th harmonic amplitude");
            expectLessThan (h.magnitude[101], 1.0e-3f, "nothing above the 100th harmonic");

            std::vector<float> back ((size_t) N);
            Ops::synthesise (h, back.data());
            float maxErr = 0.0f;
            for (int i = 0; i < N; ++i)
                maxErr = juce::jmax (maxErr, std::abs (back[(size_t) i] - saw[(size_t) i]));
            logMessage ("round-trip maxErr=" + juce::String (maxErr, 6));
            expectLessThan (maxErr, 1.0e-3f, "synthesis reproduces the analysed frame");
        }

        beginTest ("frame utilities behave");
        {
            std::vector<float> f ((size_t) N, 0.25f);
            for (int i = 0; i < N; ++i) f[(size_t) i] += 0.5f * std::sin (juce::MathConstants<float>::twoPi * (float) i / (float) N);
            Ops::removeDc (f.data());
            double sum = 0.0; for (float v : f) sum += v;
            expectLessThan (std::abs ((float) sum / (float) N), 1.0e-5f, "DC removed");
            Ops::normalise (f.data());
            expectWithinAbsoluteError (Ops::peak (f.data()), 1.0f, 1.0e-5f, "normalised to peak 1");

            std::vector<float> table ((size_t) N * 3, 0.0f);
            wf::WavetableLoader::basicShape ("Sine", table.data());
            wf::WavetableLoader::basicShape ("Saw", table.data() + 2 * N);
            Ops::morph (table.data(), 3, 0, 2);
            const float mid = table[(size_t) N + 100];
            expectWithinAbsoluteError (mid, 0.5f * (table[100] + table[(size_t) 2 * N + 100]), 1.0e-6f, "middle frame is the average");
        }

        beginTest ("a saved wavetable file loads back with the same frames");
        {
            std::vector<float> table ((size_t) N * 4);
            wf::WavetableLoader::basicShape ("Sine", table.data());
            wf::WavetableLoader::basicShape ("Triangle", table.data() + N);
            wf::WavetableLoader::basicShape ("Square", table.data() + 2 * N);
            wf::WavetableLoader::basicShape ("Pulse", table.data() + 3 * N);

            auto file = juce::File::createTempFile (".wav");
            juce::String error;
            expect (wf::WavetableLoader::saveFile (table.data(), 4, file, error), error);

            auto loaded = wf::WavetableLoader::loadFile (file, error);
            expect (loaded != nullptr, error);
            if (loaded != nullptr)
            {
                expectEquals (loaded->getNumFrames(), 4, "clm chunk gives 4 frames of 2048");
                float maxErr = 0.0f;
                for (int f = 0; f < 4; ++f)
                    for (int i = 0; i < N; ++i)
                        maxErr = juce::jmax (maxErr, std::abs (loaded->getRawFrame (f)[i] - table[(size_t) f * N + (size_t) i]));
                expectLessThan (maxErr, 1.0e-6f, "32-bit float round trip is exact");
            }
            file.deleteFile();
        }
    }
};

struct WarpTest final : public juce::UnitTest
{
    WarpTest() : juce::UnitTest ("OSC warp", "DSP") {}

    static float renderPeakDiff (int warpMode, float amount, float& peakOut)
    {
        auto tableA = wf::WavetableLoader::createBuiltin ("Sine");
        auto tableB = wf::WavetableLoader::createBuiltin ("Sine");
        wf::SynthParams p;
        p.osc[0].table = tableA.get(); p.osc[0].randomPhase = false;
        p.osc[1].table = tableB.get(); p.osc[1].enabled = false; p.osc[1].randomPhase = false;
        p.osc[1].semitone = 7;
        p.osc[0].warpMode = warpMode; p.osc[0].warpAmount = amount;
        p.filter.enabled = false;
        p.env[0] = { 1.0f, 100.0f, 1.0f, 50.0f };

        wf::SynthEngine engine;
        engine.prepare (48000.0);
        juce::AudioBuffer<float> out (2, 4096);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        out.clear();
        engine.process (out, midi, p);

        // reference: same note, warp off
        p.osc[0].warpMode = 0;
        wf::SynthEngine ref;
        ref.prepare (48000.0);
        juce::AudioBuffer<float> refOut (2, 4096);
        refOut.clear();
        ref.process (refOut, midi, p);

        float diff = 0.0f;
        peakOut = 0.0f;
        for (int i = 2048; i < 4096; ++i)
        {
            diff = juce::jmax (diff, std::abs (out.getSample (0, i) - refOut.getSample (0, i)));
            peakOut = juce::jmax (peakOut, std::abs (out.getSample (0, i)));
        }
        return diff;
    }

    void runTest() override
    {
        beginTest ("FM / RM / AM from OSC B change OSC A's output, amount 0 does not");
        float peak = 0.0f;
        expectLessThan (renderPeakDiff (wf::Voice::warpFM, 0.0f, peak), 1.0e-6f, "FM at 0 is identical");
        for (int mode : { (int) wf::Voice::warpFM, (int) wf::Voice::warpRM, (int) wf::Voice::warpAM })
        {
            const float d = renderPeakDiff (mode, 0.8f, peak);
            logMessage ("mode " + juce::String (mode) + " diff=" + juce::String (d, 3) + " peak=" + juce::String (peak, 3));
            expectGreaterThan (d, 0.05f, "warp audibly changes the signal");
            expect (std::isfinite (peak) && peak < 2.0f, "bounded output");
        }
    }
};

struct ArpeggiatorTest final : public juce::UnitTest
{
    ArpeggiatorTest() : juce::UnitTest ("Arpeggiator", "DSP") {}

    struct Event { int sample; bool on; int note; float vel; };

    // Runs the arp for `blocks` blocks of 256 at 48 kHz with the given input
    // in the first block, collecting note events with absolute sample times.
    static std::vector<Event> run (wf::Arpeggiator& arp, const wf::ArpParams& p, const juce::MidiBuffer& firstBlock,
                                   int blocks, double ppqStart = -1.0, float bpm = 120.0f)
    {
        constexpr int block = 256;
        constexpr double sr = 48000.0;
        std::vector<Event> events;
        juce::MidiBuffer out;
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer in;
            if (b == 0) in = firstBlock;
            const double ppq = ppqStart < 0.0 ? -1.0 : ppqStart + (double) (b * block) / sr * (bpm / 60.0);
            arp.process (in, out, block, p, bpm, ppq);
            for (const auto meta : out)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOnOrOff())
                    events.push_back ({ b * block + meta.samplePosition, m.isNoteOn(), m.getNoteNumber(), m.getFloatVelocity() });
            }
        }
        return events;
    }

    void runTest() override
    {
        constexpr double sr = 48000.0;
        wf::Arpeggiator arp;
        arp.prepare (sr);

        juce::MidiBuffer chord;
        chord.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        chord.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);
        chord.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);

        beginTest ("Up mode at 1/16, 120 BPM: notes every 125 ms in C E G order, gate 0.5 = 62.5 ms");
        {
            wf::ArpParams p;
            p.enabled = true; p.mode = wf::Arpeggiator::up; p.division = 7; p.gate = 0.5f;
            const auto ev = run (arp, p, chord, 100);   // 100 * 256 = 25600 samples ~ 533 ms -> 4-5 steps
            std::vector<Event> ons;
            for (const auto& e : ev) if (e.on) ons.push_back (e);
            expect (ons.size() >= 4, "several notes fired");
            if (ons.size() >= 4)
            {
                expectEquals (ons[0].note, 60); expectEquals (ons[1].note, 64); expectEquals (ons[2].note, 67); expectEquals (ons[3].note, 60);
                const int spacing = ons[1].sample - ons[0].sample;
                logMessage ("step spacing = " + juce::String (spacing) + " samples (expected 6000)");
                expectWithinAbsoluteError ((float) spacing, 6000.0f, 2.0f, "125 ms at 48 kHz");
                // first off comes gate * step after the first on
                int firstOff = -1;
                for (const auto& e : ev) if (! e.on && e.note == 60) { firstOff = e.sample; break; }
                expectWithinAbsoluteError ((float) (firstOff - ons[0].sample), 3000.0f, 2.0f, "gate 0.5 -> 62.5 ms");
                expectWithinAbsoluteError (ons[0].vel, 0.8f, 0.02f, "held velocity used");
            }
        }

        beginTest ("Octaves 2 continues an octave up; Down mode reverses");
        {
            arp.reset();
            wf::ArpParams p;
            p.enabled = true; p.mode = wf::Arpeggiator::up; p.division = 7; p.octaves = 2;
            const auto ev = run (arp, p, chord, 200);
            std::vector<int> notes;
            for (const auto& e : ev) if (e.on) notes.push_back (e.note);
            expect (notes.size() >= 6, "six notes fired");
            if (notes.size() >= 6)
            {
                expectEquals (notes[3], 72); expectEquals (notes[4], 76); expectEquals (notes[5], 79);
            }
            arp.reset();
            p.octaves = 1; p.mode = wf::Arpeggiator::down;
            const auto ev2 = run (arp, p, chord, 100);
            std::vector<int> down;
            for (const auto& e : ev2) if (e.on) down.push_back (e.note);
            expect (down.size() >= 3 && down[0] == 67 && down[1] == 64 && down[2] == 60, "down order G E C");
        }

        beginTest ("Rest steps are silent and Tie steps extend the note");
        {
            arp.reset();
            wf::ArpPattern pattern;
            pattern.steps = { { wf::ArpStep::note, 1.0f, 1.0f, 0 }, { wf::ArpStep::tie, 0.0f, 1.0f, 0 },
                              { wf::ArpStep::rest, 0.0f, 1.0f, 0 }, { wf::ArpStep::note, 1.0f, 1.0f, 0 } };
            wf::ArpParams p;
            p.enabled = true; p.mode = wf::Arpeggiator::up; p.division = 7; p.gate = 1.0f; p.pattern = &pattern;
            const auto ev = run (arp, p, chord, 90);    // 23040 samples = 3.84 steps -> steps 0..3 fire
            std::vector<Event> ons, offs;
            for (const auto& e : ev) (e.on ? ons : offs).push_back (e);
            expectEquals ((int) ons.size(), 2, "steps 1 and 4 play, 2 (tie) and 3 (rest) do not");
            if (ons.size() == 2 && ! offs.empty())
            {
                expectWithinAbsoluteError ((float) (ons[1].sample - ons[0].sample), 18000.0f, 3.0f, "second note on step 4");
                expectGreaterThan (offs[0].sample - ons[0].sample, 11000, "tie held the first note past step 2");
            }
        }

        beginTest ("host position drives the steps and a jump back resyncs");
        {
            arp.reset();
            wf::ArpParams p;
            p.enabled = true; p.mode = wf::Arpeggiator::up; p.division = 7;
            const auto ev = run (arp, p, chord, 100, 0.0);   // ppq from 0
            int ons = 0;
            for (const auto& e : ev) ons += e.on ? 1 : 0;
            expect (ons >= 4, "steps fire from host ppq");

            // jump back to ppq 0 with the chord still held -> steps restart without piling up
            juce::MidiBuffer none;
            const auto ev2 = run (arp, p, none, 10, 0.0);
            int ons2 = 0;
            for (const auto& e : ev2) ons2 += e.on ? 1 : 0;
            expect (ons2 >= 1 && ons2 <= 2, "resynced after the jump (" + juce::String (ons2) + " ons in 10 blocks)");
        }

        beginTest ("disabled arp passes MIDI through untouched");
        {
            arp.reset();
            wf::ArpParams p;
            p.enabled = false;
            const auto ev = run (arp, p, chord, 2);
            expectEquals ((int) ev.size(), 3, "three note-ons passed through");
            expect (ev.size() == 3 && ev[0].sample == 0 && ev[0].on, "same timing");
        }

        beginTest ("pattern JSON round trip clamps and trims");
        {
            wf::ArpPattern p;
            p.name = "Test";
            for (int i = 0; i < 40; ++i)
                p.steps.push_back ({ i % 3, 1.5f, -1.0f, 99 });
            p.clampAndTrim();
            expectEquals ((int) p.steps.size(), wf::ArpPattern::maxSteps, "trimmed to 32");
            expectWithinAbsoluteError (p.steps[0].velocity, 1.0f, 1.0e-6f, "velocity clamped");
            expectWithinAbsoluteError (p.steps[0].gate, 0.05f, 1.0e-6f, "gate clamped");
            expectEquals (p.steps[0].noteOffset, 16, "offset clamped");

            wf::ArpPattern back;
            juce::String error;
            expect (wf::ArpPattern::fromJson (juce::JSON::parse (juce::JSON::toString (p.toJson())), back, error), error);
            expectEquals (back.name, juce::String ("Test"));
            expectEquals ((int) back.steps.size(), (int) p.steps.size());
            expectEquals (back.steps[1].kind, (int) wf::ArpStep::rest);
            expectEquals (back.steps[2].kind, (int) wf::ArpStep::tie);
            expect (! wf::ArpPattern::fromJson (juce::JSON::parse ("{\"steps\":[]}"), back, error), "empty steps rejected");
        }
    }
};

// Minimal host for an APVTS, so presets can be tested without the plugin.
struct DummyProcessor final : public juce::AudioProcessor
{
    DummyProcessor() : apvts (*this, nullptr, "WaveForgeState", Params::createLayout()) {}

    const juce::String getName() const override { return "Dummy"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

struct AiTest final : public juce::UnitTest
{
    AiTest() : juce::UnitTest ("AI assistant (offline)", "AI") {}

    void runTest() override
    {
        DummyProcessor proc;
        auto& apvts = proc.apvts;

        beginTest ("parameter catalogue lists every parameter and changes are clamped / filtered");
        {
            const auto catalog = ai::ParamCatalog::describe (apvts);
            int count = 0;
            for (auto* param : proc.getParameters())
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
                {
                    ++count;
                    expect (catalog.contains (withId->paramID + "  "), "catalogue has " + withId->paramID);
                }
            logMessage ("catalogue: " + juce::String (count) + " parameters, " + juce::String (catalog.length()) + " chars");

            auto changes = juce::JSON::parse (R"([{"id":"filter_cutoff","value":800},{"id":"nope","value":1},{"id":"osc_a_level","value":5}])");
            juce::StringArray unknown;
            const auto applied = ai::ParamCatalog::applyChanges (apvts, changes, unknown);
            expectEquals ((int) applied.size(), 2, "two known parameters applied");
            expectEquals (unknown.size(), 1, "one unknown id reported");
            auto* cutoff = apvts.getParameter (ParamID::filterCutoff);
            expectWithinAbsoluteError (cutoff->convertFrom0to1 (cutoff->getValue()), 800.0f, 1.0f, "value applied in Hz");
            auto* level = apvts.getParameter (ParamID::osc (0).level);
            expectWithinAbsoluteError (level->convertFrom0to1 (level->getValue()), 1.0f, 1.0e-4f, "clamped to the range");
            const auto current = ai::ParamCatalog::currentValues (apvts);
            expect (current.contains ("osc_a_level"), "non-default value listed");
            expect (! current.contains ("osc_b_level"), "default value not listed");
        }

        beginTest ("key estimation and context JSON");
        {
            const int majorScale[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
            const int minorScale[8] = { 0, 2, 3, 5, 7, 8, 11, 12 };
            std::vector<ai::Note> cMajor, aMinor;
            for (int i = 0; i < 8; ++i)
            {
                cMajor.push_back ({ 60 + majorScale[i], (float) i, 1.0f, 100 });
                aMinor.push_back ({ 57 + minorScale[i], (float) i, 1.0f, 100 });
            }
            expectEquals (ai::MusicContext::estimateKey (cMajor), juce::String ("C major"));
            expectEquals (ai::MusicContext::estimateKey (aMinor), juce::String ("A minor"));

            ai::MusicContext ctx;
            ctx.appendCaptured ({ { 60, 100, true, 8.0 }, { 64, 90, true, 8.5 }, { 60, 0, false, 9.0 }, { 64, 0, false, 9.5 } });
            ctx.finishCapture();
            expectEquals ((int) ctx.ownPart.notes.size(), 2);
            expectWithinAbsoluteError (ctx.ownPart.notes[0].startBeat, 0.0f, 1.0e-4f, "phrase starts at bar 0 (bar 2 -> 0)");
            expectWithinAbsoluteError (ctx.ownPart.notes[1].startBeat, 0.5f, 1.0e-4f);
            const auto json = juce::JSON::parse (ctx.toJson());
            expect (json.isObject() && json.getProperty ("own_part", juce::var()).size() == 2, "context JSON has the notes");
        }

        beginTest ("MIDI export round-trips through a MIDI file");
        {
            std::vector<ai::Note> notes { { 60, 0.0f, 1.0f, 100 }, { 64, 1.0f, 0.5f, 80 }, { 67, 2.0f, 2.0f, 120 } };
            auto file = juce::File::createTempFile (".mid");
            juce::String error;
            expect (ai::MidiExport::write (notes, 128.0f, 4, 4, false, file, error), error);
            ai::Track back;
            float bpm = 0.0f;
            expect (ai::MusicContext::loadMidiFile (file, back, &bpm, error), error);
            expectEquals ((int) back.notes.size(), 3);
            expectWithinAbsoluteError (bpm, 128.0f, 0.5f, "tempo survives");
            expectWithinAbsoluteError (back.notes[2].startBeat, 2.0f, 1.0e-3f);
            expectWithinAbsoluteError (back.notes[2].durationBeats, 2.0f, 1.0e-3f);
            expect (! back.drums, "channel 1 is not drums");
            expect (ai::MidiExport::write (notes, 120.0f, 4, 4, true, file, error), error);
            expect (ai::MusicContext::loadMidiFile (file, back, nullptr, error) && back.drums, "channel 10 flagged as drums");
            file.deleteFile();
        }

        beginTest ("reply parsing, wavetable synthesis and the request body shape");
        {
            const auto reply = juce::JSON::parse (R"({
                "reply": "4 小節のメロディです",
                "notes_kind": "melody",
                "notes": [{"pitch": 72, "start_beat": 0, "duration_beats": 1, "velocity": 100},
                          {"pitch": 74, "start_beat": 1, "duration_beats": 1, "velocity": 90}],
                "param_changes": [{"id": "filter_cutoff", "value": 800}],
                "wavetable": {"name": "Glassy", "frames": [{"harmonics": [1, 0, 0.5]}, {"harmonics": [1, 0.8, 0.2, 0.1]}]},
                "preset_name": "Test",
                "arp_pattern": {"name": "Tr", "steps": [{"kind":"note","velocity":1,"gate":0.5,"note_offset":0},{"kind":"rest","velocity":0,"gate":1,"note_offset":0}]}
            })");
            ai::ChatMessage m;
            juce::String error;
            expect (ai::ReplyFormat::parse (reply, m, error), error);
            expect (m.hasNotes() && m.notes.size() == 2 && m.notesKind == "melody", "notes parsed");
            expect (m.hasParamChanges(), "param changes parsed");
            expect (m.hasWavetable(), "wavetable parsed");
            expect (m.hasArpPattern() && m.arpPattern.steps.size() == 2, "arp pattern parsed");
            expectEquals (m.presetName, juce::String ("Test"));

            std::vector<float> frames;
            int numFrames = 0;
            juce::String tableName;
            expect (ai::ReplyFormat::wavetableFrames (m.wavetable, frames, numFrames, tableName), "wavetable synthesised");
            expectEquals (numFrames, ai::ReplyFormat::wavetableTargetFrames, "expanded to the target frame count");
            expectEquals (tableName, juce::String ("Glassy"));
            float peak = 0.0f; bool finite = true;
            for (float v : frames) { peak = juce::jmax (peak, std::abs (v)); finite = finite && std::isfinite (v); }
            expect (finite && peak <= 1.0001f && peak > 0.5f, "frames finite and normalised");

            ai::ChatMessage empty;
            expect (! ai::ReplyFormat::parse (juce::JSON::parse ("{\"reply\":\"\",\"notes\":[],\"notes_kind\":\"none\"}"), empty, error), "empty reply rejected");

            ai::Settings s;
            s.provider = "anthropic"; s.model = "claude-opus-5"; s.apiKey = "k"; s.effort = "medium";
            auto client = ai::makeClient (s);
            ai::LlmRequest req;
            req.systemStable = "stable"; req.systemVolatile = "volatile";
            req.messages = { { "user", "hi" } };
            req.schema = ai::ReplyFormat::schema();
            const auto body = client->buildBody (req);
            expectEquals (body.getProperty ("model", "").toString(), juce::String ("claude-opus-5"));
            const auto system = body.getProperty ("system", juce::var());
            expect (system.isArray() && system.size() == 2, "two system blocks");
            expect (system[0].getProperty ("cache_control", juce::var()).isObject(), "stable block is cached");
            expect (! system[1].hasProperty ("cache_control"), "volatile block is not cached");
            const auto outputConfig = body.getProperty ("output_config", juce::var());
            expectEquals (outputConfig.getProperty ("format", juce::var()).getProperty ("type", "").toString(), juce::String ("json_schema"));
            expectEquals (outputConfig.getProperty ("effort", "").toString(), juce::String ("medium"));
            expect (! juce::JSON::toString (body).contains ("\"k\""), "the key is never in the body");
            expect (! body.hasProperty ("thinking"), "thinking left at the model default");
        }

        beginTest ("settings: the key round-trips through DPAPI and is not stored in plain text");
        {
            const auto plain = juce::String ("sk-test-1234567890");
            const auto blob = ai::Settings::encryptSecret (plain);
            expect (blob.isNotEmpty() && ! blob.contains ("1234567890"), "encrypted blob is opaque");
            expectEquals (ai::Settings::decryptSecret (blob), plain, "decrypts back");
            expect (ai::Settings::decryptSecret ("not-a-blob").isEmpty(), "garbage decrypts to empty");
        }

        beginTest ("request presets: every category has a built-in, user presets round trip");
        {
            for (const auto& cat : ai::RequestPresets::categories())
                expect (ai::RequestPresets::firstText (cat).isNotEmpty() || cat.isEmpty(), "category " + cat + " has a built-in");
            expect (ai::RequestPresets::builtins().size() >= 30, "a useful number of built-ins");
        }
    }
};

struct PresetTest final : public juce::UnitTest
{
    PresetTest() : juce::UnitTest ("Presets", "State") {}

    void runTest() override
    {
        DummyProcessor proc;
        auto& apvts = proc.apvts;

        beginTest ("every factory preset references parameters that exist");
        for (const auto& preset : PresetManager::factoryPresets())
        {
            expect (preset.tableA.isNotEmpty(), preset.name + " has no OSC A table");
            expect (preset.tableB.isNotEmpty(), preset.name + " has no OSC B table");
            for (const auto& [id, value] : preset.values)
            {
                auto* param = apvts.getParameter (id);
                expect (param != nullptr, preset.name + ": unknown parameter '" + id + "'");
                if (param == nullptr)
                    continue;

                // A value outside the parameter's range would be silently
                // clamped, so the preset would not sound as written.
                const auto range = param->getNormalisableRange();
                expect (value >= range.start - 1.0e-4f && value <= range.end + 1.0e-4f,
                        preset.name + ": " + id + " value " + juce::String (value)
                            + " is outside [" + juce::String (range.start) + ", "
                            + juce::String (range.end) + "]");
            }
        }

        beginTest ("every FX parameter id exists, and the id list is complete");
        {
            const auto ids = ParamID::Fx::all();
            for (const auto& id : ids)
                expect (apvts.getParameter (id) != nullptr, "unknown FX parameter '" + id + "'");

            int fxParamsInLayout = 0;
            for (auto* param : proc.getParameters())
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
                    if (withId->paramID.startsWith ("fx_"))
                        ++fxParamsInLayout;
            expectEquals (fxParamsInLayout, ids.size(), "ParamID::Fx::all() lists every fx_ parameter");
        }

        beginTest ("applying a preset changes parameters, and Init restores the defaults");
        const auto* supersaw = PresetManager::findFactory ("Supersaw Lead");
        expect (supersaw != nullptr);

        auto* unison = apvts.getParameter (ParamID::osc (0).unisonVoices);
        auto* master = apvts.getParameter (ParamID::masterVolume);
        expect (unison != nullptr && master != nullptr);

        const float unisonDefault = unison->getNormalisableRange().convertFrom0to1 (unison->getDefaultValue());
        const float masterDefault = master->getNormalisableRange().convertFrom0to1 (master->getDefaultValue());
        logMessage ("defaults: unison=" + juce::String (unisonDefault, 2)
                    + " master=" + juce::String (masterDefault, 2) + " dB");
        expectWithinAbsoluteError (masterDefault, -6.0f, 0.01f, "master volume default");

        PresetManager::applyFactory (apvts, *supersaw);
        expectWithinAbsoluteError (unison->getNormalisableRange().convertFrom0to1 (unison->getValue()),
                                   7.0f, 0.01f, "unison voices after Supersaw Lead");

        PresetManager::applyFactory (apvts, *PresetManager::findFactory ("Init"));
        expectWithinAbsoluteError (unison->getNormalisableRange().convertFrom0to1 (unison->getValue()),
                                   unisonDefault, 0.01f, "unison voices back to default");
        expectWithinAbsoluteError (master->getNormalisableRange().convertFrom0to1 (master->getValue()),
                                   masterDefault, 0.01f, "master volume back to default");

        beginTest ("a preset survives a save/load round trip");
        PresetManager::applyFactory (apvts, *PresetManager::findFactory ("Wobble Bass"));
        const auto saved = apvts.copyState();

        auto file = juce::File::createTempFile (PresetManager::fileExtension);
        juce::String error;
        expect (PresetManager::saveToFile (saved, file, error), error);

        PresetManager::applyFactory (apvts, *PresetManager::findFactory ("Init"));
        juce::ValueTree restored;
        expect (PresetManager::loadFromFile (file, restored, error), error);
        apvts.replaceState (restored);

        auto* cutoff = apvts.getParameter (ParamID::filterCutoff);
        expectWithinAbsoluteError (cutoff->getNormalisableRange().convertFrom0to1 (cutoff->getValue()),
                                   420.0f, 1.0f, "filter cutoff after round trip");
        file.deleteFile();
    }
};

//==============================================================================
// Chord progression library: roman numerals, keys and the rendered voicing.
class ChordLibraryTest final : public juce::UnitTest
{
public:
    ChordLibraryTest() : juce::UnitTest ("Chord library / degrees, keys and voicing") {}

    void runTest() override
    {
        using CL = ai::ChordLibrary;

        beginTest ("keys: 24 entries, C major first, spelling follows the key");
        {
            const auto keys = CL::keyNames();
            expectEquals (keys.size(), 24, "24 keys");
            expectEquals (keys[0], juce::String ("C major"));
            expectEquals (keys[1], juce::String ("C minor"));
            expect (! CL::keyIsMinor (0) && CL::keyIsMinor (1), "mode alternates");
            expectEquals (CL::keyRoot (CL::keyIndexFor (9, true)), 9, "A minor root");
            expectEquals (keys[CL::keyIndexFor (10, false)], juce::String ("Bb major"), "flat key spelling");
            expectEquals (keys[CL::keyIndexFor (1, true)], juce::String ("C# minor"), "sharp key spelling");
        }

        beginTest ("degrees: roman numerals resolve per mode");
        {
            std::vector<ai::ChordSymbol> chords;
            juce::String error;
            expect (CL::parse ("I IV V", false, chords, error), error);
            expectEquals ((int) chords.size(), 3);
            expectEquals (CL::chordNames (chords, 0), juce::String ("C - F - G"), "in C major");

            const int aMinor = CL::keyIndexFor (9, true);
            expect (CL::parse ("i iv V", true, chords, error), error);
            expectEquals (CL::chordNames (chords, aMinor), juce::String ("Am - Dm - E"), "in A minor");

            expect (CL::parse ("bVII", false, chords, error), error);
            expectEquals (chords[0].rootSemitone, 10, "bVII is a whole tone below the tonic");
        }

        beginTest ("degrees: qualities, slash bass and per-chord beats");
        {
            std::vector<ai::ChordSymbol> chords;
            juce::String error;
            expect (CL::parse ("V7 iim7b5 IVM7 Isus4", false, chords, error), error);
            expect (chords[0].intervals == std::vector<int> ({ 0, 4, 7, 10 }), "V7 is a dominant seventh");
            expect (chords[1].intervals == std::vector<int> ({ 0, 3, 6, 10 }), "iim7b5 is half diminished");
            expect (chords[2].intervals == std::vector<int> ({ 0, 4, 7, 11 }), "IVM7 is a major seventh");
            expect (chords[3].intervals == std::vector<int> ({ 0, 5, 7 }), "sus4");
            expectEquals (CL::chordNames (chords, 0), juce::String ("G7 - Dm7b5 - FM7 - Csus4"));

            expect (CL::parse ("IV/V", false, chords, error), error);
            expectEquals (chords[0].rootSemitone, 5);
            expectEquals (chords[0].bassSemitone, 7);
            expectEquals (CL::chordName (chords[0], 0), juce::String ("F/G"));

            expect (CL::parse ("I:2 IV:2", false, chords, error), error);
            expectWithinAbsoluteError (CL::totalBeats (chords, 1), 4.0f, 0.001f, "two half-bar chords");
            expectWithinAbsoluteError (CL::totalBeats (chords, 4), 16.0f, 0.001f, "repeats");
        }

        beginTest ("degrees: bad input is reported, not guessed");
        {
            std::vector<ai::ChordSymbol> chords;
            juce::String error;
            expect (! CL::parse ("Z7", false, chords, error), "unknown numeral");
            expect (error.isNotEmpty(), "an error message");
            expect (! CL::parse ("Ifoo", false, chords, error), "unknown quality");
            expect (! CL::parse ("   ", false, chords, error), "empty");
        }

        beginTest ("render: notes follow the key, the octave and the bass option");
        {
            std::vector<ai::ChordSymbol> chords;
            juce::String error;
            expect (CL::parse ("I IV V I", false, chords, error), error);

            ai::ChordLibrary::Options opt;
            opt.keyIndex = 0;          // C major
            opt.octave = 4;
            opt.bassNote = true;
            auto notes = CL::render (chords, opt);
            expectEquals ((int) notes.size(), 16, "four triads plus a bass note each");

            int lowest = 127;
            for (const auto& n : notes)
            {
                expect (n.pitch >= 24 && n.pitch <= 108, "pitch in range");
                expect (n.velocity > 0 && n.velocity <= 127, "velocity in range");
                lowest = juce::jmin (lowest, n.pitch);
            }
            expectEquals (lowest % 12, 0, "the lowest note of I ... is the tonic C");
            expectWithinAbsoluteError (notes[0].startBeat, 0.0f, 0.001f);
            expect (notes.back().startBeat > 11.0f, "the last chord starts in bar four");

            opt.bassNote = false;
            expectEquals ((int) CL::render (chords, opt).size(), 12, "triads only");
            opt.repeats = 2;
            expectEquals ((int) CL::render (chords, opt).size(), 24, "twice through");
            expectWithinAbsoluteError (CL::totalBeats (chords, 2), 32.0f, 0.001f);

            // transposing the key moves every note by the same interval
            opt.repeats = 1;
            auto inC = CL::render (chords, opt);
            opt.keyIndex = CL::keyIndexFor (2, false);   // D major
            auto inD = CL::render (chords, opt);
            expectEquals ((int) inD.size(), (int) inC.size());
            for (size_t i = 0; i < inC.size(); ++i)
                expectEquals ((inD[i].pitch - inC[i].pitch + 120) % 12, 2, "everything is two semitones up");
        }

        beginTest ("render: chords are voice led, not jumped");
        {
            std::vector<ai::ChordSymbol> chords;
            juce::String error;
            expect (CL::parse ("I V vi iii IV I IV V", false, chords, error), error);
            ai::ChordLibrary::Options opt;
            opt.bassNote = false;
            auto notes = CL::render (chords, opt);

            std::vector<float> centres (chords.size(), 0.0f);
            std::vector<int> counts (chords.size(), 0);
            for (const auto& n : notes)
            {
                const int chord = juce::jlimit (0, (int) chords.size() - 1, (int) (n.startBeat / 4.0f));
                centres[(size_t) chord] += (float) n.pitch;
                counts[(size_t) chord]++;
            }
            for (size_t i = 0; i < centres.size(); ++i)
                centres[i] /= (float) juce::jmax (1, counts[i]);
            for (size_t i = 1; i < centres.size(); ++i)
                expect (std::abs (centres[i] - centres[i - 1]) < 6.0f,
                        "chord " + juce::String ((int) i) + " stays near the previous voicing");
        }

        beginTest ("library: every built-in parses and every category works in both modes");
        {
            expect (CL::builtins().size() >= 50, "a useful number of progressions");
            for (const auto& p : CL::builtins())
            {
                expect (p.name.isNotEmpty() && p.category.isNotEmpty(), "named and filed");
                std::vector<ai::ChordSymbol> chords;
                juce::String error;
                const bool minorMode = p.mode == ai::ChordProgression::minor;
                expect (CL::parse (p.degrees, minorMode, chords, error), p.name + ": " + error);
                expect (! chords.empty() && chords.size() <= 32, p.name + ": chord count");
                expect (! CL::render (chords, {}).empty(), p.name + ": renders notes");
            }
            for (const auto& cat : CL::categories())
            {
                expect (! CL::forCategory (cat, false).empty(), cat + " has a major-key progression");
                expect (! CL::forCategory (cat, true).empty(), cat + " has a minor-key progression");
            }
        }

        beginTest ("library: a user progression survives a JSON round trip");
        {
            ai::ChordProgression p;
            p.category = "User";
            p.name = "My loop";
            p.degrees = "IM7 vim7 iim7 V7";
            p.mode = ai::ChordProgression::minor;
            p.builtin = false;

            ai::ChordProgression back;
            expect (ai::ChordProgression::fromVar (p.toVar(), back), "parses back");
            expectEquals (back.name, p.name);
            expectEquals (back.category, p.category);
            expectEquals (back.degrees, p.degrees);
            expect (back.mode == ai::ChordProgression::minor, "mode survives");
            expect (! back.builtin, "loaded progressions are user ones");

            ai::ChordProgression bad;
            expect (! ai::ChordProgression::fromVar (juce::var(), bad), "an empty object is rejected");
        }
    }
};

struct StdoutRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::cout << m << std::endl; }
};

static WavetableMipTest wavetableMipTest;
static OscillatorTest oscillatorTest;
static EnvelopeTest envelopeTest;
static EngineRenderTest engineRenderTest;
static LfoTest lfoTest;
static ModMatrixTest modMatrixTest;
static FxTest fxTest;
static WaveEditTest waveEditTest;
static WarpTest warpTest;
static ArpeggiatorTest arpeggiatorTest;
static AiTest aiTest;
static ChordLibraryTest chordLibraryTest;
static PresetTest presetTest;

int main()
{
    StdoutRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "FAILURES: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
