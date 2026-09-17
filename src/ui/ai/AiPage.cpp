#include "AiPage.h"
#include "SettingsDialog.h"

namespace ui
{

namespace
{
    juce::String jp (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

    const char* gmName (int note)
    {
        switch (note)
        {
            case 35: case 36: return "Kick";
            case 37: return "Rim";  case 38: case 40: return "Snare"; case 39: return "Clap";
            case 42: return "CHH";  case 44: return "PHH"; case 46: return "OHH";
            case 41: case 43: case 45: case 47: case 48: case 50: return "Tom";
            case 49: case 57: return "Crash"; case 51: case 59: return "Ride"; case 53: return "Bell";
            default: return nullptr;
        }
    }

    juce::String noteName (int note)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
    }
}

//==============================================================================
AiPage::AiPage (WaveForgeProcessor& p)
    : processor (p), assistant (p.getAssistant()), chatList (*this)
{
    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::accentFx);
    addAndMakeVisible (title);

    contextLabel.setColour (juce::Label::textColourId, colours::textDim);
    contextLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (contextLabel);

    usageLabel.setColour (juce::Label::textColourId, colours::textDim);
    usageLabel.setFont (juce::FontOptions (11.0f));
    usageLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (usageLabel);

    hintLabel.setColour (juce::Label::textColourId, colours::textDim);
    hintLabel.setFont (juce::FontOptions (11.5f));
    hintLabel.setText (jp("Rec で演奏 / 再生中の MIDI を取り込み、.mid をここにドロップすると他トラックも渡せます。"
                          "Presets とクイックボタンは入力欄に入るだけなので、書き足してから Send (Enter) で送信。Shift+Enter で改行。"),
                       juce::dontSendNotification);
    addAndMakeVisible (hintLabel);

    recButton.setClickingTogglesState (true);
    recButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd9534f));
    recButton.onClick = [this]
    {
        const bool on = recButton.getToggleState();
        if (on)
        {
            assistant.pullCapturedNotes();
            processor.getMidiCapture().recording.store (true);
        }
        else
        {
            processor.getMidiCapture().recording.store (false);
            assistant.pullCapturedNotes();
            assistant.context().finishCapture();
        }
        refreshContextLabel();
    };
    clearCaptureButton.onClick = [this] { assistant.pullCapturedNotes(); assistant.context().clearCapture(); refreshContextLabel(); };
    addMidiButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Add a MIDI file as context",
                                                       juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "*.mid;*.midi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) addMidiFile (fc.getResult()); });
    };
    clearTracksButton.onClick = [this] { assistant.context().contextTracks.clear(); refreshContextLabel(); };
    settingsButton.onClick = [this]
    {
        SettingsDialogContent::show (this, [this] (const ai::Settings&) { assistant.reloadSettings(); refreshAll(); });
    };
    newChatButton.onClick = [this] { assistant.clearConversation(); };
    for (auto* b : { &recButton, &clearCaptureButton, &addMidiButton, &clearTracksButton, &settingsButton, &newChatButton })
        addAndMakeVisible (b);

    chatViewport.setViewedComponent (&chatList, false);
    chatViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (chatViewport);

    input.setMultiLine (true, true);
    input.setReturnKeyStartsNewLine (true);
    input.setFont (juce::FontOptions (13.5f));
    input.setColour (juce::TextEditor::backgroundColourId, colours::widget);
    input.setColour (juce::TextEditor::outlineColourId, colours::panelEdge);
    input.setColour (juce::TextEditor::focusedOutlineColourId, colours::accentFx);
    input.setTextToShowWhenEmpty (jp("依頼を書くか、Presets から選んでください..."), colours::textDim);
    input.onSend = [this] { sendInput(); };
    addAndMakeVisible (input);

    presetsButton.onClick = [this] { showPresetsMenu(); };
    savePresetButton.onClick = [this] { saveInputAsPreset(); };
    sendButton.onClick = [this] { sendInput(); };
    cancelButton.onClick = [this] { assistant.cancel(); };
    sendButton.setColour (juce::TextButton::buttonColourId, colours::accentFx.withAlpha (0.35f));
    for (auto* b : { &presetsButton, &savePresetButton, &sendButton, &cancelButton })
        addAndMakeVisible (b);

    struct Quick { const char* label; const char* category; };
    const Quick quicks[] = { { "メロディ", "メロディ" }, { "コード", "コード" }, { "ドラム", "ドラム" }, { "ベース", "ベース" },
                             { "続き", "展開" }, { "ハモリ", "ハモリ" }, { "Arp", "アルペジオ" },
                             { "音を作る", "*design" }, { "明るく", "調整" }, { "解説", "*explain" } };
    for (const auto& q : quicks)
    {
        auto* b = quickButtons.add (new juce::TextButton (jp (q.label)));
        const juce::String category = jp (q.category);
        b->onClick = [this, category] { quick (category); };
        addAndMakeVisible (b);
    }

    assistant.onChanged = [safe = juce::Component::SafePointer<AiPage> (this)] { if (safe != nullptr) safe->refreshAll(); };
    refreshAll();
    startTimerHz (5);
}

