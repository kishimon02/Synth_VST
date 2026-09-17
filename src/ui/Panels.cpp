#include "Panels.h"

namespace ui
{

namespace
{
    constexpr int knobRow = 72;
}

//==============================================================================
OscPanel::OscPanel (WaveForgeProcessor& p, int idx)
    : Panel (idx == 0 ? "OSC A" : "OSC B", idx == 0 ? colours::accent : colours::accentB),
      processor (p), index (idx),
      enabled (p.getAPVTS(), ParamID::osc (idx).enabled, "On"),
      view (accent),
      wtPos        (p.getAPVTS(), ParamID::osc (idx).wtPos,        "WT Pos",  accent),
      octave       (p.getAPVTS(), ParamID::osc (idx).octave,       "Octave",  accent),
      semi         (p.getAPVTS(), ParamID::osc (idx).semi,         "Semi",    accent),
      fine         (p.getAPVTS(), ParamID::osc (idx).fine,         "Fine",    accent),
      level        (p.getAPVTS(), ParamID::osc (idx).level,        "Level",   accent),
      pan          (p.getAPVTS(), ParamID::osc (idx).pan,          "Pan",     accent),
      unisonVoices (p.getAPVTS(), ParamID::osc (idx).unisonVoices, "Unison",  accent),
      unisonDetune (p.getAPVTS(), ParamID::osc (idx).unisonDetune, "Detune",  accent),
      unisonBlend  (p.getAPVTS(), ParamID::osc (idx).unisonBlend,  "Blend",   accent),
      unisonWidth  (p.getAPVTS(), ParamID::osc (idx).unisonWidth,  "Width",   accent),
      phase        (p.getAPVTS(), ParamID::osc (idx).phase,        "Phase",   accent),
      randomPhase  (p.getAPVTS(), ParamID::osc (idx).randomPhase,  "Rnd Phase")
{
    addAndMakeVisible (enabled);

    tableBox.onChange = [this]
    {
        const int item = tableBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (item, (int) tableIndices.size()))
        {
            const int bankIndex = tableIndices[(size_t) item];
            if (bankIndex != processor.getOscTableIndex (index))
                processor.setOscTable (index, bankIndex);
        }
    };
    addAndMakeVisible (tableBox);

    loadButton.onClick = [this] { chooseFile(); };
    editButton.onClick = [this] { if (onEdit) onEdit (index); };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (editButton);

    mode3DButton.onClick   = [this] { setViewMode (Wavetable3DView::mode3D); };
    mode2DButton.onClick   = [this] { setViewMode (Wavetable3DView::mode2D); };
    modeSpecButton.onClick = [this] { setViewMode (Wavetable3DView::modeSpectrum); };
    for (auto* b : { &mode3DButton, &mode2DButton, &modeSpecButton })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }
    setViewMode (Wavetable3DView::mode3D);
    addAndMakeVisible (view);

    for (auto* k : { &wtPos, &octave, &semi, &fine, &level, &pan,
                     &unisonVoices, &unisonDetune, &unisonBlend, &unisonWidth, &phase })
        addAndMakeVisible (k);
    addAndMakeVisible (randomPhase);

    if (index == 0)
    {
        warpMode = std::make_unique<ParamCombo> (p.getAPVTS(), ParamID::oscAWarpMode, "Warp");
        warpAmount = std::make_unique<ParamKnob> (p.getAPVTS(), ParamID::oscAWarpAmount, "Warp Amt", accent);
        addAndMakeVisible (*warpMode);
        addAndMakeVisible (*warpAmount);
    }

    refresh();
}

void OscPanel::setViewMode (Wavetable3DView::Mode m)
{
    view.setMode (m);
    mode3DButton.setToggleState (m == Wavetable3DView::mode3D, juce::dontSendNotification);
    mode2DButton.setToggleState (m == Wavetable3DView::mode2D, juce::dontSendNotification);
    modeSpecButton.setToggleState (m == Wavetable3DView::modeSpectrum, juce::dontSendNotification);
}

void OscPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    auto header = r.removeFromTop (titleHeight + 4);
    title.setBounds (header.removeFromLeft (50));
    enabled.setBounds (header.removeFromLeft (40));
    loadButton.setBounds (header.removeFromRight (50).reduced (2, 2));
    editButton.setBounds (header.removeFromRight (50).reduced (2, 2));
    header.removeFromRight (4);
    modeSpecButton.setBounds (header.removeFromRight (30).reduced (1, 2));
    mode2DButton.setBounds (header.removeFromRight (30).reduced (1, 2));
    mode3DButton.setBounds (header.removeFromRight (30).reduced (1, 2));
    tableBox.setBounds (header.reduced (4, 2));

    auto rows = r.removeFromBottom (knobRow * 2 + 4);
    row (rows.removeFromTop (knobRow), { &wtPos, &octave, &semi, &fine, &level, &pan, &phase });
    rows.removeFromTop (4);
    if (warpMode != nullptr)
        row (rows, { &unisonVoices, &unisonDetune, &unisonBlend, &unisonWidth, &randomPhase, warpMode.get(), warpAmount.get() },
             { 1.0f, 1.0f, 1.0f, 1.0f, 1.2f, 1.6f, 1.0f });
    else
        row (rows, { &unisonVoices, &unisonDetune, &unisonBlend, &unisonWidth, &randomPhase },
             { 1.0f, 1.0f, 1.0f, 1.0f, 1.2f });

    r.removeFromTop (4);
    r.removeFromBottom (6);
    view.setBounds (r);
}

void OscPanel::refresh()
{
    auto& bank = processor.getBank();
    tableBox.clear (juce::dontSendNotification);
    tableIndices = bank.visibleIndices();
    int item = 1;
    for (int bankIndex : tableIndices)
        tableBox.addItem (bank.nameAt (bankIndex), item++);

    const int currentBank = processor.getOscTableIndex (index);
    for (size_t i = 0; i < tableIndices.size(); ++i)
        if (tableIndices[i] == currentBank)
            tableBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
    view.setTable (processor.getOscTable (index));
}

void OscPanel::tick()
{
    view.setPosition (wtPos.slider.getValue() > 0.0 ? (float) wtPos.slider.getValue() : 0.0f);
}

void OscPanel::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load wavetable (.wav, 2048 samples per frame)",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                   "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (! file.existsAsFile())
                                  return;
                              juce::String error;
                              if (! processor.loadWavetableFile (index, file, error))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Wavetable", error);
                          });
}

//==============================================================================
SubNoisePanel::SubNoisePanel (Apvts& a)
    : Panel ("SUB / NOISE"),
      subOn (a, ParamID::subEnabled, "Sub"), subShape (a, ParamID::subShape, "Shape"),
      subOctave (a, ParamID::subOctave, "Octave"), subLevel (a, ParamID::subLevel, "Level"),
      subDirect (a, ParamID::subDirect, "Direct"),
      noiseOn (a, ParamID::noiseEnabled, "Noise"), noiseType (a, ParamID::noiseType, "Type"),
      noiseLevel (a, ParamID::noiseLevel, "Level")
{
    for (auto* c : std::initializer_list<juce::Component*> { &subOn, &subShape, &subOctave, &subLevel, &subDirect,
                                                            &noiseOn, &noiseType, &noiseLevel })
        addAndMakeVisible (c);
}

void SubNoisePanel::resized()
{
    auto r = body();
    row (r.removeFromTop (knobRow), { &subOn, &subShape, &subOctave, &subLevel }, { 0.8f, 1.7f, 1.0f, 1.0f });
    r.removeFromTop (4);
    row (r.removeFromTop (knobRow), { &noiseOn, &noiseType, &noiseLevel, &subDirect }, { 0.8f, 1.7f, 1.0f, 1.0f });
}

