#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Panels.h"
#include "ui/Scope.h"
#include "ui/ScopePage.h"
#include "ui/WavetableEditor.h"
#include "ui/ai/AiPage.h"

// Main editor: header (title, preset bar), three tabbed pages (OSC / MOD /
// FX), and a footer with the output scope and the on-screen keyboard.
class WaveForgeEditor final : public juce::AudioProcessorEditor,
                              public juce::DragAndDropContainer,   // external .mid drag from the AI page
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
    void refreshPresetMenu();
    void savePresetAs();
    void loadPresetFile();
    void openWaveEditor (int osc);
    void closeWaveEditor();

    // Pages hosted by the tabbed component.
    struct OscPage final : public juce::Component
    {
        explicit OscPage (WaveForgeProcessor&);
        void resized() override;
        ui::OscPanel oscA, oscB;
        ui::SubNoisePanel subNoise;
        ui::FilterPanel filter;
        ui::EnvPanel env1, env2;
    };

    struct ModPage final : public juce::Component
    {
        explicit ModPage (WaveForgeProcessor&);
        void resized() override;
        ui::LfoPanel lfo1, lfo2;
        ui::ArpPanel arp;
        ui::ModMatrixPanel matrix;
    };

    WaveForgeProcessor& processor;
    ui::WaveForgeLookAndFeel lookAndFeel;

    juce::Label title, status;
    ui::GlobalPanel  global;
    juce::Label      presetLabel { {}, "Preset" };
    juce::ComboBox   presetBox;
    juce::TextButton savePresetButton { "Save As..." };
    juce::TextButton loadPresetButton { "Load..." };

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    OscPage oscPage;
    ModPage modPage;
    ui::FxPage fxPage;
    ui::ScopePage scopePage;
    ui::AiPage aiPage;

    // The wavetable editor replaces the tabs while open (the OSC page's GL
    // views live in native child windows, so nothing can be drawn over them).
    std::unique_ptr<ui::WavetableEditor> waveEditor;

    ui::Scope scope;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveForgeEditor)
};
