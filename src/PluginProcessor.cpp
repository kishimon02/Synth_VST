#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
WaveForgeProcessor::WaveForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WaveForgeState", Params::createLayout())
{
    refs.bind (apvts);

    customPatterns.push_back (std::make_shared<wf::ArpPattern> (wf::ArpPattern::builtins()[0]));
    customPatterns.back()->name = "Custom";
    customArpPattern.store (customPatterns.back().get(), std::memory_order_release);
    arpBuffer.ensureSize (4096);

    // Default tables: OSC A = Basic Shapes, OSC B = Sine
    setOscTable (0, juce::jmax (0, bank.indexOfSourceId (wf::WavetableLoader::builtinSourceId ("Basic Shapes"))));
    setOscTable (1, juce::jmax (0, bank.indexOfSourceId (wf::WavetableLoader::builtinSourceId ("Sine"))));

    assistant = std::make_unique<ai::Assistant> (*this);
}

void WaveForgeProcessor::setOscTable (int osc, int bankIndex)
{
    if (auto* t = bank.get (bankIndex))
    {
        oscTableIndex[osc] = bankIndex;
        oscTable[osc].store (t, std::memory_order_release);
        apvts.state.setProperty (ParamID::oscTableProperty (osc), t->getSourceId(), nullptr);
        sendChangeMessage();
    }
}

void WaveForgeProcessor::setOscTableBySourceId (int osc, const juce::String& sourceId)
{
    juce::String error;
    int index = sourceId.isNotEmpty() ? bank.resolve (sourceId, error) : -1;
    if (index < 0)
    {
        if (error.isNotEmpty())
        {
            DBG ("WaveForge: " << error);
        }
        index = juce::jmax (0, bank.indexOfSourceId (wf::WavetableLoader::builtinSourceId (osc == 0 ? "Basic Shapes" : "Sine")));
    }
    setOscTable (osc, index);
}

bool WaveForgeProcessor::loadWavetableFile (int osc, const juce::File& file, juce::String& error, bool forceReload)
{
    const int index = forceReload ? bank.reloadFile (file, error) : bank.addFile (file, error);
    if (index < 0)
        return false;
    setOscTable (osc, index);
    return true;
}

juce::File WaveForgeProcessor::userWavetableDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("WaveForge").getChildFile ("Wavetables");
}

juce::File WaveForgeProcessor::userArpPatternDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("WaveForge").getChildFile ("ArpPatterns");
}

void WaveForgeProcessor::setCustomArpPattern (const wf::ArpPattern& pattern, bool selectCustom)
{
    auto copy = std::make_shared<wf::ArpPattern> (pattern);
    copy->clampAndTrim();
    customPatterns.push_back (copy);                 // old one stays alive for the audio thread
    customArpPattern.store (copy.get(), std::memory_order_release);
    apvts.state.setProperty (ParamID::arpCustomPatternProperty, juce::JSON::toString (copy->toJson(), true), nullptr);

    if (selectCustom)
        if (auto* param = apvts.getParameter (ParamID::Arp::pattern))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (
                (float) wf::ArpPattern::builtins().size()));
    sendChangeMessage();
}

const wf::ArpPattern* WaveForgeProcessor::getActiveArpPattern() const noexcept
{
    const int index = (int) std::lround (refs.arp.pattern->load());
    const auto& builtins = wf::ArpPattern::builtins();
    if (juce::isPositiveAndBelow (index, (int) builtins.size()))
        return &builtins[(size_t) index];
    return customArpPattern.load (std::memory_order_acquire);
}

//==============================================================================
void WaveForgeProcessor::timerCallback()
{
#if WAVEFORGE_DIAGNOSTICS
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WaveForge_diag.log");
    file.appendText (juce::Time::getCurrentTime().formatted ("%H:%M:%S ")
                     + (wrapperType == wrapperType_Standalone ? "[Standalone] " : "[VST3] ")
                     + diag.snapshotAndReset (currentSampleRate) + "\n");
#endif
}

void WaveForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
#if WAVEFORGE_DIAGNOSTICS
    startTimer (2000);
#endif
    engine.prepare (sampleRate);
    fx.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    arp.prepare (sampleRate);
    preview.prepare (sampleRate);
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (refs.masterVolume->load(), -60.0f));
}

void WaveForgeProcessor::releaseResources() {}

double WaveForgeProcessor::getTailLengthSeconds() const
{
    // Delay and reverb ring on after the last note; tell the host so it
    // keeps rendering (bounce / freeze) until the tail is done.
    const bool delayOn  = refs.fx.delayEnabled  != nullptr && refs.fx.delayEnabled->load()  >= 0.5f;
    const bool reverbOn = refs.fx.reverbEnabled != nullptr && refs.fx.reverbEnabled->load() >= 0.5f;
    return (delayOn || reverbOn) ? 6.0 : 0.0;
}