//==============================================================================
FilterPanel::FilterPanel (Apvts& a)
    : Panel ("FILTER"),
      enabled (a, ParamID::filterEnabled, "On"), type (a, ParamID::filterType, "Type"),
      cutoff (a, ParamID::filterCutoff, "Cutoff"), resonance (a, ParamID::filterResonance, "Reso"),
      drive (a, ParamID::filterDrive, "Drive"), keyTrack (a, ParamID::filterKeyTrack, "Key Trk"),
      env2Amount (a, ParamID::filterEnv2Amount, "Env2"),
      routeA (a, ParamID::filterRouteA, "A"), routeB (a, ParamID::filterRouteB, "B"),
      routeSub (a, ParamID::filterRouteSub, "Sub"), routeNoise (a, ParamID::filterRouteNoise, "Noise"),
      view (a)
{
    for (auto* c : std::initializer_list<juce::Component*> { &enabled, &type, &cutoff, &resonance, &drive, &keyTrack,
                                                            &env2Amount, &routeA, &routeB, &routeSub, &routeNoise, &view })
        addAndMakeVisible (c);

    mode3DButton.onClick = [this] { setViewMode (FilterView::mode3D); };
    mode2DButton.onClick = [this] { setViewMode (FilterView::mode2D); };
    addAndMakeVisible (mode3DButton);
    addAndMakeVisible (mode2DButton);
    setViewMode (FilterView::mode2D);
}

void FilterPanel::setViewMode (FilterView::Mode m)
{
    view.setMode (m);
    mode3DButton.setToggleState (m == FilterView::mode3D, juce::dontSendNotification);
    mode2DButton.setToggleState (m == FilterView::mode2D, juce::dontSendNotification);
}

void FilterPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    auto header = r.removeFromTop (titleHeight);
    title.setBounds (header.removeFromLeft (70));
    mode2DButton.setBounds (header.removeFromRight (30).reduced (1, 2));
    mode3DButton.setBounds (header.removeFromRight (30).reduced (1, 2));

    auto viewArea = r.removeFromRight (juce::jmax (150, r.getWidth() * 38 / 100));
    r.removeFromRight (6);
    view.setBounds (viewArea.withTrimmedBottom (6).withTrimmedTop (2));

    row (r.removeFromTop (knobRow), { &enabled, &type, &cutoff, &resonance, &drive }, { 0.7f, 1.3f, 1.0f, 1.0f, 1.0f });
    r.removeFromTop (4);
    row (r.removeFromTop (knobRow), { &keyTrack, &env2Amount, &routeA, &routeB, &routeSub, &routeNoise },
         { 1.0f, 1.0f, 0.6f, 0.6f, 0.8f, 1.0f });
}

//==============================================================================
EnvPanel::EnvPanel (Apvts& a, int index, const wf::EnvDisplay& live)
    : Panel (index == 0 ? "ENV 1  (amp)" : "ENV 2  (mod)", colours::accentMod),
      view (a, index, live, colours::accentMod),
      attack  (a, ParamID::env (index).attack,  "Attack",  colours::accentMod),
      decay   (a, ParamID::env (index).decay,   "Decay",   colours::accentMod),
      sustain (a, ParamID::env (index).sustain, "Sustain", colours::accentMod),
      release (a, ParamID::env (index).release, "Release", colours::accentMod)
{
    addAndMakeVisible (view);
    for (auto* c : { &attack, &decay, &sustain, &release })
        addAndMakeVisible (c);
}

void EnvPanel::resized()
{
    auto r = body();
    row (r.removeFromBottom (knobRow), { &attack, &decay, &sustain, &release });
    r.removeFromBottom (4);
    view.setBounds (r);
}

//==============================================================================
GlobalPanel::GlobalPanel (Apvts& a)
    : master (a, ParamID::masterVolume, "Master"), polyphony (a, ParamID::polyphony, "Voices"),
      bendRange (a, ParamID::pitchBendRange, "Bend")
{
    for (auto* c : { &master, &polyphony, &bendRange })
        addAndMakeVisible (c);
}

void GlobalPanel::resized()
{
    Panel::row (getLocalBounds(), { &master, &polyphony, &bendRange });
}

