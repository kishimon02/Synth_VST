#pragma once

#include "Wavetable.h"
#include "ModMatrix.h"
#include "fx/FxParams.h"
#include "Arpeggiator.h"

namespace wf
{

// Plain-value snapshot of every parameter the voices need. The processor
// fills one of these per block from the APVTS atomics; the audio thread then
// reads plain floats (no atomics, no parameter lookups per sample).
// Mirrors docs/ER.md layer A.

struct OscParams
{
    bool  enabled = true;
    const Wavetable* table = nullptr;
    float wtPosition = 0.0f;   // 0..1
    int   octave = 0;          // -3..3
    int   semitone = 0;        // -12..12
    float fineCents = 0.0f;    // -100..100
    float level = 0.75f;       // 0..1 (linear)
    float pan = 0.0f;          // -1..1
    float phase = 0.0f;        // 0..1
    bool  randomPhase = true;
    int   unisonVoices = 1;    // 1..8
    float unisonDetune = 10.0f;// cents
    float unisonBlend = 0.75f; // 0..1
    float unisonWidth = 0.5f;  // 0..1

    // Warp (OSC A only): OSC B's raw output modulates A. Voice::WarpMode.
    int   warpMode = 0;
    float warpAmount = 0.0f;   // 0..1
};

struct SubParams
{
    bool  enabled = false;
    int   shape = 0;           // SubOscillator::Shape
    int   octave = -1;         // -2..0
    float level = 0.5f;
    bool  direct = false;      // bypass filter
};

struct NoiseParams
{
    bool  enabled = false;
    int   type = 0;            // NoiseOscillator::Type
    float level = 0.25f;
};

struct FilterParams
{
    bool  enabled = true;
    int   type = 0;            // VoiceFilter::Type
    float cutoffHz = 20000.0f;
    float resonance = 0.0f;    // 0..1
    float drive = 1.0f;        // 1..10
    float keyTrack = 0.0f;     // 0..1
    bool  routeA = true, routeB = true, routeSub = true, routeNoise = true;
    float env2Amount = 0.0f;   // semitones applied to cutoff (-96..96)
};

struct EnvParams
{
    float attackMs = 5.0f, decayMs = 200.0f, sustain = 0.8f, releaseMs = 150.0f;
};

struct LfoParams
{
    int   shape = 0;           // Lfo::Shape
    bool  tempoSync = false;
    float rateHz = 2.0f;       // used when tempoSync is false
    int   syncDivision = 5;    // Lfo::divisionBeats index (default 1/4)
    bool  retrigger = true;    // false = free-running (phase locked across voices)
    float phase = 0.0f;        // start phase in retrigger mode
    bool  unipolar = false;    // true maps the output to 0..1
};

struct GlobalParams
{
    float masterGain = 0.5f;   // linear
    int   polyphony = 8;
    int   pitchBendRange = 2;  // semitones
};

struct SynthParams
{
    OscParams     osc[2];
    SubParams     sub;
    NoiseParams   noise;
    FilterParams  filter;
    EnvParams     env[2];      // 0 = amp, 1 = mod
    LfoParams     lfo[2];
    ModSlotParams modSlots[numModSlots];
    GlobalParams  global;
    FxParams      fx;          // master chain, applied after the voices are summed
    ArpParams     arp;         // in front of the voice allocator (pattern pointer set by the processor)

    float bpm = 120.0f;        // from the host playhead, for tempo-synced LFOs / delay
};

} // namespace wf
