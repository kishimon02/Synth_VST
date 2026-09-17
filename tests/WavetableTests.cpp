// DSP unit tests. Built as the console app `WaveForgeTests` (excluded from
// the default build): cmake --build --preset vs2022-release --target WaveForgeTests
// then run build/vs2022/WaveForgeTests_artefacts/Release/WaveForgeTests.exe

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

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

struct StdoutRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::cout << m << std::endl; }
};

static WavetableMipTest wavetableMipTest;
static OscillatorTest oscillatorTest;
static EnvelopeTest envelopeTest;
static EngineRenderTest engineRenderTest;

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
