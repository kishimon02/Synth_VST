#include "MidiExport.h"

namespace ai
{

bool MidiExport::write (const std::vector<Note>& notes, float bpm, int timeSigNum, int timeSigDen, bool drums,
                        const juce::File& file, juce::String& error)
{
    if (notes.empty())
    {
        error = "No notes to export.";
        return false;
    }

    juce::MidiMessageSequence seq;
    const double usPerQuarter = 60'000'000.0 / (double) juce::jmax (20.0f, bpm);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) usPerQuarter), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (timeSigNum, timeSigDen), 0.0);

    const int channel = drums ? 10 : 1;
    for (const auto& n : notes)
    {
        const double start = (double) n.startBeat * ticksPerBeat;
        const double end = start + (double) juce::jmax (0.05f, n.durationBeats) * ticksPerBeat;
        seq.addEvent (juce::MidiMessage::noteOn (channel, n.pitch, (juce::uint8) juce::jlimit (1, 127, n.velocity)), start);
        seq.addEvent (juce::MidiMessage::noteOff (channel, n.pitch), end);
    }
    seq.updateMatchedPairs();
    seq.addEvent (juce::MidiMessage::endOfTrack(), seq.getEndTime() + ticksPerBeat);

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticksPerBeat);
    midi.addTrack (seq);

    file.getParentDirectory().createDirectory();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    out.setPosition (0);
    out.truncate();
    if (! midi.writeTo (out, 1))
    {
        error = "Could not encode the MIDI file.";
        return false;
    }
    out.flush();
    return true;
}

juce::File MidiExport::tempFileFor (const juce::String& name)
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WaveForge");
    dir.createDirectory();
    const auto legal = juce::File::createLegalFileName (name.isEmpty() ? "WaveForge suggestion" : name);
    return dir.getNonexistentChildFile (legal, ".mid", true);
}

} // namespace ai
