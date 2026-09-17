#include "PluginEditor.h"

//==============================================================================
WaveForgeEditor::OscPage::OscPage (WaveForgeProcessor& p)
    : oscA (p, 0), oscB (p, 1),
      subNoise (p.getAPVTS()), filter (p.getAPVTS()),
      env1 (p.getAPVTS(), 0, p.getEnvDisplay()), env2 (p.getAPVTS(), 1, p.getEnvDisplay())
{
    for (auto* c : std::initializer_list<juce::Component*> { &oscA, &oscB, &subNoise, &filter, &env1, &env2 })
        addAndMakeVisible (c);
}

void WaveForgeEditor::OscPage::resized()
{
    auto r = getLocalBounds();
    const int gap = 6;

    auto bottom = r.removeFromBottom (216);
    r.removeFromBottom (gap);

    auto oscs = r;
    const int half = (oscs.getWidth() - gap) / 2;
    oscA.setBounds (oscs.removeFromLeft (half));
    oscs.removeFromLeft (gap);
    oscB.setBounds (oscs);

    // bottom row: sub/noise | filter (knobs + response view) | env1 | env2  (weights)
    const float weights[4] = { 0.95f, 2.2f, 1.05f, 1.05f };
    float total = 0.0f;
    for (float w : weights) total += w;
    const int usable = bottom.getWidth() - gap * 3;
    juce::Component* comps[4] = { &subNoise, &filter, &env1, &env2 };
    for (int i = 0; i < 4; ++i)
    {
        const int w = i == 3 ? bottom.getWidth() : (int) ((float) usable * weights[i] / total);
        comps[i]->setBounds (bottom.removeFromLeft (w));
        bottom.removeFromLeft (gap);
    }
}

WaveForgeEditor::ModPage::ModPage (WaveForgeProcessor& p)
    : lfo1 (p.getAPVTS(), 0), lfo2 (p.getAPVTS(), 1), arp (p), matrix (p.getAPVTS())
{
    for (auto* c : std::initializer_list<juce::Component*> { &lfo1, &lfo2, &arp, &matrix })
        addAndMakeVisible (c);
}

void WaveForgeEditor::ModPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (150);
    r.removeFromTop (6);
    const int half = (top.getWidth() - 6) / 2;
    lfo1.setBounds (top.removeFromLeft (half));
    top.removeFromLeft (6);
    lfo2.setBounds (top);
    arp.setBounds (r.removeFromTop (176));
    r.removeFromTop (6);
    matrix.setBounds (r);
}

//==============================================================================
WaveForgeEditor::WaveForgeEditor (WaveForgeProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      global (p.getAPVTS()),
      oscPage (p), modPage (p), fxPage (p.getAPVTS()),
      scopePage (p.getScopeBuffer(), [&p] { return p.getCurrentSampleRate(); }),
      chordPage (p),
      aiPage (p),
      scope (p.getScopeBuffer()),
      keyboard (p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    title.setText ("WaveForge", juce::dontSendNotification);
    title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, ui::colours::accent);
    addAndMakeVisible (title);

    status.setFont (juce::FontOptions (12.0f));
    status.setColour (juce::Label::textColourId, ui::colours::textDim);
    addAndMakeVisible (status);
    addAndMakeVisible (global);

    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetLabel.setColour (juce::Label::textColourId, ui::colours::textDim);
    addAndMakeVisible (presetLabel);

    presetBox.setTextWhenNothingSelected ("(custom)");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && name != processor.getCurrentPresetName())
            processor.applyFactoryPreset (name);
    };
    addAndMakeVisible (presetBox);

    savePresetButton.onClick = [this] { savePresetAs(); };
    loadPresetButton.onClick = [this] { loadPresetFile(); };
    addAndMakeVisible (savePresetButton);
    addAndMakeVisible (loadPresetButton);
    refreshPresetMenu();

    tabs.addTab ("OSC", ui::colours::background, &oscPage, false);
    tabs.addTab ("MOD", ui::colours::background, &modPage, false);
    tabs.addTab ("FX",  ui::colours::background, &fxPage,  false);
    tabs.addTab ("SCOPE", ui::colours::background, &scopePage, false);
    tabs.addTab ("CHORD", ui::colours::background, &chordPage, false);
    tabs.addTab ("AI", ui::colours::background, &aiPage, false);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    chordPage.onSendToAi = [this] (const juce::String& text)
    {
        tabs.setCurrentTabIndex (tabs.getNumTabs() - 1);   // the AI tab
        aiPage.setRequestText (text);
    };

    oscPage.oscA.onEdit = [this] (int osc) { openWaveEditor (osc); };
    oscPage.oscB.onEdit = [this] (int osc) { openWaveEditor (osc); };

    addAndMakeVisible (scope);

    keyboard.setKeyWidth (22.0f);
    keyboard.setAvailableRange (24, 108);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::colours::accent.withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::colours::accent.withAlpha (0.3f));
    addAndMakeVisible (keyboard);

    processor.addChangeListener (this);
    startTimerHz (30);

    setResizable (true, true);
    setResizeLimits (1040, 720, 1800, 1200);
    setSize (1160, 800);
}

