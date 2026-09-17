#pragma once

#include <juce_core/juce_core.h>

namespace ai
{

// AI assistant settings, stored per user in %APPDATA%/WaveForge/settings.json.
// The API key is encrypted with Windows DPAPI (user scope) before it is
// written; it never goes into the plugin state / song / preset, and it must
// never be logged or shown in an error message.
struct Settings
{
    juce::String provider = "anthropic";          // "anthropic" | "openai"
    juce::String model = "claude-opus-5";
    juce::String baseUrl = "https://api.anthropic.com";   // OpenAI-compatible: e.g. https://api.openai.com/v1
    juce::String effort = "medium";               // low | medium | high (Anthropic only)
    int timeoutSeconds = 120;
    juce::String apiKey;                          // plain text, in memory only

    static juce::File file();
    static Settings load();
    bool save (juce::String& error) const;

    bool hasKey() const noexcept { return apiKey.trim().isNotEmpty(); }
    bool isAnthropic() const noexcept { return provider != "openai"; }

    // DPAPI helpers (base64 of the encrypted blob). Empty string on failure.
    static juce::String encryptSecret (const juce::String& plain);
    static juce::String decryptSecret (const juce::String& blobBase64);

    // Model presets shown in the settings dialog.
    static juce::StringArray anthropicModels() { return { "claude-opus-5", "claude-sonnet-5", "claude-haiku-4-5" }; }
};

} // namespace ai
