#include "Assistant.h"
#include "../PluginProcessor.h"
#include "../dsp/WavetableLoader.h"

namespace ai
{

Assistant::Assistant (WaveForgeProcessor& p) : processor (p)
{
    settings = Settings::load();
    catalogText = ParamCatalog::describe (processor.getAPVTS());
    ChatStore::prune();
}

Assistant::~Assistant()
{
    cancelFlag.store (true);
    pool.removeAllJobs (true, 4000);
}

void Assistant::reloadSettings()
{
    settings = Settings::load();
    if (onChanged) onChanged();
}

void Assistant::clearConversation()
{
    messages.clear();
    llmHistory.clear();
    sessionPath = juce::File();
    sessionStarted = juce::Time::getCurrentTime();
    ++epoch;
    if (onChanged) onChanged();
}

// Conversations live in %APPDATA%/WaveForge/Chats, one file per session.
void Assistant::saveSession()
{
    juce::String error;
    ChatStore::save (sessionPath, sessionStarted, messages, llmHistory, error);
}

bool Assistant::loadSession (const juce::File& file, juce::String& error)
{
    if (busy.load())
    {
        error = juce::String (juce::CharPointer_UTF8 ("送信中は会話を切り替えられません。"));
        return false;
    }
    std::vector<ChatMessage> loadedMessages;
    std::vector<LlmMessage> loadedHistory;
    juce::Time started;
    if (! ChatStore::load (file, loadedMessages, loadedHistory, started, error))
        return false;

    messages = std::move (loadedMessages);
    llmHistory = std::move (loadedHistory);
    sessionPath = file;
    sessionStarted = started;
    ++epoch;
    if (onChanged) onChanged();
    return true;
}

void Assistant::cancel()
{
    cancelFlag.store (true);
}

void Assistant::pullCapturedNotes()
{
    std::vector<CapturedEvent> events;
    processor.getMidiCapture().drain (events);
    if (! events.empty())
        musicContext.appendCaptured (events);
}

void Assistant::send (const juce::String& userTextIn)
{
    const auto userText = userTextIn.trim();
    if (userText.isEmpty() || busy.load())
        return;
    if (! settings.hasKey())
    {
        ChatMessage err;
        err.role = ChatMessage::system;
        err.isError = true;
        err.text = juce::String (juce::CharPointer_UTF8 ("API キーが設定されていません。右上の Settings から入力してください。"));
        messages.push_back (err);
        if (onChanged) onChanged();
        return;
    }

    pullCapturedNotes();
    musicContext.bpm = processor.getHostBpm();

    ChatMessage um;
    um.role = ChatMessage::user;
    um.text = userText;
    messages.push_back (um);
    llmHistory.push_back ({ "user", userText });
    while (llmHistory.size() > 16)
        llmHistory.erase (llmHistory.begin());

    LlmRequest req;
    req.systemStable = ReplyFormat::systemPrompt (catalogText);
    req.systemVolatile = juce::String (juce::CharPointer_UTF8 ("## 現在の設定 (既定値と異なるもの)\n"))
                       + ParamCatalog::currentValues (processor.getAPVTS())
                       + juce::String (juce::CharPointer_UTF8 ("\n\n## 音楽コンテキスト\n"))
                       + (musicContext.isEmpty() ? juce::String (juce::CharPointer_UTF8 ("(取り込んだパートはありません。bpm "))
                                                     + juce::String (musicContext.bpm, 1) + ")"
                                                 : musicContext.toJson());
    req.messages = llmHistory;
    req.schema = ReplyFormat::schema();

    saveSession();
    busy.store (true);
    cancelFlag.store (false);
    if (onChanged) onChanged();

    auto client = std::shared_ptr<LlmClient> (makeClient (settings));
    auto* self = this;
    pool.addJob (std::function<void()> ([self, client, req]
    {
        auto result = client->request (req, self->cancelFlag);
        juce::MessageManager::callAsync ([self, result] { self->deliver (result); });
    }));
}

void Assistant::deliver (LlmResult result)
{
    busy.store (false);
    ChatMessage reply;
    if (! result.ok)
    {
        reply.role = ChatMessage::system;
        reply.isError = true;
        reply.text = result.error;
        // do not keep a failed turn in the model history
        if (! llmHistory.empty() && llmHistory.back().role == "user")
            llmHistory.pop_back();
    }
    else
    {
        juce::String error;
        if (! ReplyFormat::parse (result.json, reply, error))
        {
            reply.role = ChatMessage::system;
            reply.isError = true;
            reply.text = error;
        }
        else
        {
            llmHistory.push_back ({ "assistant", result.text });
        }
        usageText = "in " + juce::String (result.inputTokens) + " (cached " + juce::String (result.cacheReadTokens)
                  + ")  out " + juce::String (result.outputTokens);
    }
    messages.push_back (reply);
    saveSession();
    if (onChanged) onChanged();
}

//==============================================================================
void Assistant::snapshotForUndo()
{
    undoState = processor.buildStateTree();
}

std::vector<ParamCatalog::Change> Assistant::applyParamChanges (const ChatMessage& m, juce::StringArray& unknown)
{
    snapshotForUndo();
    auto changes = ParamCatalog::applyChanges (processor.getAPVTS(), m.paramChanges, unknown);
    if (m.presetName.isNotEmpty())
        processor.setCurrentPresetName (m.presetName);
    return changes;
}

bool Assistant::applyWavetable (const ChatMessage& m, int osc, juce::String& error)
{
    std::vector<float> frames;
    int numFrames = 0;
    juce::String name;
    if (! ReplyFormat::wavetableFrames (m.wavetable, frames, numFrames, name))
    {
        error = "The reply has no usable wavetable.";
        return false;
    }
    snapshotForUndo();
    const auto file = WaveForgeProcessor::userWavetableDirectory().getChildFile (juce::File::createLegalFileName (name) + ".wav");
    if (! wf::WavetableLoader::saveFile (frames.data(), numFrames, file, error))
        return false;
    return processor.loadWavetableFile (osc, file, error, true);
}

bool Assistant::applyArpPattern (const ChatMessage& m, juce::String& error)
{
    if (! m.hasArpPattern())
    {
        error = "The reply has no arp pattern.";
        return false;
    }
    snapshotForUndo();
    processor.setCustomArpPattern (m.arpPattern, true);
    return true;
}

void Assistant::undo()
{
    if (! undoState.isValid())
        return;
    processor.applyStateTree (undoState);
    undoState = {};
    if (onChanged) onChanged();
}

} // namespace ai