AiPage::~AiPage()
{
    assistant.onChanged = nullptr;
}

//==============================================================================
void AiPage::timerCallback()
{
    if (processor.getMidiCapture().recording.load())
    {
        assistant.pullCapturedNotes();
        refreshContextLabel();
    }
    if (assistant.isBusy() != lastBusy)
        refreshAll();
    else if (lastBusy)
        chatList.tick();
}

void AiPage::refreshContextLabel()
{
    auto text = assistant.context().summary();
    if (processor.getMidiCapture().recording.load())
        text = jp("● REC  ") + text;
    if (! assistant.hasKey())
        text += jp("   |   API キー未設定 → Settings");
    contextLabel.setText (text, juce::dontSendNotification);
}

void AiPage::refreshAll()
{
    const bool wasBusy = lastBusy;
    lastBusy = assistant.isBusy();
    sendButton.setEnabled (! lastBusy);
    sendButton.setButtonText (lastBusy ? jp("送信中...") : "Send");
    input.setEnabled (! lastBusy);
    cancelButton.setVisible (lastBusy);
    usageLabel.setText (lastBusy ? jp("AI の回答を待っています...") : assistant.lastUsage(), juce::dontSendNotification);
    refreshContextLabel();

    const auto& history = assistant.history();
    chatList.rebuild (history);
    chatList.setPending (lastBusy);
    chatList.layoutFor (chatViewport.getMaximumVisibleWidth());
    if (history.size() != lastHistorySize || wasBusy != lastBusy)
    {
        lastHistorySize = history.size();
        chatViewport.setViewPosition (0, juce::jmax (0, chatList.getHeight() - chatViewport.getViewHeight()));
    }
}

void AiPage::insertRequest (const juce::String& text)
{
    input.setText (text);
    input.moveCaretToEnd();
    input.grabKeyboardFocus();
}

void AiPage::sendInput()
{
    const auto text = input.getText().trim();
    if (text.isEmpty() || assistant.isBusy())
        return;
    input.clear();
    assistant.send (text);
}

// Quick buttons and presets only fill the input; the user reviews / edits and presses Send.
void AiPage::quick (const juce::String& category)
{
    if (category == "*design")
    {
        const auto text = input.getText().trim();
        const auto prefix = jp("こんな音を作って: ");
        insertRequest (text.isEmpty() || text.startsWith (prefix) ? prefix + text.fromFirstOccurrenceOf (prefix, false, false)
                                                                   : prefix + text);
        return;
    }
    if (category == "*explain")
    {
        insertRequest (jp("今の音の仕組みを、各セクションが何をしているか初心者向けに説明して。改善案があれば 2 つ挙げて (パラメータは変えないで)。"));
        return;
    }
    const auto text = ai::RequestPresets::firstText (category);
    if (text.isNotEmpty())
        insertRequest (text);
}

