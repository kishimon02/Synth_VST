#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
WaveForgeProcessor::WaveForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WaveForgeState", Params::createLayout())
{
    refs.bind (apvts);

    // Default tables: OSC A = Basic Shapes, OSC B = Sine
    setOscTable (0, juce::jmax (0, bank.indexOfSourceId (wf::WavetableLoader::builtinSourceId ("Basic Shapes"))));
    setOscTable (1, juce::jmax (0, bank.indexOfSourceId (wf::WavetableLoader::builtinSourceId ("Sine"))));
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

bool WaveForgeProcessor::loadWavetableFile (int osc, const juce::File& file, juce::String& error)
{
    const int index = bank.addFile (file, error);
    if (index < 0)
        return false;
    setOscTable (osc, index);
    return true;
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
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;
#if WAVEFORGE_DIAGNOSTICS
    startTimer (2000);
#endif
    engine.prepare (sampleRate);
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (refs.masterVolume->load(), -60.0f));
}

void WaveForgeProcessor::releaseResources() {}

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

    const auto params = refs.snapshot (oscTable[0].load (std::memory_order_acquire),
                                       oscTable[1].load (std::memory_order_acquire));
    engine.process (buffer, midi, params);
    activeVoices.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);

    masterGain.setTargetValue (params.global.masterGain);
    masterGain.applyGain (buffer, numSamples);

    diag.blockEnd (buffer, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* WaveForgeProcessor::createEditor()
{
    return new WaveForgeEditor (*this);
}

void WaveForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int i = 0; i < 2; ++i)
        state.setProperty (ParamID::oscTableProperty (i), bank.sourceIdAt (oscTableIndex[i]), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WaveForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto state = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (state);
            for (int i = 0; i < 2; ++i)
                setOscTableBySourceId (i, state.getProperty (ParamID::oscTableProperty (i)).toString());
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WaveForgeProcessor();
}
