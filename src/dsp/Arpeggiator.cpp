#include "Arpeggiator.h"

namespace wf
{

//==============================================================================
namespace
{
    ArpPattern make (const char* name, std::initializer_list<ArpStep> steps)
    {
        ArpPattern p;
        p.name = name;
        p.steps = steps;
        return p;
    }
    constexpr ArpStep N (float vel = 1.0f, float gate = 1.0f, int off = 0) { return { ArpStep::note, vel, gate, off }; }
    constexpr ArpStep R() { return { ArpStep::rest, 0.0f, 1.0f, 0 }; }
    constexpr ArpStep T() { return { ArpStep::tie, 0.0f, 1.0f, 0 }; }
}

const std::vector<ArpPattern>& ArpPattern::builtins()
{
    static const std::vector<ArpPattern> patterns
    {
        make ("Straight",     { N() }),
        make ("Accent 1",     { N (1.0f), N (0.6f), N (0.7f), N (0.6f) }),
        make ("Off-beat",     { R(), N(), R(), N() }),
        make ("3-3-2",        { N (1.0f, 0.9f), T(), T(), N (0.9f, 0.9f), T(), T(), N (1.0f, 0.9f), T() }),
        make ("Dotted",       { N(), R(), T(), N(), R(), T(), N(), R() }),
        make ("Triplet Skip", { N(), N(), R(), N(), N(), R() }),
        make ("Trance Gate",  { N (1.0f, 0.5f), N (0.7f, 0.5f), N (0.9f, 1.0f), R(),
                                N (1.0f, 0.5f), N (0.7f, 0.5f), N (0.9f, 1.0f), N (0.6f, 0.5f) }),
        make ("Pulse",        { N (1.0f, 1.0f), T(), T(), T() }),
        make ("Ratchet",      { N(), N (0.5f, 0.4f), N (0.6f, 0.4f), N (0.8f, 0.4f) }),
        make ("Skip Up",      { N (1.0f, 1.0f, 0), N (0.8f, 1.0f, 2), N (0.9f, 1.0f, 1), N (0.8f, 1.0f, 3) }),
    };
    return patterns;
}

juce::StringArray ArpPattern::builtinNames()
{
    juce::StringArray names;
    for (const auto& p : builtins())
        names.add (p.name);
    return names;
}

void ArpPattern::clampAndTrim()
{
    if ((int) steps.size() > maxSteps)
        steps.resize ((size_t) maxSteps);
    for (auto& s : steps)
    {
        s.kind = juce::jlimit (0, 2, s.kind);
        s.velocity = juce::jlimit (0.0f, 1.0f, s.velocity);
        s.gate = juce::jlimit (0.05f, 1.0f, s.gate);
        s.noteOffset = juce::jlimit (-16, 16, s.noteOffset);
    }
    if (steps.empty())
        steps.push_back ({});
}

juce::var ArpPattern::toJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name);
    juce::Array<juce::var> arr;
    for (const auto& s : steps)
    {
        auto* step = new juce::DynamicObject();
        step->setProperty ("kind", s.kind == ArpStep::rest ? "rest" : s.kind == ArpStep::tie ? "tie" : "note");
        step->setProperty ("velocity", s.velocity);
        step->setProperty ("gate", s.gate);
        step->setProperty ("note_offset", s.noteOffset);
        arr.add (juce::var (step));
    }
    obj->setProperty ("steps", arr);
    return juce::var (obj);
}

