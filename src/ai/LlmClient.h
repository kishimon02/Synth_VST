#pragma once

#include <juce_core/juce_core.h>
#include "Settings.h"
#include <atomic>
#include <memory>
#include <vector>

namespace ai
{

struct LlmMessage
{
    juce::String role;      // "user" | "assistant"
    juce::String content;
};

struct LlmRequest
{
    juce::String systemStable;      // cached across requests (role, rules, parameter catalogue)
    juce::String systemVolatile;    // per-request context, not cached (may be empty)
    std::vector<LlmMessage> messages;
    juce::var schema;               // JSON schema the reply must follow (structured output)
    int maxTokens = 16000;
};

struct LlmResult
{
    bool ok = false;
    juce::String error;             // user-facing, never contains the key
    juce::String text;              // raw JSON text of the reply
    juce::var json;                 // parsed reply
    juce::String stopReason;
    int inputTokens = 0, outputTokens = 0, cacheReadTokens = 0;
};

// Synchronous HTTP client for one provider. Always call from a background
// thread; `cancel` is polled while the request is in flight.
class LlmClient
{
public:
    virtual ~LlmClient() = default;
    virtual LlmResult request (const LlmRequest&, std::atomic<bool>& cancel) = 0;
    virtual juce::String testConnection() = 0;          // "" on success, otherwise a message
    virtual juce::var buildBody (const LlmRequest&) const = 0;   // exposed for tests
};

std::unique_ptr<LlmClient> makeClient (const Settings&);

// Shared helpers
juce::String httpErrorMessage (int status, const juce::String& providerMessage);

} // namespace ai
