#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Preset storage for WaveForge.
//
// A preset is the APVTS state serialised as XML, with the two wavetable
// sourceIds carried as properties on the tree - the same payload the host
// saves with the song, so "save as preset" and "save in song" cannot diverge.
//
// User presets live in %APPDATA%/WaveForge/Presets/*.wfpreset.
// Factory presets are built in code (no files to install) from a list of
// parameter overrides applied on top of the defaults.
class PresetManager
{
public:
    static constexpr const char* fileExtension = ".wfpreset";

    struct FactoryPreset
    {
        juce::String name;
        juce::String tableA, tableB;                               // wavetable sourceIds
        std::vector<std::pair<juce::String, float>> values;        // parameter id -> value in its own units
    };

    static juce::File presetDirectory();
    static juce::Array<juce::File> userPresets();

    static const std::vector<FactoryPreset>& factoryPresets();
    static const FactoryPreset* findFactory (const juce::String& name);

    // Sets every parameter back to its default. Used before applying a preset
    // so leftovers from the previous one cannot leak through.
    static void resetToDefaults (juce::AudioProcessorValueTreeState& apvts);

    static void applyFactory (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset);

    // Returns false and fills `error` on failure.
    static bool saveToFile (const juce::ValueTree& state, const juce::File& file, juce::String& error);
    static bool loadFromFile (const juce::File& file, juce::ValueTree& stateOut, juce::String& error);
};
