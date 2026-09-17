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
        const auto mod1 = ParamID::modSlot (0);
        const auto mod2 = ParamID::modSlot (1);
        namespace F = ParamID::Fx;

        std::vector<FactoryPreset> p;

        p.push_back ({ "Init", builtin ("Basic Shapes"), builtin ("Sine"), {} });

        p.push_back ({ "Supersaw Lead", builtin ("Basic Shapes"), builtin ("Basic Shapes"),
        {
            { a.wtPos, 0.87f }, { a.level, 0.7f }, { a.unisonVoices, 7.0f }, { a.unisonDetune, 22.0f },
            { a.unisonBlend, 0.9f }, { a.unisonWidth, 0.85f },
            { b.enabled, 1.0f }, { b.wtPos, 0.87f }, { b.octave, -1.0f }, { b.level, 0.35f },
            { b.unisonVoices, 3.0f }, { b.unisonDetune, 14.0f },
            { ParamID::filterType, 1.0f }, { ParamID::filterCutoff, 9000.0f }, { ParamID::filterResonance, 0.18f },
            { ParamID::filterKeyTrack, 0.4f },
            { env1.attack, 3.0f }, { env1.decay, 900.0f }, { env1.sustain, 0.75f }, { env1.release, 260.0f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.6f }, { F::chorusDepth, 0.25f }, { F::chorusMix, 0.3f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, 14.0f }, { F::delayFeedback, 0.3f },
            { F::delayLowpass, 4000.0f }, { F::delayMix, 0.18f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.7f }, { F::reverbMix, 0.2f },
            { ParamID::masterVolume, -9.0f }, { ParamID::polyphony, 8.0f }
        }});

        p.push_back ({ "Soft Pad", builtin ("Basic Shapes"), builtin ("Harmonics"),
        {
            { a.wtPos, 0.42f }, { a.level, 0.6f }, { a.unisonVoices, 5.0f }, { a.unisonDetune, 11.0f },
            { a.unisonWidth, 0.9f },
            { b.enabled, 1.0f }, { b.wtPos, 0.25f }, { b.octave, -1.0f }, { b.level, 0.4f }, { b.pan, 0.3f },
            { ParamID::filterType, 1.0f }, { ParamID::filterCutoff, 1500.0f }, { ParamID::filterResonance, 0.12f },
            { ParamID::filterEnv2Amount, 22.0f },
            { env1.attack, 900.0f }, { env1.decay, 1500.0f }, { env1.sustain, 0.8f }, { env1.release, 1800.0f },
            { env2.attack, 1400.0f }, { env2.decay, 2500.0f }, { env2.sustain, 0.35f }, { env2.release, 2000.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, 1.0f }, { lfo1.retrigger, 0.0f }, { lfo1.shape, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, (float) wf::ModSource::lfo1 }, { mod1.dest, (float) wf::ModDest::oscAWtPos },
            { mod1.amount, 0.22f },
            { F::chorusEnabled, 1.0f }, { F::chorusRate, 0.3f }, { F::chorusDepth, 0.4f }, { F::chorusMix, 0.4f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.85f }, { F::reverbDamping, 0.4f },
            { F::reverbPredelay, 20.0f }, { F::reverbMix, 0.35f },
            { ParamID::masterVolume, -10.0f }
        }});

        p.push_back ({ "Wobble Bass", builtin ("Basic Shapes"), builtin ("PWM"),
        {
            { a.wtPos, 0.9f }, { a.level, 0.75f }, { a.unisonVoices, 2.0f }, { a.unisonDetune, 8.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.4f }, { b.level, 0.3f },
            { ParamID::subEnabled, 1.0f }, { ParamID::subLevel, 0.6f }, { ParamID::subOctave, -1.0f },
            { ParamID::filterType, 1.0f }, { ParamID::filterCutoff, 420.0f }, { ParamID::filterResonance, 0.55f },
            { ParamID::filterDrive, 2.5f },
            { env1.attack, 2.0f }, { env1.decay, 400.0f }, { env1.sustain, 0.9f }, { env1.release, 120.0f },
            { lfo1.tempoSync, 1.0f }, { lfo1.division, 6.0f }, { lfo1.retrigger, 1.0f }, { lfo1.shape, 0.0f },
            { mod1.enabled, 1.0f }, { mod1.source, (float) wf::ModSource::lfo1 }, { mod1.dest, (float) wf::ModDest::filterCutoff },
            { mod1.amount, 0.45f },
            { F::distEnabled, 1.0f }, { F::distMode, 0.0f }, { F::distDrive, 10.0f }, { F::distOutput, -3.0f },
            { F::distMix, 0.6f },
            { ParamID::masterVolume, -8.0f }, { ParamID::polyphony, 2.0f }
        }});

        p.push_back ({ "Glass Bell", builtin ("Harmonics"), builtin ("Sine"),
        {
            { a.wtPos, 0.15f }, { a.level, 0.8f }, { a.unisonVoices, 1.0f },
            { b.enabled, 1.0f }, { b.octave, 1.0f }, { b.semi, 7.0f }, { b.level, 0.25f },
            { ParamID::filterEnabled, 0.0f },
            { env1.attack, 2.0f }, { env1.decay, 1800.0f }, { env1.sustain, 0.0f }, { env1.release, 1200.0f },
            { env2.attack, 2.0f }, { env2.decay, 700.0f }, { env2.sustain, 0.0f }, { env2.release, 600.0f },
            { mod1.enabled, 1.0f }, { mod1.source, (float) wf::ModSource::env2 }, { mod1.dest, (float) wf::ModDest::oscAWtPos },
            { mod1.amount, 0.55f },
            { mod2.enabled, 1.0f }, { mod2.source, (float) wf::ModSource::velocity }, { mod2.dest, (float) wf::ModDest::oscAWtPos },
            { mod2.amount, 0.2f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, 6.0f }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.35f }, { F::delayMix, 0.25f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.8f }, { F::reverbMix, 0.3f },
            { ParamID::masterVolume, -10.0f }
        }});

        p.push_back ({ "Pluck Echo", builtin ("Basic Shapes"), builtin ("PWM"),
        {
            { a.wtPos, 0.62f }, { a.level, 0.7f }, { a.unisonVoices, 2.0f }, { a.unisonDetune, 6.0f },
            { b.enabled, 1.0f }, { b.wtPos, 0.5f }, { b.octave, 1.0f }, { b.level, 0.25f },
            { ParamID::filterType, 1.0f }, { ParamID::filterCutoff, 600.0f }, { ParamID::filterResonance, 0.25f },
            { ParamID::filterKeyTrack, 0.5f }, { ParamID::filterEnv2Amount, 40.0f },
            { env1.attack, 1.0f }, { env1.decay, 350.0f }, { env1.sustain, 0.0f }, { env1.release, 200.0f },
            { env2.attack, 1.0f }, { env2.decay, 180.0f }, { env2.sustain, 0.0f }, { env2.release, 150.0f },
            { F::eqEnabled, 1.0f }, { F::eqLowGain, -3.0f }, { F::eqHighGain, 2.5f },
            { F::delayEnabled, 1.0f }, { F::delayDivision, 14.0f }, { F::delayPingPong, 1.0f },
            { F::delayFeedback, 0.5f }, { F::delayLowpass, 3500.0f }, { F::delayMix, 0.35f },
            { F::reverbEnabled, 1.0f }, { F::reverbSize, 0.5f }, { F::reverbMix, 0.2f },
            { ParamID::masterVolume, -8.0f }
        }});

        return p;
    }();

    return presets;
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
