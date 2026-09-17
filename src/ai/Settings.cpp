#include "Settings.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <wincrypt.h>
#endif

namespace ai
{

juce::File Settings::file()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("WaveForge").getChildFile ("settings.json");
}

Settings Settings::load()
{
    Settings s;
    const auto f = file();
    if (! f.existsAsFile())
        return s;
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject())
        return s;
    s.provider = v.getProperty ("provider", s.provider).toString();
    s.model = v.getProperty ("model", s.model).toString();
    s.baseUrl = v.getProperty ("base_url", s.baseUrl).toString();
    s.effort = v.getProperty ("effort", s.effort).toString();
    s.timeoutSeconds = juce::jlimit (10, 600, (int) v.getProperty ("timeout_sec", s.timeoutSeconds));
    s.apiKey = decryptSecret (v.getProperty ("api_key_encrypted", "").toString());
    return s;
}

bool Settings::save (juce::String& error) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("provider", provider);
    obj->setProperty ("model", model);
    obj->setProperty ("base_url", baseUrl);
    obj->setProperty ("effort", effort);
    obj->setProperty ("timeout_sec", timeoutSeconds);
    obj->setProperty ("api_key_encrypted", hasKey() ? encryptSecret (apiKey.trim()) : juce::String());
    const auto f = file();
    if (! f.getParentDirectory().createDirectory())
    {
        error = "Could not create " + f.getParentDirectory().getFullPathName();
        return false;
    }
    if (! f.replaceWithText (juce::JSON::toString (juce::var (obj), false)))
    {
        error = "Could not write " + f.getFullPathName();
        return false;
    }
    return true;
}

#if JUCE_WINDOWS
juce::String Settings::encryptSecret (const juce::String& plain)
{
    const auto utf8 = plain.toStdString();
    DATA_BLOB in { (DWORD) utf8.size(), (BYTE*) utf8.data() };
    DATA_BLOB out { 0, nullptr };
    if (! CryptProtectData (&in, L"WaveForge API key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    juce::MemoryBlock block (out.pbData, out.cbData);
    LocalFree (out.pbData);
    return block.toBase64Encoding();
}

juce::String Settings::decryptSecret (const juce::String& blobBase64)
{
    if (blobBase64.isEmpty())
        return {};
    juce::MemoryBlock block;
    if (! block.fromBase64Encoding (blobBase64))
        return {};
    DATA_BLOB in { (DWORD) block.getSize(), (BYTE*) block.getData() };
    DATA_BLOB out { 0, nullptr };
    if (! CryptUnprotectData (&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    juce::String result (juce::CharPointer_UTF8 ((const char*) out.pbData), juce::CharPointer_UTF8 ((const char*) out.pbData + out.cbData));
    SecureZeroMemory (out.pbData, out.cbData);
    LocalFree (out.pbData);
    return result;
}
#else
// Non-Windows builds are not a target; keep the key readable only by this
// user's file permissions.
juce::String Settings::encryptSecret (const juce::String& plain) { return juce::Base64::toBase64 (plain); }
juce::String Settings::decryptSecret (const juce::String& blob)
{
    juce::MemoryOutputStream out;
    return juce::Base64::convertFromBase64 (out, blob) ? out.toString() : juce::String();
}
#endif

} // namespace ai
