#pragma once

#include "SynthParams.h"
#include "WavetableOscillator.h"
#include "SubOscillator.h"
#include "NoiseOscillator.h"
#include "Envelope.h"
#include "Filter.h"
#include "Lfo.h"
#include "ModMatrix.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace wf
{

// One polyphonic voice: OSC A + OSC B (each with unison) + Sub + Noise ->
// filter -> amp envelope, with the modulation matrix applied at control rate.
// Everything inside the inner sample loop is plain arithmetic.
class Voice
{
public:
    static constexpr int controlInterval = 32;

    // OSC A warp: how OSC B's (raw, pre-level) output modulates OSC A.
    enum WarpMode { warpOff = 0, warpFM, warpRM, warpAM, numWarpModes };
    static juce::StringArray warpModeNames() { return { "Off", "FM from B", "Ring Mod B", "AM from B" }; }

    void prepare (double sampleRate);

    // Starts a note. If the voice is still sounding it fades out over ~3 ms
    // and then starts the new note (click-free stealing).
    // `globalLfoPhase` is the engine's free-running phase per LFO; a
    // free-running LFO starts from it so every voice stays phase-locked.
    void noteOn (int midiNote, float velocity, const SynthParams& p,
                 uint32_t ageStamp, const float* globalLfoPhase);
    void noteOff();
    void kill();
    void reset();   // kill + drop every bit of filter / oscillator state

    bool isActive() const noexcept    { return active; }
    bool isReleasing() const noexcept { return env1.isReleasing() && pendingNote < 0; }
    int  getNote() const noexcept     { return note; }
    uint32_t getAge() const noexcept  { return age; }

    // For the envelope display: elapsed time and current levels.
    int   getSamplesSinceNoteOn() const noexcept  { return samplesSinceOn; }
    int   getSamplesSinceNoteOff() const noexcept { return samplesSinceOff; }   // -1 while held
    float getEnvLevel (int index) const noexcept  { return index == 0 ? env1.getLevel() : env2.getLevel(); }

    void setPitchBendSemitones (float semis) noexcept { pitchBend = semis; }
    void setControllers (float modWheelIn, float aftertouchIn) noexcept
    {
        modWheel = modWheelIn;
        aftertouch = aftertouchIn;
    }

    // Adds this voice's output to `out` (stereo) between start and start+num.
    void render (juce::AudioBuffer<float>& out, int start, int num, const SynthParams& p);

private:
    void startPending (const SynthParams& p);
    void updateControl (const SynthParams& p, int numSamples);

    UnisonOscillator oscA, oscB;
    SubOscillator sub;
    NoiseOscillator noise;
    VoiceFilter filter;
    Envelope env1, env2;
    Lfo lfo[2];
    juce::Random rng;

    double sampleRate = 44100.0;
    bool active = false;
    int note = 60, pendingNote = -1;
    float velocity = 1.0f, pendingVelocity = 1.0f, pitchBend = 0.0f;
    float modWheel = 0.0f, aftertouch = 0.0f, randomValue = 0.0f;
    uint32_t age = 0, pendingAge = 0;
    float pendingLfoPhase[2] { 0.0f, 0.0f };
    int samplesSinceOn = 0, samplesSinceOff = -1;

    // Cached per control block
    float ampGain = 1.0f;
    float gainA = 0.0f, gainB = 0.0f, gainSub = 0.0f, gainNoise = 0.0f;
    float panAL = 1.0f, panAR = 1.0f, panBL = 1.0f, panBR = 1.0f;
    bool  routeA = true, routeB = true, routeSub = true, routeNoise = true, filterOn = true;
    int   warpMode = warpOff;
    float warpAmount = 0.0f;
    int   controlCounter = 0;
};

} // namespace wf