void AiPage::showPresetsMenu()
{
    auto presets = ai::RequestPresets::all();
    juce::PopupMenu menu;
    int id = 1;
    for (const auto& category : ai::RequestPresets::categories())
    {
        juce::PopupMenu sub;
        for (const auto& p : presets)
            if (p.category == category)
                sub.addItem (id++, p.name + (p.builtin ? "" : "  (user)"));
        menu.addSubMenu (category, sub);
    }
    auto userPresets = ai::RequestPresets::loadUser();
    if (! userPresets.empty())
    {
        juce::PopupMenu del;
        int delId = 10000;
        for (const auto& p : userPresets)
            del.addItem (delId++, p.name);
        menu.addSeparator();
        menu.addSubMenu (jp("ユーザープリセットを削除"), del);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsButton),
                        [this, presets, userPresets] (int result)
                        {
                            if (result <= 0) return;
                            if (result >= 10000)
                            {
                                auto remaining = userPresets;
                                const int index = result - 10000;
                                if (juce::isPositiveAndBelow (index, (int) remaining.size()))
                                    remaining.erase (remaining.begin() + index);
                                juce::String error;
                                ai::RequestPresets::saveUser (remaining, error);
                                return;
                            }
                            // ids were assigned category by category in the same order
                            int id = 1;
                            for (const auto& category : ai::RequestPresets::categories())
                                for (const auto& p : presets)
                                    if (p.category == category && id++ == result)
                                    {
                                        insertRequest (p.text);
                                        return;
                                    }
                        });
}

void AiPage::saveInputAsPreset()
{
    const auto text = input.getText().trim();
    if (text.isEmpty())
        return;
    auto* w = new juce::AlertWindow (jp("依頼をプリセットに保存"), jp("名前とカテゴリを付けてください。"), juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", text.substring (0, 24), jp("名前"));
    w->addComboBox ("category", ai::RequestPresets::categories(), jp("カテゴリ"));
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, text] (int result)
    {
        std::unique_ptr<juce::AlertWindow> owner (w);
        if (result != 1) return;
        ai::RequestPreset p;
        p.name = w->getTextEditorContents ("name").trim();
        p.category = w->getComboBoxComponent ("category")->getText();
        p.text = text;
        p.builtin = false;
        if (p.name.isEmpty()) return;
        auto user = ai::RequestPresets::loadUser();
        user.push_back (p);
        juce::String error;
        ai::RequestPresets::saveUser (user, error);
    }), false);
}

void AiPage::addMidiFile (const juce::File& file)
{
    ai::Track track;
    juce::String error;
    float bpm = 0.0f;
    if (! ai::MusicContext::loadMidiFile (file, track, &bpm, error))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "MIDI", error);
        return;
    }
    assistant.context().contextTracks.push_back (track);
    refreshContextLabel();
}

bool AiPage::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
            return true;
    return false;
}

void AiPage::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
            addMidiFile (juce::File (f));
}

//==============================================================================
void AiPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

void AiPage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    auto top = r.removeFromTop (26);
    title.setBounds (top.removeFromLeft (110));
    newChatButton.setBounds (top.removeFromRight (80).reduced (2, 1));
    settingsButton.setBounds (top.removeFromRight (80).reduced (2, 1));
    usageLabel.setBounds (top);

    r.removeFromTop (4);
    auto capture = r.removeFromTop (26);
    recButton.setBounds (capture.removeFromLeft (60).reduced (2, 1));
    clearCaptureButton.setBounds (capture.removeFromLeft (60).reduced (2, 1));
    clearTracksButton.setBounds (capture.removeFromRight (100).reduced (2, 1));
    addMidiButton.setBounds (capture.removeFromRight (90).reduced (2, 1));
    contextLabel.setBounds (capture.reduced (6, 0));

    r.removeFromTop (2);
    hintLabel.setBounds (r.removeFromTop (16));
    r.removeFromTop (4);

    auto bottom = r.removeFromBottom (108);
    r.removeFromBottom (6);
    chatViewport.setBounds (r);
    chatList.layoutFor (chatViewport.getMaximumVisibleWidth());

    auto quickRow = bottom.removeFromBottom (26);
    const int qw = quickRow.getWidth() / juce::jmax (1, quickButtons.size());
    for (auto* b : quickButtons)
        b->setBounds (quickRow.removeFromLeft (qw).reduced (2, 1));
    bottom.removeFromBottom (4);
    auto inputRow = bottom;
    auto left = inputRow.removeFromLeft (92);
    presetsButton.setBounds (left.removeFromTop (34).reduced (2));
    savePresetButton.setBounds (left.removeFromTop (34).reduced (2));
    auto right = inputRow.removeFromRight (92);
    sendButton.setBounds (right.removeFromTop (34).reduced (2));
    cancelButton.setBounds (right.removeFromTop (34).reduced (2));
    input.setBounds (inputRow.reduced (2));
}

