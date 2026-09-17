#include "ReplyFormat.h"
#include "../dsp/WaveEditOps.h"

namespace ai
{

bool ChatMessage::hasWavetable() const noexcept
{
    if (! wavetable.isObject()) return false;
    const auto* frames = wavetable.getProperty ("frames", juce::var()).getArray();
    return frames != nullptr && ! frames->isEmpty();
}

//==============================================================================
namespace
{
    juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> props)
    {
        auto* o = new juce::DynamicObject();
        for (const auto& [k, v] : props)
            o->setProperty (k, v);
        return juce::var (o);
    }
    juce::var arr (std::initializer_list<juce::var> items)
    {
        juce::Array<juce::var> a;
        for (const auto& v : items) a.add (v);
        return a;
    }
    juce::var strings (std::initializer_list<const char*> items)
    {
        juce::Array<juce::var> a;
        for (auto* s : items) a.add (juce::var (s));
        return a;
    }
    juce::var type (const char* t) { return obj ({ { "type", t } }); }
}

juce::var ReplyFormat::schema()
{
    const auto noteSchema = obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strings ({ "pitch", "start_beat", "duration_beats", "velocity" }) },
        { "properties", obj ({ { "pitch", type ("integer") }, { "start_beat", type ("number") },
                               { "duration_beats", type ("number") }, { "velocity", type ("integer") } }) } });

    const auto changeSchema = obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strings ({ "id", "value" }) },
        { "properties", obj ({ { "id", type ("string") }, { "value", type ("number") } }) } });

    const auto frameSchema = obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strings ({ "harmonics" }) },
        { "properties", obj ({ { "harmonics", obj ({ { "type", "array" }, { "items", type ("number") } }) } }) } });

    const auto wavetableSchema = obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strings ({ "name", "frames" }) },
        { "properties", obj ({ { "name", type ("string") },
                               { "frames", obj ({ { "type", "array" }, { "items", frameSchema } }) } }) } });

    const auto stepSchema = obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strings ({ "kind", "velocity", "gate", "note_offset" }) },
        { "properties", obj ({ { "kind", obj ({ { "type", "string" }, { "enum", strings ({ "note", "rest", "tie" }) } }) },
                               { "velocity", type ("number") }, { "gate", type ("number") },
                               { "note_offset", type ("integer") } }) } });

    const auto arpSchema = obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strings ({ "name", "steps" }) },
        { "properties", obj ({ { "name", type ("string") },
                               { "steps", obj ({ { "type", "array" }, { "items", stepSchema } }) } }) } });

    return obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strings ({ "reply", "notes_kind", "notes", "param_changes", "wavetable", "preset_name", "arp_pattern" }) },
        { "properties", obj ({
            { "reply", type ("string") },
            { "notes_kind", obj ({ { "type", "string" },
                                   { "enum", strings ({ "none", "melody", "chords", "drums", "bass", "harmony",
                                                        "variation", "continuation", "phrase" }) } }) },
            { "notes", obj ({ { "type", "array" }, { "items", noteSchema } }) },
            { "param_changes", obj ({ { "type", "array" }, { "items", changeSchema } }) },
            { "wavetable", wavetableSchema },
            { "preset_name", type ("string") },
            { "arp_pattern", arpSchema } }) } });
}

juce::String ReplyFormat::systemPrompt (const juce::String& catalog)
{
    return juce::String (juce::CharPointer_UTF8 (
R"(あなたは WaveForge (Serum 風のウェーブテーブル・シンセサイザー VST) に内蔵された作曲・音作りアシスタントです。
ユーザーは日本語で話します。返答 (reply) は日本語で、短く具体的に。必ず指定の JSON 形式で返します。

## 返し方のルール
- すべてのフィールドを必ず埋める。該当しないものは: notes = [], notes_kind = "none", param_changes = [],
  wavetable = {"name":"","frames":[]}, preset_name = "", arp_pattern = {"name":"","steps":[]}
- フレーズ (メロディ / コード / ドラム / ベース / ハモリ / 続き / バリエーション / 自由なフレーズ) を求められたら
  notes に入れ、notes_kind を選ぶ。reply には狙い (キー、リズムの特徴、使い方) を 2〜4 文で。
- 音作り (音を作る / 調整する) を求められたら param_changes に変更するパラメータだけを入れる。
  値は各パラメータ固有の単位 (Hz, ms, dB, 0..1, choice はインデックス) で書く。変更しないものは入れない。
  reply でなぜその値にしたかを説明する。新しい音を一から作るときは preset_name に名前を付ける。
- 「解説して」と言われたら reply で説明し、param_changes は空にする。
- ウェーブテーブルを設計するときは wavetable.frames に 2〜8 個のキーフレームを入れる。各フレームは倍音 1 次から順の
  振幅 (0..1、最大 64 個)。フレーム間は自動で補間される。設計したら param_changes で osc_a_wt_pos などを合わせる。
- アルペジオのパターンを求められたら arp_pattern に 1〜32 ステップを入れる (kind: note/rest/tie、velocity 0..1、
  gate 0.05..1、note_offset は通常 0)。必要なら param_changes で arp_enabled=1, arp_division, arp_octaves も設定する。

## 音楽のルール
- 拍は beat 単位。4/4 なら 1 小節 = 4 beat、start_beat は 0 から (フレーズの先頭)。要望の小節数にきっちり収める。
- 取り込んだパート (own_part) やコンテキストトラックがあれば、そのキー・コード・リズムに合わせる。
  無ければ reply で仮定したキーとテンポを述べる。
- コードは同時発音のノートで表す (同じ start_beat)。ドラムは General MIDI のノート番号
  (36 Kick, 38 Snare, 42 Closed HH, 46 Open HH, 49 Crash, 51 Ride, 37 Rim, 39 Clap, 45/47/50 Toms)。
- velocity は 1..127。人間らしい強弱を付ける。
- ハモリは元メロディと同じリズムで、ダイアトニックな 3 度 / 6 度を基本にする。

## パラメータ・カタログ (id  "名前"  型 範囲 単位 既定値)
)")) + catalog + juce::String (juce::CharPointer_UTF8 (
R"(
## 音作りの指針
- master_volume_db は触らない。レベルは osc_*_level / sub_level / noise_level と fx の mix で作る。
- ウェーブテーブルは osc_a_table / osc_b_table では選べない。既存テーブルを使う場合は reply で
  「Basic Shapes / Sine / Harmonics / PWM のどれを OSC A/B に」と書き、位置は osc_*_wt_pos で指定する。
  Basic Shapes: 0=sine, 0.33=triangle, 0.66=saw, 1.0=square。Harmonics: 位置が高いほど倍音が増える。PWM: 位置で幅が狭くなる。
- フィルターは filter_cutoff (Hz) と filter_resonance (0..1)、動きは filter_env2_amount (半音) と env2_*、
  または mod スロット (mod1_enabled=1, mod1_source, mod1_dest, mod1_amount) で。
  mod source: 0=None 1=Env1 2=Env2 3=LFO1 4=LFO2 5=Velocity 6=Mod Wheel 7=Aftertouch 8=Key Track 9=Random。
  mod dest: 0=None 1=OSC A WT Pos 2=OSC B WT Pos 3=OSC A Pitch 4=OSC B Pitch 5=OSC A Level 6=OSC B Level
  7=OSC A Pan 8=OSC B Pan 9=OSC A Detune 10=OSC B Detune 11=Sub Level 12=Noise Level 13=Filter Cutoff
  14=Filter Reso 15=Filter Drive 16=Amp Level 17=OSC A Warp。
- 現在の設定 (既定値と異なるものだけ) は毎回渡される。「調整」の依頼ではそれを起点に少しだけ変える。
)"));
}