WaveForgeEditor::~WaveForgeEditor()
{
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void WaveForgeEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    oscPage.oscA.refresh();
    oscPage.oscB.refresh();
    refreshPresetMenu();
}

void WaveForgeEditor::timerCallback()
{
    oscPage.oscA.tick();
    oscPage.oscB.tick();
    status.setText ("voices " + juce::String (processor.getActiveVoiceCount())
                        + "    " + juce::String (processor.getHostBpm(), 1) + " BPM",
                    juce::dontSendNotification);
}

void WaveForgeEditor::refreshPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    int id = 1;
    presetBox.addSectionHeading ("Factory");
    for (const auto& p : PresetManager::factoryPresets())
        presetBox.addItem (p.name, id++);

    const auto current = processor.getCurrentPresetName();
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        if (presetBox.getItemText (i) == current)
        {
            presetBox.setSelectedId (presetBox.getItemId (i), juce::dontSendNotification);
            return;
        }
    presetBox.setText (current, juce::dontSendNotification);
}

void WaveForgeEditor::savePresetAs()
{
    const auto dir = PresetManager::presetDirectory();
    dir.createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> ("Save preset",
                                                       dir.getChildFile (processor.getCurrentPresetName()
                                                                         + PresetManager::fileExtension),
                                                       juce::String ("*") + PresetManager::fileExtension);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file == juce::File())
                                      return;
                                  if (file.getFileExtension().isEmpty())
                                      file = file.withFileExtension (PresetManager::fileExtension);

                                  juce::String error;
                                  if (! processor.savePresetToFile (file, error))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                              "Save preset", error);
                                  else
                                      refreshPresetMenu();
                              });
}

void WaveForgeEditor::loadPresetFile()
{
    const auto dir = PresetManager::presetDirectory();
    dir.createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> ("Load preset", dir,
                                                       juce::String ("*") + PresetManager::fileExtension);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (! file.existsAsFile())
                                      return;
                                  juce::String error;
                                  if (! processor.loadPresetFromFile (file, error))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                              "Load preset", error);
                              });
}

void WaveForgeEditor::openWaveEditor (int osc)
{
    waveEditor = std::make_unique<ui::WavetableEditor> (processor, osc);
    waveEditor->onClose = [this] { closeWaveEditor(); };
    addAndMakeVisible (*waveEditor);
    tabs.setVisible (false);
    resized();
}

void WaveForgeEditor::closeWaveEditor()
{
    // Deferred: the close button that called us lives inside the editor.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<WaveForgeEditor> (this)]
    {
        if (safe == nullptr) return;
        safe->waveEditor.reset();
        safe->tabs.setVisible (true);
        safe->resized();
    });
}

//==============================================================================
void WaveForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::background);
}

void WaveForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto top = area.removeFromTop (60);
    const int presetWidth = juce::jmin (440, top.getWidth() * 2 / 5);
    auto presetArea = top.removeFromRight (presetWidth).withSizeKeepingCentre (presetWidth, 26);
    loadPresetButton.setBounds (presetArea.removeFromRight (78).reduced (2, 1));
    savePresetButton.setBounds (presetArea.removeFromRight (88).reduced (2, 1));
    presetLabel.setBounds (presetArea.removeFromLeft (52));
    presetBox.setBounds (presetArea.reduced (2, 1));
    global.setBounds (top.removeFromRight (210));
    title.setBounds (top.removeFromLeft (120).withSizeKeepingCentre (120, 26));
    status.setBounds (top.withSizeKeepingCentre (top.getWidth(), 26));

    area.removeFromTop (4);
    auto footer = area.removeFromBottom (86);
    area.removeFromBottom (6);
    scope.setBounds (footer.removeFromLeft (220));
    footer.removeFromLeft (6);
    keyboard.setBounds (footer);

    tabs.setBounds (area);
    if (waveEditor != nullptr)
        waveEditor->setBounds (area);
}
