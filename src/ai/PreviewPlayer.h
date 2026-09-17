#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "MusicContext.h"
#include <atomic>
#include <memory>
#include <vector>

namespace ai
{

// Plays suggested notes through the synth. The message thread builds a
// sample-stamped event list and publishes it atomically; the audio thread
// only reads it. Old lists are kept alive (bounded) so a swap is safe.
class PreviewPlayer
{
public:
    struct Event { int sample; bool on; int note; float velocity; };
    struct Sequence { std::vector<Event> events; int lengthSamples = 0; };

    void prepare (double sampleRate) { sr = sampleRate; }

    // message thread
    void start (const std::vector<Note>& notes, float bpm)
    {
        auto seq = std::make_shared<Sequence>();
        const double samplesPerBeat = sr * 60.0 / (double) juce::jmax (20.0f, bpm);
        for (const auto& n : notes)
        {
            const int on = (int) (n.startBeat * samplesPerBeat);
            const int off = on + juce::jmax (1, (int) (n.durationBeats * samplesPerBeat)) - 1;
            seq->events.push_back ({ on, true, n.pitch, (float) n.velocity / 127.0f });
            seq->events.push_back ({ off, false, n.pitch, 0.0f });
            seq->lengthSamples = juce::jmax (seq->lengthSamples, off + 1);
        }
        std::stable_sort (seq->events.begin(), seq->events.end(),
                          [] (const Event& a, const Event& b) { return a.sample < b.sample || (a.sample == b.sample && ! a.on && b.on); });
        keepAlive.push_back (seq);
        if (keepAlive.size() > 8) keepAlive.erase (keepAlive.begin());
        pending.store (seq.get(), std::memory_order_release);
        stopRequested.store (false);
    }

    void stop() { stopRequested.store (true); }
    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }

    // audio thread: adds events for this block into `midi`
    void process (juce::MidiBuffer& midi, int numSamples)
    {
        if (auto* p = pending.exchange (nullptr, std::memory_order_acq_rel))
        {
            silence (midi, 0);
            current = p; position = 0; nextEvent = 0;
            playing.store (true, std::memory_order_relaxed);
        }
        if (current == nullptr)
            return;
        if (stopRequested.exchange (false))
        {
            silence (midi, 0);
            current = nullptr;
            playing.store (false, std::memory_order_relaxed);
            return;
        }
        const auto& ev = current->events;
        while (nextEvent < (int) ev.size() && ev[(size_t) nextEvent].sample < position + numSamples)
        {
            const auto& e = ev[(size_t) nextEvent];
            const int offset = juce::jlimit (0, numSamples - 1, e.sample - position);
            if (e.on) { midi.addEvent (juce::MidiMessage::noteOn (1, e.note, e.velocity), offset); heldMask[(size_t) e.note] = true; }
            else      { midi.addEvent (juce::MidiMessage::noteOff (1, e.note), offset); heldMask[(size_t) e.note] = false; }
            ++nextEvent;
        }
        position += numSamples;
        if (nextEvent >= (int) ev.size() && position >= current->lengthSamples)
        {
            current = nullptr;
            playing.store (false, std::memory_order_relaxed);
        }
    }

private:
    void silence (juce::MidiBuffer& midi, int sample)
    {
        for (int n = 0; n < 128; ++n)
            if (heldMask[(size_t) n]) { midi.addEvent (juce::MidiMessage::noteOff (1, n), sample); heldMask[(size_t) n] = false; }
    }

    double sr = 44100.0;
    std::vector<std::shared_ptr<Sequence>> keepAlive;
    std::atomic<Sequence*> pending { nullptr };
    std::atomic<bool> stopRequested { false }, playing { false };
    const Sequence* current = nullptr;
    int position = 0, nextEvent = 0;
    std::array<bool, 128> heldMask {};
};

} // namespace ai