bool WaveForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void WaveForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    diag.blockStart (numSamples);
    buffer.clear();

    // Merge on-screen keyboard events with host MIDI (and light up host notes).
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Tempo / position for synced LFOs and the arpeggiator. Standalone has no
    // playhead, so the last known tempo (default 120) is kept and ppq = -1.
    double ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            if (const auto bpm = pos->getBpm())
                hostBpm.store ((float) *bpm, std::memory_order_relaxed);
            if (pos->getIsPlaying())
                if (const auto q = pos->getPpqPosition())
                    ppq = *q;
        }
    hostPpq.store (ppq, std::memory_order_relaxed);

    auto params = refs.snapshot (oscTable[0].load (std::memory_order_acquire),
                                 oscTable[1].load (std::memory_order_acquire),
                                 hostBpm.load (std::memory_order_relaxed));
    params.arp.pattern = getActiveArpPattern();

    // AI capture: the raw played notes with an absolute beat position
    {
        const double beatsPerSample = (double) params.bpm / 60.0 / currentSampleRate;
        const double baseBeat = ppq >= 0.0 ? ppq : internalBeat;
        if (capture.recording.load (std::memory_order_relaxed))
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOn())
                    capture.push ({ m.getNoteNumber(), m.getVelocity(), true, baseBeat + meta.samplePosition * beatsPerSample });
                else if (m.isNoteOff())
                    capture.push ({ m.getNoteNumber(), 0, false, baseBeat + meta.samplePosition * beatsPerSample });
            }
        internalBeat = baseBeat + numSamples * beatsPerSample;
    }

    arp.process (midi, arpBuffer, numSamples, params.arp, params.bpm, ppq);
    arpStep.store (arp.getCurrentStep(), std::memory_order_relaxed);
    preview.process (arpBuffer, numSamples);        // AI audition bypasses the arp
    engine.process (buffer, arpBuffer, params);
    activeVoices.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);

    fx.process (buffer, params.fx, params.bpm);

    masterGain.setTargetValue (params.global.masterGain);
    masterGain.applyGain (buffer, numSamples);

    scope.push (buffer.getReadPointer (0),
                buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : nullptr, numSamples);

    diag.blockEnd (buffer, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* WaveForgeProcessor::createEditor()
{
    return new WaveForgeEditor (*this);
}

juce::ValueTree WaveForgeProcessor::buildStateTree()
{
    auto state = apvts.copyState();
    for (int i = 0; i < 2; ++i)
        state.setProperty (ParamID::oscTableProperty (i), bank.sourceIdAt (oscTableIndex[i]), nullptr);
    state.setProperty (ParamID::arpCustomPatternProperty,
                       juce::JSON::toString (getCustomArpPattern().toJson(), true), nullptr);
    state.setProperty ("preset_name", currentPresetName, nullptr);
    return state;
}

void WaveForgeProcessor::applyStateTree (const juce::ValueTree& state)
{
    apvts.replaceState (state);
    for (int i = 0; i < 2; ++i)
        setOscTableBySourceId (i, state.getProperty (ParamID::oscTableProperty (i)).toString());

    {
        wf::ArpPattern pattern;
        juce::String error;
        const auto json = state.getProperty (ParamID::arpCustomPatternProperty).toString();
        if (json.isNotEmpty() && wf::ArpPattern::fromJson (juce::JSON::parse (json), pattern, error))
            setCustomArpPattern (pattern, false);
    }

    const auto name = state.getProperty ("preset_name").toString();
    currentPresetName = name.isNotEmpty() ? name : juce::String ("Init");
    sendChangeMessage();
}

void WaveForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = buildStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void WaveForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            applyStateTree (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
void WaveForgeProcessor::applyFactoryPreset (const juce::String& name)
{
    const auto* preset = PresetManager::findFactory (name);
    if (preset == nullptr)
        return;

    PresetManager::applyFactory (apvts, *preset);
    setOscTableBySourceId (0, preset->tableA);
    setOscTableBySourceId (1, preset->tableB);
    currentPresetName = name;
    sendChangeMessage();
}

bool WaveForgeProcessor::savePresetToFile (const juce::File& file, juce::String& error)
{
    currentPresetName = file.getFileNameWithoutExtension();
    if (! PresetManager::saveToFile (buildStateTree(), file, error))
        return false;
    sendChangeMessage();
    return true;
}

bool WaveForgeProcessor::loadPresetFromFile (const juce::File& file, juce::String& error)
{
    juce::ValueTree state;
    if (! PresetManager::loadFromFile (file, state, error))
        return false;

    if (! state.hasType (apvts.state.getType()))
    {
        error = "That file is not a WaveForge preset.";
        return false;
    }

    applyStateTree (state);
    currentPresetName = file.getFileNameWithoutExtension();
    sendChangeMessage();
    return true;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WaveForgeProcessor();
}