bool ReplyFormat::parse (const juce::var& json, ChatMessage& out, juce::String& error)
{
    if (! json.isObject())
    {
        error = "Reply is not a JSON object.";
        return false;
    }
    out.role = ChatMessage::assistant;
    out.text = json.getProperty ("reply", "").toString();
    out.notesKind = json.getProperty ("notes_kind", "none").toString();
    out.notes = MusicContext::notesFromVar (json.getProperty ("notes", juce::var()));
    if (out.notes.empty()) out.notesKind = "none";
    out.paramChanges = json.getProperty ("param_changes", juce::var());
    if (! out.paramChanges.isArray()) out.paramChanges = juce::Array<juce::var>();
    out.wavetable = json.getProperty ("wavetable", juce::var());
    out.presetName = json.getProperty ("preset_name", "").toString();

    const auto arp = json.getProperty ("arp_pattern", juce::var());
    juce::String arpError;
    if (arp.isObject() && arp.getProperty ("steps", juce::var()).size() > 0)
        wf::ArpPattern::fromJson (arp, out.arpPattern, arpError);
    if (out.text.isEmpty() && ! out.hasNotes() && ! out.hasParamChanges() && ! out.hasWavetable() && ! out.hasArpPattern())
    {
        error = "The reply was empty.";
        return false;
    }
    return true;
}

bool ReplyFormat::wavetableFrames (const juce::var& wavetable, std::vector<float>& frames, int& numFrames, juce::String& name)
{
    namespace Ops = wf::WaveEditOps;
    constexpr int N = wf::Wavetable::frameSize;
    const auto* keyFrames = wavetable.getProperty ("frames", juce::var()).getArray();
    if (keyFrames == nullptr || keyFrames->isEmpty())
        return false;
    name = wavetable.getProperty ("name", "AI Table").toString().trim();
    if (name.isEmpty()) name = "AI Table";

    // synthesise each key frame from its harmonic amplitudes (sine phases)
    std::vector<std::vector<float>> keys;
    for (const auto& kf : *keyFrames)
    {
        const auto* amps = kf.getProperty ("harmonics", juce::var()).getArray();
        if (amps == nullptr || amps->isEmpty()) continue;
        Ops::Harmonics h;
        for (int k = 1; k <= juce::jmin (Ops::numHarmonics, amps->size()); ++k)
        {
            h.magnitude[(size_t) k] = juce::jlimit (0.0f, 1.0f, (float) (double) (*amps)[k - 1]);
            h.phase[(size_t) k] = -juce::MathConstants<float>::halfPi;
        }
        std::vector<float> frame ((size_t) N);
        Ops::synthesise (h, frame.data());
        Ops::normalise (frame.data());
        keys.push_back (std::move (frame));
        if ((int) keys.size() >= 8) break;
    }
    if (keys.empty())
        return false;

    if (keys.size() == 1)
    {
        frames = keys[0];
        numFrames = 1;
        return true;
    }

    // place key frames evenly across the target and morph between them
    numFrames = wavetableTargetFrames;
    frames.assign ((size_t) numFrames * N, 0.0f);
    std::vector<int> positions;
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const int pos = (int) std::lround ((double) i * (numFrames - 1) / (double) (keys.size() - 1));
        positions.push_back (pos);
        std::copy (keys[i].begin(), keys[i].end(), frames.begin() + (size_t) pos * N);
    }
    for (size_t i = 0; i + 1 < positions.size(); ++i)
        Ops::morph (frames.data(), numFrames, positions[i], positions[i + 1]);
    return true;
}

} // namespace ai
