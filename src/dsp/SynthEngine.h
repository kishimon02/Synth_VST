#pragma once

#include "Voice.h"
#include <array>

namespace wf
{

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

    // Last known controller state (used by the mod matrix in Phase 2).
    float getModWheel() const noexcept   { return modWheel; }
    float getAftertouch() const noexcept { return aftertouch; }

private:
    void handleMidi (const juce::MidiMessage& m, const SynthParams& p);
    void noteOn (int note, float velocity, const SynthParams& p);
    void noteOff (int note);
    Voice* findFreeVoice (const SynthParams& p);
    void renderSegment (juce::AudioBuffer<float>& out, int start, int num, const SynthParams& p);

    std::array<Voice, maxVoices> voices;
    std::array<bool, 128> heldKeys {};        // physically held
    std::array<bool, 128> sustainedKeys {};   // released while pedal down
    bool sustainPedal = false;
    float pitchBendSemis = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    uint32_t ageCounter = 0;
};

} // namespace wf
