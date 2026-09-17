#pragma once

#include "dsp/WavetableLoader.h"
#include <vector>

// Owns every wavetable loaded during the plugin's lifetime. Tables are only
// ever appended (message thread) and never freed while the processor lives,
// so the audio thread can hold raw pointers to them safely.
class WavetableBank
{
public:
    WavetableBank()
    {
        for (const auto& name : wf::WavetableLoader::builtinNames())
            if (auto t = wf::WavetableLoader::createBuiltin (name))
                tables.push_back (std::move (t));
    }

    int size() const noexcept { return (int) tables.size(); }

    const wf::Wavetable* get (int index) const noexcept
    {
        return juce::isPositiveAndBelow (index, size()) ? tables[(size_t) index].get() : nullptr;
    }

    juce::String nameAt (int index) const     { auto* t = get (index); return t != nullptr ? t->getName() : juce::String(); }
    juce::String sourceIdAt (int index) const { auto* t = get (index); return t != nullptr ? t->getSourceId() : juce::String(); }

    // Latest match wins, so a re-saved user table shadows its older copy
    // (the old one stays alive for any voice still reading it).
    int indexOfSourceId (const juce::String& id) const noexcept
    {
        for (int i = size() - 1; i >= 0; --i)
            if (tables[(size_t) i]->getSourceId() == id)
                return i;
        return -1;
    }

    // Finds an already-loaded table by sourceId or loads it. Returns the bank
    // index, or -1 (with `error` set).
    int resolve (const juce::String& sourceId, juce::String& error)
    {
        const int existing = indexOfSourceId (sourceId);
        if (existing >= 0)
            return existing;
        if (auto t = wf::WavetableLoader::fromSourceId (sourceId, error))
        {
            tables.push_back (std::move (t));
            return size() - 1;
        }
        return -1;
    }

    int addFile (const juce::File& file, juce::String& error)
    {
        return resolve (file.getFullPathName(), error);
    }

    // Loads the file again even if a table with that path is already cached
    // (used after the editor overwrites a user table).
    int reloadFile (const juce::File& file, juce::String& error)
    {
        if (auto t = wf::WavetableLoader::loadFile (file, error))
        {
            tables.push_back (std::move (t));
            return size() - 1;
        }
        return -1;
    }

    // Names shown in the table menu: user tables appear with their file name.
    std::vector<int> visibleIndices() const
    {
        // Hide shadowed duplicates (same sourceId, older copy).
        std::vector<int> out;
        for (int i = 0; i < size(); ++i)
            if (indexOfSourceId (tables[(size_t) i]->getSourceId()) == i)
                out.push_back (i);
        return out;
    }

private:
    std::vector<std::shared_ptr<wf::Wavetable>> tables;
};
