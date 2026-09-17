#include "MusicContext.h"

namespace ai
{

void MusicContext::appendCaptured (const std::vector<CapturedEvent>& events)
{
    for (const auto& e : events)
    {
        if (captureOrigin < 0.0)
            captureOrigin = std::floor (e.beat / (double) timeSigNumerator) * (double) timeSigNumerator;   // bar start
        if (e.on)
        {
            openNotes[e.note] = { e.beat, e.velocity };
        }
        else if (auto it = openNotes.find (e.note); it != openNotes.end())
        {
            Note n;
            n.pitch = e.note;
            n.startBeat = (float) (it->second.first - captureOrigin);
            n.durationBeats = juce::jmax (0.05f, (float) (e.beat - it->second.first));
            n.velocity = juce::jlimit (1, 127, it->second.second);
            ownPart.notes.push_back (n);
            openNotes.erase (it);
        }
    }
}

void MusicContext::clearCapture()
{
    ownPart.notes.clear();
    openNotes.clear();
    captureOrigin = -1.0;
}

void MusicContext::finishCapture()
{
    for (auto& [note, startVel] : openNotes)
    {
        Note n;
        n.pitch = note;
        n.startBeat = (float) (startVel.first - captureOrigin);
        n.durationBeats = 1.0f;
        n.velocity = juce::jlimit (1, 127, startVel.second);
        ownPart.notes.push_back (n);
    }
    openNotes.clear();
    std::sort (ownPart.notes.begin(), ownPart.notes.end(), [] (const Note& a, const Note& b) { return a.startBeat < b.startBeat; });
}

bool MusicContext::loadMidiFile (const juce::File& file, Track& out, float* bpmOut, juce::String& error)
{
    juce::FileInputStream in (file);
    juce::MidiFile midi;
    if (! in.openedOk() || ! midi.readFrom (in))
    {
        error = "Not a readable MIDI file: " + file.getFileName();
        return false;
    }
    const int ticksPerBeat = midi.getTimeFormat() > 0 ? midi.getTimeFormat() : 960;
    out.name = file.getFileNameWithoutExtension();
    out.notes.clear();
    int drumHits = 0, total = 0;
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq (*midi.getTrack (t));
        seq.updateMatchedPairs();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto* ev = seq.getEventPointer (i);
            const auto& m = ev->message;
            if (m.isTempoMetaEvent() && bpmOut != nullptr)
                *bpmOut = (float) (60.0 / m.getTempoSecondsPerQuarterNote());
            if (! m.isNoteOn() || ev->noteOffObject == nullptr)
                continue;
            Note n;
            n.pitch = m.getNoteNumber();
            n.startBeat = (float) (m.getTimeStamp() / ticksPerBeat);
            n.durationBeats = juce::jmax (0.05f, (float) ((ev->noteOffObject->message.getTimeStamp() - m.getTimeStamp()) / ticksPerBeat));
            n.velocity = m.getVelocity();
            out.notes.push_back (n);
            ++total;
            if (m.getChannel() == 10) ++drumHits;
        }
    }
    if (out.notes.empty())
    {
        error = "No notes in " + file.getFileName();
        return false;
    }
    out.drums = total > 0 && drumHits * 2 > total;
    std::sort (out.notes.begin(), out.notes.end(), [] (const Note& a, const Note& b) { return a.startBeat < b.startBeat; });
    // shift so the first bar starts at 0
    const float first = std::floor (out.notes.front().startBeat / 4.0f) * 4.0f;
    for (auto& n : out.notes) n.startBeat -= first;
    return true;
}

