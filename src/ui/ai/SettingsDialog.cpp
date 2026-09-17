#include "SettingsDialog.h"

namespace ui
{

SettingsDialogContent::SettingsDialogContent (std::function<void (const ai::Settings&)> onSavedIn)
    : juce::Thread ("WaveForge AI connection test"), onSaved (std::move (onSavedIn))
{
    const auto s = ai::Settings::load();

    for (auto* l : { &providerLabel, &keyLabel, &modelLabel, &urlLabel, &effortLabel })
    {
        l->setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (l);
    }

    providerBox.addItem ("Anthropic (Claude)", 1);
    providerBox.addItem ("OpenAI compatible", 2);
    providerBox.setSelectedId (s.isAnthropic() ? 1 : 2, juce::dontSendNotification);
    providerBox.onChange = [this] { refreshForProvider(); };
    addAndMakeVisible (providerBox);

    keyEditor.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    keyEditor.setText (s.apiKey, false);
    keyEditor.setColour (juce::TextEditor::backgroundColourId, colours::widget);
    keyEditor.setColour (juce::TextEditor::outlineColourId, colours::panelEdge);
    addAndMakeVisible (keyEditor);

    modelBox.setEditableText (true);
    modelBox.setText (s.model, juce::dontSendNotification);
    addAndMakeVisible (modelBox);

    urlEditor.setText (s.baseUrl, false);
    urlEditor.setColour (juce::TextEditor::backgroundColourId, colours::widget);
    urlEditor.setColour (juce::TextEditor::outlineColourId, colours::panelEdge);
    addAndMakeVisible (urlEditor);

    effortBox.addItemList ({ "low", "medium", "high" }, 1);
    effortBox.setText (s.effort, juce::dontSendNotification);
    addAndMakeVisible (effortBox);

    status.setColour (juce::Label::textColourId, colours::textDim);
    status.setFont (juce::FontOptions (12.0f));
    status.setText ("The key is stored encrypted (Windows DPAPI) in " + ai::Settings::file().getFullPathName(),
                    juce::dontSendNotification);
    addAndMakeVisible (status);

    testButton.onClick = [this]
    {
        if (testing.exchange (true)) return;
        status.setText ("Testing...", juce::dontSendNotification);
        startThread();
    };
    saveButton.onClick = [this]
    {
        auto s2 = collect();
        juce::String error;
        if (! s2.save (error))
        {
            status.setText (error, juce::dontSendNotification);
            return;
        }
        if (onSaved) onSaved (s2);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (1);
    };
    cancelButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (0);
    };
    for (auto* b : { &testButton, &saveButton, &cancelButton })
        addAndMakeVisible (b);

    refreshForProvider();
    setSize (520, 300);
}

SettingsDialogContent::~SettingsDialogContent()
{
    stopThread (3000);
}

void SettingsDialogContent::show (juce::Component* parent, std::function<void (const ai::Settings&)> onSaved)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new SettingsDialogContent (std::move (onSaved)));
    o.dialogTitle = "AI Assistant Settings";
    o.dialogBackgroundColour = colours::panel;
    o.componentToCentreAround = parent;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void SettingsDialogContent::refreshForProvider()
{
    const bool anthropic = providerBox.getSelectedId() == 1;
    const auto current = modelBox.getText();
    modelBox.clear (juce::dontSendNotification);
    if (anthropic)
    {
        modelBox.addItemList (ai::Settings::anthropicModels(), 1);
        if (current.isEmpty() || current.startsWith ("gpt")) modelBox.setText ("claude-opus-5", juce::dontSendNotification);
        else modelBox.setText (current, juce::dontSendNotification);
        if (urlEditor.getText().isEmpty() || urlEditor.getText().contains ("openai.com/v1"))
            urlEditor.setText ("https://api.anthropic.com", false);
    }
    else
    {
        modelBox.addItemList ({ "gpt-5", "gpt-5-mini" }, 1);
        if (current.isEmpty() || current.startsWith ("claude")) modelBox.setText ("gpt-5", juce::dontSendNotification);
        else modelBox.setText (current, juce::dontSendNotification);
        if (urlEditor.getText().isEmpty() || urlEditor.getText().contains ("anthropic.com"))
            urlEditor.setText ("https://api.openai.com/v1", false);
    }
    effortBox.setEnabled (anthropic);
}

ai::Settings SettingsDialogContent::collect() const
{
    ai::Settings s;
    s.provider = providerBox.getSelectedId() == 1 ? "anthropic" : "openai";
    s.apiKey = keyEditor.getText().trim();
    s.model = modelBox.getText().trim();
    s.baseUrl = urlEditor.getText().trim();
    s.effort = effortBox.getText();
    return s;
}

void SettingsDialogContent::run()
{
    auto client = ai::makeClient (collect());
    const auto result = client->testConnection();
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SettingsDialogContent> (this), result]
    {
        if (safe == nullptr) return;
        safe->status.setText (result.isEmpty() ? "Connection OK." : result, juce::dontSendNotification);
        safe->testing.store (false);
    });
}

void SettingsDialogContent::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    auto line = [&r] (juce::Label& l, juce::Component& c)
    {
        auto row = r.removeFromTop (30);
        l.setBounds (row.removeFromLeft (90));
        c.setBounds (row.reduced (0, 3));
        r.removeFromTop (4);
    };
    line (providerLabel, providerBox);
    line (keyLabel, keyEditor);
    line (modelLabel, modelBox);
    line (urlLabel, urlEditor);
    line (effortLabel, effortBox);
    r.removeFromTop (4);
    status.setBounds (r.removeFromTop (40));
    auto buttons = r.removeFromBottom (30);
    cancelButton.setBounds (buttons.removeFromRight (90).reduced (2));
    saveButton.setBounds (buttons.removeFromRight (90).reduced (2));
    testButton.setBounds (buttons.removeFromLeft (140).reduced (2));
}

} // namespace ui
