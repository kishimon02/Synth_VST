#pragma once

#include "Wavetable.h"

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

struct GlobalParams
{
    float masterGain = 0.5f;   // linear
    int   polyphony = 8;
    int   pitchBendRange = 2;  // semitones
};

struct SynthParams
{
    OscParams    osc[2];
    SubParams    sub;
    NoiseParams  noise;
    FilterParams filter;
    EnvParams    env[2];       // 0 = amp, 1 = mod
    GlobalParams global;
};

} // namespace wf
