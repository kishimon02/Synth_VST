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
ArpPanel::ArpPanel (WaveForgeProcessor& p)
    : Panel ("ARP", colours::accentMod), processor (p),
      enabled (p.getAPVTS(), ParamID::Arp::enabled, "On"),
      mode (p.getAPVTS(), ParamID::Arp::mode, "Mode"),
      division (p.getAPVTS(), ParamID::Arp::division, "Rate"),
      octaves (p.getAPVTS(), ParamID::Arp::octaves, "Octaves", colours::accentMod),
      gate (p.getAPVTS(), ParamID::Arp::gate, "Gate", colours::accentMod),
      swing (p.getAPVTS(), ParamID::Arp::swing, "Swing", colours::accentMod),
      latch (p.getAPVTS(), ParamID::Arp::latch, "Latch"),
      pattern (p.getAPVTS(), ParamID::Arp::pattern, "Pattern"),
      steps (*this)
{
    for (auto* c : std::initializer_list<juce::Component*> { &enabled, &mode, &division, &octaves, &gate, &swing, &latch, &pattern,
                                                            &shorterButton, &longerButton, &loadButton, &saveButton, &lengthLabel, &steps })
        addAndMakeVisible (c);
    shorterButton.onClick = [this] { changeLength (-1); };
    longerButton.onClick  = [this] { changeLength (+1); };
    loadButton.onClick = [this] { loadPattern(); };
    saveButton.onClick = [this] { savePattern(); };
    lengthLabel.setColour (juce::Label::textColourId, colours::textDim);
    lengthLabel.setJustificationType (juce::Justification::centred);
    startTimerHz (30);
}

void ArpPanel::resized()
{
    auto r = body();
    row (r.removeFromTop (knobRow), { &enabled, &mode, &division, &octaves, &gate, &swing, &latch, &pattern },
         { 0.6f, 1.3f, 1.1f, 1.0f, 1.0f, 1.0f, 0.8f, 1.5f });
    r.removeFromTop (4);
    auto tools = r.removeFromRight (150);
    steps.setBounds (r.withTrimmedRight (6));
    tools.removeFromTop (2);
    auto line1 = tools.removeFromTop (24);
    shorterButton.setBounds (line1.removeFromLeft (28).reduced (1));
    longerButton.setBounds (line1.removeFromRight (28).reduced (1));
    lengthLabel.setBounds (line1);
    tools.removeFromTop (4);
    auto line2 = tools.removeFromTop (24);
    loadButton.setBounds (line2.removeFromLeft (line2.getWidth() / 2).reduced (1));
    saveButton.setBounds (line2.reduced (1));
}

void ArpPanel::timerCallback()
{
    const auto* active = processor.getActiveArpPattern();
    const int step = processor.getArpCurrentStep();
    if (active != lastShown || step != lastStep)
    {
        lastShown = active; lastStep = step;
        if (active != nullptr)
            lengthLabel.setText (juce::String (active->steps.size()) + " steps", juce::dontSendNotification);
        steps.repaint();
    }
}

wf::ArpPattern ArpPanel::activePatternCopy() const
{
    if (const auto* p = processor.getActiveArpPattern())
        return *p;
    return wf::ArpPattern::builtins()[0];
}

void ArpPanel::commit (wf::ArpPattern p)
{
    p.name = "Custom";
    processor.setCustomArpPattern (p, true);
}

void ArpPanel::changeLength (int delta)
{
    auto p = activePatternCopy();
    const int n = juce::jlimit (1, wf::ArpPattern::maxSteps, (int) p.steps.size() + delta);
    if (n > (int) p.steps.size()) p.steps.push_back ({});
    else p.steps.resize ((size_t) n);
    commit (p);
}

void ArpPanel::loadPattern()
{
    const auto dir = WaveForgeProcessor::userArpPatternDirectory();
    dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Load arp pattern", dir, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (! file.existsAsFile()) return;
                              wf::ArpPattern p;
                              juce::String error;
                              if (wf::ArpPattern::fromJson (juce::JSON::parse (file.loadFileAsString()), p, error))
                                  commit (p);
                              else
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Arp pattern", error);
                          });
}

