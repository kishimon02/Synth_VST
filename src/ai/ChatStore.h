#pragma once

#include "ReplyFormat.h"
#include "LlmClient.h"

namespace ai
{

// A conversation on disk, as shown in the History menu.
struct ChatSession
{
    juce::File file;
    juce::String title;        // the first request, shortened
    juce::Time started, updated;
    int messageCount = 0;
};

// Chat transcripts in %APPDATA%/WaveForge/Chats, one JSON file per session,
// kept for 30 days. Sessions carry both what the UI shows and what goes back
// to the model, so an old conversation can be reopened and continued.
class ChatStore
{
public:
    static constexpr int retentionDays = 30;

    static juce::File directory();

    // Writes the session. `file` is filled in from `started` on the first save.
    static bool save (juce::File& file, juce::Time started, const std::vector<ChatMessage>& messages,
                      const std::vector<LlmMessage>& llmHistory, juce::String& error);

    static bool load (const juce::File&, std::vector<ChatMessage>& messages, std::vector<LlmMessage>& llmHistory,
                      juce::Time& started, juce::String& error);

    // Newest first, limited to the retention window.
    static std::vector<ChatSession> list (const juce::File& dir = directory());

    // Removes sessions older than `days`. Returns how many were deleted.
    static int prune (const juce::File& dir = directory(), int days = retentionDays);

    static juce::String titleFor (const std::vector<ChatMessage>&);

    static juce::var toVar (const ChatMessage&);
    static bool fromVar (const juce::var&, ChatMessage& out);
};

} // namespace ai
