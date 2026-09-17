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
    void refreshPresetMenu();
    void chooseFile (int osc);
    void savePresetAs();
    void loadPresetFile();

    struct OscRow
    {
        juce::Label      label;
        juce::ComboBox   tableBox;
        juce::TextButton loadButton { "Load .wav" };
        WavetableView2D  view;
    };

    WaveForgeProcessor& processor;
    juce::Label title;

    juce::Label      presetLabel { {}, "Preset" };
    juce::ComboBox   presetBox;
    juce::TextButton savePresetButton { "Save As..." };
    juce::TextButton loadPresetButton { "Load..." };

    // One column per FX unit: on/off + mix. The full parameter set is in the
    // generic list below; this strip is for quick A/B while auditioning.
    struct FxStrip
    {
        juce::Label        label;
        juce::ToggleButton enable { "On" };
        juce::Slider       mix;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    };

    OscRow oscRows[2];
    juce::Label fxLabel { {}, "FX" };
    FxStrip fxStrips[5];
    juce::Viewport paramViewport;
    juce::GenericAudioProcessorEditor genericParams;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveForgeEditor)
};
