#include "ChordPage.h"

namespace ui
{

namespace
{
    juce::String jp (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

    void styleLabel (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (11.5f));
        l.setColour (juce::Label::textColourId, colours::textDim);
        l.setJustificationType (juce::Justification::centredRight);
    }
}

//==============================================================================
ChordPage::ChordPage (WaveForgeProcessor& p) : processor (p)
{
    styleLabel (keyLabel, jp("キー"));
    styleLabel (categoryLabel, jp("カテゴリ"));
    styleLabel (progressionLabel, jp("コード進行"));
    styleLabel (octaveLabel, "Oct");
    styleLabel (repeatLabel, jp("繰り返し"));
    styleLabel (degreesLabel, "Degrees");
    for (auto* l : { &keyLabel, &categoryLabel, &progressionLabel, &octaveLabel, &repeatLabel, &degreesLabel })
        addAndMakeVisible (l);

    statusLabel.setFont (juce::FontOptions (12.0f));
    statusLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (statusLabel);

    int id = 1;
    for (const auto& name : ai::ChordLibrary::keyNames())
        keyBox.addItem (name, id++);
    keyBox.setSelectedId (1, juce::dontSendNotification);          // C major
    keyBox.onChange = [this]
    {
        refreshProgressions (currentName);
        applySelectedProgression();
    };

    categoryBox.onChange = [this] { refreshProgressions(); applySelectedProgression(); };
    progressionBox.onChange = [this] { applySelectedProgression(); };

    for (int oct = 2; oct <= 6; ++oct)
        octaveBox.addItem ("C" + juce::String (oct), oct);
    octaveBox.setSelectedId (4, juce::dontSendNotification);
    octaveBox.onChange = [this] { rebuild(); };

    for (int r : { 1, 2, 4 })
        repeatBox.addItem (juce::String (r) + "x", r);
    repeatBox.setSelectedId (1, juce::dontSendNotification);
    repeatBox.onChange = [this] { rebuild(); };

    bassToggle.setToggleState (true, juce::dontSendNotification);
    bassToggle.onClick = [this] { rebuild(); };

    for (auto* c : std::initializer_list<juce::Component*> { &keyBox, &categoryBox, &progressionBox, &octaveBox,
                                                            &repeatBox, &bassToggle })
        addAndMakeVisible (c);

    degreesEditor.setFont (juce::FontOptions (13.0f));
    degreesEditor.setColour (juce::TextEditor::backgroundColourId, colours::widget);
    degreesEditor.setColour (juce::TextEditor::outlineColourId, colours::panelEdge);
    degreesEditor.setTextToShowWhenEmpty ("IVM7 V7 iiim7 vim7", colours::textDim);
    degreesEditor.onReturnKey = [this] { rebuild(); };
    degreesEditor.onFocusLost = [this] { rebuild(); };
    addAndMakeVisible (degreesEditor);

    applyButton.onClick = [this] { rebuild(); };
    savePresetButton.onClick = [this] { saveAsUserPreset(); };
    deleteButton.onClick = [this] { deleteSelectedUserPreset(); };
    auditionButton.onClick = [this] { startPreview (notes, 0.0); };
    stopButton.onClick = [this] { processor.getPreviewPlayer().stop(); };
    saveMidiButton.onClick = [this] { saveMidiFile(); };
    sendToAiButton.setButtonText (jp("AI に渡す"));
    sendToAiButton.onClick = [this] { if (onSendToAi) onSendToAi (requestTextForAi()); };
    auditionButton.setColour (juce::TextButton::buttonColourId, colours::accentFx.withAlpha (0.35f));
    for (auto* b : std::initializer_list<juce::Component*> { &applyButton, &savePresetButton, &deleteButton,
                                                            &auditionButton, &stopButton, &saveMidiButton,
                                                            &sendToAiButton, &dragHandle })
        addAndMakeVisible (b);

    refreshCategories();
    refreshProgressions();
    applySelectedProgression();
}

//==============================================================================
int ChordPage::keyIndex() const { return juce::jlimit (0, 23, keyBox.getSelectedId() - 1); }

ai::ChordLibrary::Options ChordPage::options() const
{
    ai::ChordLibrary::Options o;
    o.keyIndex = keyIndex();
    o.octave = juce::jlimit (2, 6, octaveBox.getSelectedId());
    o.repeats = juce::jlimit (1, 8, repeatBox.getSelectedId());
    o.bassNote = bassToggle.getToggleState();
    return o;
}

void ChordPage::refreshCategories()
{
    const auto selected = categoryBox.getText();
    categoryBox.clear (juce::dontSendNotification);
    int id = 1;
    for (const auto& c : ai::ChordLibrary::categories())
        categoryBox.addItem (c, id++);
    for (int i = 0; i < categoryBox.getNumItems(); ++i)
        if (categoryBox.getItemText (i) == selected)
        {
            categoryBox.setSelectedId (categoryBox.getItemId (i), juce::dontSendNotification);
            return;
        }
    categoryBox.setSelectedId (1, juce::dontSendNotification);
}

void ChordPage::refreshProgressions (const juce::String& keepName)
{
    visible = ai::ChordLibrary::forCategory (categoryBox.getText(), ai::ChordLibrary::keyIsMinor (keyIndex()));
    progressionBox.clear (juce::dontSendNotification);
    int id = 1;
    for (const auto& p : visible)
        progressionBox.addItem (p.name + (p.builtin ? "" : jp("  (自作)")), id++);

    if (visible.empty())
    {
        progressionBox.setTextWhenNoChoicesAvailable (jp("このキーで使える進行がありません"));
        return;
    }
    int wanted = 1;
    for (size_t i = 0; i < visible.size(); ++i)
        if (visible[i].name == keepName)
            wanted = (int) i + 1;
    progressionBox.setSelectedId (wanted, juce::dontSendNotification);
}

void ChordPage::applySelectedProgression()
{
    const int index = progressionBox.getSelectedId() - 1;
    if (juce::isPositiveAndBelow (index, (int) visible.size()))
    {
        currentName = visible[(size_t) index].name;
        degreesEditor.setText (visible[(size_t) index].degrees, juce::dontSendNotification);
        deleteButton.setEnabled (! visible[(size_t) index].builtin);
    }
    else
    {
        deleteButton.setEnabled (false);
    }
    rebuild();
}

//==============================================================================
void ChordPage::rebuild()
{
    const auto opt = options();
    parseError.clear();
    chords.clear();
    notes.clear();
    exported = juce::File();

    if (ai::ChordLibrary::parse (degreesEditor.getText(), ai::ChordLibrary::keyIsMinor (opt.keyIndex), chords, parseError))
        notes = ai::ChordLibrary::render (chords, opt);

    chordButtons.clear();
    for (size_t i = 0; i < chords.size(); ++i)
    {
        auto* b = chordButtons.add (new juce::TextButton (ai::ChordLibrary::chordName (chords[i], opt.keyIndex)));
        b->setColour (juce::TextButton::buttonColourId, colours::widget);
        b->onClick = [this, i] { auditionChord ((int) i); };
        addAndMakeVisible (b);
    }
    layoutChordButtons();

    if (parseError.isNotEmpty())
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff8a80));
        statusLabel.setText (parseError, juce::dontSendNotification);
    }
    else
    {
        const float beats = ai::ChordLibrary::totalBeats (chords, opt.repeats);
        statusLabel.setColour (juce::Label::textColourId, colours::text);
        statusLabel.setText (keyBox.getText() + "   " + ai::ChordLibrary::chordNames (chords, opt.keyIndex)
                                 + "   " + juce::String (beats / 4.0f, 0) + jp(" 小節 / ")
                                 + juce::String ((int) notes.size()) + " notes",
                             juce::dontSendNotification);
    }

    const bool playable = ! notes.empty();
    for (auto* b : { &auditionButton, &saveMidiButton, &sendToAiButton })
        b->setEnabled (playable);
    dragHandle.setEnabled (playable);
    savePresetButton.setEnabled (playable);
    repaint();
}

