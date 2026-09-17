#include "Voice.h"

namespace wf
{

namespace
{
    inline float noteToHz (float note) noexcept
    {
        return 440.0f * std::exp2 ((note - 69.0f) / 12.0f);
    }

    inline void panGains (float pan, float& l, float& r) noexcept
    {
        const float p = juce::jlimit (-1.0f, 1.0f, pan);
        l = std::sqrt (0.5f * (1.0f - p));
        r = std::sqrt (0.5f * (1.0f + p));
    }

    inline float lfoRate (const LfoParams& lp, float bpm) noexcept
    {
        return lp.tempoSync ? Lfo::syncedRateHz (bpm, lp.syncDivision) : lp.rateHz;
    }
}

void Voice::prepare (double sr)
{
    sampleRate = sr;
    oscA.setSampleRate (sr);
    oscB.setSampleRate (sr);
    sub.setSampleRate (sr);
    env1.setSampleRate (sr);
    env2.setSampleRate (sr);
    for (auto& l : lfo)
        l.setSampleRate (sr);
    filter.prepare (sr);
    rng.setSeed ((juce::int64) this);
    noise.seed ((uint32_t) rng.nextInt());
    active = false;
    pendingNote = -1;
}

void Voice::noteOn (int midiNote, float vel, const SynthParams& p,
                    uint32_t ageStamp, const float* globalLfoPhase)
{
    pendingNote = midiNote;
    pendingVelocity = vel;
    pendingAge = ageStamp;
    for (int i = 0; i < 2; ++i)
        pendingLfoPhase[i] = p.lfo[i].retrigger ? p.lfo[i].phase : globalLfoPhase[i];

    if (active && env1.getLevel() > 1.0e-3f)
    {
        // Steal: fade the current note out, then start the new one from render().
        env1.fastRelease (3.0f);
        env2.fastRelease (3.0f);
        return;
    }

    startPending (p);
}

void Voice::startPending (const SynthParams& p)
{
    note = pendingNote;
    velocity = pendingVelocity;
    age = pendingAge;
    pendingNote = -1;
    active = true;
    samplesSinceOn = 0;
    samplesSinceOff = -1;
    randomValue = rng.nextFloat() * 2.0f - 1.0f;

    oscA.setTable (p.osc[0].table);
    oscB.setTable (p.osc[1].table);
    oscA.start (p.osc[0].unisonVoices, p.osc[0].phase, p.osc[0].randomPhase, rng);
    oscB.start (p.osc[1].unisonVoices, p.osc[1].phase, p.osc[1].randomPhase, rng);
    sub.reset();
    filter.reset();

    for (int i = 0; i < 2; ++i)
        lfo[i].reset (pendingLfoPhase[i], (uint32_t) rng.nextInt() | 1u);

    env1.setParameters (p.env[0].attackMs, p.env[0].decayMs, p.env[0].sustain, p.env[0].releaseMs);
    env2.setParameters (p.env[1].attackMs, p.env[1].decayMs, p.env[1].sustain, p.env[1].releaseMs);
    env1.noteOn();
    env2.noteOn();

    controlCounter = 0;
    updateControl (p, 0);
}

void Voice::noteOff()
{
    if (pendingNote >= 0)
    {
        // Released before the steal fade finished: drop the pending note.
        pendingNote = -1;
        return;
    }
    env1.noteOff();
    env2.noteOff();
    if (samplesSinceOff < 0)
        samplesSinceOff = 0;
}

void Voice::kill()
{
    env1.kill();
    env2.kill();
    active = false;
    pendingNote = -1;
}

void Voice::updateControl (const SynthParams& p, int numSamples)
{
    // --- LFOs first: they are modulation sources for everything below.
    float lfoValue[2];
    for (int i = 0; i < 2; ++i)
    {
        lfo[i].setRate (lfoRate (p.lfo[i], p.bpm));
        float v = lfo[i].processControl (p.lfo[i].shape, numSamples);
        if (p.lfo[i].unipolar)
            v = v * 0.5f + 0.5f;
        lfoValue[i] = v;
    }

    // --- Modulation matrix
    float sources[ModSource::count] = {};
    sources[ModSource::none]          = 0.0f;
    sources[ModSource::env1]          = env1.getLevel();
    sources[ModSource::env2]          = env2.getLevel();
    sources[ModSource::lfo1]          = lfoValue[0];
    sources[ModSource::lfo2]          = lfoValue[1];
    sources[ModSource::velocity]      = velocity;
    sources[ModSource::modWheel]      = modWheel;
    sources[ModSource::aftertouch]    = aftertouch;
    sources[ModSource::keyTrack]      = ((float) note - 60.0f) / 48.0f;
    sources[ModSource::randomPerNote] = randomValue;

    float mod[ModDest::count];
    accumulateModulation (p.modSlots, sources, mod);

    const float bentNote = (float) note + pitchBend;

    // --- Oscillators
    for (int i = 0; i < 2; ++i)
    {
        const auto& o = p.osc[i];
        auto& osc = i == 0 ? oscA : oscB;
        const float pitchMod  = mod[i == 0 ? ModDest::oscAPitch  : ModDest::oscBPitch];
        const float wtMod     = mod[i == 0 ? ModDest::oscAWtPos  : ModDest::oscBWtPos];
        const float detuneMod = mod[i == 0 ? ModDest::oscADetune : ModDest::oscBDetune];

        const float n = bentNote + (float) (o.octave * 12 + o.semitone) + o.fineCents * 0.01f + pitchMod;
        osc.setTable (o.table);
        osc.update (noteToHz (n),
                    juce::jlimit (0.0f, 1.0f, o.wtPosition + wtMod),
                    juce::jmax (0.0f, o.unisonDetune + detuneMod),
                    o.unisonWidth, o.unisonBlend);
    }
    warpMode = p.osc[0].warpMode;
    warpAmount = juce::jlimit (0.0f, 1.0f, p.osc[0].warpAmount + mod[ModDest::oscAWarp]);
    gainA = p.osc[0].enabled ? juce::jlimit (0.0f, 2.0f, p.osc[0].level + mod[ModDest::oscALevel]) : 0.0f;
    gainB = p.osc[1].enabled ? juce::jlimit (0.0f, 2.0f, p.osc[1].level + mod[ModDest::oscBLevel]) : 0.0f;
    panGains (p.osc[0].pan + mod[ModDest::oscAPan], panAL, panAR);
    panGains (p.osc[1].pan + mod[ModDest::oscBPan], panBL, panBR);

    // --- Sub / noise
    sub.update (noteToHz (bentNote + (float) (p.sub.octave * 12)), p.sub.shape);
    gainSub   = p.sub.enabled   ? juce::jlimit (0.0f, 2.0f, p.sub.level   + mod[ModDest::subLevel])   : 0.0f;
    noise.setType (p.noise.type);
    gainNoise = p.noise.enabled ? juce::jlimit (0.0f, 2.0f, p.noise.level + mod[ModDest::noiseLevel]) : 0.0f;

    // --- Filter: cutoff moved by key tracking, the Env2 amount knob and the matrix
    filterOn = p.filter.enabled;
    routeA = p.filter.routeA; routeB = p.filter.routeB;
    routeSub = p.filter.routeSub && ! p.sub.direct; routeNoise = p.filter.routeNoise;
    if (filterOn)
    {
        const float semis = p.filter.keyTrack * ((float) note - 60.0f)
                          + p.filter.env2Amount * env2.getLevel()
                          + mod[ModDest::filterCutoff];
        const float cutoff = p.filter.cutoffHz * std::exp2 (semis / 12.0f);
        filter.update (p.filter.type, cutoff,
                       juce::jlimit (0.0f, 1.0f, p.filter.resonance + mod[ModDest::filterResonance]),
                       juce::jlimit (1.0f, 10.0f, p.filter.drive + mod[ModDest::filterDrive]));
    }

    // --- Amp: velocity curve, 6 dB of headroom, then the matrix (tremolo etc.)
    const float vel = 0.5f * (0.2f + 0.8f * velocity * velocity);
    ampGain = vel * juce::jlimit (0.0f, 1.0f, 1.0f + mod[ModDest::ampLevel]);
}

void Voice::render (juce::AudioBuffer<float>& out, int start, int num, const SynthParams& p)
{
    if (! active)
        return;

    float* L = out.getWritePointer (0, start);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

    samplesSinceOn += num;                     // block granularity is plenty for a display
    if (samplesSinceOff >= 0) samplesSinceOff += num;

    for (int i = 0; i < num; ++i)
    {
        if (controlCounter == 0)
            updateControl (p, controlInterval);
        if (++controlCounter >= controlInterval)
            controlCounter = 0;

        const float amp = env1.process() * ampGain;
        env2.process();

        if (! env1.isActive())
        {
            if (pendingNote >= 0)
            {
                startPending (p);       // stolen voice restarts here, click-free
                continue;
            }
            active = false;
            return;
        }

        // B first: its raw (pre-level) mono output can warp A.
        float al, ar, bl, br;
        oscB.process (bl, br);
        const float bMono = 0.5f * (bl + br);
        switch (warpMode)
        {
            case warpFM:   // phase modulation, up to half a cycle at full amount
                oscA.process (al, ar, bMono * warpAmount * 0.5f);
                break;
            case warpRM:   // dry -> fully ring-modulated
                oscA.process (al, ar);
                { const float g = 1.0f - warpAmount + warpAmount * bMono; al *= g; ar *= g; }
                break;
            case warpAM:   // unipolar version of the above (tremolo-like)
                oscA.process (al, ar);
                { const float g = 1.0f - warpAmount + warpAmount * (0.5f + 0.5f * bMono); al *= g; ar *= g; }
                break;
            default:
                oscA.process (al, ar);
                break;
        }
        const float s = sub.process() * gainSub;
        const float n = noise.process() * gainNoise;

        al *= gainA * panAL; ar *= gainA * panAR;
        bl *= gainB * panBL; br *= gainB * panBR;

        float fl = 0.0f, fr = 0.0f, dl = 0.0f, dr = 0.0f;
        (routeA     ? fl : dl) += al; (routeA     ? fr : dr) += ar;
        (routeB     ? fl : dl) += bl; (routeB     ? fr : dr) += br;
        (routeSub   ? fl : dl) += s;  (routeSub   ? fr : dr) += s;
        (routeNoise ? fl : dl) += n;  (routeNoise ? fr : dr) += n;

        if (filterOn)
            filter.process (fl, fr);

        L[i] += (fl + dl) * amp;
        if (R != nullptr)
            R[i] += (fr + dr) * amp;
    }
}

} // namespace wf
