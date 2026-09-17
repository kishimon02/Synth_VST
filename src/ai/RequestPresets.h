#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace ai
{

// Library of ready-made requests. Built-ins live in code; user presets in
// %APPDATA%/WaveForge/RequestPresets.json. Categories are ordered as shown
// in the Presets menu.
struct RequestPreset
{
    juce::String category, name, text;
    bool builtin = true;
};

class RequestPresets
{
public:
    static const std::vector<RequestPreset>& builtins();
    static juce::StringArray categories();            // in menu order

    static juce::File userFile();
    static std::vector<RequestPreset> loadUser();
    static bool saveUser (const std::vector<RequestPreset>&, juce::String& error);

    // Built-ins followed by user presets.
    static std::vector<RequestPreset> all();

    // The first built-in of a category (quick buttons).
    static juce::String firstText (const juce::String& category);
};

} // namespace ai