void ChordPage::auditionChord (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) chords.size()))
        return;

    // A single chord is rendered from beat 0, so the playhead is offset to
    // where that chord sits in the progression.
    double offset = 0.0;
    for (int i = 0; i < index; ++i)
        offset += chords[(size_t) i].beats;

    startPreview (ai::ChordLibrary::renderChord (chords[(size_t) index], options()), offset);
}

// Starts the audition and the 30 Hz repaint that moves the white position line.
void ChordPage::startPreview (const std::vector<ai::Note>& toPlay, double offsetBeats)
{
    previewOffsetBeats = offsetBeats;
    previewId = processor.getPreviewPlayer().start (toPlay, processor.getHostBpm());
    startTimerHz (30);
}

double ChordPage::playheadBeat() const
{
    const auto& preview = processor.getPreviewPlayer();
    if (previewId == 0 || ! preview.isPlaying() || preview.playingId() != previewId)
        return -1.0;
    return preview.playPositionBeats() + previewOffsetBeats;
}

void ChordPage::timerCallback()
{
    if (playheadBeat() < 0.0)
    {
        stopTimer();          // the line is gone: one last repaint clears it
        previewId = 0;
    }
    repaint (rollArea);
}

//==============================================================================
void ChordPage::saveAsUserPreset()
{
    const auto degrees = degreesEditor.getText().trim();
    if (degrees.isEmpty())
        return;
    auto* w = new juce::AlertWindow (jp("コード進行を保存"), jp("名前とカテゴリを付けてください。"),
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", currentName, jp("名前"));
    w->addComboBox ("category", ai::ChordLibrary::categories(), jp("カテゴリ"));
    if (auto* box = w->getComboBoxComponent ("category"))
        for (int i = 0; i < box->getNumItems(); ++i)
            if (box->getItemText (i) == categoryBox.getText())
                box->setSelectedId (box->getItemId (i), juce::dontSendNotification);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    const bool minorKey = ai::ChordLibrary::keyIsMinor (keyIndex());
    w->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, w, degrees, minorKey] (int result)
        {
            std::unique_ptr<juce::AlertWindow> owner (w);
            if (result != 1)
                return;
            ai::ChordProgression p;
            p.name = w->getTextEditorContents ("name").trim();
            p.category = w->getComboBoxComponent ("category")->getText();
            p.degrees = degrees;
            p.mode = minorKey ? ai::ChordProgression::minor : ai::ChordProgression::major;
            p.builtin = false;
            if (p.name.isEmpty())
                return;
            auto user = ai::ChordLibrary::loadUser();
            user.push_back (p);
            juce::String error;
            if (! ai::ChordLibrary::saveUser (user, error))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Chord presets", error);
                return;
            }
            refreshCategories();
            for (int i = 0; i < categoryBox.getNumItems(); ++i)
                if (categoryBox.getItemText (i) == p.category)
                    categoryBox.setSelectedId (categoryBox.getItemId (i), juce::dontSendNotification);
            refreshProgressions (p.name);
            applySelectedProgression();
        }), false);
}

