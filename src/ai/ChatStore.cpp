#include "ChatStore.h"

namespace ai
{

namespace
{
    juce::String roleName (ChatMessage::Role r)
    {
        return r == ChatMessage::user ? "user" : r == ChatMessage::assistant ? "assistant" : "system";
    }

    ChatMessage::Role roleFrom (const juce::String& s)
    {
        return s == "user" ? ChatMessage::user : s == "assistant" ? ChatMessage::assistant : ChatMessage::system;
    }
}

//==============================================================================
juce::File ChatStore::directory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("WaveForge").getChildFile ("Chats");
}

juce::var ChatStore::toVar (const ChatMessage& m)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("role", roleName (m.role));
    o->setProperty ("text", m.text);
    if (m.isError)          o->setProperty ("error", true);
    if (m.notesKind.isNotEmpty() && m.notesKind != "none")
        o->setProperty ("notes_kind", m.notesKind);
    if (! m.notes.empty())
    {
        juce::Array<juce::var> notes;
        for (const auto& n : m.notes)
        {
            auto* no = new juce::DynamicObject();
            no->setProperty ("pitch", n.pitch);
            no->setProperty ("start_beat", n.startBeat);
            no->setProperty ("duration_beats", n.durationBeats);
            no->setProperty ("velocity", n.velocity);
            notes.add (juce::var (no));
        }
        o->setProperty ("notes", notes);
    }
    if (m.hasParamChanges()) o->setProperty ("param_changes", m.paramChanges);
    if (m.hasWavetable())    o->setProperty ("wavetable", m.wavetable);
    if (m.hasArpPattern())   o->setProperty ("arp_pattern", m.arpPattern.toJson());
    if (m.presetName.isNotEmpty()) o->setProperty ("preset_name", m.presetName);
    return juce::var (o);
}

bool ChatStore::fromVar (const juce::var& v, ChatMessage& out)
{
    if (! v.isObject())
        return false;
    out.role = roleFrom (v.getProperty ("role", "user").toString());
    out.text = v.getProperty ("text", "").toString();
    out.isError = (bool) v.getProperty ("error", false);
    out.notesKind = v.getProperty ("notes_kind", "none").toString();
    out.presetName = v.getProperty ("preset_name", "").toString();

    out.notes.clear();
    if (const auto* arr = v.getProperty ("notes", juce::var()).getArray())
        for (const auto& n : *arr)
        {
            Note note;
            note.pitch = juce::jlimit (0, 127, (int) n.getProperty ("pitch", 60));
            note.startBeat = (float) (double) n.getProperty ("start_beat", 0.0);
            note.durationBeats = (float) (double) n.getProperty ("duration_beats", 1.0);
            note.velocity = juce::jlimit (1, 127, (int) n.getProperty ("velocity", 100));
            out.notes.push_back (note);
        }

    out.paramChanges = v.getProperty ("param_changes", juce::var());
    out.wavetable = v.getProperty ("wavetable", juce::var());

    out.arpPattern = {};
    const auto arp = v.getProperty ("arp_pattern", juce::var());
    if (arp.isObject())
    {
        juce::String error;
        wf::ArpPattern::fromJson (arp, out.arpPattern, error);
    }
    return true;
}

juce::String ChatStore::titleFor (const std::vector<ChatMessage>& messages)
{
    for (const auto& m : messages)
        if (m.role == ChatMessage::user && m.text.isNotEmpty())
        {
            auto line = m.text.upToFirstOccurrenceOf ("\n", false, false).trim();
            return line.length() > 44 ? line.substring (0, 43) + juce::String::charToString ((juce::juce_wchar) 0x2026)
                                      : line;
        }
    return juce::String (juce::CharPointer_UTF8 ("(依頼なし)"));
}

