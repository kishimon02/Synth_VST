#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Params.h"
#include "ParamRefs.h"
#include "WavetableBank.h"
#include "PresetManager.h"
#include "Diagnostics.h"
#include "dsp/SynthEngine.h"

class WaveForgeProcessor final : public juce::AudioProcessor,
                                 public juce::ChangeBroadcaster,   // fires when a wavetable assignment changes
                                 private juce::Timer
{
public:
    WaveForgeProcessor();
    ~WaveForgeProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Shared with the editor (message thread)
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    WavetableBank& getBank() noexcept { return bank; }

    int  getOscTableIndex (int osc) const noexcept { return oscTableIndex[osc]; }
    const wf::Wavetable* getOscTable (int osc) const noexcept { return oscTable[osc].load (std::memory_order_relaxed); }
    void setOscTable (int osc, int bankIndex);
    bool loadWavetableFile (int osc, const juce::File& file, juce::String& error);

    int getActiveVoiceCount() const noexcept { return activeVoices.load (std::memory_order_relaxed); }
    float getHostBpm() const noexcept { return hostBpm.load (std::memory_order_relaxed); }

    // Presets (message thread only)
    void applyFactoryPreset (const juce::String& name);
    bool savePresetToFile (const juce::File& file, juce::String& error);
    bool loadPresetFromFile (const juce::File& file, juce::String& error);
    const juce::String& getCurrentPresetName() const noexcept { return currentPresetName; }

private:
    void timerCallback() override;
    void setOscTableBySourceId (int osc, const juce::String& sourceId);
    juce::ValueTree buildStateTree();
    void applyStateTree (const juce::ValueTree& state);

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    ParamRefs refs;
    WavetableBank bank;
    wf::SynthEngine engine;

    std::atomic<const wf::Wavetable*> oscTable[2] { nullptr, nullptr };
    int oscTableIndex[2] { 0, 0 };
    std::atomic<int> activeVoices { 0 };
    std::atomic<float> hostBpm { 120.0f };
    juce::String currentPresetName { "Init" };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain { 0.5f };
    double currentSampleRate = 44100.0;
    Diagnostics diag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveForgeProcessor)
};
