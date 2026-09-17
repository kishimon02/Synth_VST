#include "SynthEngine.h"

namespace wf
{

void SynthEngine::prepare (double sr)
{
    sampleRate = sr;
    for (auto& v : voices)
        v.prepare (sr);
    heldKeys.fill (false);
    sustainedKeys.fill (false);
    sustainPedal = false;
    globalLfoPhase.fill (0.0f);
}

int SynthEngine::getActiveVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
        n += v.isActive() ? 1 : 0;
    return n;
}

void SynthEngine::allNotesOff (bool immediate)
{
    for (auto& v : voices)
        immediate ? v.kill() : v.noteOff();
    heldKeys.fill (false);
    sustainedKeys.fill (false);
}

void SynthEngine::pushControllersToVoices() noexcept
{
    for (auto& v : voices)
        v.setControllers (modWheel, aftertouch);
}

Voice* SynthEngine::findFreeVoice (const SynthParams& p)
{
    const int limit = juce::jlimit (1, maxVoices, p.global.polyphony);

    // 1) an idle voice within the polyphony limit
    for (int i = 0; i < limit; ++i)
        if (! voices[(size_t) i].isActive())
            return &voices[(size_t) i];

    // 2) the oldest releasing voice, else the oldest voice
    Voice* best = nullptr;
    for (int pass = 0; pass < 2 && best == nullptr; ++pass)
        for (int i = 0; i < limit; ++i)
        {
            auto& v = voices[(size_t) i];
            if (pass == 0 && ! v.isReleasing())
                continue;
            if (best == nullptr || v.getAge() < best->getAge())
                best = &v;
        }
    return best;
}

void SynthEngine::noteOn (int note, float velocity, const SynthParams& p)
{
    heldKeys[(size_t) note] = true;
    sustainedKeys[(size_t) note] = false;

    // Retrigger: a voice already playing this note is released first.
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note && ! v.isReleasing())
            v.noteOff();

    if (auto* v = findFreeVoice (p))
    {
        v->setPitchBendSemitones (pitchBendSemis);
        v->setControllers (modWheel, aftertouch);
        v->noteOn (note, velocity, p, ++ageCounter, globalLfoPhase.data());
    }
}

void SynthEngine::noteOff (int note)
{
    heldKeys[(size_t) note] = false;
    if (sustainPedal)
    {
        sustainedKeys[(size_t) note] = true;
        return;
    }
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note && ! v.isReleasing())
            v.noteOff();
}

void SynthEngine::handleMidi (const juce::MidiMessage& m, const SynthParams& p)
{
    if (m.isNoteOn())
    {
        noteOn (m.getNoteNumber(), m.getFloatVelocity(), p);
    }
    else if (m.isNoteOff())
    {
        noteOff (m.getNoteNumber());
    }
    else if (m.isPitchWheel())
    {
        pitchBendSemis = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * (float) p.global.pitchBendRange;
        for (auto& v : voices)
            v.setPitchBendSemitones (pitchBendSemis);
    }
    else if (m.isSustainPedalOn())
    {
        sustainPedal = true;
    }
    else if (m.isSustainPedalOff())
    {
        sustainPedal = false;
        for (int n = 0; n < 128; ++n)
            if (sustainedKeys[(size_t) n])
            {
                sustainedKeys[(size_t) n] = false;
                noteOff (n);
            }
    }
    else if (m.isController() && m.getControllerNumber() == 1)
    {
        modWheel = (float) m.getControllerValue() / 127.0f;
        pushControllersToVoices();
    }
    else if (m.isChannelPressure())
    {
        aftertouch = (float) m.getChannelPressureValue() / 127.0f;
        pushControllersToVoices();
    }
    else if (m.isAftertouch())
    {
        aftertouch = (float) m.getAfterTouchValue() / 127.0f;
        pushControllersToVoices();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        allNotesOff (m.isAllSoundOff());
    }
}

void SynthEngine::renderSegment (juce::AudioBuffer<float>& out, int start, int num, const SynthParams& p)
{
    if (num <= 0)
        return;
    for (auto& v : voices)
        if (v.isActive())
            v.render (out, start, num, p);
}

void SynthEngine::process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const SynthParams& p)
{
    const int numSamples = out.getNumSamples();

    int pos = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderSegment (out, pos, eventPos - pos, p);
        pos = eventPos;
        handleMidi (metadata.getMessage(), p);
    }
    renderSegment (out, pos, numSamples - pos, p);

    // Advance the free-running master phases once per block, after rendering,
    // so a voice started during this block copies the block's starting phase.
    // At most one block of error - inaudible for an LFO - and it costs two
    // additions per block instead of two per sample.
    for (int i = 0; i < 2; ++i)
    {
        const float rate = p.lfo[i].tempoSync ? Lfo::syncedRateHz (p.bpm, p.lfo[i].syncDivision)
                                              : p.lfo[i].rateHz;
        const float inc = (float) (juce::jlimit (0.0f, 400.0f, rate) / sampleRate);
        globalLfoPhase[(size_t) i] = Lfo::advancePhase (globalLfoPhase[(size_t) i], inc, numSamples);
    }
}

} // namespace wf
