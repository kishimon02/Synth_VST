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
        l = std::sqrt (0.5f * (1.0f - pan));
        r = std::sqrt (0.5f * (1.0f + pan));
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
    filter.prepare (sr);
    rng.setSeed ((juce::int64) this);
    noise.seed ((uint32_t) rng.nextInt());
    active = false;
    pendingNote = -1;
}

void Voice::noteOn (int midiNote, float vel, const SynthParams& p, uint32_t ageStamp)
{
    if (active && env1.getLevel() > 1.0e-3f)
    {
        // Steal: fade the current note, then start the new one from render().
        pendingNote = midiNote;
        pendingVelocity = vel;
        pendingAge = ageStamp;
        env1.fastRelease (3.0f);
        env2.fastRelease (3.0f);
        return;
    }

    pendingNote = midiNote;
    pendingVelocity = vel;
    pendingAge = ageStamp;
    startPending (p);
}

void Voice::startPending (const SynthParams& p)
{
    note = pendingNote;
    velocity = pendingVelocity;
    age = pendingAge;
    pendingNote = -1;
    active = true;

    oscA.setTable (p.osc[0].table);
    oscB.setTable (p.osc[1].table);
    oscA.start (p.osc[0].unisonVoices, p.osc[0].phase, p.osc[0].randomPhase, rng);
    oscB.start (p.osc[1].unisonVoices, p.osc[1].phase, p.osc[1].randomPhase, rng);
    sub.reset();
    filter.reset();

    env1.setParameters (p.env[0].attackMs, p.env[0].decayMs, p.env[0].sustain, p.env[0].releaseMs);
    env2.setParameters (p.env[1].attackMs, p.env[1].decayMs, p.env[1].sustain, p.env[1].releaseMs);
    env1.noteOn();
    env2.noteOn();

    controlCounter = 0;
    updateControl (p);
}

void Voice::noteOff()
{
    if (pendingNote >= 0)
    {
        // Note released before the steal fade finished: drop the pending note.
        pendingNote = -1;
        return;
    }
    env1.noteOff();
    env2.noteOff();
}

void Voice::kill()
{
    env1.kill();
    env2.kill();
    active = false;
    pendingNote = -1;
}

void Voice::updateControl (const SynthParams& p)
{
    const float bentNote = (float) note + pitchBend;

    // --- oscillators
    for (int i = 0; i < 2; ++i)
    {
        const auto& o = p.osc[i];
        auto& osc = i == 0 ? oscA : oscB;
        const float n = bentNote + (float) (o.octave * 12 + o.semitone) + o.fineCents * 0.01f;
        osc.setTable (o.table);
        osc.update (noteToHz (n), o.wtPosition, o.unisonDetune, o.unisonWidth, o.unisonBlend);
    }
    gainA = p.osc[0].enabled ? p.osc[0].level : 0.0f;
    gainB = p.osc[1].enabled ? p.osc[1].level : 0.0f;
    panGains (p.osc[0].pan, panAL, panAR);
    panGains (p.osc[1].pan, panBL, panBR);

    // --- sub / noise
    sub.update (noteToHz (bentNote + (float) (p.sub.octave * 12)), p.sub.shape);
    gainSub = p.sub.enabled ? p.sub.level : 0.0f;
    noise.setType (p.noise.type);
    gainNoise = p.noise.enabled ? p.noise.level : 0.0f;

    // --- filter: cutoff * keytrack * env2
    filterOn = p.filter.enabled;
    routeA = p.filter.routeA; routeB = p.filter.routeB;
    routeSub = p.filter.routeSub && ! p.sub.direct; routeNoise = p.filter.routeNoise;
    if (filterOn)
    {
        const float semis = p.filter.keyTrack * ((float) note - 60.0f)
                          + p.filter.env2Amount * env2.getLevel();
        const float cutoff = p.filter.cutoffHz * std::exp2 (semis / 12.0f);
        filter.update (p.filter.type, cutoff, p.filter.resonance, p.filter.drive);
    }

    // Velocity -> amplitude (perceptual curve), with 6 dB of headroom so a
    // few unison-stacked notes stay below 0 dBFS before the master gain.
    ampVel = 0.5f * (0.2f + 0.8f * velocity * velocity);
}

void Voice::render (juce::AudioBuffer<float>& out, int start, int num, const SynthParams& p)
{
    if (! active)
        return;

    float* L = out.getWritePointer (0, start);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

    for (int i = 0; i < num; ++i)
    {
        if (controlCounter == 0)
            updateControl (p);
        if (++controlCounter >= controlInterval)
            controlCounter = 0;

        const float amp = env1.process() * ampVel;
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

        float al, ar, bl, br;
        oscA.process (al, ar);
        oscB.process (bl, br);
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
