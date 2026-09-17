#include "ChordLibrary.h"

namespace ai
{

namespace
{
    juce::String jp (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

    // Scale degrees (1..7) as semitones above the tonic.
    const int majorDegrees[7] = { 0, 2, 4, 5, 7, 9, 11 };
    const int minorDegrees[7] = { 0, 2, 3, 5, 7, 8, 10 };   // natural minor

    struct Roman { const char* text; int degree; bool lower; };
    const Roman romans[] =
    {
        { "VII", 7, false }, { "vii", 7, true }, { "VI", 6, false }, { "vi", 6, true },
        { "V", 5, false },   { "v", 5, true },   { "IV", 4, false }, { "iv", 4, true },
        { "III", 3, false }, { "iii", 3, true }, { "II", 2, false }, { "ii", 2, true },
        { "I", 1, false },   { "i", 1, true }
    };

    struct Quality { const char* suffix; std::vector<int> intervals; };
    const std::vector<Quality>& qualities()
    {
        static const std::vector<Quality> q
        {
            { "m",      { 0, 3, 7 } },          { "M",       { 0, 4, 7 } },      { "maj",   { 0, 4, 7 } },
            { "5",      { 0, 7 } },             { "6",       { 0, 4, 7, 9 } },   { "m6",    { 0, 3, 7, 9 } },
            { "7",      { 0, 4, 7, 10 } },      { "M7",      { 0, 4, 7, 11 } },  { "maj7",  { 0, 4, 7, 11 } },
            { "m7",     { 0, 3, 7, 10 } },      { "mM7",     { 0, 3, 7, 11 } },
            { "9",      { 0, 4, 7, 10, 14 } },  { "M9",      { 0, 4, 7, 11, 14 } }, { "maj9", { 0, 4, 7, 11, 14 } },
            { "m9",     { 0, 3, 7, 10, 14 } },  { "add9",    { 0, 4, 7, 14 } },  { "madd9", { 0, 3, 7, 14 } },
            { "sus2",   { 0, 2, 7 } },          { "sus4",    { 0, 5, 7 } },      { "7sus4", { 0, 5, 7, 10 } },
            { "dim",    { 0, 3, 6 } },          { "dim7",    { 0, 3, 6, 9 } },   { "m7b5",  { 0, 3, 6, 10 } },
            { "aug",    { 0, 4, 8 } },          { "aug7",    { 0, 4, 8, 10 } },  { "7b9",   { 0, 4, 7, 10, 13 } },
            { "13",     { 0, 4, 7, 10, 21 } }
        };
        return q;
    }

    const char* sharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const char* flatNames[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    bool keyUsesFlats (int root, bool minor)
    {
        if (minor)   // D, G, C, F, Bb, Eb, Ab minor
            return root == 2 || root == 7 || root == 0 || root == 5 || root == 10 || root == 3 || root == 8;
        return root == 5 || root == 10 || root == 3 || root == 8 || root == 1 || root == 6;   // F, Bb, Eb, Ab, Db, Gb
    }

    int average (const std::vector<int>& v)
    {
        if (v.empty()) return 60;
        int sum = 0;
        for (int p : v) sum += p;
        return sum / (int) v.size();
    }

    // Chord tones placed in the octave above `low`, so successive chords move as
    // little as possible. The first chord of a progression is root position.
    std::vector<int> voicingFor (const ChordSymbol& c, int keyRootPc, int base, const std::vector<int>& previous)
    {
        std::vector<int> pitches;
        if (previous.empty())
        {
            int chordRoot = base + keyRootPc + c.rootSemitone;
            while (chordRoot >= base + 12) chordRoot -= 12;
            for (int iv : c.intervals)
                pitches.push_back (chordRoot + iv);
        }
        else
        {
            const int low = average (previous) - 6;
            for (int iv : c.intervals)
            {
                const int pc = (keyRootPc + c.rootSemitone + iv) % 12;
                pitches.push_back (low + (((pc - low) % 12) + 12) % 12);
            }
            std::sort (pitches.begin(), pitches.end());
            for (size_t i = 1; i < pitches.size(); ++i)
                if (pitches[i] <= pitches[i - 1])
                    pitches[i] = pitches[i - 1] + 12;
        }
        for (auto& p : pitches)
            p = juce::jlimit (24, 108, p);
        return pitches;
    }
}

//==============================================================================
juce::var ChordProgression::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("category", category);
    o->setProperty ("name", name);
    o->setProperty ("degrees", degrees);
    o->setProperty ("mode", mode == minor ? "minor" : mode == any ? "any" : "major");
    return juce::var (o);
}

bool ChordProgression::fromVar (const juce::var& v, ChordProgression& out)
{
    out.category = v.getProperty ("category", "").toString();
    out.name = v.getProperty ("name", "").toString();
    out.degrees = v.getProperty ("degrees", "").toString();
    const auto m = v.getProperty ("mode", "major").toString();
    out.mode = m == "minor" ? minor : m == "any" ? any : major;
    out.builtin = false;
    return out.name.isNotEmpty() && out.degrees.isNotEmpty();
}

//==============================================================================
juce::StringArray ChordLibrary::keyNames()
{
    juce::StringArray names;
    for (int root = 0; root < 12; ++root)
        for (int minor = 0; minor < 2; ++minor)
            names.add (juce::String ((keyUsesFlats (root, minor == 1) ? flatNames : sharpNames)[root])
                           + (minor == 1 ? " minor" : " major"));
    return names;
}

int  ChordLibrary::keyRoot (int keyIndex)    { return juce::jlimit (0, 23, keyIndex) / 2; }
bool ChordLibrary::keyIsMinor (int keyIndex) { return (juce::jlimit (0, 23, keyIndex) % 2) == 1; }

juce::String ChordLibrary::pitchClassName (int pitchClass, int keyIndex)
{
    const int pc = ((pitchClass % 12) + 12) % 12;
    return (keyUsesFlats (keyRoot (keyIndex), keyIsMinor (keyIndex)) ? flatNames : sharpNames)[pc];
}

//==============================================================================
bool ChordLibrary::parse (const juce::String& degrees, bool minorMode, std::vector<ChordSymbol>& out, juce::String& error)
{
    out.clear();
    auto tokens = juce::StringArray::fromTokens (degrees.trim(), " \t\n|,", "");
    tokens.removeEmptyStrings();
    if (tokens.isEmpty())
    {
        error = jp("コードが書かれていません。");
        return false;
    }
    if (tokens.size() > 32)
        tokens.removeRange (32, tokens.size() - 32);

    const int* table = minorMode ? minorDegrees : majorDegrees;

    for (const auto& raw : tokens)
    {
        ChordSymbol c;
        c.degree = raw;
        juce::String token = raw;

        // ":beats"
        if (token.contains (":"))
        {
            c.beats = juce::jlimit (0.25f, 16.0f, token.fromLastOccurrenceOf (":", false, false).getFloatValue());
            token = token.upToLastOccurrenceOf (":", false, false);
        }

        // "/bass"
        juce::String bass;
        if (token.contains ("/"))
        {
            bass = token.fromLastOccurrenceOf ("/", false, false);
            token = token.upToLastOccurrenceOf ("/", false, false);
        }

        auto readRoot = [table] (juce::String text, int& semitone, bool& lower, juce::String& rest) -> bool
        {
            int accidental = 0;
            if (text.startsWith ("b"))      { accidental = -1; text = text.substring (1); }
            else if (text.startsWith ("#")) { accidental = 1;  text = text.substring (1); }
            for (const auto& r : romans)
                if (text.startsWith (r.text))
                {
                    semitone = table[r.degree - 1] + accidental;
                    lower = r.lower;
                    rest = text.substring ((int) juce::String (r.text).length());
                    return true;
                }
            return false;
        };

        bool lower = false;
        juce::String suffix;
        if (! readRoot (token, c.rootSemitone, lower, suffix))
        {
            error = jp("読めないコードです: ") + raw;
            return false;
        }

        if (bass.isNotEmpty())
        {
            bool bassLower = false;
            juce::String bassRest;
            if (! readRoot (bass, c.bassSemitone, bassLower, bassRest) || bassRest.isNotEmpty())
            {
                error = jp("読めないベース音です: ") + raw;
                return false;
            }
        }

        c.quality = suffix;
        if (suffix.isEmpty())
        {
            c.intervals = lower ? std::vector<int> { 0, 3, 7 } : std::vector<int> { 0, 4, 7 };
            c.minorTriad = lower;
        }
        else
        {
            bool found = false;
            for (const auto& q : qualities())
                if (suffix == q.suffix)
                {
                    c.intervals = q.intervals;
                    found = true;
                    break;
                }
            if (! found)
            {
                error = jp("知らないコードの種類です: ") + raw;
                return false;
            }
        }
        out.push_back (c);
    }
    return true;
}

juce::String ChordLibrary::chordName (const ChordSymbol& c, int keyIndex)
{
    const int root = keyRoot (keyIndex);
    juce::String name = pitchClassName (root + c.rootSemitone, keyIndex);
    if (c.quality.isEmpty())
        name += c.minorTriad ? "m" : "";
    else
        name += c.quality == "maj7" ? "M7" : c.quality == "maj9" ? "M9" : c.quality == "M" || c.quality == "maj" ? ""
                                                                                                                : c.quality;
    if (c.bassSemitone >= 0)
        name += "/" + pitchClassName (root + c.bassSemitone, keyIndex);
    return name;
}

juce::String ChordLibrary::chordNames (const std::vector<ChordSymbol>& chords, int keyIndex, const juce::String& separator)
{
    juce::StringArray names;
    for (const auto& c : chords)
        names.add (chordName (c, keyIndex));
    return names.joinIntoString (separator);
}

//==============================================================================
float ChordLibrary::totalBeats (const std::vector<ChordSymbol>& chords, int repeats)
{
    float beats = 0.0f;
    for (const auto& c : chords)
        beats += c.beats;
    return beats * (float) juce::jlimit (1, 8, repeats);
}

std::vector<Note> ChordLibrary::render (const std::vector<ChordSymbol>& chords, const Options& opt)
{
    std::vector<Note> notes;
    if (chords.empty())
        return notes;

    const int rootPc = keyRoot (opt.keyIndex);
    const int base = 12 * (juce::jlimit (0, 8, opt.octave) + 1);
    const int velocity = juce::jlimit (1, 127, opt.velocity);
    std::vector<int> previous;
    float beat = 0.0f;

    for (int r = 0; r < juce::jlimit (1, 8, opt.repeats); ++r)
        for (const auto& c : chords)
        {
            auto pitches = voicingFor (c, rootPc, base, previous);
            for (int p : pitches)
                notes.push_back ({ p, beat, c.beats * 0.98f, velocity });

            if (opt.bassNote)
            {
                const int pc = (rootPc + (c.bassSemitone >= 0 ? c.bassSemitone : c.rootSemitone)) % 12;
                notes.push_back ({ juce::jlimit (24, 108, base - 12 + pc), beat, c.beats * 0.98f,
                                   juce::jmin (127, velocity + 8) });
            }
            previous = pitches;
            beat += c.beats;
        }
    return notes;
}

std::vector<Note> ChordLibrary::renderChord (const ChordSymbol& c, const Options& opt, float startBeat)
{
    auto options = opt;
    options.repeats = 1;
    auto notes = render ({ c }, options);
    for (auto& n : notes)
        n.startBeat += startBeat;
    return notes;
}

//==============================================================================
namespace
{
    ChordProgression make (const char* category, const char* name, const char* degrees,
                           ChordProgression::Mode mode = ChordProgression::major)
    {
        ChordProgression p;
        p.category = jp (category);
        p.name = jp (name);
        p.degrees = degrees;
        p.mode = mode;
        p.builtin = true;
        return p;
    }

