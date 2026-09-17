#pragma once

namespace wf
{

// Plain-value snapshot of the master FX chain, filled once per block from the
// APVTS atomics (see ParamRefs). Mirrors docs/ER.md FX_UNIT and its subtypes.

struct DistortionParams
{
    bool  enabled = false;
    int   mode = 0;            // Distortion::Mode (soft / hard / fold)
    float driveDb = 12.0f;     // 0..40 dB of pre-gain
    bool  oversample = true;   // 2x oversampling around the shaper
    float outputDb = 0.0f;     // -24..6 dB applied to the wet signal
    float mix = 1.0f;          // 0..1
};

struct EqParams
{
    bool  enabled = false;
    float lowGainDb = 0.0f,  lowFreqHz = 120.0f;                 // low shelf
    float midGainDb = 0.0f,  midFreqHz = 1000.0f, midQ = 1.0f;   // peak
    float highGainDb = 0.0f, highFreqHz = 6000.0f;               // high shelf
};

struct ChorusParams
{
    bool  enabled = false;
    float rateHz = 0.8f;       // 0.05..10
    float depth = 0.3f;        // 0..1
    float feedback = 0.0f;     // -0.95..0.95
    float delayMs = 7.0f;      // 1..50 centre delay
    float mix = 0.5f;
};

struct DelayParams
{
    bool  enabled = false;
    bool  tempoSync = true;
    float timeMs = 375.0f;     // used when tempoSync is false
    int   division = 6;        // Lfo::divisionBeats index (default 1/8)
    float feedback = 0.4f;     // 0..0.95
    float lowpassHz = 6000.0f; // one-pole low-pass in the feedback path
    bool  pingPong = false;
    float mix = 0.3f;
};

struct ReverbParams
{
    bool  enabled = false;
    float size = 0.6f;         // 0..1 room size
    float damping = 0.5f;      // 0..1
    float width = 1.0f;        // 0..1 stereo width
    float predelayMs = 10.0f;  // 0..250
    float mix = 0.25f;
};

struct FxParams
{
    DistortionParams distortion;
    EqParams         eq;
    ChorusParams     chorus;
    DelayParams      delay;
    ReverbParams     reverb;
};

} // namespace wf