//==============================================================================
void AiPage::ChatList::rebuild (const std::vector<ai::ChatMessage>& history)
{
    if (history.size() < built)
    {
        views.clear();
        built = 0;
    }
    for (size_t i = built; i < history.size(); ++i)
    {
        auto v = std::make_unique<MessageView> (page, history[i]);
        addAndMakeVisible (*v);
        views.push_back (std::move (v));
    }
    built = history.size();
}

void AiPage::ChatList::layoutFor (int width)
{
    int y = 0;
    for (auto& v : views)
    {
        const int h = v->heightFor (width - 8);
        v->setBounds (4, y, width - 8, h);
        y += h + 6;
    }
    if (pending.isVisible())
    {
        pending.setBounds (4, y, width - 8, PendingView::preferredHeight());
        y += PendingView::preferredHeight() + 6;
    }
    setSize (juce::jmax (1, width), juce::jmax (1, y));
}

void AiPage::ChatList::setPending (bool busy)
{
    if (busy && ! pending.isVisible())
        pending.begin();
    pending.setVisible (busy);
}

void AiPage::PendingView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panelEdge.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::accentFx.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    auto inner = getLocalBounds().reduced (12, 6);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (colours::accentFx);
    g.drawText ("ASSISTANT", inner.removeFromTop (14), juce::Justification::centredLeft);

    const auto elapsedMs = juce::Time::getMillisecondCounter() - startTime;
    const int phase = (int) (elapsedMs / 300) % 4;

    // three pulsing dots followed by the status text
    auto line = inner.removeFromTop (18);
    const float cy = (float) line.getCentreY();
    for (int i = 0; i < 3; ++i)
    {
        const float alpha = i == phase ? 1.0f : 0.3f;
        g.setColour (colours::accentFx.withAlpha (alpha));
        g.fillEllipse ((float) line.getX() + (float) i * 12.0f, cy - 3.5f, 7.0f, 7.0f);
    }
    line.removeFromLeft (44);
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (colours::text);
    g.drawText (jp("AI の回答を待っています... ") + juce::String ((int) (elapsedMs / 1000)) + " s"
                    + jp("   (Cancel で中止)"),
                line, juce::Justification::centredLeft);
}

//==============================================================================
AiPage::MessageView::MessageView (AiPage& p, const ai::ChatMessage& m) : page (p), message (m)
{
    if (message.role == ai::ChatMessage::assistant)
    {
        if (message.hasNotes())
        {
            notesCard = std::make_unique<NotesCard> (page, message);
            addAndMakeVisible (*notesCard);
        }
        if (message.hasParamChanges() || message.hasWavetable())
        {
            paramsCard = std::make_unique<ParamsCard> (page, message);
            addAndMakeVisible (*paramsCard);
        }
        if (message.hasArpPattern())
        {
            arpCard = std::make_unique<ArpCard> (page, message);
            addAndMakeVisible (*arpCard);
        }
    }
}

int AiPage::MessageView::heightFor (int width)
{
    juce::AttributedString s (message.text);
    s.setFont (juce::FontOptions (13.0f));
    juce::TextLayout layout;
    layout.createLayout (s, (float) juce::jmax (50, width - 24));
    textHeight = juce::jmax (18, (int) std::ceil (layout.getHeight()) + 4);
    int h = 18 + textHeight + 10;
    if (notesCard != nullptr)  h += NotesCard::preferredHeight() + 6;
    if (paramsCard != nullptr) h += paramsCard->preferredHeight() + 6;
    if (arpCard != nullptr)    h += ArpCard::preferredHeight() + 6;
    return h;
}

void AiPage::MessageView::resized()
{
    auto r = getLocalBounds().reduced (12, 6);
    r.removeFromTop (14 + textHeight);
    if (notesCard != nullptr)  { notesCard->setBounds (r.removeFromTop (NotesCard::preferredHeight())); r.removeFromTop (6); }
    if (paramsCard != nullptr) { paramsCard->setBounds (r.removeFromTop (paramsCard->preferredHeight())); r.removeFromTop (6); }
    if (arpCard != nullptr)    { arpCard->setBounds (r.removeFromTop (ArpCard::preferredHeight())); r.removeFromTop (6); }
}