void ChordPage::deleteSelectedUserPreset()
{
    const int index = progressionBox.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (index, (int) visible.size()) || visible[(size_t) index].builtin)
        return;
    const auto target = visible[(size_t) index];
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, jp("コード進行を削除"),
                                        jp("「") + target.name + jp("」を削除しますか？"), "Delete", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([this, target] (int result)
    {
        if (result != 1)
            return;
        auto user = ai::ChordLibrary::loadUser();
        for (size_t i = 0; i < user.size(); ++i)
            if (user[i].name == target.name && user[i].category == target.category && user[i].degrees == target.degrees)
            {
                user.erase (user.begin() + (long) i);
                break;
            }
        juce::String error;
        if (! ai::ChordLibrary::saveUser (user, error))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Chord presets", error);
            return;
        }
        refreshCategories();
        refreshProgressions();
        applySelectedProgression();
    }));
}

//==============================================================================
juce::String ChordPage::requestTextForAi() const
{
    const float beats = ai::ChordLibrary::totalBeats (chords, options().repeats);
    return jp("キー ") + keyBox.getText() + jp("、コード進行 ")
         + ai::ChordLibrary::chordNames (chords, keyIndex())
         + jp(" (全 ") + juce::String (beats / 4.0f, 0) + jp(" 小節) に合うメロディを作って。");
}

