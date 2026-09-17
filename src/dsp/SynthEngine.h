#pragma once

#include "Voice.h"
#include <array>

namespace wf
{

// Snapshot of the newest sounding voice for the envelope displays, written
// by the audio thread once per block, read by the UI timer.
struct EnvDisplay
{
    std::atomic<bool>  active { false };
    std::atomic<float> heldMs { 0.0f };       // time since note-on
    std::atomic<float> releaseMs { -1.0f };   // time since note-off, -1 while held
    std::atomic<float> level[2] { 0.0f, 0.0f };
};

// Voice allocation and MIDI handling. All voices are pre-allocated; nothing
// here allocates or locks on the audio thread.
class SynthEngine
{
public:
    static constexpr int maxVoices = 16;

    void prepare (double sampleRate);

    // Renders into `out` (cleared by the caller), splitting the block at MIDI
    // events so note timing is sample-accurate.
    void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const SynthParams& p);

    void allNotesOff (bool immediate);
    int  getActiveVoiceCount() const noexcept;

    float getModWheel() const noexcept   { return modWheel; }
    float getAftertouch() const noexcept { return aftertouch; }
    float getGlobalLfoPhase (int index) const noexcept { return globalLfoPhase[(size_t) index]; }
    const EnvDisplay& getEnvDisplay() const noexcept { return envDisplay; }

private:
    void handleMidi (const juce::MidiMessage& m, const SynthParams& p);
    void noteOn (int note, float velocity, const SynthParams& p);
    void noteOff (int note);
    Voice* findFreeVoice (const SynthParams& p);
    void renderSegment (juce::AudioBuffer<float>& out, int start, int num, const SynthParams& p);
    void pushControllersToVoices() noexcept;

    std::array<Voice, maxVoices> voices;
    std::array<bool, 128> heldKeys {};        // physically held
    std::array<bool, 128> sustainedKeys {};   // released while pedal down
    bool sustainPedal = false;
    float pitchBendSemis = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    uint32_t ageCounter = 0;

    // Free-running master phase per LFO. Voices in free mode copy it at
    // note-on and then advance at the same rate, so they stay locked.
    double sampleRate = 44100.0;
    std::array<float, 2> globalLfoPhase { 0.0f, 0.0f };
    EnvDisplay envDisplay;
};

} // namespace wf
