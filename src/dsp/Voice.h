#pragma once

#include "SynthParams.h"
#include "WavetableOscillator.h"
#include "SubOscillator.h"
#include "NoiseOscillator.h"
#include "Envelope.h"
#include "Filter.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace wf
{

// One polyphonic voice: OSC A + OSC B (each with unison) + Sub + Noise ->
// filter -> amp envelope. Modulation is recomputed every `controlInterval`
// samples; everything inside the inner loop is plain arithmetic.
class Voice
{
public:
    static constexpr int controlInterval = 32;

    void prepare (double sampleRate);

    // Starts a note. If the voice is still sounding it fades out over ~3 ms
    // and then starts the new note (click-free stealing).
    void noteOn (int midiNote, float velocity, const SynthParams& p, uint32_t ageStamp);
    void noteOff();
    void kill();

    bool isActive() const noexcept    { return active; }
    bool isReleasing() const noexcept { return env1.isReleasing() && pendingNote < 0; }
    int  getNote() const noexcept     { return note; }
    uint32_t getAge() const noexcept  { return age; }

    void setPitchBendSemitones (float semis) noexcept { pitchBend = semis; }

    // Adds this voice's output to `out` (stereo) between start and start+num.
    void render (juce::AudioBuffer<float>& out, int start, int num, const SynthParams& p);

private:
    void startPending (const SynthParams& p);
    void updateControl (const SynthParams& p);

    UnisonOscillator oscA, oscB;
    SubOscillator sub;
    NoiseOscillator noise;
    VoiceFilter filter;
    Envelope env1, env2;
    juce::Random rng;

    double sampleRate = 44100.0;
    bool active = false;
    int note = 60, pendingNote = -1;
    float velocity = 1.0f, pendingVelocity = 1.0f, pitchBend = 0.0f;
    uint32_t age = 0, pendingAge = 0;

    // Cached per control block
    float ampVel = 1.0f;
    float gainA = 0.0f, gainB = 0.0f, gainSub = 0.0f, gainNoise = 0.0f;
    float panAL = 1.0f, panAR = 1.0f, panBL = 1.0f, panBR = 1.0f;
    bool  routeA = true, routeB = true, routeSub = true, routeNoise = true, filterOn = true;
    int   controlCounter = 0;
};

} // namespace wf
