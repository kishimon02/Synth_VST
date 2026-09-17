#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <map>
#include <vector>

namespace ai
{

struct Note
{
    int   pitch = 60;
    float startBeat = 0.0f;       // beats from the start of the phrase
    float durationBeats = 1.0f;
    int   velocity = 100;         // 1..127
};

struct Track
{
    juce::String name;
    std::vector<Note> notes;
    bool drums = false;
};

// Audio-thread -> UI ring of note on/off events with absolute beat positions.
struct CapturedEvent { int note; int velocity; bool on; double beat; };

class MidiCapture
{
public:
    static constexpr int capacity = 4096;

    std::atomic<bool> recording { false };

    // audio thread
    void push (const CapturedEvent& e) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 > 0) buffer[(size_t) start1] = e;
        fifo.finishedWrite (size1);
    }

    // message thread
    void drain (std::vector<CapturedEvent>& out)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (fifo.getNumReady(), start1, size1, start2, size2);
        for (int i = 0; i < size1; ++i) out.push_back (buffer[(size_t) (start1 + i)]);
        for (int i = 0; i < size2; ++i) out.push_back (buffer[(size_t) (start2 + i)]);
        fifo.finishedRead (size1 + size2);
    }

private:
    juce::AbstractFifo fifo { capacity };
    std::array<CapturedEvent, capacity> buffer {};
};

// What the model is told about the music: tempo, meter, the user's own part
// (captured from the host / keyboard) and other tracks dropped as .mid.
class MusicContext
{
public:
    float bpm = 120.0f;
    int timeSigNumerator = 4, timeSigDenominator = 4;
    Track ownPart { "own part" };
    std::vector<Track> contextTracks;

    // Pairs note on/off events into notes (message thread).
    void appendCaptured (const std::vector<CapturedEvent>& events);
    void clearCapture();
    void finishCapture();                 // closes still-open notes, shifts the phrase to start at bar 0

    static bool loadMidiFile (const juce::File& file, Track& out, float* bpmOut, juce::String& error);

    // Krumhansl-Schmuckler key estimate, e.g. "A minor". Empty if no notes.
    static juce::String estimateKey (const std::vector<Note>& notes);
    static float lengthInBars (const std::vector<Note>& notes, int timeSigNumerator);

    juce::String toJson() const;          // compact JSON for the prompt
    juce::String summary() const;         // one line for the UI
    bool isEmpty() const noexcept { return ownPart.notes.empty() && contextTracks.empty(); }

    static juce::var notesToVar (const std::vector<Note>& notes);
    static std::vector<Note> notesFromVar (const juce::var& arr);

private:
    std::map<int, std::pair<double, int>> openNotes;   // note -> (start beat, velocity)
    double captureOrigin = -1.0;                       // beat of the first captured event
};

} // namespace ai