void AiPage::MessageView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool user = message.role == ai::ChatMessage::user;
    g.setColour (message.isError ? juce::Colour (0xff3a2326) : user ? colours::widget : colours::panelEdge.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 8.0f);

    auto inner = getLocalBounds().reduced (12, 6);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (message.isError ? juce::Colour (0xffff8a80) : user ? colours::accent : colours::accentFx);
    g.drawText (message.isError ? "ERROR" : user ? "YOU" : "ASSISTANT", inner.removeFromTop (14), juce::Justification::centredLeft);

    juce::AttributedString s (message.text);
    s.setFont (juce::FontOptions (13.0f));
    s.setColour (colours::text);
    juce::TextLayout layout;
    layout.createLayout (s, (float) inner.getWidth());
    layout.draw (g, inner.removeFromTop (textHeight).toFloat());
}

//==============================================================================
AiPage::NotesCard::NotesCard (AiPage& p, const ai::ChatMessage& m) : page (p), message (m), dragHandle (*this)
{
    auditionButton.onClick = [this] { page.processor.getPreviewPlayer().start (message.notes, page.processor.getHostBpm()); };
    stopButton.onClick = [this] { page.processor.getPreviewPlayer().stop(); };
    saveButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save MIDI", juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                                           .getChildFile (message.notesKind + ".mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file == juce::File()) return;
                                  if (file.getFileExtension().isEmpty()) file = file.withFileExtension (".mid");
                                  juce::String error;
                                  const auto& ctx = page.assistant.context();
                                  if (! ai::MidiExport::write (message.notes, page.processor.getHostBpm(), ctx.timeSigNumerator,
                                                               ctx.timeSigDenominator, message.isDrums(), file, error))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "MIDI", error);
                              });
    };
    for (auto* b : std::initializer_list<juce::Component*> { &auditionButton, &stopButton, &dragHandle, &saveButton })
        addAndMakeVisible (b);
}

juce::File AiPage::NotesCard::ensureFile()
{
    if (exported.existsAsFile())
        return exported;
    juce::String error;
    const auto& ctx = page.assistant.context();
    auto file = ai::MidiExport::tempFileFor ("WaveForge " + message.notesKind);
    if (ai::MidiExport::write (message.notes, page.processor.getHostBpm(), ctx.timeSigNumerator, ctx.timeSigDenominator,
                               message.isDrums(), file, error))
        exported = file;
    return exported;
}

void AiPage::NotesCard::DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 8)
        return;
    dragging = true;
    const auto file = card.ensureFile();
    if (! file.existsAsFile())
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        container->performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void AiPage::NotesCard::resized()
{
    auto r = getLocalBounds();
    auto buttons = r.removeFromBottom (26);
    auditionButton.setBounds (buttons.removeFromLeft (80).reduced (2, 1));
    stopButton.setBounds (buttons.removeFromLeft (60).reduced (2, 1));
    saveButton.setBounds (buttons.removeFromRight (100).reduced (2, 1));
    dragHandle.setBounds (buttons.removeFromRight (110).reduced (2, 1));
}