bool ArpPattern::fromJson (const juce::var& v, ArpPattern& out, juce::String& error)
{
    if (! v.isObject())
    {
        error = "Pattern JSON must be an object.";
        return false;
    }
    ArpPattern p;
    p.name = v.getProperty ("name", "Custom").toString();
    const auto* arr = v.getProperty ("steps", juce::var()).getArray();
    if (arr == nullptr || arr->isEmpty())
    {
        error = "Pattern has no steps.";
        return false;
    }
    for (const auto& s : *arr)
    {
        ArpStep step;
        const auto kind = s.getProperty ("kind", "note").toString();
        step.kind = kind == "rest" ? ArpStep::rest : kind == "tie" ? ArpStep::tie : ArpStep::note;
        step.velocity = (float) (double) s.getProperty ("velocity", 1.0);
        step.gate = (float) (double) s.getProperty ("gate", 1.0);
        step.noteOffset = (int) s.getProperty ("note_offset", 0);
        p.steps.push_back (step);
    }
    p.clampAndTrim();
    out = std::move (p);
    return true;
}

//==============================================================================
void Arpeggiator::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void Arpeggiator::reset()
{
    numHeld = 0; pressCounter = 0; physicalKeys = 0;
    sequenceLength = 0; sequenceIndex = 0;
    numSounding = 0;
    lastStepFired = -1;
    internalStepPos = 0.0;
    currentStepIndex = -1;
    lastMode = -1; lastOctaves = -1;
}

void Arpeggiator::noteOn (int note, float velocity)
{
    for (int i = 0; i < numHeld; ++i)
        if (held[(size_t) i].note == note) { held[(size_t) i].velocity = velocity; return; }
    if (numHeld < (int) held.size())
        held[(size_t) numHeld++] = { note, velocity, pressCounter++ };
}

void Arpeggiator::noteOff (int note)
{
    for (int i = 0; i < numHeld; ++i)
        if (held[(size_t) i].note == note)
        {
            for (int j = i; j < numHeld - 1; ++j)
                held[(size_t) j] = held[(size_t) j + 1];
            --numHeld;
            return;
        }
}

void Arpeggiator::rebuildSequence (const ArpParams& p)
{
    sequenceLength = 0;
    if (numHeld == 0)
        return;

    // base order
    std::array<int, 16> base {};
    for (int i = 0; i < numHeld; ++i) base[(size_t) i] = i;
    auto byPitch = [this] (int a, int b) { return held[(size_t) a].note < held[(size_t) b].note; };
    auto byOrder = [this] (int a, int b) { return held[(size_t) a].order < held[(size_t) b].order; };
    if (p.mode == asPlayed || p.mode == chord)
        std::sort (base.begin(), base.begin() + numHeld, byOrder);
    else
        std::sort (base.begin(), base.begin() + numHeld, byPitch);

    const int octaves = juce::jlimit (1, 4, p.octaves);
    auto push = [this] (int note) { if (sequenceLength < (int) sequence.size()) sequence[(size_t) sequenceLength++] = note; };

    switch (p.mode)
    {
        case down:
            for (int o = octaves - 1; o >= 0; --o)
                for (int i = numHeld - 1; i >= 0; --i)
                    push (held[(size_t) base[(size_t) i]].note + 12 * o);
            break;
        case upDown:
        case downUp:
        {
            std::array<int, 64> ascending {};
            int n = 0;
            for (int o = 0; o < octaves; ++o)
                for (int i = 0; i < numHeld && n < 64; ++i)
                    ascending[(size_t) n++] = held[(size_t) base[(size_t) i]].note + 12 * o;
            if (p.mode == upDown)
            {
                for (int i = 0; i < n; ++i) push (ascending[(size_t) i]);
                for (int i = n - 2; i >= 1; --i) push (ascending[(size_t) i]);
            }
            else
            {
                for (int i = n - 1; i >= 0; --i) push (ascending[(size_t) i]);
                for (int i = 1; i < n - 1; ++i) push (ascending[(size_t) i]);
            }
            break;
        }
        case chord:   // one entry per octave; fireStep plays every held note at that octave
            for (int o = 0; o < octaves; ++o) push (12 * o);
            break;
        case up:
        case asPlayed:
        case random:
        default:
            for (int o = 0; o < octaves; ++o)
                for (int i = 0; i < numHeld; ++i)
                    push (held[(size_t) base[(size_t) i]].note + 12 * o);
            break;
    }
    if (sequenceLength > 0)
        sequenceIndex %= sequenceLength;
}