    constexpr auto kMinor = ChordProgression::minor;
    constexpr auto kAny = ChordProgression::any;
}

const std::vector<ChordProgression>& ChordLibrary::builtins()
{
    static const std::vector<ChordProgression> progressions
    {
        // --- 王道・ポップス
        make ("王道・ポップス", "王道進行 (4536)", "IVM7 V7 iiim7 vim7"),
        make ("王道・ポップス", "1-5-6-4", "I V vi IV"),
        make ("王道・ポップス", "6-4-1-5", "vi IV I V"),
        make ("王道・ポップス", "4-5-1-6", "IV V I vi"),
        make ("王道・ポップス", "1-6-4-5", "I vi IV V"),
        make ("王道・ポップス", "小室進行", "vi IV V I"),
        make ("王道・ポップス", "マイナー王道", "i VI III VII", kMinor),
        make ("王道・ポップス", "マイナー 4-5-1", "iv V7 i i", kMinor),
        make ("王道・ポップス", "マイナー下降", "i VII VI V7", kMinor),

        // --- カノン・循環
        make ("カノン・循環", "カノン進行", "I V vi iii IV I IV V"),
        make ("カノン・循環", "カノン進行 (短縮)", "I V vi iii IV V"),
        make ("カノン・循環", "逆循環", "I vi ii V"),
        make ("カノン・循環", "循環コード (7th)", "IM7 vim7 iim7 V7"),
        make ("カノン・循環", "3-6-2-5", "iiim7 vim7 iim7 V7"),
        make ("カノン・循環", "下降ベース", "I V/VII vi vi/V IV I/III iim7 V7"),
        make ("カノン・循環", "マイナーカノン", "i v VI III iv i iv V", kMinor),

        // --- J-POP・切ない
        make ("J-POP・切ない", "丸の内進行", "IVM7 III7 vim7 I7"),
        make ("J-POP・切ない", "丸の内進行 (8 小節)", "IVM7 III7 vim7 I7 IVM7 III7 vim7 V7"),
        make ("J-POP・切ない", "切ない 4-3-6", "IVM7 iiim7 vim7 vim7"),
        make ("J-POP・切ない", "同主短調の借用", "IM7 I7 IVM7 ivm6"),
        make ("J-POP・切ない", "4-5-6 終止", "IV V vi vi"),
        make ("J-POP・切ない", "泣きのマイナー", "im7 VIM7 VII III", kMinor),
        make ("J-POP・切ない", "マイナー 1-4-7-3", "i iv VII III", kMinor),

        // --- ロック・EDM
        make ("ロック・EDM", "4 つ打ち定番", "vi IV I V"),
        make ("ロック・EDM", "アンセム", "IV I V vi"),
        make ("ロック・EDM", "ミクソリディアン", "I bVII IV I"),
        make ("ロック・EDM", "ロック 1-4-5", "I IV V V"),
        make ("ロック・EDM", "マイナーロック", "i VII VI VII", kMinor),
        make ("ロック・EDM", "EDM ドロップ", "i VI VII v", kMinor),
        make ("ロック・EDM", "トランス", "i VII VI III", kMinor),

        // --- ジャズ・おしゃれ
        make ("ジャズ・おしゃれ", "ツーファイブワン", "iim7 V7 IM7 IM7"),
        make ("ジャズ・おしゃれ", "枯葉 (A)", "iim7 V7 IM7 IVM7 viim7b5 III7 vim7 vim7"),
        make ("ジャズ・おしゃれ", "セカンダリー 3-6-2-5", "iiim7 VI7 iim7 V7"),
        make ("ジャズ・おしゃれ", "ボサノバ", "IM7 II7 iim7 V7"),
        make ("ジャズ・おしゃれ", "モーダル", "IM7 bVIIM7 IVM7 IM7"),
        make ("ジャズ・おしゃれ", "半音経過 (dim)", "IM7 #Idim7 iim7 V7"),
        make ("ジャズ・おしゃれ", "マイナーツーファイブ", "iim7b5 V7 im7 im7", kMinor),
        make ("ジャズ・おしゃれ", "マイナー循環 (7th)", "im7 ivm7 VIIM7 IIIM7", kMinor),

        // --- ブルース・ファンク
        make ("ブルース・ファンク", "12 小節ブルース", "I7 I7 I7 I7 IV7 IV7 I7 I7 V7 IV7 I7 V7"),
        make ("ブルース・ファンク", "クイックチェンジ", "I7 IV7 I7 I7 IV7 IV7 I7 I7 V7 IV7 I7 V7"),
        make ("ブルース・ファンク", "ファンク 1 コード", "I9 I9 IV9 I9"),
        make ("ブルース・ファンク", "ファンク 4-5", "I7 IV9 I7 V9"),
        make ("ブルース・ファンク", "ソウル 2-5", "iim7 V7 iim7 V7"),
        make ("ブルース・ファンク", "マイナーブルース", "im7 im7 im7 im7 ivm7 ivm7 im7 im7 VI7 V7 im7 V7", kMinor),

        // --- バラード・映画
        make ("バラード・映画", "バラード王道", "I iii IV V"),
        make ("バラード・映画", "感動の終止", "IV V iii vi IV V I I"),
        make ("バラード・映画", "ペダルポイント", "IM7 IV/I IM7 V/I"),
        make ("バラード・映画", "讃美歌風", "I IV I V"),
        make ("バラード・映画", "マイナーバラード", "i III iv VI", kMinor),
        make ("バラード・映画", "映画音楽風", "i iv VI V", kMinor),
        make ("バラード・映画", "シネマティック上昇", "i III VI VII", kMinor),

        // --- 単体コード (どのキーでも使える)
        make ("単体コード", "メジャー", "I", kAny),
        make ("単体コード", "マイナー", "im", kAny),
        make ("単体コード", "パワーコード (5th)", "I5", kAny),
        make ("単体コード", "メジャー 7th", "IM7", kAny),
        make ("単体コード", "マイナー 7th", "im7", kAny),
        make ("単体コード", "ドミナント 7th", "I7", kAny),
        make ("単体コード", "マイナー メジャー 7th", "ImM7", kAny),
        make ("単体コード", "6th", "I6", kAny),
        make ("単体コード", "マイナー 6th", "im6", kAny),
        make ("単体コード", "9th", "I9", kAny),
        make ("単体コード", "メジャー 9th", "IM9", kAny),
        make ("単体コード", "マイナー 9th", "im9", kAny),
        make ("単体コード", "add9", "Iadd9", kAny),
        make ("単体コード", "sus4", "Isus4", kAny),
        make ("単体コード", "sus2", "Isus2", kAny),
        make ("単体コード", "7sus4", "I7sus4", kAny),
        make ("単体コード", "ディミニッシュ", "Idim", kAny),
        make ("単体コード", "ディミニッシュ 7th", "Idim7", kAny),
        make ("単体コード", "ハーフディミニッシュ", "im7b5", kAny),
        make ("単体コード", "オーギュメント", "Iaug", kAny)
    };
    return progressions;
}

juce::StringArray ChordLibrary::categories()
{
    juce::StringArray out;
    for (const auto& p : all())
        out.addIfNotAlreadyThere (p.category);
    return out;
}

std::vector<ChordProgression> ChordLibrary::all()
{
    auto out = builtins();
    for (const auto& p : loadUser())
        out.push_back (p);
    return out;
}

std::vector<ChordProgression> ChordLibrary::forCategory (const juce::String& category, bool minorKey)
{
    std::vector<ChordProgression> out;
    for (const auto& p : all())
        if (p.category == category && (p.mode == ChordProgression::any
                                        || p.mode == (minorKey ? ChordProgression::minor : ChordProgression::major)))
            out.push_back (p);
    return out;
}

//==============================================================================
juce::File ChordLibrary::userFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("WaveForge").getChildFile ("ChordProgressions.json");
}

std::vector<ChordProgression> ChordLibrary::loadUser()
{
    std::vector<ChordProgression> out;
    const auto f = userFile();
    if (! f.existsAsFile())
        return out;
    if (const auto* arr = juce::JSON::parse (f.loadFileAsString()).getArray())
        for (const auto& v : *arr)
        {
            ChordProgression p;
            if (ChordProgression::fromVar (v, p))
                out.push_back (p);
        }
    return out;
}

bool ChordLibrary::saveUser (const std::vector<ChordProgression>& presets, juce::String& error)
{
    juce::Array<juce::var> arr;
    for (const auto& p : presets)
        arr.add (p.toVar());
    const auto f = userFile();
    if (! f.getParentDirectory().createDirectory() || ! f.replaceWithText (juce::JSON::toString (juce::var (arr))))
    {
        error = "Could not write " + f.getFullPathName();
        return false;
    }
    return true;
}

} // namespace ai
