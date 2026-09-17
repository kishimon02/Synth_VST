#pragma once

#include <cmath>
#include <algorithm>

namespace wf
{

// Analog-style ADSR: the attack rises toward an overshoot target (so it is
// fast at first and reaches 1.0 in the requested time), decay and release are
// exponential approaches. Times in milliseconds; minimum 1 ms to avoid clicks.
class Envelope
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    void setSampleRate (double sr) noexcept { sampleRate = sr; }

    void setParameters (float attackMs, float decayMs, float sustainLevel, float releaseMs) noexcept
    {
        sustain = std::clamp (sustainLevel, 0.0f, 1.0f);
        attackCoef  = coefForTime (attackMs,  1.79f); // ln(1 / (1 - 1/1.2))
        decayCoef   = coefForTime (decayMs,   4.6f);  // ~1% settle
        releaseCoef = coefForTime (releaseMs, 4.6f);
    }

    void noteOn() noexcept  { stage = Stage::attack; }
    void noteOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }
    void kill() noexcept    { stage = Stage::idle; level = 0.0f; }

    // Quick fade used for voice stealing (release time in ms).
    void fastRelease (float ms) noexcept
    {
        releaseCoef = coefForTime (ms, 4.6f);
        stage = Stage::release;
    }

    bool isActive() const noexcept    { return stage != Stage::idle; }
    bool isReleasing() const noexcept { return stage == Stage::release; }
    float getLevel() const noexcept   { return level; }
    Stage getStage() const noexcept   { return stage; }

    inline float process() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                level += (1.2f - level) * attackCoef;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                level += (sustain - level) * decayCoef;
                if (std::abs (level - sustain) < 1.0e-4f) { level = sustain; stage = Stage::sustain; }
                break;
            case Stage::sustain:
                level = sustain;
                break;
            case Stage::release:
                level += (0.0f - level) * releaseCoef;
                if (level < 1.0e-4f) { level = 0.0f; stage = Stage::idle; }
                break;
            case Stage::idle:
            default:
                level = 0.0f;
                break;
        }
        return level;
    }

private:
    float coefForTime (float ms, float logRange) const noexcept
    {
        const float seconds = std::max (ms, 1.0f) * 0.001f;
        return 1.0f - std::exp (-logRange / (seconds * (float) sampleRate));
    }

    double sampleRate = 44100.0;
    Stage stage = Stage::idle;
    float level = 0.0f, sustain = 1.0f;
    float attackCoef = 0.01f, decayCoef = 0.001f, releaseCoef = 0.001f;
};

} // namespace wf
