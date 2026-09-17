#pragma once

#include "../Controls.h"
#include "../../ai/Settings.h"
#include "../../ai/LlmClient.h"

namespace ui
{

// Provider / API key / model / base URL / effort, with a connection test.
// Opened from the AI page. The key field is masked; the value only leaves
// this dialog through ai::Settings::save (DPAPI encrypted).
class SettingsDialogContent final : public juce::Component, private juce::Thread
{
public:
    explicit SettingsDialogContent (std::function<void (const ai::Settings&)> onSaved);
    ~SettingsDialogContent() override;

    static void show (juce::Component* parent, std::function<void (const ai::Settings&)> onSaved);

    void resized() override;

private:
    void run() override;                 // connection test
    void refreshForProvider();
    ai::Settings collect() const;

    juce::Label providerLabel { {}, "Provider" }, keyLabel { {}, "API key" }, modelLabel { {}, "Model" },
                urlLabel { {}, "Base URL" }, effortLabel { {}, "Effort" }, status;
    juce::ComboBox providerBox, modelBox, effortBox;
    juce::TextEditor keyEditor, urlEditor;
    juce::TextButton testButton { "Test connection" }, saveButton { "Save" }, cancelButton { "Cancel" };
    std::function<void (const ai::Settings&)> onSaved;
    std::atomic<bool> testing { false };
    juce::String testResult;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsDialogContent)
};

} // namespace ui