juce::String MusicContext::estimateKey (const std::vector<Note>& notes)
{
    if (notes.empty())
        return {};
    // Krumhansl-Kessler profiles
    static const float major[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    float weights[12] = {};
    for (const auto& n : notes)
        weights[((n.pitch % 12) + 12) % 12] += n.durationBeats;

    auto correlate = [&weights] (const float* profile, int shift)
    {
        float mw = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mw += weights[i]; mp += profile[i]; }
        mw /= 12.0f; mp /= 12.0f;
        float num = 0, dw = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float w = weights[i] - mw, p = profile[((i - shift) % 12 + 12) % 12] - mp;
            num += w * p; dw += w * w; dp += p * p;
        }
        return dw > 0 ? num / std::sqrt (dw * dp) : 0.0f;
    };

    float best = -2.0f; int bestRoot = 0; bool bestMinor = false;
    for (int root = 0; root < 12; ++root)
    {
        const float cMaj = correlate (major, root), cMin = correlate (minor, root);
        if (cMaj > best) { best = cMaj; bestRoot = root; bestMinor = false; }
        if (cMin > best) { best = cMin; bestRoot = root; bestMinor = true; }
    }
    return juce::String (names[bestRoot]) + (bestMinor ? " minor" : " major");
}

float MusicContext::lengthInBars (const std::vector<Note>& notes, int num)
{
    float end = 0.0f;
    for (const auto& n : notes) end = juce::jmax (end, n.startBeat + n.durationBeats);
    return end / (float) juce::jmax (1, num);
}

juce::var MusicContext::notesToVar (const std::vector<Note>& notes)
{
    juce::Array<juce::var> arr;
    for (const auto& n : notes)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("pitch", n.pitch);
        o->setProperty ("start_beat", (double) juce::roundToInt (n.startBeat * 1000.0f) / 1000.0);
        o->setProperty ("duration_beats", (double) juce::roundToInt (n.durationBeats * 1000.0f) / 1000.0);
        o->setProperty ("velocity", n.velocity);
        arr.add (juce::var (o));
    }
    return arr;
}

std::vector<Note> MusicContext::notesFromVar (const juce::var& v)
{
    std::vector<Note> out;
    if (const auto* arr = v.getArray())
        for (const auto& item : *arr)
        {
            Note n;
            n.pitch = juce::jlimit (0, 127, (int) item.getProperty ("pitch", 60));
            n.startBeat = juce::jmax (0.0f, (float) (double) item.getProperty ("start_beat", 0.0));
            n.durationBeats = juce::jlimit (0.05f, 64.0f, (float) (double) item.getProperty ("duration_beats", 1.0));
            n.velocity = juce::jlimit (1, 127, (int) item.getProperty ("velocity", 100));
            out.push_back (n);
        }
    std::sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.startBeat < b.startBeat; });
    return out;
}

juce::String MusicContext::toJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("bpm", (double) bpm);
    obj->setProperty ("time_signature", juce::String (timeSigNumerator) + "/" + juce::String (timeSigDenominator));
    if (! ownPart.notes.empty())
    {
        obj->setProperty ("own_part_key", estimateKey (ownPart.notes));
        obj->setProperty ("own_part_bars", (double) lengthInBars (ownPart.notes, timeSigNumerator));
        obj->setProperty ("own_part", notesToVar (ownPart.notes));
    }
    juce::Array<juce::var> tracks;
    for (const auto& t : contextTracks)
    {
        auto* to = new juce::DynamicObject();
        to->setProperty ("name", t.name);
        to->setProperty ("drums", t.drums);
        to->setProperty ("key", estimateKey (t.notes));
        to->setProperty ("notes", notesToVar (t.notes));
        tracks.add (juce::var (to));
    }
    obj->setProperty ("context_tracks", tracks);
    return juce::JSON::toString (juce::var (obj), true);
}

juce::String MusicContext::summary() const
{
    juce::String s;
    if (ownPart.notes.empty())
        s = "no captured notes";
    else
        s = juce::String (ownPart.notes.size()) + " notes, " + juce::String (lengthInBars (ownPart.notes, timeSigNumerator), 1)
          + " bars, key " + estimateKey (ownPart.notes);
    if (! contextTracks.empty())
        s += "  |  " + juce::String (contextTracks.size()) + " context track(s)";
    return s;
}

} // namespace ai