void ArpPanel::savePattern()
{
    const auto dir = WaveForgeProcessor::userArpPatternDirectory();
    dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save arp pattern", dir.getChildFile ("Pattern.json"), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File()) return;
                              if (file.getFileExtension().isEmpty()) file = file.withFileExtension (".json");
                              auto p = activePatternCopy();
                              p.name = file.getFileNameWithoutExtension();
                              file.replaceWithText (juce::JSON::toString (p.toJson()));
                          });
}

void ArpPanel::StepView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::widget);
    g.fillRoundedRectangle (r, 6.0f);
    const auto* p = panel.processor.getActiveArpPattern();
    if (p == nullptr || p->steps.empty())
        return;
    const int n = (int) p->steps.size();
    const int current = panel.processor.getArpCurrentStep();
    const float cw = (r.getWidth() - 8.0f) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const auto& s = p->steps[(size_t) i];
        auto cell = juce::Rectangle<float> (r.getX() + 4.0f + cw * (float) i, r.getY() + 4.0f, cw, r.getHeight() - 8.0f).reduced (1.0f, 0.0f);
        g.setColour (i == current ? colours::accentMod.withAlpha (0.25f) : colours::panel.withAlpha (0.5f));
        g.fillRoundedRectangle (cell, 3.0f);
        if (s.kind == wf::ArpStep::note)
        {
            const float h = cell.getHeight() * juce::jlimit (0.05f, 1.0f, s.velocity);
            auto bar = cell.withTop (cell.getBottom() - h).reduced (1.0f, 0.0f);
            bar.setWidth (bar.getWidth() * juce::jlimit (0.2f, 1.0f, s.gate));
            g.setColour (i == current ? juce::Colours::white : colours::accentMod);
            g.fillRoundedRectangle (bar, 2.0f);
            if (s.noteOffset != 0)
            {
                g.setColour (colours::text);
                g.setFont (juce::FontOptions (9.0f));
                g.drawText ((s.noteOffset > 0 ? "+" : "") + juce::String (s.noteOffset), cell.toNearestInt(), juce::Justification::centredTop);
            }
        }
        else
        {
            g.setColour (colours::textDim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (s.kind == wf::ArpStep::tie ? "tie" : "-", cell.toNearestInt(), juce::Justification::centred);
        }
    }
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

int ArpPanel::StepView::stepAt (float x) const
{
    const auto* p = panel.processor.getActiveArpPattern();
    if (p == nullptr || p->steps.empty()) return -1;
    const float cw = ((float) getWidth() - 8.0f) / (float) p->steps.size();
    return juce::jlimit (0, (int) p->steps.size() - 1, (int) ((x - 4.0f) / cw));
}

void ArpPanel::StepView::mouseDown (const juce::MouseEvent& e)
{
    editing = panel.activePatternCopy();
    dragStep = stepAt (e.position.x);
    dragged = false;
}

void ArpPanel::StepView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0 || ! juce::isPositiveAndBelow (dragStep, (int) editing.steps.size()))
        return;
    if (! dragged && std::abs (e.getDistanceFromDragStartY()) < 3) return;
    dragged = true;
    auto& s = editing.steps[(size_t) dragStep];
    s.kind = wf::ArpStep::note;
    s.velocity = juce::jlimit (0.05f, 1.0f, 1.0f - (e.position.y - 4.0f) / ((float) getHeight() - 8.0f));
    panel.commit (editing);
}

void ArpPanel::StepView::mouseUp (const juce::MouseEvent& e)
{
    if (dragStep < 0 || dragged) { dragStep = -1; return; }
    if (! juce::isPositiveAndBelow (dragStep, (int) editing.steps.size())) return;
    auto& s = editing.steps[(size_t) dragStep];
    if (e.mods.isRightButtonDown())
        s.noteOffset = s.noteOffset >= 3 ? 0 : s.noteOffset + 1;      // right click cycles the offset
    else
        s.kind = (s.kind + 1) % 3;                                    // note -> rest -> tie
    panel.commit (editing);
    dragStep = -1;
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
