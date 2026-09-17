#pragma once

#include "Controls.h"
#include "Wavetable3DView.h"
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

private:
    void chooseFile();

    WaveForgeProcessor& processor;
    const int index;
    ParamToggle enabled;
    juce::ComboBox tableBox;
    juce::TextButton loadButton { "Load .wav" };
    Wavetable3DView view;
    ParamKnob wtPos, octave, semi, fine, level, pan;
    ParamKnob unisonVoices, unisonDetune, unisonBlend, unisonWidth, phase;
    ParamToggle randomPhase;
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
    ParamToggle enabled; ParamCombo type; ParamKnob cutoff, resonance, drive, keyTrack, env2Amount;
    ParamToggle routeA, routeB, routeSub, routeNoise;
};

class EnvPanel final : public Panel
{
public:
    EnvPanel (Apvts&, int index);
    void resized() override;
private:
    ParamKnob attack, decay, sustain, release;
};

class GlobalPanel final : public Panel
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
