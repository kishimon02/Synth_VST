#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Lfo.h"
#include <array>
#include <vector>

namespace wf
{

// One step of an arpeggiator pattern.
struct ArpStep
{
    enum Kind { note = 0, rest, tie };
    int   kind = note;
    float velocity = 1.0f;     // 0..1, scales the held note's velocity
    float gate = 1.0f;         // 0.05..1, multiplied with the global gate
    int   noteOffset = 0;      // added to the sequence index (0 = normal order)
};

struct ArpPattern
{
    static constexpr int maxSteps = 32;

    juce::String name;
    std::vector<ArpStep> steps;

    static const std::vector<ArpPattern>& builtins();
    static juce::StringArray builtinNames();          // + "Custom" appended by the parameter layout

    // JSON: { "name": "...", "steps": [ { "kind": "note|rest|tie", "velocity": 0..1, "gate": 0..1, "note_offset": n }, ... ] }
    juce::var toJson() const;
    static bool fromJson (const juce::var& v, ArpPattern& out, juce::String& error);
    void clampAndTrim();   // ranges + maxSteps
};

struct ArpParams
{
    bool  enabled = false;
    int   mode = 0;            // Arpeggiator::Mode
    int   division = 7;        // Lfo::divisionBeats index (default 1/16)
    int   octaves = 1;         // 1..4
    float gate = 0.5f;         // 0.05..1 (multiplied with the step's gate)
    float swing = 0.0f;        // 0..0.75 of a step, applied to odd steps
    bool  latch = false;
    const ArpPattern* pattern = nullptr;   // nullptr = straight
};

// Turns held notes into a note sequence, driven by the host position when
// the transport runs (loops and relocations follow), otherwise by an
// internal clock. Lives in front of the voice allocator: it consumes note
// on/off from the incoming MIDI and emits its own. No allocation in process().
class Arpeggiator
{
public:
    enum Mode { up = 0, down, upDown, downUp, asPlayed, random, chord, numModes };
    static juce::StringArray modeNames() { return { "Up", "Down", "Up-Down", "Down-Up", "As Played", "Random", "Chord" }; }

    void prepare (double sampleRate);
    void reset();

    // `in` -> `out` (out is cleared first). hostPpq < 0 means "not playing":
    // the internal clock is used and steps start from the first note.
    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                  const ArpParams& p, float bpm, double hostPpq);

    int getCurrentStep() const noexcept { return currentStepIndex; }   // for the UI (-1 = idle)

private:
    struct Held { int note; float velocity; int order; };
    struct Sounding { int note; int offIn; };   // offIn: samples until note-off, relative to block start

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void rebuildSequence (const ArpParams& p);
    void fireStep (juce::MidiBuffer& out, int sample, int absoluteStep, const ArpParams& p, int stepLen);
    void emitOff (juce::MidiBuffer& out, int index, int sample);
    void allSoundingOff (juce::MidiBuffer& out, int sample);

    double sampleRate = 44100.0;
    std::array<Held, 16> held {};
    int numHeld = 0, pressCounter = 0;
    int physicalKeys = 0;                        // keys currently down (for latch)
    std::array<int, 64> sequence {};             // note numbers
    int sequenceLength = 0, sequenceIndex = 0;
    std::array<Sounding, 16> sounding {};
    int numSounding = 0;
    bool wasEnabled = false, lastLatch = false;
    int lastMode = -1, lastOctaves = -1;
    juce::int64 lastStepFired = -1;               // absolute step index
    double internalStepPos = 0.0;                // steps, internal clock
    bool hostWasPlaying = false;
    int currentStepIndex = -1;
    uint32_t rng = 0x2545F491u;
};

} // namespace wf
