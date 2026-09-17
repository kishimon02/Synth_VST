#pragma once

#include "Controls.h"
#include "PianoRoll.h"
#include "../PluginProcessor.h"
#include "../ai/ChordLibrary.h"
#include "../ai/MidiExport.h"

namespace ui
{

// Chord progression browser: pick a key, then a category, then a progression.
// The result is shown as a piano roll that can be auditioned chord by chord,
// dragged into the DAW as a .mid, or handed to the AI page as a request.
class ChordPage final : public juce::Component,
                       private juce::Timer     // runs only while the preview plays
{
public:
    explicit ChordPage (WaveForgeProcessor&);

    // Set by the editor: opens the AI tab with this text in the request box.
    std::function<void (const juce::String&)> onSendToAi;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    class DragHandle final : public juce::TextButton
    {
    public:
        explicit DragHandle (ChordPage& p) : juce::TextButton ("Drag to DAW"), page (p) {}
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    private:
        ChordPage& page;
        bool dragging = false;
    };

    //==========================================================================
    void timerCallback() override;
    void startPreview (const std::vector<ai::Note>& toPlay, double offsetBeats);
    double playheadBeat() const;

    void refreshCategories();
    void refreshProgressions (const juce::String& keepName = {});
    void applySelectedProgression();
    void rebuild();
    void layoutChordButtons();
    void auditionChord (int index);
    void saveAsUserPreset();
    void deleteSelectedUserPreset();
    void saveMidiFile();
    juce::File ensureMidiFile();
    juce::String requestTextForAi() const;
    ai::ChordLibrary::Options options() const;
    int keyIndex() const;

    WaveForgeProcessor& processor;

    juce::Label keyLabel, categoryLabel, progressionLabel, octaveLabel, repeatLabel, degreesLabel, statusLabel;
    juce::ComboBox keyBox, categoryBox, progressionBox, octaveBox, repeatBox;
    juce::ToggleButton bassToggle { "Bass" };
    juce::OwnedArray<juce::TextButton> chordButtons;
    juce::TextEditor degreesEditor;
    juce::TextButton applyButton { "Apply" }, savePresetButton { "Save..." }, deleteButton { "Delete" },
                     auditionButton { "Audition" }, stopButton { "Stop" }, saveMidiButton { "Save .mid..." },
                     sendToAiButton;
    DragHandle dragHandle { *this };

    int previewId = 0;                 // which preview sequence this page started
    double previewOffsetBeats = 0.0;   // where that sequence sits in the roll

    std::vector<ai::ChordProgression> visible;     // progressions of the current category and mode
    std::vector<ai::ChordSymbol> chords;
    std::vector<ai::Note> notes;
    juce::String parseError, currentName;
    juce::Rectangle<int> chordRow, rollArea;
    juce::File exported;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordPage)
};

} // namespace ui