//==============================================================================
LfoPanel::LfoPanel (Apvts& a, int index)
    : Panel ("LFO " + juce::String (index + 1), colours::accentMod),
      shape (a, ParamID::lfo (index).shape, "Shape"),
      tempoSync (a, ParamID::lfo (index).tempoSync, "Sync"),
      retrigger (a, ParamID::lfo (index).retrigger, "Retrig"),
      unipolar (a, ParamID::lfo (index).unipolar, "Unipolar"),
      rate (a, ParamID::lfo (index).rate, "Rate", colours::accentMod),
      division (a, ParamID::lfo (index).division, "Division"),
      phase (a, ParamID::lfo (index).phase, "Phase", colours::accentMod)
{
    for (auto* c : std::initializer_list<juce::Component*> { &shape, &tempoSync, &retrigger, &unipolar, &rate, &division, &phase })
        addAndMakeVisible (c);
}

void LfoPanel::resized()
{
    auto r = body();
    row (r.removeFromTop (knobRow), { &shape, &rate, &division, &phase });
    r.removeFromTop (2);
    row (r.removeFromTop (28), { &tempoSync, &retrigger, &unipolar });
}

//==============================================================================
ModMatrixPanel::ModMatrixPanel (Apvts& a) : Panel ("MOD MATRIX", colours::accentMod)
{
    header.setText ("        Source                          Destination                     Amount", juce::dontSendNotification);
    header.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (header);

    for (int i = 0; i < wf::numModSlots; ++i)
    {
        auto ids = ParamID::modSlot (i);
        auto row = std::make_unique<Row>();
        row->enabled = std::make_unique<ParamToggle> (a, ids.enabled, juce::String (i + 1));
        addAndMakeVisible (*row->enabled);

        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (a.getParameter (ids.source)))
            row->source.addItemList (c->choices, 1);
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (a.getParameter (ids.dest)))
            row->dest.addItemList (c->choices, 1);
        addAndMakeVisible (row->source);
        addAndMakeVisible (row->dest);

        row->amount.setSliderStyle (juce::Slider::LinearHorizontal);
        row->amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        row->amount.setColour (juce::Slider::trackColourId, colours::accentMod.withAlpha (0.7f));
        addAndMakeVisible (row->amount);

        row->sourceAtt = std::make_unique<Apvts::ComboBoxAttachment> (a, ids.source, row->source);
        row->destAtt   = std::make_unique<Apvts::ComboBoxAttachment> (a, ids.dest, row->dest);
        row->amountAtt = std::make_unique<Apvts::SliderAttachment> (a, ids.amount, row->amount);
        rows.push_back (std::move (row));
    }
}

void ModMatrixPanel::resized()
{
    auto r = body();
    header.setBounds (r.removeFromTop (16));
    const int rowH = juce::jmin (28, r.getHeight() / (int) rows.size());
    for (auto& row : rows)
    {
        auto line = r.removeFromTop (rowH).reduced (0, 2);
        row->enabled->setBounds (line.removeFromLeft (44));
        row->source.setBounds (line.removeFromLeft (150).reduced (2, 0));
        row->dest.setBounds (line.removeFromLeft (170).reduced (2, 0));
        row->amount.setBounds (line);
    }
}

//==============================================================================
FxUnitPanel::FxUnitPanel (Apvts& a, const juce::String& titleText, const juce::String& enableId,
                          std::vector<std::unique_ptr<juce::Component>> controlsIn)
    : Panel (titleText, colours::accentFx),
      enabled (a, enableId, "On"),
      controls (std::move (controlsIn))
{
    addAndMakeVisible (enabled);
    for (auto& c : controls)
        addAndMakeVisible (*c);
}

void FxUnitPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    auto left = r.removeFromLeft (96);
    title.setBounds (left.removeFromTop (titleHeight));
    enabled.setBounds (left.removeFromTop (24));

    std::vector<juce::Component*> items;
    for (auto& c : controls)
        items.push_back (c.get());
    row (r.withHeight (knobRow).withY (r.getY() + (r.getHeight() - knobRow) / 2), items);
}

