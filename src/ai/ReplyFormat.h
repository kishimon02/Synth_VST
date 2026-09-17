#pragma once

#include <juce_core/juce_core.h>
#include "MusicContext.h"
#include "../dsp/Arpeggiator.h"
#include <vector>

namespace ai
{

// One chat entry. Assistant replies carry the structured parts the model
// returned; the UI turns them into cards.
struct ChatMessage
{
    enum Role { user, assistant, system };
    Role role = user;
    juce::String text;              // the user's request or the model's "reply"
    bool isError = false;

    // structured payload (assistant only)
    juce::String notesKind;         // "none" | melody | chords | drums | bass | harmony | variation | continuation | phrase | arp
    std::vector<Note> notes;
    juce::var paramChanges;         // [{id, value}]
    juce::var wavetable;            // {name, frames:[{harmonics:[...]}]}
    wf::ArpPattern arpPattern;      // steps empty = none
    juce::String presetName;

    bool hasNotes() const noexcept { return ! notes.empty() && notesKind != "none"; }
    bool hasParamChanges() const noexcept { return paramChanges.isArray() && paramChanges.size() > 0; }
    bool hasWavetable() const noexcept;
    bool hasArpPattern() const noexcept { return ! arpPattern.steps.empty(); }
    bool isDrums() const noexcept { return notesKind == "drums"; }
};

// Everything about the wire format that needs no processor: the reply
// schema, the system prompt, reply parsing and wavetable synthesis.
struct ReplyFormat
{
    static juce::var schema();
    static juce::String systemPrompt (const juce::String& parameterCatalog);
    static bool parse (const juce::var& json, ChatMessage& out, juce::String& error);

    // {name, frames:[{harmonics}]} -> numFrames x 2048 samples (key frames morphed)
    static bool wavetableFrames (const juce::var& wavetable, std::vector<float>& frames, int& numFrames, juce::String& name);
    static constexpr int wavetableTargetFrames = 32;
};

} // namespace ai