void Arpeggiator::emitOff (juce::MidiBuffer& out, int index, int sample)
{
    out.addEvent (juce::MidiMessage::noteOff (1, sounding[(size_t) index].note), sample);
    for (int j = index; j < numSounding - 1; ++j)
        sounding[(size_t) j] = sounding[(size_t) j + 1];
    --numSounding;
}

void Arpeggiator::allSoundingOff (juce::MidiBuffer& out, int sample)
{
    while (numSounding > 0)
        emitOff (out, numSounding - 1, sample);
}

void Arpeggiator::fireStep (juce::MidiBuffer& out, int sample, int absoluteStep, const ArpParams& p, int stepLen)
{
    const ArpPattern* pattern = p.pattern != nullptr && ! p.pattern->steps.empty() ? p.pattern : &ArpPattern::builtins()[0];
    const int patternLen = (int) pattern->steps.size();
    const int stepIndex = (int) (((absoluteStep % patternLen) + patternLen) % patternLen);
    const auto& step = pattern->steps[(size_t) stepIndex];
    currentStepIndex = stepIndex;

    const float gate = juce::jlimit (0.05f, 1.0f, step.gate * p.gate);
    if (step.kind == ArpStep::tie)
    {
        // keep whatever is sounding through this step
        for (int i = 0; i < numSounding; ++i)
            sounding[(size_t) i].offIn = sample + juce::jmax (1, (int) ((float) stepLen * gate));
        return;
    }
    if (step.kind == ArpStep::rest || sequenceLength == 0)
        return;

    // new note(s): stop whatever is still sounding (mono-style arp)
    allSoundingOff (out, sample);

    const int gateSamples = juce::jmax (1, (int) ((float) stepLen * gate));
    const int idx = ((sequenceIndex + step.noteOffset) % sequenceLength + sequenceLength) % sequenceLength;
    int chosen = idx;
    if (p.mode == random)
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        chosen = (int) (rng % (uint32_t) sequenceLength);
    }
    sequenceIndex = (sequenceIndex + 1) % sequenceLength;

    auto play = [&] (int note, float vel)
    {
        note = juce::jlimit (0, 127, note);
        if (numSounding >= (int) sounding.size()) return;
        out.addEvent (juce::MidiMessage::noteOn (1, note, juce::jlimit (0.01f, 1.0f, vel * step.velocity)), sample);
        sounding[(size_t) numSounding++] = { note, sample + gateSamples };
    };

    if (p.mode == chord)
    {
        const int octaveOffset = sequence[(size_t) chosen];
        for (int i = 0; i < numHeld; ++i)
            play (held[(size_t) i].note + octaveOffset, held[(size_t) i].velocity);
    }
    else
    {
        const int note = sequence[(size_t) chosen];
        float vel = 1.0f;   // velocity of the held key this note was derived from (any octave)
        for (int i = 0; i < numHeld; ++i)
            if ((note - held[(size_t) i].note) % 12 == 0 && note >= held[(size_t) i].note)
                { vel = held[(size_t) i].velocity; break; }
        play (note, vel);
    }
}

