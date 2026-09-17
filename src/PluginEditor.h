#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "ui/WavetableView2D.h"

// Phase 1 interim editor: wavetable selection + 2D preview per oscillator,
// a generic parameter list for everything else, and the on-screen keyboard.
// The designed UI (tabs, knobs, 3D view) lands in Phase 4.
class WaveForgeEditor final : public juce::AudioProcessorEditor,
                              private juce::ChangeListener,
                              private juce::Timer
{
public:
    explicit WaveForgeEditor (WaveForgeProcessor&);
    ~WaveForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshTableCombos();
    void chooseFile (int osc);

    struct OscRow
    {
        juce::Label      label;
        juce::ComboBox   tableBox;
        juce::TextButton loadButton { "Load .wav" };
        WavetableView2D  view;
    };

    WaveForgeProcessor& processor;
    juce::Label title;
    OscRow oscRows[2];
    juce::Viewport paramViewport;
    juce::GenericAudioProcessorEditor genericParams;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveForgeEditor)
};
