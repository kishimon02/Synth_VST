#include "PresetManager.h"

#include "Params.h"
#include "dsp/ModMatrix.h"
#include "dsp/WavetableLoader.h"

namespace
{
    juce::String builtin (const char* name) { return wf::WavetableLoader::builtinSourceId (name); }
}

juce::File PresetManager::presetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("WaveForge")
               .getChildFile ("Presets");
}

juce::Array<juce::File> PresetManager::userPresets()
{
    juce::Array<juce::File> files;
    const auto dir = presetDirectory();
    if (dir.isDirectory())
        dir.findChildFiles (files, juce::File::findFiles, false, juce::String ("*") + fileExtension);
    return files;
}

const std::vector<PresetManager::FactoryPreset>& PresetManager::factoryPresets()
{
    static const std::vector<FactoryPreset> presets = []
    {
        const auto a = ParamID::osc (0);
        const auto b = ParamID::osc (1);
        const auto env1 = ParamID::env (0);
        const auto env2 = ParamID::env (1);
        const auto lfo1 = ParamID::lfo (0);
        const auto lfo2 = ParamID::lfo (1);
        const auto mod1 = ParamID::modSlot (0);
        const auto mod2 = ParamID::modSlot (1);
        const auto mod3 = ParamID::modSlot (2);
        namespace F = ParamID::Fx;
        namespace A = ParamID::Arp;

        // Wavetables
        const auto shapes = builtin ("Basic Shapes");
        const auto sine = builtin ("Sine");
        const auto harm = builtin ("Harmonics");
        const auto pwm = builtin ("PWM");

        // Handy constants for the choice parameters.
        constexpr float lp12 = 0.0f, lp24 = 1.0f, hp12 = 2.0f, bp12 = 4.0f;
        constexpr float distSoft = 0.0f, distHard = 1.0f, distFold = 2.0f;
        constexpr float subSine = 0.0f, subSquare = 3.0f;
        constexpr float noiseWhite = 0.0f, noisePink = 1.0f;
        constexpr float warpFm = 1.0f, warpRm = 2.0f;
        // LFO / delay divisions: 3 = 1 bar, 4 = 1/2, 5 = 1/4, 6 = 1/8, 7 = 1/16, 14 = 1/8 dotted
        constexpr float divBar = 3.0f, divHalf = 4.0f, divQuarter = 5.0f, divEighth = 6.0f,
                        divSixteenth = 7.0f, divDottedEighth = 14.0f;
        constexpr float srcEnv2 = (float) wf::ModSource::env2;
        constexpr float srcLfo1 = (float) wf::ModSource::lfo1;
        constexpr float srcLfo2 = (float) wf::ModSource::lfo2;
        constexpr float srcVelocity = (float) wf::ModSource::velocity;
        constexpr float srcWheel = (float) wf::ModSource::modWheel;
        constexpr float dstWtPosA = (float) wf::ModDest::oscAWtPos;
        constexpr float dstWtPosB = (float) wf::ModDest::oscBWtPos;
        constexpr float dstCutoff = (float) wf::ModDest::filterCutoff;
        constexpr float dstAmp = (float) wf::ModDest::ampLevel;
        constexpr float dstPitchA = (float) wf::ModDest::oscAPitch;
        constexpr float dstWarp = (float) wf::ModDest::oscAWarp;
        constexpr float dstDetuneA = (float) wf::ModDest::oscADetune;

        std::vector<FactoryPreset> p;

        // ------------------------------------------------------------ Basic
        p.push_back ({ "Basic", "Init", shapes, sine, {} });

        p.push_back ({ "Basic", "Raw Saw", shapes, sine,
        {
            { a.wtPos, 0.87f }, { a.level, 0.8f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 14000.0f },
            { env1.attack, 2.0f }, { env1.release, 120.0f }
        }});

        p.push_back ({ "Basic", "Raw Square", shapes, sine,
        {
            { a.wtPos, 1.0f }, { a.level, 0.7f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 9000.0f },
            { env1.attack, 2.0f }, { env1.release, 120.0f }
        }});

        p.push_back ({ "Basic", "Pure Sine", sine, sine,
        {
            { a.level, 0.85f }, { ParamID::filterEnabled, 0.0f },
            { env1.attack, 4.0f }, { env1.release, 200.0f }
        }});

        // ------------------------------------------------------------ EDM / Dance
        p.push_back ({ "EDM / Dance", "Supersaw Lead", shapes, shapes,
        {
            { a.wtPos, 0.87f }, { a.level, 0.7f }, { a.unisonVoices, 7.0f }, { a.unisonDetune, 22.0f },
            { a.unisonBlend, 0.9f }, { a.unisonWidth, 0.85f },
            { b.enabled, 1.0f }, { b.wtPos, 0.87f }, { b.octave, -1.0f }, { b.level, 0.35f },
            { b.unisonVoices, 3.0f }, { b.unisonDetune, 14.0f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 9000.0f }, { ParamID::filterResonance, 0.18f },
            { ParamID::filterKeyTrack, 0.4f },
            { env1.attack, 3.0f }, { env1.decay, 900.0f }, { env1.sustain, 0.75f }, { env1.release, 260.0f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.6f }, { F::chorusDepth, 0.25f }, { F::chorusMix, 0.3f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divDottedEighth }, { F::delayFeedback, 0.3f },
            { F::delayLowpass, 4000.0f }, { F::delayMix, 0.18f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.7f }, { F::reverbMix, 0.2f },
            { ParamID::masterVolume, -9.0f }, { ParamID::polyphony, 8.0f }
        }});

        p.push_back ({ "EDM / Dance", "Festival Pluck", shapes, shapes,
        {
            { a.wtPos, 0.8f }, { a.level, 0.75f }, { a.unisonVoices, 5.0f }, { a.unisonDetune, 16.0f },
            { a.unisonWidth, 0.8f },
            { b.enabled, 1.0f }, { b.wtPos, 0.95f }, { b.octave, 1.0f }, { b.level, 0.2f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 700.0f }, { ParamID::filterResonance, 0.2f },
            { ParamID::filterKeyTrack, 0.6f }, { ParamID::filterEnv2Amount, 52.0f },
            { env1.attack, 1.0f }, { env1.decay, 320.0f }, { env1.sustain, 0.0f }, { env1.release, 240.0f },
            { env2.attack, 1.0f }, { env2.decay, 200.0f }, { env2.sustain, 0.0f }, { env2.release, 180.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divDottedEighth }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.42f }, { F::delayLowpass, 5000.0f }, { F::delayMix, 0.3f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.65f }, { F::reverbMix, 0.25f },
            { ParamID::masterVolume, -6.0f }
        }});

        p.push_back ({ "EDM / Dance", "Big Room Stab", shapes, shapes,
        {
            { a.wtPos, 0.9f }, { a.level, 0.7f }, { a.unisonVoices, 7.0f }, { a.unisonDetune, 26.0f },
            { a.unisonWidth, 1.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.62f }, { b.octave, -1.0f }, { b.level, 0.4f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 5000.0f }, { ParamID::filterKeyTrack, 0.3f },
            { env1.attack, 2.0f }, { env1.decay, 260.0f }, { env1.sustain, 0.35f }, { env1.release, 200.0f },
            { F::distEnabled, 1.0f }, { F::distMode, distSoft }, { F::distDrive, 8.0f }, { F::distOutput, -4.0f },
            { F::distMix, 0.5f },
            { F::eqEnabled, 1.0f }, { F::eqLowGain, 2.0f }, { F::eqHighGain, 2.0f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.55f }, { F::reverbMix, 0.2f },
            { ParamID::masterVolume, -9.0f }
        }});

        p.push_back ({ "EDM / Dance", "Hoover Lead", pwm, shapes,
        {
            { a.wtPos, 0.4f }, { a.level, 0.7f }, { a.unisonVoices, 6.0f }, { a.unisonDetune, 34.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.87f }, { b.semi, -12.0f }, { b.level, 0.45f },
            { b.unisonVoices, 2.0f }, { b.unisonDetune, 20.0f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 3500.0f }, { ParamID::filterResonance, 0.25f },
            { env1.attack, 6.0f }, { env1.decay, 600.0f }, { env1.sustain, 0.8f }, { env1.release, 200.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 5.5f }, { lfo1.shape, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcWheel }, { mod1.dest, dstPitchA }, { mod1.amount, -0.15f },
            { F::distEnabled, 1.0f }, { F::distMode, distSoft }, { F::distDrive, 10.0f }, { F::distOutput, -5.0f },
            { F::chorusEnabled, 1.0f }, { F::chorusDepth, 0.4f }, { F::chorusMix, 0.35f },
            { ParamID::masterVolume, -9.0f }
        }});

        p.push_back ({ "EDM / Dance", "Sidechain Pad", shapes, harm,
        {
            { a.wtPos, 0.55f }, { a.level, 0.65f }, { a.unisonVoices, 5.0f }, { a.unisonDetune, 12.0f },
            { a.unisonWidth, 0.9f },
            { b.enabled, 1.0f }, { b.wtPos, 0.3f }, { b.octave, -1.0f }, { b.level, 0.35f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 4000.0f },
            { env1.attack, 60.0f }, { env1.decay, 1200.0f }, { env1.sustain, 0.85f }, { env1.release, 700.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, divQuarter }, { lfo1.shape, 2.0f },
            { lfo1.retrigger, 0.0f }, { lfo1.unipolar, 1.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstAmp }, { mod1.amount, 0.8f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.8f }, { F::reverbMix, 0.3f },
            { ParamID::masterVolume, -7.0f }
        }});

        p.push_back ({ "EDM / Dance", "Trance Gate", shapes, shapes,
        {
            { a.wtPos, 0.85f }, { a.level, 0.7f }, { a.unisonVoices, 5.0f }, { a.unisonDetune, 18.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.6f }, { b.octave, -1.0f }, { b.level, 0.3f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 6000.0f }, { ParamID::filterResonance, 0.15f },
            { env1.attack, 2.0f }, { env1.decay, 400.0f }, { env1.sustain, 0.9f }, { env1.release, 180.0f },
            { A::enabled, 1.0f }, { A::mode, 6.0f }, { A::division, divSixteenth }, { A::gate, 0.5f },
            { A::pattern, 6.0f }, { A::octaves, 1.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divEighth }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.35f }, { F::delayMix, 0.22f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.7f }, { F::reverbMix, 0.25f },
            { ParamID::masterVolume, -8.0f }
        }});

        // ------------------------------------------------------------ Techno / House
        p.push_back ({ "Techno / House", "Acid Bass", shapes, sine,
        {
            { a.wtPos, 0.87f }, { a.level, 0.85f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 260.0f }, { ParamID::filterResonance, 0.78f },
            { ParamID::filterDrive, 3.5f }, { ParamID::filterKeyTrack, 0.5f }, { ParamID::filterEnv2Amount, 46.0f },
            { env1.attack, 1.0f }, { env1.decay, 260.0f }, { env1.sustain, 0.6f }, { env1.release, 120.0f },
            { env2.attack, 1.0f }, { env2.decay, 220.0f }, { env2.sustain, 0.0f }, { env2.release, 100.0f },
            { F::distEnabled, 1.0f }, { F::distMode, distSoft }, { F::distDrive, 12.0f }, { F::distOutput, -6.0f },
            { ParamID::polyphony, 1.0f }, { ParamID::masterVolume, -6.0f }
        }});

        p.push_back ({ "Techno / House", "House Stab", shapes, harm,
        {
            { a.wtPos, 0.75f }, { a.level, 0.7f }, { a.unisonVoices, 3.0f }, { a.unisonDetune, 10.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.2f }, { b.semi, 7.0f }, { b.level, 0.3f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 1800.0f }, { ParamID::filterResonance, 0.3f },
            { ParamID::filterEnv2Amount, 30.0f },
            { env1.attack, 1.0f }, { env1.decay, 260.0f }, { env1.sustain, 0.0f }, { env1.release, 180.0f },
            { env2.attack, 1.0f }, { env2.decay, 140.0f }, { env2.sustain, 0.0f }, { env2.release, 120.0f },
            { F::eqEnabled, 1.0f }, { F::eqLowGain, -6.0f }, { F::eqLowFreq, 200.0f }, { F::eqHighGain, 2.0f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.45f }, { F::reverbPredelay, 15.0f }, { F::reverbMix, 0.22f },
            { ParamID::masterVolume, -6.0f }
        }});

        p.push_back ({ "Techno / House", "Rolling Bass", shapes, sine,
        {
            { a.wtPos, 0.62f }, { a.level, 0.7f },
            { ParamID::subEnabled, 1.0f }, { ParamID::subShape, subSine }, { ParamID::subLevel, 0.7f },
            { ParamID::subOctave, -1.0f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 520.0f }, { ParamID::filterResonance, 0.25f },
            { ParamID::filterKeyTrack, 0.3f },
            { env1.attack, 1.0f }, { env1.decay, 180.0f }, { env1.sustain, 0.4f }, { env1.release, 90.0f },
            { F::distEnabled, 1.0f }, { F::distMode, distSoft }, { F::distDrive, 6.0f }, { F::distOutput, -4.0f },
            { F::distMix, 0.5f },
            { ParamID::polyphony, 2.0f }, { ParamID::masterVolume, -6.0f }
        }});

        p.push_back ({ "Techno / House", "Dub Chord", shapes, harm,
        {
            { a.wtPos, 0.7f }, { a.level, 0.6f }, { a.unisonVoices, 3.0f }, { a.unisonDetune, 12.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.25f }, { b.octave, 1.0f }, { b.level, 0.25f },
            { ParamID::filterType, hp12 }, { ParamID::filterCutoff, 260.0f },
            { env1.attack, 4.0f }, { env1.decay, 300.0f }, { env1.sustain, 0.0f }, { env1.release, 260.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divDottedEighth }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.62f }, { F::delayLowpass, 2200.0f }, { F::delayMix, 0.45f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.75f }, { F::reverbDamping, 0.6f }, { F::reverbMix, 0.3f },
            { ParamID::masterVolume, -7.0f }
        }});

        // ------------------------------------------------------------ Bass Music
        p.push_back ({ "Bass Music", "Wobble Bass", shapes, pwm,
        {
            { a.wtPos, 0.9f }, { a.level, 0.75f }, { a.unisonVoices, 2.0f }, { a.unisonDetune, 8.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.4f }, { b.level, 0.3f },
            { ParamID::subEnabled, 1.0f }, { ParamID::subLevel, 0.6f }, { ParamID::subOctave, -1.0f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 420.0f }, { ParamID::filterResonance, 0.55f },
            { ParamID::filterDrive, 2.5f },
            { env1.attack, 2.0f }, { env1.decay, 400.0f }, { env1.sustain, 0.9f }, { env1.release, 120.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, divEighth }, { lfo1.retrigger, 1.0f }, { lfo1.shape, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstCutoff }, { mod1.amount, 0.45f },
            { F::distEnabled, 1.0f }, { F::distMode, distSoft }, { F::distDrive, 10.0f }, { F::distOutput, -3.0f },
            { F::distMix, 0.6f },
            { ParamID::masterVolume, -8.0f }, { ParamID::polyphony, 2.0f }
        }});

        p.push_back ({ "Bass Music", "Reese Bass", shapes, shapes,
        {
            { a.wtPos, 0.87f }, { a.level, 0.6f }, { a.unisonVoices, 4.0f }, { a.unisonDetune, 30.0f },
            { a.unisonWidth, 0.6f },
            { b.enabled, 1.0f }, { b.wtPos, 0.87f }, { b.fine, -18.0f }, { b.level, 0.55f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 380.0f }, { ParamID::filterResonance, 0.2f },
            { ParamID::filterDrive, 2.0f },
            { env1.attack, 5.0f }, { env1.decay, 600.0f }, { env1.sustain, 0.9f }, { env1.release, 200.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, divBar }, { lfo1.retrigger, 0.0f }, { lfo1.shape, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstDetuneA }, { mod1.amount, 0.3f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.25f }, { F::chorusDepth, 0.5f }, { F::chorusMix, 0.3f },
            { ParamID::polyphony, 2.0f }, { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Bass Music", "Neuro Growl", shapes, harm,
        {
            { a.wtPos, 0.8f }, { a.level, 0.7f },
            { b.enabled, 1.0f }, { b.wtPos, 0.5f }, { b.octave, 1.0f }, { b.level, 0.0f },
            { ParamID::oscAWarpMode, warpFm }, { ParamID::oscAWarpAmount, 0.35f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 700.0f }, { ParamID::filterResonance, 0.45f },
            { env1.attack, 2.0f }, { env1.decay, 500.0f }, { env1.sustain, 0.85f }, { env1.release, 140.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, divSixteenth }, { lfo1.shape, 4.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstWarp }, { mod1.amount, 0.5f },
            { lfo2.tempoSync, 1.0f }, { lfo2.division, divEighth }, { lfo2.shape, 1.0f },
            { mod2.enabled, 1.0f }, { mod2.source, srcLfo2 }, { mod2.dest, dstCutoff }, { mod2.amount, 0.4f },
            { F::distEnabled, 1.0f }, { F::distMode, distFold }, { F::distDrive, 9.0f }, { F::distOutput, -8.0f },
            { F::distMix, 0.55f },
            { ParamID::polyphony, 2.0f }, { ParamID::masterVolume, -9.0f }
        }});

        p.push_back ({ "Bass Music", "Sub Drop", sine, sine,
        {
            { a.level, 0.9f }, { a.octave, -1.0f },
            { ParamID::filterEnabled, 0.0f },
            { env1.attack, 2.0f }, { env1.decay, 2000.0f }, { env1.sustain, 0.6f }, { env1.release, 600.0f },
            { env2.attack, 1.0f }, { env2.decay, 900.0f }, { env2.sustain, 0.0f }, { env2.release, 400.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcEnv2 }, { mod1.dest, dstPitchA }, { mod1.amount, 0.35f },
            { F::eqEnabled, 1.0f }, { F::eqLowGain, 4.0f }, { F::eqLowFreq, 60.0f },
            { ParamID::polyphony, 1.0f }, { ParamID::masterVolume, -6.0f }
        }});

        // ------------------------------------------------------------ Hip-Hop / Trap
        p.push_back ({ "Hip-Hop / Trap", "808 Sub", sine, shapes,
        {
            { a.level, 0.9f }, { a.octave, -1.0f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 900.0f },
            { env1.attack, 1.0f }, { env1.decay, 2500.0f }, { env1.sustain, 0.25f }, { env1.release, 900.0f },
            { F::distEnabled, 1.0f }, { F::distMode, distSoft }, { F::distDrive, 7.0f }, { F::distOutput, -5.0f },
            { F::distMix, 0.4f },
            { ParamID::polyphony, 1.0f }, { ParamID::masterVolume, -5.0f }
        }});

        p.push_back ({ "Hip-Hop / Trap", "Trap Bell", harm, sine,
        {
            { a.wtPos, 0.2f }, { a.level, 0.75f },
            { b.enabled, 1.0f }, { b.octave, 1.0f }, { b.level, 0.2f },
            { ParamID::filterEnabled, 0.0f },
            { env1.attack, 1.0f }, { env1.decay, 900.0f }, { env1.sustain, 0.0f }, { env1.release, 700.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divSixteenth }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.4f }, { F::delayLowpass, 6000.0f }, { F::delayMix, 0.25f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.6f }, { F::reverbMix, 0.28f },
            { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Hip-Hop / Trap", "Dark Pluck", shapes, pwm,
        {
            { a.wtPos, 0.55f }, { a.level, 0.7f }, { a.unisonVoices, 2.0f }, { a.unisonDetune, 7.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.5f }, { b.octave, -1.0f }, { b.level, 0.3f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 800.0f }, { ParamID::filterResonance, 0.3f },
            { ParamID::filterEnv2Amount, 24.0f },
            { env1.attack, 1.0f }, { env1.decay, 450.0f }, { env1.sustain, 0.0f }, { env1.release, 300.0f },
            { env2.attack, 1.0f }, { env2.decay, 180.0f }, { env2.sustain, 0.0f }, { env2.release, 150.0f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.7f }, { F::reverbDamping, 0.65f }, { F::reverbMix, 0.3f },
            { ParamID::masterVolume, -6.0f }
        }});

        p.push_back ({ "Hip-Hop / Trap", "Detuned Lead", shapes, shapes,
        {
            { a.wtPos, 0.85f }, { a.level, 0.7f }, { a.unisonVoices, 3.0f }, { a.unisonDetune, 24.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.85f }, { b.fine, 12.0f }, { b.level, 0.35f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 3200.0f }, { ParamID::filterResonance, 0.2f },
            { env1.attack, 8.0f }, { env1.decay, 700.0f }, { env1.sustain, 0.7f }, { env1.release, 300.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divEighth }, { F::delayFeedback, 0.35f },
            { F::delayMix, 0.25f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.6f }, { F::reverbMix, 0.25f },
            { ParamID::masterVolume, -8.0f }
        }});

        // ------------------------------------------------------------ Lo-Fi / Chill
        p.push_back ({ "Lo-Fi / Chill", "Lo-fi Keys", harm, sine,
        {
            { a.wtPos, 0.12f }, { a.level, 0.7f },
            { b.enabled, 1.0f }, { b.level, 0.25f }, { b.fine, 8.0f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 2200.0f },
            { env1.attack, 6.0f }, { env1.decay, 1400.0f }, { env1.sustain, 0.25f }, { env1.release, 500.0f },
            { F::eqEnabled, 1.0f }, { F::eqLowGain, -4.0f }, { F::eqLowFreq, 90.0f }, { F::eqHighGain, -6.0f },
            { F::eqHighFreq, 5000.0f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.35f }, { F::chorusDepth, 0.45f }, { F::chorusMix, 0.35f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.5f }, { F::reverbDamping, 0.7f }, { F::reverbMix, 0.25f },
            { ParamID::masterVolume, -6.0f }
        }});

        p.push_back ({ "Lo-Fi / Chill", "Tape Pad", shapes, harm,
        {
            { a.wtPos, 0.35f }, { a.level, 0.6f }, { a.unisonVoices, 4.0f }, { a.unisonDetune, 14.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.15f }, { b.octave, -1.0f }, { b.level, 0.3f },
            { ParamID::noiseEnabled, 1.0f }, { ParamID::noiseType, noisePink }, { ParamID::noiseLevel, 0.06f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 1700.0f },
            { env1.attack, 700.0f }, { env1.decay, 2000.0f }, { env1.sustain, 0.8f }, { env1.release, 1600.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 0.25f }, { lfo1.retrigger, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstPitchA }, { mod1.amount, 0.04f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.2f }, { F::chorusDepth, 0.55f }, { F::chorusMix, 0.4f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.8f }, { F::reverbDamping, 0.6f }, { F::reverbMix, 0.35f },
            { ParamID::masterVolume, -7.0f }
        }});

        p.push_back ({ "Lo-Fi / Chill", "Dusty Bell", harm, sine,
        {
            { a.wtPos, 0.3f }, { a.level, 0.65f },
            { ParamID::noiseEnabled, 1.0f }, { ParamID::noiseType, noisePink }, { ParamID::noiseLevel, 0.08f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 3000.0f },
            { env1.attack, 2.0f }, { env1.decay, 1200.0f }, { env1.sustain, 0.0f }, { env1.release, 900.0f },
            { F::eqEnabled, 1.0f }, { F::eqHighGain, -5.0f }, { F::eqHighFreq, 4000.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divQuarter }, { F::delayFeedback, 0.3f },
            { F::delayLowpass, 2000.0f }, { F::delayMix, 0.25f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.65f }, { F::reverbMix, 0.3f },
            { ParamID::masterVolume, -7.0f }
        }});

        p.push_back ({ "Lo-Fi / Chill", "Soft Wurli", harm, sine,
        {
            { a.wtPos, 0.08f }, { a.level, 0.7f },
            { b.enabled, 1.0f }, { b.octave, 1.0f }, { b.level, 0.15f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 2600.0f },
            { env1.attack, 3.0f }, { env1.decay, 1600.0f }, { env1.sustain, 0.15f }, { env1.release, 450.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 5.0f }, { lfo1.retrigger, 0.0f }, { lfo1.unipolar, 1.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstAmp }, { mod1.amount, 0.18f },
            { mod2.enabled, 1.0f }, { mod2.source, srcVelocity }, { mod2.dest, dstWtPosA }, { mod2.amount, 0.15f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.45f }, { F::reverbMix, 0.2f },
            { ParamID::masterVolume, -6.0f }
        }});

        // ------------------------------------------------------------ Pop
        p.push_back ({ "Pop", "Pluck Echo", shapes, pwm,
        {
            { a.wtPos, 0.62f }, { a.level, 0.7f }, { a.unisonVoices, 2.0f }, { a.unisonDetune, 6.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.5f }, { b.octave, 1.0f }, { b.level, 0.25f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 600.0f }, { ParamID::filterResonance, 0.25f },
            { ParamID::filterKeyTrack, 0.5f }, { ParamID::filterEnv2Amount, 40.0f },
            { env1.attack, 1.0f }, { env1.decay, 350.0f }, { env1.sustain, 0.0f }, { env1.release, 200.0f },
            { env2.attack, 1.0f }, { env2.decay, 180.0f }, { env2.sustain, 0.0f }, { env2.release, 150.0f },
            { F::eqEnabled, 1.0f }, { F::eqLowGain, -3.0f }, { F::eqHighGain, 2.5f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divDottedEighth }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.5f }, { F::delayLowpass, 3500.0f }, { F::delayMix, 0.35f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.5f }, { F::reverbMix, 0.2f },
            { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Pop", "Poly Stack", shapes, shapes,
        {
            { a.wtPos, 0.7f }, { a.level, 0.6f }, { a.unisonVoices, 3.0f }, { a.unisonDetune, 9.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.45f }, { b.octave, 1.0f }, { b.level, 0.3f }, { b.pan, 0.25f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 6500.0f },
            { env1.attack, 6.0f }, { env1.decay, 800.0f }, { env1.sustain, 0.7f }, { env1.release, 350.0f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.7f }, { F::chorusDepth, 0.3f }, { F::chorusMix, 0.35f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.55f }, { F::reverbMix, 0.22f },
            { ParamID::masterVolume, -7.0f }
        }});

        p.push_back ({ "Pop", "Vocal Pad", harm, shapes,
        {
            { a.wtPos, 0.45f }, { a.level, 0.65f }, { a.unisonVoices, 4.0f }, { a.unisonDetune, 10.0f },
            { a.unisonWidth, 0.85f },
            { b.enabled, 1.0f }, { b.wtPos, 0.3f }, { b.level, 0.25f },
            { ParamID::filterType, bp12 }, { ParamID::filterCutoff, 1100.0f }, { ParamID::filterResonance, 0.4f },
            { env1.attack, 450.0f }, { env1.decay, 1500.0f }, { env1.sustain, 0.8f }, { env1.release, 900.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 0.4f }, { lfo1.retrigger, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstCutoff }, { mod1.amount, 0.25f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.85f }, { F::reverbPredelay, 25.0f }, { F::reverbMix, 0.4f },
            { ParamID::masterVolume, -7.0f }
        }});

        p.push_back ({ "Pop", "Bell Lead", sine, harm,
        {
            { a.level, 0.7f },
            { b.enabled, 1.0f }, { b.wtPos, 0.25f }, { b.octave, 1.0f }, { b.level, 0.35f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 7000.0f },
            { env1.attack, 2.0f }, { env1.decay, 1000.0f }, { env1.sustain, 0.4f }, { env1.release, 500.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcVelocity }, { mod1.dest, dstWtPosB }, { mod1.amount, 0.3f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divDottedEighth }, { F::delayFeedback, 0.3f },
            { F::delayMix, 0.22f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.6f }, { F::reverbMix, 0.25f },
            { ParamID::masterVolume, -7.0f }
        }});

        // ------------------------------------------------------------ Synthwave / 80s
        p.push_back ({ "Synthwave / 80s", "Retro Brass", shapes, shapes,
        {
            { a.wtPos, 0.82f }, { a.level, 0.7f }, { a.unisonVoices, 3.0f }, { a.unisonDetune, 13.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.82f }, { b.fine, -10.0f }, { b.level, 0.45f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 900.0f }, { ParamID::filterResonance, 0.2f },
            { ParamID::filterEnv2Amount, 34.0f },
            { env1.attack, 40.0f }, { env1.decay, 900.0f }, { env1.sustain, 0.75f }, { env1.release, 300.0f },
            { env2.attack, 30.0f }, { env2.decay, 700.0f }, { env2.sustain, 0.4f }, { env2.release, 300.0f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.5f }, { F::chorusDepth, 0.35f }, { F::chorusMix, 0.4f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.6f }, { F::reverbMix, 0.25f },
            { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Synthwave / 80s", "Neon Lead", shapes, pwm,
        {
            { a.wtPos, 0.88f }, { a.level, 0.75f }, { a.unisonVoices, 2.0f }, { a.unisonDetune, 12.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.4f }, { b.octave, -1.0f }, { b.level, 0.3f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 4500.0f }, { ParamID::filterResonance, 0.25f },
            { ParamID::filterKeyTrack, 0.4f },
            { env1.attack, 10.0f }, { env1.decay, 600.0f }, { env1.sustain, 0.8f }, { env1.release, 400.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divDottedEighth }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.45f }, { F::delayLowpass, 4500.0f }, { F::delayMix, 0.35f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.75f }, { F::reverbMix, 0.3f },
            { ParamID::polyphony, 4.0f }, { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Synthwave / 80s", "Analog Pad", shapes, harm,
        {
            { a.wtPos, 0.42f }, { a.level, 0.6f }, { a.unisonVoices, 5.0f }, { a.unisonDetune, 11.0f },
            { a.unisonWidth, 0.9f },
            { b.enabled, 1.0f }, { b.wtPos, 0.25f }, { b.octave, -1.0f }, { b.level, 0.4f }, { b.pan, 0.3f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 1500.0f }, { ParamID::filterResonance, 0.12f },
            { ParamID::filterEnv2Amount, 22.0f },
            { env1.attack, 900.0f }, { env1.decay, 1500.0f }, { env1.sustain, 0.8f }, { env1.release, 1800.0f },
            { env2.attack, 1400.0f }, { env2.decay, 2500.0f }, { env2.sustain, 0.35f }, { env2.release, 2000.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, 1.0f }, { lfo1.retrigger, 0.0f }, { lfo1.shape, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstWtPosA }, { mod1.amount, 0.22f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.3f }, { F::chorusDepth, 0.4f }, { F::chorusMix, 0.4f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.85f }, { F::reverbDamping, 0.4f },
            { F::reverbPredelay, 20.0f }, { F::reverbMix, 0.35f },
            { ParamID::masterVolume, -10.0f }
        }});

        p.push_back ({ "Synthwave / 80s", "PWM Strings", pwm, pwm,
        {
            { a.wtPos, 0.5f }, { a.level, 0.6f }, { a.unisonVoices, 4.0f }, { a.unisonDetune, 15.0f },
            { a.unisonWidth, 0.9f },
            { b.enabled, 1.0f }, { b.wtPos, 0.3f }, { b.octave, -1.0f }, { b.level, 0.3f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 3200.0f },
            { env1.attack, 300.0f }, { env1.decay, 1200.0f }, { env1.sustain, 0.85f }, { env1.release, 800.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 0.6f }, { lfo1.retrigger, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstWtPosA }, { mod1.amount, 0.35f },
            { mod2.enabled, 1.0f }, { mod2.source, srcLfo1 }, { mod2.dest, dstWtPosB }, { mod2.amount, -0.3f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.45f }, { F::chorusDepth, 0.45f }, { F::chorusMix, 0.45f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.7f }, { F::reverbMix, 0.28f },
            { ParamID::masterVolume, -9.0f }
        }});

        // ------------------------------------------------------------ Rock
        p.push_back ({ "Rock", "Power Fifths", shapes, shapes,
        {
            { a.wtPos, 0.95f }, { a.level, 0.7f },
            { b.enabled, 1.0f }, { b.wtPos, 0.95f }, { b.semi, 7.0f }, { b.level, 0.6f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 3000.0f },
            { env1.attack, 2.0f }, { env1.decay, 500.0f }, { env1.sustain, 0.8f }, { env1.release, 150.0f },
            { F::distEnabled, 1.0f }, { F::distMode, distHard }, { F::distDrive, 16.0f }, { F::distOutput, -10.0f },
            { F::eqEnabled, 1.0f }, { F::eqMidGain, 4.0f }, { F::eqMidFreq, 1600.0f },
            { ParamID::polyphony, 4.0f }, { ParamID::masterVolume, -9.0f }
        }});

        p.push_back ({ "Rock", "Organ Stab", harm, harm,
        {
            { a.wtPos, 0.5f }, { a.level, 0.65f },
            { b.enabled, 1.0f }, { b.wtPos, 0.5f }, { b.octave, 1.0f }, { b.level, 0.4f },
            { ParamID::filterEnabled, 0.0f },
            { env1.attack, 1.0f }, { env1.decay, 200.0f }, { env1.sustain, 0.9f }, { env1.release, 60.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 6.5f }, { lfo1.retrigger, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstPitchA }, { mod1.amount, 0.03f },
            { F::distEnabled, 1.0f }, { F::distMode, distSoft }, { F::distDrive, 8.0f }, { F::distOutput, -6.0f },
            { F::distMix, 0.5f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.4f }, { F::reverbMix, 0.18f },
            { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Rock", "Fuzz Lead", shapes, shapes,
        {
            { a.wtPos, 0.9f }, { a.level, 0.7f }, { a.unisonVoices, 2.0f }, { a.unisonDetune, 5.0f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 2400.0f }, { ParamID::filterResonance, 0.25f },
            { ParamID::filterDrive, 3.0f },
            { env1.attack, 3.0f }, { env1.decay, 600.0f }, { env1.sustain, 0.85f }, { env1.release, 250.0f },
            { F::distEnabled, 1.0f }, { F::distMode, distFold }, { F::distDrive, 14.0f }, { F::distOutput, -10.0f },
            { F::distMix, 0.7f },
            { F::eqEnabled, 1.0f }, { F::eqMidGain, 5.0f }, { F::eqMidFreq, 1200.0f }, { F::eqHighGain, -2.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divQuarter }, { F::delayFeedback, 0.25f },
            { F::delayMix, 0.18f },
            { ParamID::polyphony, 2.0f }, { ParamID::masterVolume, -9.0f }
        }});

        // ------------------------------------------------------------ Jazz / Soul
        p.push_back ({ "Jazz / Soul", "E-Piano", sine, harm,
        {
            { a.level, 0.7f },
            { b.enabled, 1.0f }, { b.wtPos, 0.18f }, { b.octave, 1.0f }, { b.level, 0.22f },
            { ParamID::oscAWarpMode, warpFm }, { ParamID::oscAWarpAmount, 0.18f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 4200.0f },
            { env1.attack, 2.0f }, { env1.decay, 1600.0f }, { env1.sustain, 0.2f }, { env1.release, 400.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcVelocity }, { mod1.dest, dstWarp }, { mod1.amount, 0.35f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.4f }, { F::chorusDepth, 0.25f }, { F::chorusMix, 0.25f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.45f }, { F::reverbMix, 0.18f },
            { ParamID::masterVolume, -6.0f }
        }});

        p.push_back ({ "Jazz / Soul", "Vibraphone", sine, sine,
        {
            { a.level, 0.8f },
            { b.enabled, 1.0f }, { b.octave, 2.0f }, { b.level, 0.12f },
            { ParamID::filterEnabled, 0.0f },
            { env1.attack, 2.0f }, { env1.decay, 1800.0f }, { env1.sustain, 0.0f }, { env1.release, 1400.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 5.5f }, { lfo1.retrigger, 1.0f }, { lfo1.unipolar, 1.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstAmp }, { mod1.amount, 0.3f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.7f }, { F::reverbMix, 0.3f },
            { ParamID::masterVolume, -6.0f }
        }});

        p.push_back ({ "Jazz / Soul", "Soft Clav", pwm, shapes,
        {
            { a.wtPos, 0.35f }, { a.level, 0.7f },
            { b.enabled, 1.0f }, { b.wtPos, 0.9f }, { b.level, 0.18f },
            { ParamID::filterType, bp12 }, { ParamID::filterCutoff, 1400.0f }, { ParamID::filterResonance, 0.35f },
            { ParamID::filterKeyTrack, 0.6f }, { ParamID::filterEnv2Amount, 28.0f },
            { env1.attack, 1.0f }, { env1.decay, 420.0f }, { env1.sustain, 0.1f }, { env1.release, 180.0f },
            { env2.attack, 1.0f }, { env2.decay, 150.0f }, { env2.sustain, 0.0f }, { env2.release, 120.0f },
            { F::eqEnabled, 1.0f }, { F::eqHighGain, 3.0f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.4f }, { F::reverbMix, 0.16f },
            { ParamID::masterVolume, -6.0f }
        }});

        // ------------------------------------------------------------ Cinematic / Ambient
        p.push_back ({ "Cinematic / Ambient", "Glass Bell", harm, sine,
        {
            { a.wtPos, 0.15f }, { a.level, 0.8f }, { a.unisonVoices, 1.0f },
            { b.enabled, 1.0f }, { b.octave, 1.0f }, { b.semi, 7.0f }, { b.level, 0.25f },
            { ParamID::filterEnabled, 0.0f },
            { env1.attack, 2.0f }, { env1.decay, 1800.0f }, { env1.sustain, 0.0f }, { env1.release, 1200.0f },
            { env2.attack, 2.0f }, { env2.decay, 700.0f }, { env2.sustain, 0.0f }, { env2.release, 600.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcEnv2 }, { mod1.dest, dstWtPosA }, { mod1.amount, 0.55f },
            { mod2.enabled, 1.0f }, { mod2.source, srcVelocity }, { mod2.dest, dstWtPosA }, { mod2.amount, 0.2f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divEighth }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.35f }, { F::delayMix, 0.25f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.8f }, { F::reverbMix, 0.3f },
            { ParamID::masterVolume, -10.0f }
        }});

        p.push_back ({ "Cinematic / Ambient", "Cinematic Pad", shapes, harm,
        {
            { a.wtPos, 0.3f }, { a.level, 0.6f }, { a.unisonVoices, 6.0f }, { a.unisonDetune, 18.0f },
            { a.unisonWidth, 1.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.4f }, { b.octave, -1.0f }, { b.level, 0.35f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 1200.0f }, { ParamID::filterEnv2Amount, 30.0f },
            { env1.attack, 1600.0f }, { env1.decay, 3000.0f }, { env1.sustain, 0.85f }, { env1.release, 3000.0f },
            { env2.attack, 2500.0f }, { env2.decay, 4000.0f }, { env2.sustain, 0.5f }, { env2.release, 2500.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, 0.0f }, { lfo1.retrigger, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstWtPosA }, { mod1.amount, 0.3f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.95f }, { F::reverbDamping, 0.35f },
            { F::reverbPredelay, 40.0f }, { F::reverbMix, 0.45f },
            { ParamID::masterVolume, -9.0f }
        }});

        p.push_back ({ "Cinematic / Ambient", "Dark Drone", shapes, shapes,
        {
            { a.wtPos, 0.45f }, { a.octave, -2.0f }, { a.level, 0.7f }, { a.unisonVoices, 4.0f },
            { a.unisonDetune, 22.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.6f }, { b.octave, -1.0f }, { b.fine, 6.0f }, { b.level, 0.3f },
            { ParamID::noiseEnabled, 1.0f }, { ParamID::noiseType, noisePink }, { ParamID::noiseLevel, 0.05f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 500.0f }, { ParamID::filterResonance, 0.15f },
            { env1.attack, 2500.0f }, { env1.decay, 4000.0f }, { env1.sustain, 0.9f }, { env1.release, 4000.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 0.08f }, { lfo1.retrigger, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstCutoff }, { mod1.amount, 0.3f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 1.0f }, { F::reverbDamping, 0.7f }, { F::reverbMix, 0.5f },
            { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Cinematic / Ambient", "Ambient Shimmer", harm, sine,
        {
            { a.wtPos, 0.2f }, { a.level, 0.55f }, { a.unisonVoices, 3.0f }, { a.unisonDetune, 9.0f },
            { b.enabled, 1.0f }, { b.octave, 2.0f }, { b.level, 0.2f }, { b.pan, -0.3f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 5000.0f },
            { env1.attack, 1200.0f }, { env1.decay, 3000.0f }, { env1.sustain, 0.7f }, { env1.release, 3500.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divHalf }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.7f }, { F::delayLowpass, 5000.0f }, { F::delayMix, 0.45f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.95f }, { F::reverbPredelay, 60.0f }, { F::reverbMix, 0.5f },
            { ParamID::masterVolume, -9.0f }
        }});

        p.push_back ({ "Cinematic / Ambient", "Choir Pad", harm, harm,
        {
            { a.wtPos, 0.35f }, { a.level, 0.6f }, { a.unisonVoices, 5.0f }, { a.unisonDetune, 13.0f },
            { a.unisonWidth, 0.95f },
            { b.enabled, 1.0f }, { b.wtPos, 0.55f }, { b.level, 0.25f },
            { ParamID::filterType, bp12 }, { ParamID::filterCutoff, 900.0f }, { ParamID::filterResonance, 0.3f },
            { env1.attack, 800.0f }, { env1.decay, 2000.0f }, { env1.sustain, 0.85f }, { env1.release, 1600.0f },
            { lfo1.tempoSync, 0.0f }, { lfo1.rate, 4.5f }, { lfo1.retrigger, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstPitchA }, { mod1.amount, 0.05f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.9f }, { F::reverbPredelay, 30.0f }, { F::reverbMix, 0.45f },
            { ParamID::masterVolume, -8.0f }
        }});

        // ------------------------------------------------------------ Chiptune / Game
        p.push_back ({ "Chiptune / Game", "Square Lead", shapes, shapes,
        {
            { a.wtPos, 1.0f }, { a.level, 0.7f }, { a.randomPhase, 0.0f },
            { ParamID::filterEnabled, 0.0f },
            { env1.attack, 1.0f }, { env1.decay, 200.0f }, { env1.sustain, 0.9f }, { env1.release, 40.0f },
            { ParamID::polyphony, 1.0f }, { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Chiptune / Game", "PWM Arp", pwm, pwm,
        {
            { a.wtPos, 0.35f }, { a.level, 0.7f }, { a.randomPhase, 0.0f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 6000.0f },
            { env1.attack, 1.0f }, { env1.decay, 160.0f }, { env1.sustain, 0.6f }, { env1.release, 60.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, divHalf }, { lfo1.retrigger, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstWtPosA }, { mod1.amount, 0.25f },
            { A::enabled, 1.0f }, { A::mode, 0.0f }, { A::division, divSixteenth }, { A::octaves, 2.0f },
            { A::gate, 0.55f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divEighth }, { F::delayFeedback, 0.25f },
            { F::delayMix, 0.2f },
            { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Chiptune / Game", "Game Bass", shapes, sine,
        {
            { a.wtPos, 1.0f }, { a.octave, -1.0f }, { a.level, 0.75f }, { a.randomPhase, 0.0f },
            { ParamID::subEnabled, 1.0f }, { ParamID::subShape, subSquare }, { ParamID::subLevel, 0.4f },
            { ParamID::filterType, lp12 }, { ParamID::filterCutoff, 2500.0f },
            { env1.attack, 1.0f }, { env1.decay, 150.0f }, { env1.sustain, 0.8f }, { env1.release, 50.0f },
            { ParamID::polyphony, 1.0f }, { ParamID::masterVolume, -7.0f }
        }});

        p.push_back ({ "Chiptune / Game", "Noise Perc", sine, sine,
        {
            { a.level, 0.0f },
            { ParamID::noiseEnabled, 1.0f }, { ParamID::noiseType, noiseWhite }, { ParamID::noiseLevel, 0.8f },
            { ParamID::filterType, hp12 }, { ParamID::filterCutoff, 1200.0f }, { ParamID::filterKeyTrack, 0.8f },
            { env1.attack, 1.0f }, { env1.decay, 120.0f }, { env1.sustain, 0.0f }, { env1.release, 80.0f },
            { ParamID::masterVolume, -8.0f }
        }});

        // ------------------------------------------------------------ Experimental
        p.push_back ({ "Experimental", "Ring Mod Lead", shapes, harm,
        {
            { a.wtPos, 0.6f }, { a.level, 0.7f },
            { b.enabled, 1.0f }, { b.wtPos, 0.3f }, { b.semi, 5.0f }, { b.level, 0.0f },
            { ParamID::oscAWarpMode, warpRm }, { ParamID::oscAWarpAmount, 0.6f },
            { ParamID::filterType, lp24 }, { ParamID::filterCutoff, 4000.0f },
            { env1.attack, 5.0f }, { env1.decay, 700.0f }, { env1.sustain, 0.6f }, { env1.release, 300.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcWheel }, { mod1.dest, dstWarp }, { mod1.amount, 0.4f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divEighth }, { F::delayFeedback, 0.35f },
            { F::delayMix, 0.25f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.7f }, { F::reverbMix, 0.25f },
            { ParamID::masterVolume, -8.0f }
        }});

        p.push_back ({ "Experimental", "Random Bleeps", shapes, sine,
        {
            { a.wtPos, 0.8f }, { a.level, 0.6f },
            { ParamID::filterType, bp12 }, { ParamID::filterCutoff, 1500.0f }, { ParamID::filterResonance, 0.5f },
            { env1.attack, 1.0f }, { env1.decay, 180.0f }, { env1.sustain, 0.0f }, { env1.release, 120.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, divSixteenth }, { lfo1.shape, 4.0f },
            { mod1.enabled, 1.0f }, { mod1.source, srcLfo1 }, { mod1.dest, dstCutoff }, { mod1.amount, 0.6f },
            { mod2.enabled, 1.0f }, { mod2.source, (float) wf::ModSource::randomPerNote }, { mod2.dest, dstWtPosA },
            { mod2.amount, 0.5f },
            { mod3.enabled, 1.0f }, { mod3.source, (float) wf::ModSource::randomPerNote }, { mod3.dest, dstPitchA },
            { mod3.amount, 0.06f },
            { A::enabled, 1.0f }, { A::mode, 5.0f }, { A::division, divSixteenth }, { A::gate, 0.4f },
            { A::pattern, 9.0f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, divSixteenth }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.5f }, { F::delayMix, 0.3f },
            { ParamID::masterVolume, -8.0f }
        }});

        return p;
    }();

    return presets;
}

juce::StringArray PresetManager::categories()
{
    juce::StringArray out;
    for (const auto& p : factoryPresets())
        out.addIfNotAlreadyThere (p.category);
    return out;
}

const PresetManager::FactoryPreset* PresetManager::findFactory (const juce::String& name)
{
    for (const auto& p : factoryPresets())
        if (p.name == name)
            return &p;
    return nullptr;
}

void PresetManager::resetToDefaults (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* param : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
}

void PresetManager::applyFactory (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset)
{
    resetToDefaults (apvts);

    for (const auto& [id, value] : preset.values)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (value));
        else
            jassertfalse; // parameter id in a factory preset no longer exists
    }
}

bool PresetManager::saveToFile (const juce::ValueTree& state, const juce::File& file, juce::String& error)
{
    auto xml = state.createXml();
    if (xml == nullptr)
    {
        error = "Could not serialise the current state.";
        return false;
    }

    if (! file.getParentDirectory().createDirectory())
    {
        error = "Could not create " + file.getParentDirectory().getFullPathName();
        return false;
    }

    if (! xml->writeTo (file))
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    return true;
}

bool PresetManager::loadFromFile (const juce::File& file, juce::ValueTree& stateOut, juce::String& error)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
    {
        error = "Not a valid preset file: " + file.getFileName();
        return false;
    }

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid())
    {
        error = "Preset file is empty or corrupt: " + file.getFileName();
        return false;
    }

    stateOut = tree;
    return true;
}
