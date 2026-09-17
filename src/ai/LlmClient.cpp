#include "LlmClient.h"

namespace ai
{

juce::String httpErrorMessage (int status, const juce::String& providerMessage)
{
    juce::String base;
    switch (status)
    {
        case 0:   base = "Could not connect. Check your internet connection / base URL."; break;
        case 400: base = "The request was rejected (400)."; break;
        case 401: base = "Authentication failed (401). Check the API key in Settings."; break;
        case 403: base = "Access denied (403). The key may lack permission for this model."; break;
        case 404: base = "Not found (404). Check the model name / base URL."; break;
        case 429: base = "Rate limited (429). Wait a moment and try again."; break;
        case 529: base = "The API is overloaded (529). Try again shortly."; break;
        default:  base = status >= 500 ? "The API returned a server error (" + juce::String (status) + ")."
                                       : "The API returned status " + juce::String (status) + "."; break;
    }
    if (providerMessage.isNotEmpty())
        base += "\n" + providerMessage;
    return base;
}

namespace
{
    // POSTs `body` as JSON and returns the response text; status 0 = no connection.
    juce::String post (const juce::String& endpoint, const juce::String& headers, const juce::String& body,
                       int timeoutSeconds, std::atomic<bool>& cancel, int& statusOut)
    {
        statusOut = 0;
        juce::URL url (endpoint);
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                           .withHttpRequestCmd ("POST")
                           .withExtraHeaders (headers)
                           .withConnectionTimeoutMs (timeoutSeconds * 1000)
                           .withStatusCode (&statusOut)
                           .withProgressCallback ([&cancel] (int, int) { return ! cancel.load(); });
        auto stream = url.withPOSTData (body).createInputStream (options);
        if (stream == nullptr)
            return {};
        return stream->readEntireStreamAsString();
    }

    juce::String get (const juce::String& endpoint, const juce::String& headers, int timeoutSeconds, int& statusOut)
    {
        statusOut = 0;
        juce::URL url (endpoint);
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders (headers)
                           .withConnectionTimeoutMs (timeoutSeconds * 1000)
                           .withStatusCode (&statusOut);
        auto stream = url.createInputStream (options);
        return stream != nullptr ? stream->readEntireStreamAsString() : juce::String();
    }

    juce::String providerError (const juce::var& v)
    {
        const auto err = v.getProperty ("error", juce::var());
        if (err.isObject())
            return err.getProperty ("message", "").toString();
        return {};
    }

    // Parses the JSON the model returned as text; sets result.ok.
    void finish (LlmResult& r, const juce::String& text)
    {
        r.text = text.trim();
        r.json = juce::JSON::parse (r.text);
        if (r.json.isVoid() || ! r.json.isObject())
        {
            r.ok = false;
            r.error = "The reply was not valid JSON.";
            return;
        }
        r.ok = true;
    }
}

//==============================================================================
class AnthropicClient final : public LlmClient
{
public:
    explicit AnthropicClient (Settings s) : settings (std::move (s)) {}

    juce::var buildBody (const LlmRequest& req) const override
    {
        auto* body = new juce::DynamicObject();
        body->setProperty ("model", settings.model);
        body->setProperty ("max_tokens", req.maxTokens);

        // system: stable block (cached) + optional volatile block
        juce::Array<juce::var> system;
        {
            auto* block = new juce::DynamicObject();
            block->setProperty ("type", "text");
            block->setProperty ("text", req.systemStable);
            auto* cache = new juce::DynamicObject();
            cache->setProperty ("type", "ephemeral");
            block->setProperty ("cache_control", juce::var (cache));
            system.add (juce::var (block));
        }
        if (req.systemVolatile.isNotEmpty())
        {
            auto* block = new juce::DynamicObject();
            block->setProperty ("type", "text");
            block->setProperty ("text", req.systemVolatile);
            system.add (juce::var (block));
        }
        body->setProperty ("system", system);

        juce::Array<juce::var> messages;
        for (const auto& m : req.messages)
        {
            auto* msg = new juce::DynamicObject();
            msg->setProperty ("role", m.role);
            msg->setProperty ("content", m.content);
            messages.add (juce::var (msg));
        }
        body->setProperty ("messages", messages);

        auto* outputConfig = new juce::DynamicObject();
        const auto effort = settings.effort;
        outputConfig->setProperty ("effort", (effort == "low" || effort == "high" || effort == "medium") ? effort : "medium");
        if (req.schema.isObject())
        {
            auto* format = new juce::DynamicObject();
            format->setProperty ("type", "json_schema");
            format->setProperty ("schema", req.schema);
            outputConfig->setProperty ("format", juce::var (format));
        }
        body->setProperty ("output_config", juce::var (outputConfig));
        return juce::var (body);
    }

