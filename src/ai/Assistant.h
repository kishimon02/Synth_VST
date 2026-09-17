#pragma once

#include "Settings.h"
#include "LlmClient.h"
#include "ReplyFormat.h"
#include "ChatStore.h"
#include "MusicContext.h"
#include "ParamCatalog.h"
#include <juce_audio_processors/juce_audio_processors.h>

class WaveForgeProcessor;

namespace ai
{

// Conversation state + request execution. Lives in the processor so the
// chat survives closing the editor. All public methods run on the message
// thread; the HTTP request runs on a single background thread.
class Assistant
{
public:
    explicit Assistant (WaveForgeProcessor&);
    ~Assistant();

    void reloadSettings();
    const Settings& getSettings() const noexcept { return settings; }
    bool hasKey() const noexcept { return settings.hasKey(); }

    bool isBusy() const noexcept { return busy.load(); }
    void send (const juce::String& userText);
    void cancel();
    void clearConversation();

    const std::vector<ChatMessage>& history() const noexcept { return messages; }
    juce::String lastUsage() const { return usageText; }

    // Saved conversations (kept for ChatStore::retentionDays days).
    std::vector<ChatSession> savedSessions() const { return ChatStore::list(); }
    bool loadSession (const juce::File&, juce::String& error);
    juce::Time sessionStartTime() const noexcept { return sessionStarted; }
    const juce::File& sessionFile() const noexcept { return sessionPath; }
    // Bumped whenever the history is replaced wholesale, so the view can rebuild.
    juce::uint32 historyEpoch() const noexcept { return epoch; }

    MusicContext& context() noexcept { return musicContext; }
    void pullCapturedNotes();               // drain the capture ring into the context

    // Applying results (message thread). Each apply keeps one undo state.
    std::vector<ParamCatalog::Change> applyParamChanges (const ChatMessage&, juce::StringArray& unknownIds);
    bool applyWavetable (const ChatMessage&, int osc, juce::String& error);
    bool applyArpPattern (const ChatMessage&, juce::String& error);
    bool canUndo() const noexcept { return undoState.isValid(); }
    void undo();

    std::function<void()> onChanged;        // history / busy state changed

private:
    void deliver (LlmResult result);
    void snapshotForUndo();
    void saveSession();

    WaveForgeProcessor& processor;
    Settings settings;
    MusicContext musicContext;
    std::vector<ChatMessage> messages;
    std::vector<LlmMessage> llmHistory;     // what goes back to the model (bounded)
    juce::String catalogText;               // built once
    juce::String usageText;
    juce::ThreadPool pool { 1 };
    std::atomic<bool> busy { false }, cancelFlag { false };
    juce::File sessionPath;
    juce::Time sessionStarted { juce::Time::getCurrentTime() };
    juce::uint32 epoch = 0;
    juce::ValueTree undoState;
    juce::String undoLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Assistant)
};

} // namespace ai