//==============================================================================
FxPage::FxPage (Apvts& a)
{
    namespace F = ParamID::Fx;
    auto knob   = [&a] (const char* id, const char* text) -> std::unique_ptr<juce::Component>
                  { return std::make_unique<ParamKnob> (a, id, text, colours::accentFx); };
    auto combo  = [&a] (const char* id, const char* text) -> std::unique_ptr<juce::Component>
                  { return std::make_unique<ParamCombo> (a, id, text); };
    auto toggle = [&a] (const char* id, const char* text) -> std::unique_ptr<juce::Component>
                  { return std::make_unique<ParamToggle> (a, id, text); };

    auto add = [this, &a] (const char* title, const char* enableId, std::vector<std::unique_ptr<juce::Component>> c)
    {
        units.push_back (std::make_unique<FxUnitPanel> (a, title, enableId, std::move (c)));
        addAndMakeVisible (*units.back());
    };

    {
        std::vector<std::unique_ptr<juce::Component>> c;
        c.push_back (combo (F::distMode, "Mode"));       c.push_back (knob (F::distDrive, "Drive"));
        c.push_back (toggle (F::distOversample, "2x OS")); c.push_back (knob (F::distOutput, "Output"));
        c.push_back (knob (F::distMix, "Mix"));
        add ("DISTORTION", F::distEnabled, std::move (c));
    }
    {
        std::vector<std::unique_ptr<juce::Component>> c;
        c.push_back (knob (F::eqLowGain, "Low Gain"));   c.push_back (knob (F::eqLowFreq, "Low Freq"));
        c.push_back (knob (F::eqMidGain, "Mid Gain"));   c.push_back (knob (F::eqMidFreq, "Mid Freq"));
        c.push_back (knob (F::eqMidQ, "Mid Q"));
        c.push_back (knob (F::eqHighGain, "High Gain")); c.push_back (knob (F::eqHighFreq, "High Freq"));
        add ("EQ", F::eqEnabled, std::move (c));
    }
    {
        std::vector<std::unique_ptr<juce::Component>> c;
        c.push_back (knob (F::chorusRate, "Rate"));       c.push_back (knob (F::chorusDepth, "Depth"));
        c.push_back (knob (F::chorusFeedback, "Feedback")); c.push_back (knob (F::chorusDelay, "Delay"));
        c.push_back (knob (F::chorusMix, "Mix"));
        add ("CHORUS", F::chorusEnabled, std::move (c));
    }
    {
        std::vector<std::unique_ptr<juce::Component>> c;
        c.push_back (toggle (F::delayTempoSync, "Sync"));  c.push_back (knob (F::delayTime, "Time"));
        c.push_back (combo (F::delayDivision, "Division")); c.push_back (knob (F::delayFeedback, "Feedback"));
        c.push_back (knob (F::delayLowpass, "Lowpass"));   c.push_back (toggle (F::delayPingPong, "Ping Pong"));
        c.push_back (knob (F::delayMix, "Mix"));
        add ("DELAY", F::delayEnabled, std::move (c));
    }
    {
        std::vector<std::unique_ptr<juce::Component>> c;
        c.push_back (knob (F::reverbSize, "Size"));       c.push_back (knob (F::reverbDamping, "Damping"));
        c.push_back (knob (F::reverbWidth, "Width"));     c.push_back (knob (F::reverbPredelay, "Predelay"));
        c.push_back (knob (F::reverbMix, "Mix"));
        add ("REVERB", F::reverbEnabled, std::move (c));
    }
}

void FxPage::resized()
{
    auto r = getLocalBounds();
    const int h = (r.getHeight() - 6 * ((int) units.size() - 1)) / (int) units.size();
    for (auto& u : units)
    {
        u->setBounds (r.removeFromTop (h));
        r.removeFromTop (6);
    }
}

} // namespace ui
