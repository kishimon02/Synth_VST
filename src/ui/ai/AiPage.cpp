#include "AiPage.h"
#include "SettingsDialog.h"
#include "../PianoRoll.h"

namespace ui
{

namespace
{
    juce::String jp (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

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
    hintLabel.setText (jp("取り込みは 3 通り: Rec で演奏 / 再生中の MIDI、DAW のパートをここにドラッグ、Add .mid... で読み込み。"
                          "DAW の「コピー」は DAW 内部の形式なので貼り付けられません。文字の貼り付けは Paste ボタンで。"),
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
    historyButton.onClick = [this] { showHistoryMenu(); };
    for (auto* b : { &recButton, &clearCaptureButton, &addMidiButton, &clearTracksButton, &settingsButton,
                     &newChatButton, &historyButton })
        addAndMakeVisible (b);

    sessionLabel.setColour (juce::Label::textColourId, colours::textDim);
    sessionLabel.setFont (juce::FontOptions (11.5f));
    addAndMakeVisible (sessionLabel);

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
    // Studio One keeps Ctrl+V for itself, so the clipboard needs a button of its own.
    pasteButton.onClick = [this]
    {
        const auto text = juce::SystemClipboard::getTextFromClipboard();
        if (text.isEmpty())
            return;
        input.insertTextAtCaret (text);
        input.grabKeyboardFocus();
    };
    pasteButton.setTooltip (jp("クリップボードから貼り付け。DAW によっては Ctrl+V がホストに取られるので、"
                               "このボタンか入力欄の右クリックを使ってください。"));
    sendButton.onClick = [this] { sendInput(); };
    cancelButton.onClick = [this] { assistant.cancel(); };
    sendButton.setColour (juce::TextButton::buttonColourId, colours::accentFx.withAlpha (0.35f));
    for (auto* b : { &presetsButton, &savePresetButton, &pasteButton, &sendButton, &cancelButton })
        addAndMakeVisible (b);

    modelBox.setTooltip (jp("このあとの依頼に使うモデル"));
    modelBox.onChange = [this] { assistant.setModel (modelBox.getText()); };
    addAndMakeVisible (modelBox);

    effortLabel.setText (jp("思考"), juce::dontSendNotification);
    effortLabel.setFont (juce::FontOptions (11.5f));
    effortLabel.setColour (juce::Label::textColourId, colours::textDim);
    effortLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (effortLabel);

    const char* effortLabels[3] = { "Low", "Medium", "High" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = effortButtons.add (new juce::TextButton (effortLabels[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (0x4e5f);
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                                  | (i < 2 ? juce::Button::ConnectedOnRight : 0));
        b->setColour (juce::TextButton::buttonOnColourId, colours::accentFx.withAlpha (0.55f));
        b->onClick = [this, i] { assistant.setEffort (effortIds (i)); };
        addAndMakeVisible (b);
    }

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
    const auto epoch = assistant.historyEpoch();
    chatList.rebuild (history, epoch);
    chatList.setPending (lastBusy);
    chatList.layoutFor (chatViewport.getMaximumVisibleWidth());
    if (history.size() != lastHistorySize || epoch != lastEpoch || wasBusy != lastBusy)
    {
        lastHistorySize = history.size();
        lastEpoch = epoch;
        chatViewport.setViewPosition (0, juce::jmax (0, chatList.getHeight() - chatViewport.getViewHeight()));
    }

    refreshModelBar();
    sessionLabel.setText (history.empty() ? jp("新しい会話")
                                          : assistant.sessionStartTime().formatted ("%m/%d %H:%M") + jp(" の会話")
                                                + (assistant.sessionFile() == juce::File() ? jp(" (未保存)") : juce::String()),
                          juce::dontSendNotification);
}

// Keeps the model / effort bar in step with the stored settings.
void AiPage::refreshModelBar()
{
    const auto& settings = assistant.getSettings();
    auto models = settings.isAnthropic() ? ai::Settings::anthropicModels()
                                         : juce::StringArray { "gpt-5", "gpt-5-mini" };
    if (settings.model.isNotEmpty() && ! models.contains (settings.model))
        models.add (settings.model);

    juce::StringArray shown;
    for (int i = 0; i < modelBox.getNumItems(); ++i)
        shown.add (modelBox.getItemText (i));
    if (shown != models)
    {
        modelBox.clear (juce::dontSendNotification);
        modelBox.addItemList (models, 1);
    }
    modelBox.setSelectedId (models.indexOf (settings.model) + 1, juce::dontSendNotification);
    modelBox.setEnabled (! lastBusy);

    for (int i = 0; i < effortButtons.size(); ++i)
    {
        effortButtons[i]->setToggleState (settings.effort == effortIds (i), juce::dontSendNotification);
        effortButtons[i]->setEnabled (settings.isAnthropic() && ! lastBusy);
    }
    effortLabel.setEnabled (settings.isAnthropic());
}

void AiPage::setRequestText (const juce::String& text)
{
    insertRequest (text);
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

// The last 30 days of conversations, newest first, grouped by day.
void AiPage::showHistoryMenu()
{
    auto sessions = assistant.savedSessions();
    juce::PopupMenu menu;

    if (sessions.empty())
    {
        menu.addItem (9999, jp("保存された会話はありません"), false);
    }
    else
    {
        const auto today = juce::Time::getCurrentTime();
        juce::PopupMenu day;
        juce::String currentDay;
        int id = 1;
        auto flushDay = [&menu, &day, &currentDay]
        {
            if (currentDay.isNotEmpty())
                menu.addSubMenu (currentDay, day);
            day = juce::PopupMenu();
        };

        for (const auto& s : sessions)
        {
            const auto days = today.getDayOfYear() - s.updated.getDayOfYear();
            const bool sameYear = today.getYear() == s.updated.getYear();
            const auto label = sameYear && days == 0 ? jp("今日")
                             : sameYear && days == 1 ? jp("昨日")
                                                     : s.updated.formatted ("%m/%d");
            if (label != currentDay)
            {
                flushDay();
                currentDay = label;
            }
            day.addItem (id++, s.updated.formatted ("%H:%M") + "   " + s.title + "   ("
                                   + juce::String (s.messageCount) + ")",
                         true, s.file == assistant.sessionFile());
        }
        flushDay();
    }

    menu.addSeparator();
    menu.addItem (9001, jp("保存フォルダを開く"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&historyButton),
                        [this, sessions] (int result)
                        {
                            if (result <= 0)
                                return;
                            if (result == 9001)
                            {
                                auto dir = ai::ChatStore::directory();
                                dir.createDirectory();
                                dir.revealToUser();
                                return;
                            }
                            const int index = result - 1;
                            if (! juce::isPositiveAndBelow (index, (int) sessions.size()))
                                return;
                            juce::String error;
                            if (! assistant.loadSession (sessions[(size_t) index].file, error))
                                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                        "History", error);
                        });
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

static bool looksLikeMidiFile (const juce::String& path)
{
    const auto p = path.trim().unquoted();
    return p.endsWithIgnoreCase (".mid") || p.endsWithIgnoreCase (".midi");
}

bool AiPage::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (looksLikeMidiFile (f))
            return true;
    return false;
}

void AiPage::fileDragEnter (const juce::StringArray&, int, int)
{
    dragOver = true;
    repaint();
}

void AiPage::fileDragExit (const juce::StringArray&)
{
    dragOver = false;
    repaint();
}

void AiPage::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    for (const auto& f : files)
        if (looksLikeMidiFile (f))
            addMidiFile (juce::File (f));
}

bool AiPage::isInterestedInTextDrag (const juce::String& text)
{
    const auto path = text.trim().unquoted();
    return looksLikeMidiFile (path) && juce::File::isAbsolutePath (path);
}

void AiPage::textDragEnter (const juce::String&, int, int)
{
    dragOver = true;
    repaint();
}

void AiPage::textDragExit (const juce::String&)
{
    dragOver = false;
    repaint();
}

void AiPage::textDropped (const juce::String& text, int, int)
{
    dragOver = false;
    repaint();
    const juce::File file (text.trim().unquoted());
    if (file.existsAsFile())
        addMidiFile (file);
}

//==============================================================================
void AiPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    if (dragOver)
    {
        g.setColour (colours::accentFx.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::accentFx);
        g.drawRoundedRectangle (r.reduced (2.0f), 8.0f, 2.5f);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText (jp("ここにドロップして取り込む (.mid)"), getLocalBounds(), juce::Justification::centred);
    }
}

void AiPage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    auto top = r.removeFromTop (26);
    title.setBounds (top.removeFromLeft (104));
    newChatButton.setBounds (top.removeFromRight (82).reduced (2, 1));
    settingsButton.setBounds (top.removeFromRight (78).reduced (2, 1));
    historyButton.setBounds (top.removeFromRight (78).reduced (2, 1));
    sessionLabel.setBounds (top.removeFromLeft (juce::jmin (210, top.getWidth() / 2)));
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

    auto bottom = r.removeFromBottom (136);
    r.removeFromBottom (6);
    chatViewport.setBounds (r);
    chatList.layoutFor (chatViewport.getMaximumVisibleWidth());

    auto quickRow = bottom.removeFromBottom (26);
    const int qw = quickRow.getWidth() / juce::jmax (1, quickButtons.size());
    for (auto* b : quickButtons)
        b->setBounds (quickRow.removeFromLeft (qw).reduced (2, 1));
    bottom.removeFromBottom (4);

    // model bar: model and effort together, right aligned under the Send button
    auto modelRow = bottom.removeFromBottom (24);
    modelRow.removeFromRight (2);                       // line up with the Send button
    auto barArea = modelRow.removeFromRight (juce::jmin (modelRow.getWidth(), 188 + 46 + 3 * 58));
    modelBox.setBounds (barArea.removeFromLeft (188).reduced (2, 1));
    effortLabel.setBounds (barArea.removeFromLeft (46));
    for (auto* b : effortButtons)
        b->setBounds (barArea.removeFromLeft (58).reduced (0, 1));
    bottom.removeFromBottom (4);
    auto inputRow = bottom;
    auto left = inputRow.removeFromLeft (92);
    const int leftButton = juce::jmax (22, left.getHeight() / 3);
    presetsButton.setBounds (left.removeFromTop (leftButton).reduced (2, 1));
    savePresetButton.setBounds (left.removeFromTop (leftButton).reduced (2, 1));
    pasteButton.setBounds (left.removeFromTop (leftButton).reduced (2, 1));
    auto right = inputRow.removeFromRight (92);
    sendButton.setBounds (right.removeFromTop (34).reduced (2));
    cancelButton.setBounds (right.removeFromTop (34).reduced (2));
    input.setBounds (inputRow.reduced (2));
}

//==============================================================================
void AiPage::ChatList::rebuild (const std::vector<ai::ChatMessage>& history, juce::uint32 epoch)
{
    if (history.size() < built || epoch != builtEpoch)
    {
        views.clear();
        built = 0;
        builtEpoch = epoch;
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
    auditionButton.onClick = [this]
    {
        previewId = page.processor.getPreviewPlayer().start (message.notes, page.processor.getHostBpm());
        startTimerHz (30);     // moves the white position line
    };
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
    dragHandle.setTooltip (jp("アレンジ画面へドラッグして .mid を置きます。プラグイン画面が重なっている場所には落とせないので、"
                               "画面を少し脇にどけて、トラックが見えている所までドラッグしてください。"));
    saveButton.setTooltip (jp(".mid として保存します。ドラッグがうまくいかないときは保存してから読み込んでください。"));
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

double AiPage::NotesCard::playheadBeat() const
{
    const auto& preview = page.processor.getPreviewPlayer();
    if (previewId == 0 || ! preview.isPlaying() || preview.playingId() != previewId)
        return -1.0;
    return preview.playPositionBeats();
}

void AiPage::NotesCard::timerCallback()
{
    if (playheadBeat() < 0.0)
    {
        stopTimer();          // the line is gone: one last repaint clears it
        previewId = 0;
    }
    repaint();
}

void AiPage::NotesCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().withTrimmedBottom (28.0f);
    const int bars = drawPianoRoll (g, r, message.notes, page.assistant.context().timeSigNumerator,
                                    message.isDrums(), message.isDrums() ? colours::accentB : colours::accentFx,
                                    playheadBeat());
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (message.notesKind.toUpperCase() + "   " + juce::String (message.notes.size()) + " notes   "
                    + juce::String (bars) + " bars",
                r.reduced (8.0f, 3.0f).toNearestInt(), juce::Justification::topRight);
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