void Arpeggiator::process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                           const ArpParams& p, float bpm, double hostPpq)
{
    out.clear();

    if (! p.enabled)
    {
        if (wasEnabled)
        {
            allSoundingOff (out, 0);
            // hand the physically held keys back to the synth
            for (int i = 0; i < numHeld; ++i)
                out.addEvent (juce::MidiMessage::noteOn (1, held[(size_t) i].note, held[(size_t) i].velocity), 0);
            numHeld = 0;
            physicalKeys = 0;
            wasEnabled = false;
            currentStepIndex = -1;
        }
        out.addEvents (in, 0, numSamples, 0);
        return;
    }
    if (! wasEnabled)
    {
        wasEnabled = true;
        lastStepFired = -1;
        internalStepPos = 0.0;
        sequenceIndex = 0;
    }

    // --- input: note on/off feed the held set, everything else passes through.
    // Latch keeps released notes; the first press after a full release
    // starts a new set.
    if (p.latch != lastLatch)
    {
        lastLatch = p.latch;
        if (! p.latch) { numHeld = 0; physicalKeys = 0; }   // latch off: drop latched notes
    }
    bool heldChanged = false;
    for (const auto meta : in)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (p.latch && physicalKeys == 0)
                numHeld = 0;
            noteOn (m.getNoteNumber(), m.getFloatVelocity());
            ++physicalKeys;
            heldChanged = true;
        }
        else if (m.isNoteOff())
        {
            physicalKeys = juce::jmax (0, physicalKeys - 1);
            if (! p.latch)
            {
                noteOff (m.getNoteNumber());
                heldChanged = true;
            }
        }
        else
        {
            out.addEvent (m, meta.samplePosition);
        }
    }

    if (heldChanged || p.mode != lastMode || p.octaves != lastOctaves)
    {
        const bool wasEmpty = sequenceLength == 0;
        rebuildSequence (p);
        if (wasEmpty) sequenceIndex = 0;
        lastMode = p.mode; lastOctaves = p.octaves;
    }

    // --- clock
    const float safeBpm = bpm > 1.0f ? bpm : 120.0f;
    const double stepBeats = (double) Lfo::divisionBeats (p.division);
    const double stepLenSamples = juce::jmax (1.0, sampleRate * 60.0 / safeBpm * stepBeats);
    const bool hostPlaying = hostPpq >= 0.0;
    double stepsAtStart;
    if (hostPlaying)
    {
        stepsAtStart = hostPpq / stepBeats;
        if (! hostWasPlaying) lastStepFired = (juce::int64) std::floor (stepsAtStart) - 1;
    }
    else
    {
        if (hostWasPlaying) { internalStepPos = 0.0; lastStepFired = -1; }
        stepsAtStart = internalStepPos;
        internalStepPos += (double) numSamples / stepLenSamples;
    }
    hostWasPlaying = hostPlaying;

    // resync after a jump (loop / relocate)
    const auto floorStart = (juce::int64) std::floor (stepsAtStart);
    if (floorStart < lastStepFired - 1 || floorStart > lastStepFired + 2)
    {
        lastStepFired = floorStart - 1;
        allSoundingOff (out, 0);
    }

    // fire every step whose (swung) position lands inside this block
    const double swingSamples = (double) juce::jlimit (0.0f, 0.75f, p.swing) * stepLenSamples;
    for (int guard = 0; guard < 64; ++guard)
    {
        const juce::int64 n = lastStepFired + 1;
        double pos = ((double) n - stepsAtStart) * stepLenSamples;
        if ((n & 1) != 0) pos += swingSamples;
        if (pos >= (double) numSamples)
            break;
        const int sample = juce::jlimit (0, numSamples - 1, (int) pos);
        // note-offs that fall strictly before this step go first; an off due
        // exactly on the step is left for the step (a tie extends it)
        for (int i = numSounding - 1; i >= 0; --i)
            if (sounding[(size_t) i].offIn < sample)
                emitOff (out, i, juce::jlimit (0, numSamples - 1, sounding[(size_t) i].offIn));
        fireStep (out, sample, (int) n, p, (int) stepLenSamples);
        lastStepFired = n;
    }

    // remaining note-offs inside this block
    for (int i = numSounding - 1; i >= 0; --i)
        if (sounding[(size_t) i].offIn < numSamples)
            emitOff (out, i, juce::jmax (0, sounding[(size_t) i].offIn));
    for (int i = 0; i < numSounding; ++i)
        sounding[(size_t) i].offIn -= numSamples;

    if (numHeld == 0 && numSounding == 0)
        currentStepIndex = -1;
}

} // namespace wf
