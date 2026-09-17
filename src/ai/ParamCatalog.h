#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace ai
{

// Describes every plugin parameter to the model and applies the changes it
// returns. Values travel in the parameter's own units (Hz, ms, dB, 0..1, or
// a choice index) so the catalogue text and the reply share one vocabulary.
class ParamCatalog
{
public:
    struct Change
    {
        juce::String id, name;
        float oldValue = 0.0f, newValue = 0.0f;   // in parameter units
        juce::String oldText, newText;            // formatted for display
    };

    // One line per parameter: id, name, type/range or choices, default.
    static juce::String describe (juce::AudioProcessorValueTreeState& apvts);

    // JSON object { id: value } of parameters that differ from their default.
    static juce::String currentValues (juce::AudioProcessorValueTreeState& apvts, bool onlyNonDefault = true);

    // Applies [{id, value}] (juce::var array). Unknown ids are skipped and
    // listed in `unknown`; values are clamped to the parameter range.
    static std::vector<Change> applyChanges (juce::AudioProcessorValueTreeState& apvts, const juce::var& changes,
                                             juce::StringArray& unknown);

    // Preview without applying (same clamping), for the diff card.
    static std::vector<Change> previewChanges (juce::AudioProcessorValueTreeState& apvts, const juce::var& changes,
                                               juce::StringArray& unknown);

private:
    static std::vector<Change> collect (juce::AudioProcessorValueTreeState& apvts, const juce::var& changes,
                                        juce::StringArray& unknown, bool apply);
};

} // namespace ai
