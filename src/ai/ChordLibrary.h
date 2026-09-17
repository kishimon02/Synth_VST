#pragma once

#include "MusicContext.h"
#include <vector>

namespace ai
{

// One chord of a progression, already resolved against a key.
struct ChordSymbol
{
    int   rootSemitone = 0;                   // above the key tonic
    int   bassSemitone = -1;                  // slash bass above the tonic, -1 = the chord root
    std::vector<int> intervals { 0, 4, 7 };   // above the chord root
    float beats = 4.0f;
    juce::String degree;                      // the token as written, e.g. "IVM7"
    juce::String quality;                     // the suffix, e.g. "M7" ("" = plain triad)
    bool  minorTriad = false;                 // lower-case degree with no explicit quality
};

// A progression stored as roman numerals, so it can be played in any key.
struct ChordProgression
{
    enum Mode { major = 0, minor = 1, any = 2 };

    juce::String category, name, degrees;
    Mode mode = major;
    bool builtin = true;

    juce::var toVar() const;
    static bool fromVar (const juce::var&, ChordProgression& out);
};

// Built-in progressions, the roman-numeral parser and the voicing used to turn
// a progression into notes. Everything here is plain data + maths, so the tests
// and the UI share it.
class ChordLibrary
{
public:
    //==========================================================================
    // Keys: 24 entries, ordered by root with the major key first (C major, C
    // minor, Db major, C# minor, ...).
    static juce::StringArray keyNames();
    static int  keyRoot (int keyIndex);            // 0..11 (C = 0)
    static bool keyIsMinor (int keyIndex);
    static int  keyIndexFor (int root, bool minor) { return root * 2 + (minor ? 1 : 0); }
    static juce::String pitchClassName (int pitchClass, int keyIndex);   // spelled to suit the key

    //==========================================================================
    static const std::vector<ChordProgression>& builtins();
    static juce::StringArray categories();                               // in menu order
    static std::vector<ChordProgression> all();                          // built-ins then user
    static std::vector<ChordProgression> forCategory (const juce::String& category, bool minorKey);

    static juce::File userFile();                                        // %APPDATA%/WaveForge/ChordProgressions.json
    static std::vector<ChordProgression> loadUser();
    static bool saveUser (const std::vector<ChordProgression>&, juce::String& error);

    //==========================================================================
    // "IVM7 V7 iiim7 vim7". A token is [b|#] roman [quality] [/bass] [:beats],
    // e.g. "bVII", "IV/V", "I7:2". Up to 32 chords.
    static bool parse (const juce::String& degrees, bool minorMode, std::vector<ChordSymbol>& out, juce::String& error);
    static juce::String chordName (const ChordSymbol&, int keyIndex);
    static juce::String chordNames (const std::vector<ChordSymbol>&, int keyIndex, const juce::String& separator = " - ");

    //==========================================================================
    struct Options
    {
        int  keyIndex = 0;        // into keyNames()
        int  octave = 4;          // the voicing sits around C<octave> (C4 = 60)
        bool bassNote = true;     // add the root an octave below
        int  repeats = 1;
        int  velocity = 96;
    };

    // The whole progression, voice-led from chord to chord.
    static std::vector<Note> render (const std::vector<ChordSymbol>&, const Options&);
    // One chord on its own (auditioning a single chord button).
    static std::vector<Note> renderChord (const ChordSymbol&, const Options&, float startBeat = 0.0f);
    static float totalBeats (const std::vector<ChordSymbol>&, int repeats);
};

} // namespace ai
