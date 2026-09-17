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