void AiPage::NotesCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().withTrimmedBottom (28.0f);
    g.setColour (colours::widget);
    g.fillRoundedRectangle (r, 6.0f);
    auto plot = r.reduced (6.0f, 4.0f).withTrimmedLeft (36.0f);

    const auto& notes = message.notes;
    int lo = 127, hi = 0;
    float endBeat = 1.0f;
    for (const auto& n : notes)
    {
        lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch);
        endBeat = juce::jmax (endBeat, n.startBeat + n.durationBeats);
    }
    const int beatsPerBar = page.assistant.context().timeSigNumerator;
    const float bars = std::ceil (endBeat / (float) beatsPerBar);
    const float totalBeats = bars * (float) beatsPerBar;
    lo = juce::jmax (0, lo - 1); hi = juce::jmin (127, hi + 1);
    const float rowH = plot.getHeight() / (float) (hi - lo + 1);

    g.setColour (colours::panelEdge);
    for (int b = 0; b <= (int) totalBeats; ++b)
    {
        const float x = plot.getX() + plot.getWidth() * (float) b / totalBeats;
        g.setColour (b % beatsPerBar == 0 ? colours::textDim : colours::panelEdge);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
    g.setFont (juce::FontOptions (9.5f));
    for (int pitch = lo; pitch <= hi; ++pitch)
    {
        const float y = plot.getBottom() - rowH * (float) (pitch - lo + 1);
        if (message.isDrums())
        {
            if (auto* name = gmName (pitch))
            {
                g.setColour (colours::textDim);
                g.drawText (name, (int) r.getX() + 4, (int) y, 34, (int) rowH + 1, juce::Justification::centredLeft);
            }
        }
        else if (pitch % 12 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (noteName (pitch), (int) r.getX() + 4, (int) y, 34, (int) rowH + 1, juce::Justification::centredLeft);
            g.setColour (colours::panelEdge);
            g.drawHorizontalLine ((int) (y + rowH), plot.getX(), plot.getRight());
        }
    }
    const auto colour = message.isDrums() ? colours::accentB : colours::accentFx;
    for (const auto& n : notes)
    {
        const float x = plot.getX() + plot.getWidth() * n.startBeat / totalBeats;
        const float w = juce::jmax (2.0f, plot.getWidth() * n.durationBeats / totalBeats - 1.0f);
        const float y = plot.getBottom() - rowH * (float) (n.pitch - lo + 1);
        g.setColour (colour.withAlpha (0.45f + 0.55f * (float) n.velocity / 127.0f));
        g.fillRoundedRectangle (x, y + 1.0f, w, juce::jmax (2.0f, rowH - 2.0f), 2.0f);
    }
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (message.notesKind.toUpperCase() + "   " + juce::String (notes.size()) + " notes   " + juce::String ((int) bars) + " bars",
                r.reduced (8.0f, 3.0f).toNearestInt(), juce::Justification::topRight);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

//==============================================================================
AiPage::ParamsCard::ParamsCard (AiPage& p, const ai::ChatMessage& m) : page (p), message (m)
{
    changes = ai::ParamCatalog::previewChanges (page.processor.getAPVTS(), message.paramChanges, unknown);
    applyButton.onClick = [this]
    {
        juce::StringArray unknownIds;
        changes = page.assistant.applyParamChanges (message, unknownIds);
        applied = true;
        refreshButtons();
        repaint();
    };
    undoButton.onClick = [this] { page.assistant.undo(); applied = false; refreshButtons(); };
    wavetableButton.onClick = [this]
    {
        juce::String error;
        if (! page.assistant.applyWavetable (message, 0, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Wavetable", error);
        else
            refreshButtons();
    };
    addAndMakeVisible (applyButton);
    addAndMakeVisible (undoButton);
    if (message.hasWavetable())
        addAndMakeVisible (wavetableButton);
    applyButton.setColour (juce::TextButton::buttonColourId, colours::accentFx.withAlpha (0.35f));
    refreshButtons();
}

void AiPage::ParamsCard::refreshButtons()
{
    applyButton.setEnabled (! changes.empty() && ! applied);
    undoButton.setEnabled (page.assistant.canUndo());
}

int AiPage::ParamsCard::preferredHeight() const
{
    int lines = (int) changes.size() + (unknown.isEmpty() ? 0 : 1) + (message.hasWavetable() ? 1 : 0);
    return 24 + juce::jmax (1, lines) * 16 + 34;
}

void AiPage::ParamsCard::resized()
{
    auto r = getLocalBounds();
    auto buttons = r.removeFromBottom (28);
    applyButton.setBounds (buttons.removeFromLeft (80).reduced (2, 2));
    undoButton.setBounds (buttons.removeFromLeft (70).reduced (2, 2));
    if (message.hasWavetable())
        wavetableButton.setBounds (buttons.removeFromLeft (190).reduced (2, 2));
}

void AiPage::ParamsCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().withTrimmedBottom (30.0f);
    g.setColour (colours::widget);
    g.fillRoundedRectangle (r, 6.0f);
    auto inner = r.reduced (8.0f, 4.0f).toNearestInt();
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (colours::accentFx);
    g.drawText ("PARAMETER CHANGES" + (message.presetName.isNotEmpty() ? "  -  " + message.presetName : juce::String())
                    + (applied ? "   (applied)" : ""),
                inner.removeFromTop (16), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.0f));
    for (const auto& c : changes)
    {
        auto line = inner.removeFromTop (16);
        g.setColour (colours::textDim);
        g.drawText (c.name, line.removeFromLeft (200), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.drawText (c.oldText + "  ->  " + c.newText, line, juce::Justification::centredLeft);
    }
    if (message.hasWavetable())
    {
        g.setColour (colours::accentB);
        g.drawText ("New wavetable: " + message.wavetable.getProperty ("name", "").toString() + "  ("
                        + juce::String (message.wavetable.getProperty ("frames", juce::var()).size()) + " key frames)",
                    inner.removeFromTop (16), juce::Justification::centredLeft);
    }
    if (! unknown.isEmpty())
    {
        g.setColour (colours::textDim);
        g.drawText ("ignored unknown ids: " + unknown.joinIntoString (", "), inner.removeFromTop (16), juce::Justification::centredLeft);
    }
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

//==============================================================================
AiPage::ArpCard::ArpCard (AiPage& p, const ai::ChatMessage& m) : page (p), message (m)
{
    applyButton.onClick = [this]
    {
        juce::String error;
        if (! page.assistant.applyArpPattern (message, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Arp", error);
    };
    saveButton.onClick = [this]
    {
        const auto dir = WaveForgeProcessor::userArpPatternDirectory();
        dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save arp pattern",
                                                       dir.getChildFile (juce::File::createLegalFileName (message.arpPattern.name) + ".json"), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file == juce::File()) return;
                                  if (file.getFileExtension().isEmpty()) file = file.withFileExtension (".json");
                                  auto pattern = message.arpPattern;
                                  pattern.name = file.getFileNameWithoutExtension();
                                  file.replaceWithText (juce::JSON::toString (pattern.toJson()));
                              });
    };
    applyButton.setColour (juce::TextButton::buttonColourId, colours::accentMod.withAlpha (0.35f));
    addAndMakeVisible (applyButton);
    addAndMakeVisible (saveButton);
}

void AiPage::ArpCard::resized()
{
    auto buttons = getLocalBounds().removeFromBottom (28);
    applyButton.setBounds (buttons.removeFromLeft (110).reduced (2, 2));
    saveButton.setBounds (buttons.removeFromLeft (130).reduced (2, 2));
}

void AiPage::ArpCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().withTrimmedBottom (30.0f);
    g.setColour (colours::widget);
    g.fillRoundedRectangle (r, 6.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (colours::accentMod);
    g.drawText ("ARP PATTERN  -  " + message.arpPattern.name + "  (" + juce::String (message.arpPattern.steps.size()) + " steps)",
                r.reduced (8.0f, 4.0f).toNearestInt().removeFromTop (16), juce::Justification::centredLeft);
    auto area = r.reduced (8.0f, 4.0f).withTrimmedTop (18.0f);
    const int n = (int) message.arpPattern.steps.size();
    const float cw = area.getWidth() / (float) juce::jmax (1, n);
    for (int i = 0; i < n; ++i)
    {
        const auto& s = message.arpPattern.steps[(size_t) i];
        auto cell = juce::Rectangle<float> (area.getX() + cw * (float) i, area.getY(), cw, area.getHeight()).reduced (1.0f, 0.0f);
        g.setColour (colours::panel.withAlpha (0.5f));
        g.fillRoundedRectangle (cell, 2.0f);
        if (s.kind == wf::ArpStep::note)
        {
            const float h = cell.getHeight() * juce::jlimit (0.05f, 1.0f, s.velocity);
            g.setColour (colours::accentMod);
            g.fillRoundedRectangle (cell.withTop (cell.getBottom() - h).withWidth (cell.getWidth() * juce::jlimit (0.2f, 1.0f, s.gate)), 2.0f);
        }
        else
        {
            g.setColour (colours::textDim);
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (s.kind == wf::ArpStep::tie ? "tie" : "-", cell.toNearestInt(), juce::Justification::centred);
        }
    }
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace ui
