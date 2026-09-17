#pragma once

#include "Controls.h"
#include "../PluginProcessor.h"
#include "../dsp/WaveEditOps.h"

namespace ui
{

// Wavetable editor: draw the waveform or its harmonics per frame, manage
// frames (add / duplicate / delete / morph), start from basic shapes, and
// apply the result to an oscillator. "Apply" writes a Serum-compatible .wav
// into %APPDATA%/WaveForge/Wavetables and loads it, so the table is also
// reusable elsewhere and survives in presets by path.
class WavetableEditor final : public juce::Component
{
public:
    WavetableEditor (WaveForgeProcessor&, int osc);

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;   // Ctrl+Z / Ctrl+Y

    // Call before changing the model. Single-frame edits snapshot only that
    // frame; structural edits (add / delete / morph) snapshot the table.
    void pushUndo (bool wholeTable);
    void undo();
    void redo();

private:
    static constexpr int N = wf::Wavetable::frameSize;
    static constexpr size_t maxUndo = 64;

    struct Snapshot
    {
        int frameIndex = -1;          // -1 = whole table
        std::vector<float> data;
        int numFrames = 1, current = 0;
    };
    Snapshot captureState (bool wholeTable) const;
    void restoreState (Snapshot& s);   // swaps, so the same snapshot becomes the redo entry
    std::vector<Snapshot> undoStack, redoStack;

    // --- model
    float* frame (int index) noexcept { return frames.data() + (size_t) index * N; }
    const float* frame (int index) const noexcept { return frames.data() + (size_t) index * N; }
    void selectFrame (int index);
    void frameEdited();          // waveform changed -> harmonics + views
    void harmonicsEdited();      // harmonics changed -> waveform + views
    void refreshLabels();
    void applyToOsc();
    void saveAs();

    // --- child views
    class WaveCanvas final : public juce::Component
    {
    public:
        explicit WaveCanvas (WavetableEditor& e) : editor (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
    private:
        void drawTo (juce::Point<float> p, bool first);
        WavetableEditor& editor;
        int lastIndex = -1; float lastValue = 0.0f;
        juce::Point<float> lineStart;
    };

    class HarmonicCanvas final : public juce::Component
    {
    public:
        explicit HarmonicCanvas (WavetableEditor& e) : editor (e) {}
        static constexpr int shown = 64;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent&) override;
    private:
        WavetableEditor& editor;
    };

    class FrameStrip final : public juce::Component
    {
    public:
        explicit FrameStrip (WavetableEditor& e) : editor (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
        void mouseDrag (const juce::MouseEvent&) override;
    private:
        WavetableEditor& editor;
    };

    WaveForgeProcessor& processor;
    const int osc;
    std::vector<float> frames;
    int numFrames = 1, current = 0;
    wf::WaveEditOps::Harmonics harmonics;

    juce::Label title, frameLabel, hint, status;
    juce::TextEditor nameEditor;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, addButton { "Add" }, dupButton { "Dup" },
                     delButton { "Del" }, morphButton { "Morph 1->N" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::TextButton sineButton { "Sine" }, triButton { "Tri" }, sawButton { "Saw" }, squareButton { "Square" },
                     pulseButton { "Pulse" };
    juce::TextButton normButton { "Normalize" }, invertButton { "Invert" }, reverseButton { "Reverse" },
                     dcButton { "Remove DC" }, smoothButton { "Smooth" }, clearButton { "Clear" };
    juce::TextButton applyButton, saveButton { "Save .wav..." }, closeButton { "Close" };
    WaveCanvas wave;
    HarmonicCanvas bars;
    FrameStrip strip;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableEditor)
};

} // namespace ui
