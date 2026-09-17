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

    int indexOfSourceId (const juce::String& id) const noexcept
    {
        for (int i = 0; i < size(); ++i)
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

private:
    std::vector<std::shared_ptr<wf::Wavetable>> tables;
};