    LlmResult request (const LlmRequest& req, std::atomic<bool>& cancel) override
    {
        LlmResult r;
        int status = 0;
        const auto text = post (endpoint ("/v1/messages"), headers(),
                                juce::JSON::toString (buildBody (req), true), settings.timeoutSeconds, cancel, status);
        if (cancel.load()) { r.error = "Cancelled."; return r; }
        const auto v = juce::JSON::parse (text);
        if (status != 200)
        {
            r.error = httpErrorMessage (status, providerError (v));
            return r;
        }
        if (! v.isObject())
        {
            r.error = "Unexpected reply from the API.";
            return r;
        }
        r.stopReason = v.getProperty ("stop_reason", "").toString();
        if (r.stopReason == "refusal")
        {
            r.error = "The model declined this request.";
            return r;
        }
        const auto usage = v.getProperty ("usage", juce::var());
        r.inputTokens = (int) usage.getProperty ("input_tokens", 0);
        r.outputTokens = (int) usage.getProperty ("output_tokens", 0);
        r.cacheReadTokens = (int) usage.getProperty ("cache_read_input_tokens", 0);

        juce::String reply;
        if (const auto* content = v.getProperty ("content", juce::var()).getArray())
            for (const auto& block : *content)
                if (block.getProperty ("type", "").toString() == "text")
                    reply += block.getProperty ("text", "").toString();
        if (r.stopReason == "max_tokens")
        {
            r.error = "The reply was cut off (max_tokens). Ask for something shorter.";
            return r;
        }
        finish (r, reply);
        return r;
    }

    juce::String testConnection() override
    {
        int status = 0;
        const auto text = get (endpoint ("/v1/models?limit=1"), headers(), 20, status);
        if (status == 200) return {};
        return httpErrorMessage (status, providerError (juce::JSON::parse (text)));
    }

private:
    juce::String endpoint (const juce::String& path) const
    {
        auto base = settings.baseUrl.trim();
        if (base.isEmpty()) base = "https://api.anthropic.com";
        return base.trimCharactersAtEnd ("/") + path;
    }
    juce::String headers() const
    {
        return "Content-Type: application/json\r\nx-api-key: " + settings.apiKey.trim()
             + "\r\nanthropic-version: 2023-06-01\r\n";
    }
    Settings settings;
};

//==============================================================================
class OpenAiCompatClient final : public LlmClient
{
public:
    explicit OpenAiCompatClient (Settings s) : settings (std::move (s)) {}

    juce::var buildBody (const LlmRequest& req) const override
    {
        auto* body = new juce::DynamicObject();
        body->setProperty ("model", settings.model);
        body->setProperty ("max_completion_tokens", req.maxTokens);

        juce::Array<juce::var> messages;
        {
            auto* sys = new juce::DynamicObject();
            sys->setProperty ("role", "system");
            sys->setProperty ("content", req.systemStable + (req.systemVolatile.isNotEmpty() ? "\n\n" + req.systemVolatile : ""));
            messages.add (juce::var (sys));
        }
        for (const auto& m : req.messages)
        {
            auto* msg = new juce::DynamicObject();
            msg->setProperty ("role", m.role);
            msg->setProperty ("content", m.content);
            messages.add (juce::var (msg));
        }
        body->setProperty ("messages", messages);

        if (req.schema.isObject())
        {
            auto* format = new juce::DynamicObject();
            format->setProperty ("type", "json_schema");
            auto* js = new juce::DynamicObject();
            js->setProperty ("name", "waveforge_reply");
            js->setProperty ("schema", req.schema);
            js->setProperty ("strict", true);
            format->setProperty ("json_schema", juce::var (js));
            body->setProperty ("response_format", juce::var (format));
        }
        return juce::var (body);
    }

    LlmResult request (const LlmRequest& req, std::atomic<bool>& cancel) override
    {
        LlmResult r;
        int status = 0;
        const auto text = post (endpoint ("/chat/completions"), headers(),
                                juce::JSON::toString (buildBody (req), true), settings.timeoutSeconds, cancel, status);
        if (cancel.load()) { r.error = "Cancelled."; return r; }
        const auto v = juce::JSON::parse (text);
        if (status != 200)
        {
            r.error = httpErrorMessage (status, providerError (v));
            return r;
        }
        const auto* choices = v.getProperty ("choices", juce::var()).getArray();
        if (choices == nullptr || choices->isEmpty())
        {
            r.error = "Unexpected reply from the API.";
            return r;
        }
        const auto first = (*choices)[0];
        r.stopReason = first.getProperty ("finish_reason", "").toString();
        const auto usage = v.getProperty ("usage", juce::var());
        r.inputTokens = (int) usage.getProperty ("prompt_tokens", 0);
        r.outputTokens = (int) usage.getProperty ("completion_tokens", 0);
        const auto message = first.getProperty ("message", juce::var());
        if (! message.getProperty ("refusal", juce::var()).isVoid() && message.getProperty ("refusal", "").toString().isNotEmpty())
        {
            r.error = "The model declined this request.";
            return r;
        }
        if (r.stopReason == "length")
        {
            r.error = "The reply was cut off (length). Ask for something shorter.";
            return r;
        }
        finish (r, message.getProperty ("content", "").toString());
        return r;
    }

    juce::String testConnection() override
    {
        int status = 0;
        const auto text = get (endpoint ("/models"), headers(), 20, status);
        if (status == 200) return {};
        return httpErrorMessage (status, providerError (juce::JSON::parse (text)));
    }

private:
    juce::String endpoint (const juce::String& path) const
    {
        auto base = settings.baseUrl.trim();
        if (base.isEmpty()) base = "https://api.openai.com/v1";
        return base.trimCharactersAtEnd ("/") + path;
    }
    juce::String headers() const
    {
        return "Content-Type: application/json\r\nAuthorization: Bearer " + settings.apiKey.trim() + "\r\n";
    }
    Settings settings;
};

std::unique_ptr<LlmClient> makeClient (const Settings& s)
{
    if (s.isAnthropic())
        return std::make_unique<AnthropicClient> (s);
    return std::make_unique<OpenAiCompatClient> (s);
}

} // namespace ai