juce::File ChordPage::ensureMidiFile()
{
    if (exported.existsAsFile())
        return exported;
    juce::String error;
    auto file = ai::MidiExport::tempFileFor ("WaveForge " + currentName);
    if (ai::MidiExport::write (notes, processor.getHostBpm(), 4, 4, false, file, error))
        exported = file;
    return exported;
}

void ChordPage::saveMidiFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI",
                                                   juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                                       .getChildFile (juce::File::createLegalFileName (currentName) + ".mid"),
                                                   "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              if (file.getFileExtension().isEmpty())
                                  file = file.withFileExtension (".mid");
                              juce::String error;
                              if (! ai::MidiExport::write (notes, processor.getHostBpm(), 4, 4, false, file, error))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "MIDI", error);
                          });
}

void ChordPage::DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 8)
        return;
    dragging = true;
    const auto file = page.ensureMidiFile();
    if (! file.existsAsFile())
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        container->performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

//==============================================================================
void ChordPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    drawPianoRoll (g, rollArea.toFloat(), notes, 4, false, colours::accentFx, playheadBeat());
    if (notes.empty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (jp("コード進行を選ぶとここに表示されます"), rollArea, juce::Justification::centred);
    }
}

void ChordPage::layoutChordButtons()
{
    if (chordRow.isEmpty() || chordButtons.isEmpty())
        return;
    auto row = chordRow;
    const int w = juce::jmin (150, row.getWidth() / juce::jmax (1, chordButtons.size()));
    for (auto* b : chordButtons)
        b->setBounds (row.removeFromLeft (w).reduced (2, 1));
}

void ChordPage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    auto top = r.removeFromTop (26);
    auto right = top.removeFromRight (330);
    octaveLabel.setBounds (right.removeFromLeft (34));
    octaveBox.setBounds (right.removeFromLeft (70).reduced (2, 1));
    repeatLabel.setBounds (right.removeFromLeft (64));
    repeatBox.setBounds (right.removeFromLeft (64).reduced (2, 1));
    bassToggle.setBounds (right.reduced (6, 0));

    keyLabel.setBounds (top.removeFromLeft (40));
    keyBox.setBounds (top.removeFromLeft (128).reduced (2, 1));
    categoryLabel.setBounds (top.removeFromLeft (76));
    categoryBox.setBounds (top.removeFromLeft (168).reduced (2, 1));
    progressionLabel.setBounds (top.removeFromLeft (84));
    progressionBox.setBounds (top.removeFromLeft (juce::jmax (150, top.getWidth())).reduced (2, 1));

    r.removeFromTop (4);
    statusLabel.setBounds (r.removeFromTop (18));
    r.removeFromTop (2);
    chordRow = r.removeFromTop (32);
    layoutChordButtons();
    r.removeFromTop (6);

    auto degrees = r.removeFromBottom (26);
    r.removeFromBottom (4);
    auto buttons = r.removeFromBottom (28);
    r.removeFromBottom (6);
    rollArea = r;

    auditionButton.setBounds (buttons.removeFromLeft (90).reduced (2, 1));
    stopButton.setBounds (buttons.removeFromLeft (64).reduced (2, 1));
    saveMidiButton.setBounds (buttons.removeFromRight (104).reduced (2, 1));
    dragHandle.setBounds (buttons.removeFromRight (112).reduced (2, 1));
    sendToAiButton.setBounds (buttons.removeFromRight (104).reduced (2, 1));

    degreesLabel.setBounds (degrees.removeFromLeft (62));
    deleteButton.setBounds (degrees.removeFromRight (74).reduced (2, 1));
    savePresetButton.setBounds (degrees.removeFromRight (84).reduced (2, 1));
    applyButton.setBounds (degrees.removeFromRight (74).reduced (2, 1));
    degreesEditor.setBounds (degrees.reduced (2, 1));
}

} // namespace ui
