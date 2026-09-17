#include "PluginEditor.h"

WaveForgeEditor::WaveForgeEditor (WaveForgeProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      genericParams (p),
      keyboard (p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    for (int i = 0; i < 2; ++i)
    {
        auto& row = oscRows[i];
        row.label.setText (i == 0 ? "OSC A" : "OSC B", juce::dontSendNotification);
        row.label.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        addAndMakeVisible (row.label);

        row.tableBox.onChange = [this, i]
        {
            const int index = oscRows[i].tableBox.getSelectedId() - 1;
            if (index >= 0 && index != processor.getOscTableIndex (i))
                processor.setOscTable (i, index);
        };
        addAndMakeVisible (row.tableBox);

        row.loadButton.onClick = [this, i] { chooseFile (i); };
        addAndMakeVisible (row.loadButton);
        addAndMakeVisible (row.view);
    }

    paramViewport.setViewedComponent (&genericParams, false);
    paramViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (paramViewport);

    keyboard.setKeyWidth (22.0f);
    keyboard.setAvailableRange (24, 108);
    addAndMakeVisible (keyboard);

    processor.addChangeListener (this);
    refreshTableCombos();
    startTimerHz (30);

    setResizable (true, true);
    setResizeLimits (800, 600, 1800, 1200);
    setSize (1000, 760);
}

WaveForgeEditor::~WaveForgeEditor()
{
    processor.removeChangeListener (this);
}

void WaveForgeEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshTableCombos();
}

void WaveForgeEditor::refreshTableCombos()
{
    auto& bank = processor.getBank();
    for (int i = 0; i < 2; ++i)
    {
        auto& box = oscRows[i].tableBox;
        box.clear (juce::dontSendNotification);
        for (int t = 0; t < bank.size(); ++t)
            box.addItem (bank.nameAt (t), t + 1);
        box.setSelectedId (processor.getOscTableIndex (i) + 1, juce::dontSendNotification);
        oscRows[i].view.setTable (processor.getOscTable (i));
    }
}

void WaveForgeEditor::chooseFile (int osc)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load wavetable (.wav, 2048 samples per frame)",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       "*.wav");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, osc] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (! file.existsAsFile())
                                      return;
                                  juce::String error;
                                  if (! processor.loadWavetableFile (osc, file, error))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                              "Wavetable", error);
                              });
}

void WaveForgeEditor::timerCallback()
{
    auto& apvts = processor.getAPVTS();
    for (int i = 0; i < 2; ++i)
    {
        oscRows[i].view.setTable (processor.getOscTable (i));
        if (auto* pos = apvts.getRawParameterValue (ParamID::osc (i).wtPos))
            oscRows[i].view.setPosition (pos->load());
    }
    title.setText ("WaveForge  -  Phase 1 (wavetable engine)   voices: "
                       + juce::String (processor.getActiveVoiceCount()),
                   juce::dontSendNotification);
}

void WaveForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1f26));
}

void WaveForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    title.setBounds (area.removeFromTop (28));
    keyboard.setBounds (area.removeFromBottom (80));
    area.removeFromBottom (8);

    auto oscArea = area.removeFromTop (150);
    const int colW = oscArea.getWidth() / 2;
    for (int i = 0; i < 2; ++i)
    {
        auto col = oscArea.removeFromLeft (colW).reduced (4);
        auto& row = oscRows[i];
        auto header = col.removeFromTop (26);
        row.label.setBounds (header.removeFromLeft (60));
        row.loadButton.setBounds (header.removeFromRight (90));
        row.tableBox.setBounds (header.reduced (4, 0));
        col.removeFromTop (4);
        row.view.setBounds (col);
    }

    area.removeFromTop (8);
    paramViewport.setBounds (area);
    genericParams.setSize (area.getWidth() - paramViewport.getScrollBarThickness(), genericParams.getHeight());
}