//==============================================================================
bool ChatStore::save (juce::File& file, juce::Time started, const std::vector<ChatMessage>& messages,
                      const std::vector<LlmMessage>& llmHistory, juce::String& error)
{
    if (messages.empty())
        return true;

    if (file == juce::File())
        file = directory().getChildFile (started.formatted ("chat-%Y%m%d-%H%M%S") + ".json");

    juce::Array<juce::var> entries;
    for (const auto& m : messages)
        entries.add (toVar (m));

    juce::Array<juce::var> history;
    for (const auto& m : llmHistory)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("role", m.role);
        o->setProperty ("content", m.content);
        history.add (juce::var (o));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("started", started.toISO8601 (true));
    root->setProperty ("updated", juce::Time::getCurrentTime().toISO8601 (true));
    root->setProperty ("title", titleFor (messages));
    root->setProperty ("messages", entries);
    root->setProperty ("llm", history);

    if (! file.getParentDirectory().createDirectory() || ! file.replaceWithText (juce::JSON::toString (juce::var (root))))
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    return true;
}

bool ChatStore::load (const juce::File& file, std::vector<ChatMessage>& messages, std::vector<LlmMessage>& llmHistory,
                      juce::Time& started, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "No such conversation: " + file.getFileName();
        return false;
    }
    const auto root = juce::JSON::parse (file.loadFileAsString());
    if (! root.isObject())
    {
        error = "Could not read " + file.getFileName();
        return false;
    }

    messages.clear();
    llmHistory.clear();
    started = juce::Time::fromISO8601 (root.getProperty ("started", "").toString());
    if (started.toMilliseconds() == 0)
        started = file.getCreationTime();

    if (const auto* arr = root.getProperty ("messages", juce::var()).getArray())
        for (const auto& v : *arr)
        {
            ChatMessage m;
            if (fromVar (v, m))
                messages.push_back (m);
        }
    if (const auto* arr = root.getProperty ("llm", juce::var()).getArray())
        for (const auto& v : *arr)
            llmHistory.push_back ({ v.getProperty ("role", "user").toString(), v.getProperty ("content", "").toString() });

    if (messages.empty())
    {
        error = "The conversation is empty: " + file.getFileName();
        return false;
    }
    return true;
}

//==============================================================================
std::vector<ChatSession> ChatStore::list (const juce::File& dir)
{
    std::vector<ChatSession> out;
    const auto oldest = juce::Time::getCurrentTime() - juce::RelativeTime::days (retentionDays);

    for (const auto& file : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        const auto root = juce::JSON::parse (file.loadFileAsString());
        if (! root.isObject())
            continue;

        ChatSession s;
        s.file = file;
        s.title = root.getProperty ("title", "").toString();
        s.started = juce::Time::fromISO8601 (root.getProperty ("started", "").toString());
        s.updated = juce::Time::fromISO8601 (root.getProperty ("updated", "").toString());
        if (s.started.toMilliseconds() == 0) s.started = file.getCreationTime();
        if (s.updated.toMilliseconds() == 0) s.updated = file.getLastModificationTime();
        if (const auto* arr = root.getProperty ("messages", juce::var()).getArray())
            s.messageCount = arr->size();
        if (s.title.isEmpty() || s.messageCount == 0)
            continue;
        if (s.updated < oldest)
            continue;
        out.push_back (s);
    }

    std::sort (out.begin(), out.end(), [] (const ChatSession& a, const ChatSession& b)
                                       { return a.updated > b.updated; });
    return out;
}

int ChatStore::prune (const juce::File& dir, int days)
{
    const auto oldest = juce::Time::getCurrentTime() - juce::RelativeTime::days (juce::jmax (1, days));
    int removed = 0;
    for (const auto& file : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        auto updated = juce::Time::fromISO8601 (juce::JSON::parse (file.loadFileAsString())
                                                    .getProperty ("updated", "").toString());
        if (updated.toMilliseconds() == 0)
            updated = file.getLastModificationTime();
        if (updated < oldest && file.deleteFile())
            ++removed;
    }
    return removed;
}

} // namespace ai
