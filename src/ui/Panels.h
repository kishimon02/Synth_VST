#pragma once

#include "Controls.h"
#include "Wavetable3DView.h"
#include "FilterView.h"
#include "EnvView.h"
#include "../PluginProcessor.h"

// One panel per synth section. Each owns its parameter widgets; the editor
// only arranges panels on pages and forwards refresh / timer ticks.
namespace ui
{

class OscPanel final : public Panel
{
public:
    OscPanel (WaveForgeProcessor&, int index);
    void resized() override;
    void refresh();      // table list / selection changed
    void tick();         // 30 Hz: WT position -> 3D view

    std::function<void (int osc)> onEdit;   // "Edit" pressed

private:
    void chooseFile();
    void setViewMode (Wavetable3DView::Mode);

    WaveForgeProcessor& processor;
    const int index;
    ParamToggle enabled;
    juce::ComboBox tableBox;
    juce::TextButton loadButton { "Load" }, editButton { "Edit" };
    juce::TextButton mode3DButton { "3D" }, mode2DButton { "2D" }, modeSpecButton { "SP" };
    std::vector<int> tableIndices;   // combo item -> bank index
    Wavetable3DView view;
    ParamKnob wtPos, octave, semi, fine, level, pan;
    ParamKnob unisonVoices, unisonDetune, unisonBlend, unisonWidth, phase;
    ParamToggle randomPhase;
    std::unique_ptr<ParamCombo> warpMode;    // OSC A only
    std::unique_ptr<ParamKnob>  warpAmount;
    std::unique_ptr<juce::FileChooser> chooser;
};

class SubNoisePanel final : public Panel
{
public:
    explicit SubNoisePanel (Apvts&);
    void resized() override;
private:
    ParamToggle subOn; ParamCombo subShape; ParamKnob subOctave, subLevel; ParamToggle subDirect;
    ParamToggle noiseOn; ParamCombo noiseType; ParamKnob noiseLevel;
};

class FilterPanel final : public Panel
{
public:
    explicit FilterPanel (Apvts&);
    void resized() override;
private:
    void setViewMode (FilterView::Mode);
    ParamToggle enabled; ParamCombo type; ParamKnob cutoff, resonance, drive, keyTrack, env2Amount;
    ParamToggle routeA, routeB, routeSub, routeNoise;
    FilterView view;
    juce::TextButton mode3DButton { "3D" }, mode2DButton { "2D" };
};

class EnvPanel final : public Panel
{
public:
    EnvPanel (Apvts&, int index, const wf::EnvDisplay& live);
    void resized() override;
private:
    EnvView view;
    ParamKnob attack, decay, sustain, release;
};

// Master / voices / bend range as a frameless strip for the editor header.
class GlobalPanel final : public juce::Component
{
public:
    explicit GlobalPanel (Apvts&);
    void resized() override;
private:
    ParamKnob master, polyphony, bendRange;
};

class LfoPanel final : public Panel
{
public:
    LfoPanel (Apvts&, int index);
    void resized() override;
private:
    ParamCombo shape; ParamToggle tempoSync, retrigger, unipolar;
    ParamKnob rate; ParamCombo division; ParamKnob phase;
};

class ModMatrixPanel final : public Panel
{
public:
    explicit ModMatrixPanel (Apvts&);
    void resized() override;
private:
    struct Row
    {
        std::unique_ptr<ParamToggle> enabled;
        juce::ComboBox source, dest;
        juce::Slider amount;
        std::unique_ptr<Apvts::ComboBoxAttachment> sourceAtt, destAtt;
        std::unique_ptr<Apvts::SliderAttachment> amountAtt;
    };
    std::vector<std::unique_ptr<Row>> rows;
    juce::Label header;
};

// Arpeggiator: controls plus an editable step view. Editing a step (or the
// length) writes the Custom pattern into the processor and selects it.
class ArpPanel final : public Panel, private juce::Timer
{
public:
    explicit ArpPanel (WaveForgeProcessor&);
    void resized() override;

private:
    class StepView final : public juce::Component
    {
    public:
        explicit StepView (ArpPanel& p) : panel (p) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
    private:
        int stepAt (float x) const;
        ArpPanel& panel;
        int dragStep = -1;
        bool dragged = false;
        wf::ArpPattern editing;
    };

    void timerCallback() override;
    void changeLength (int delta);
    void loadPattern();
    void savePattern();
    wf::ArpPattern activePatternCopy() const;
    void commit (wf::ArpPattern pattern);

    WaveForgeProcessor& processor;
    ParamToggle enabled; ParamCombo mode, division; ParamKnob octaves, gate, swing; ParamToggle latch;
    ParamCombo pattern;
    juce::TextButton shorterButton { "-" }, longerButton { "+" }, loadButton { "Load..." }, saveButton { "Save..." };
    juce::Label lengthLabel;
    StepView steps;
    std::unique_ptr<juce::FileChooser> chooser;
    const wf::ArpPattern* lastShown = nullptr;
    int lastStep = -1;
};

// One FX unit: title, On toggle, then a row of controls.
class FxUnitPanel final : public Panel
{
public:
    FxUnitPanel (Apvts&, const juce::String& title, const juce::String& enableId,
                 std::vector<std::unique_ptr<juce::Component>> controlsIn);
    void resized() override;
private:
    ParamToggle enabled;
    std::vector<std::unique_ptr<juce::Component>> controls;
};

class FxPage final : public juce::Component
{
public:
    explicit FxPage (Apvts&);
    void resized() override;
private:
    std::vector<std::unique_ptr<FxUnitPanel>> units;
};

} // namespace ui
