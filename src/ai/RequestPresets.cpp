#include "RequestPresets.h"

namespace ai
{

namespace
{
    RequestPreset make (const char* category, const char* name, const char* text)
    {
        return { juce::CharPointer_UTF8 (category), juce::CharPointer_UTF8 (name), juce::CharPointer_UTF8 (text), true };
    }
}

const std::vector<RequestPreset>& RequestPresets::builtins()
{
    static const std::vector<RequestPreset> presets
    {
        // --- メロディ
        make ("メロディ", "合うメロディ 4 小節", "取り込んだパートに合う 4 小節のメロディを作って。歌えるシンプルなリズムで、音域は C4〜C6。"),
        make ("メロディ", "切ないメロディ 8 小節", "切ない雰囲気の 8 小節のメロディを作って。ロングトーンと休符を活かし、最後は解決しないで終わって。"),
        make ("メロディ", "キャッチーなフック", "サビで使えるキャッチーな 4 小節のフックを作って。跳ねたリズムで、2 小節ごとに同じモチーフを繰り返して。"),
        make ("メロディ", "静かなイントロ", "イントロ向けの静かな 4 小節のメロディ。音数は少なめ、8 分より長い音符中心で。"),
        make ("メロディ", "対旋律", "取り込んだメロディに対する対旋律 (カウンターライン) を 4 小節作って。主旋律が動くところは長く、休むところで動いて。"),
        make ("メロディ", "ゲーム音楽風", "8bit ゲーム音楽風の元気な 4 小節のメロディ。16 分の細かい動きとオクターブ跳躍を入れて。"),

        // --- コード
        make ("コード", "コード進行 4 小節", "取り込んだパートに合う 4 小節のコード進行を作って。1 小節 1 コード、4 声で、ボイシングは C3〜C5 に収めて。"),
        make ("コード", "ジャジーな進行", "テンション (7th, 9th) を含むジャジーな 4 小節のコード進行。2 小節目と 4 小節目は 2 コード入れて。"),
        make ("コード", "エモい進行 8 小節", "エモーショナルな 8 小節のコード進行 (王道進行の変形でよい)。最後は次のセクションに戻れる形で。"),
        make ("コード", "代理コード版", "取り込んだコード進行を、代理コードと転回形を使って響きを変えたバージョンにして。小節数は同じで。"),
        make ("コード", "ロングパッド", "パッド向けに 4 小節を 1 コード 2 小節で伸ばすシンプルな進行。ボイシングは広く。"),

        // --- ドラム
        make ("ドラム", "Lo-fi ドラム", "Lo-fi ヒップホップのドラムパターンを 2 小節。少しレイドバックしたスネア、ゴーストノート付きで。GM ノート番号で。"),
        make ("ドラム", "4 つ打ちハウス", "4 つ打ちのハウスのドラム 2 小節。裏拍のオープンハイハット、16 分のクローズドハイハット、2・4 拍にクラップ。"),
        make ("ドラム", "トラップ", "トラップのドラム 2 小節。ハイハットは 16 分と 32 分のロールを混ぜ、808 キックは少なめに。"),
        make ("ドラム", "ロック 8 ビート", "ロックの 8 ビート 2 小節。2 小節目の終わりにフィルを入れて。"),
        make ("ドラム", "ブレイクビーツ", "ジャングル / ブレイクビーツ風の細かいドラム 2 小節。スネアは複雑に、キックはシンコペーション。"),
        make ("ドラム", "ボサノバ", "ボサノバ風の軽いパーカッションパターン 2 小節。リムショットとシェイカー中心で。"),

        // --- ベース / ハモリ / 展開
        make ("ベース", "ベースライン (8 分)", "取り込んだコードに合う 8 分音符中心のベースライン 4 小節。ルートと 5 度を基本に、小節の終わりに経過音を。"),
        make ("ベース", "ファンキーなベース", "16 分のシンコペーションが効いたファンキーなベースライン 4 小節。休符を活かして。"),
        make ("ベース", "シンプルなルート", "ルート音を全音符で伸ばすだけのシンプルなベース 4 小節。"),
        make ("ハモリ", "3 度下のハモリ", "取り込んだメロディに 3 度下のハモリを付けて。キーに合わせてダイアトニックに。"),
        make ("ハモリ", "6 度上のハモリ", "取り込んだメロディに 6 度上のハモリを付けて。跳躍が大きいところは 3 度に切り替えて。"),
        make ("展開", "続きを 4 小節", "取り込んだフレーズの続きを 4 小節作って。モチーフを引き継ぎつつ、後半で少し高い音域へ。"),
        make ("展開", "バリエーション", "取り込んだフレーズのバリエーションを作って。リズムは維持して音程を変えて。"),
        make ("展開", "リズム変奏", "取り込んだフレーズの音程はそのままで、リズムだけ変えたバリエーションを作って。"),
        make ("展開", "Bメロ案", "取り込んだフレーズを A メロとして、対比になる B メロ 8 小節を作って。"),

        // --- アルペジオ (arp_pattern を返させる)
        make ("アルペジオ", "16 分の上昇", "アルペジエーターのパターンを作って: 16 分で上昇、2 オクターブ、全ステップ note でベロシティに強弱を。arp_pattern で返して。"),
        make ("アルペジオ", "トランス 3-3-2", "トランス風の 3-3-2 のリズムのアルペジオパターン (8 ステップ、tie で長さを作る) を arp_pattern で返して。"),
        make ("アルペジオ", "休符多めランダム", "休符を多めに入れたスカスカのアルペジオパターン 16 ステップを arp_pattern で。たまに note_offset で音を飛ばして。"),
        make ("アルペジオ", "ゲート刻み", "ゲートを短く刻んだアルペジオパターン 16 ステップ。4 拍目だけ長い音 (tie) を入れて arp_pattern で。"),

        // --- 音作り
        make ("音作り", "80s ブラスリード", "80s のシンセブラス風のリード音を作って。少し太めのユニゾン、ややアタック速め、明るめのフィルター。ウェーブテーブルも設計して。"),
        make ("音作り", "深いサブベース", "深いサブベース。Sub を中心にサイン寄り、アタックは速く、フィルターは低め、FX は控えめに。"),
        make ("音作り", "広がるパッド", "広がりのあるパッド。遅いアタックと長いリリース、ユニゾン広め、Chorus と Reverb を使って。LFO でゆっくり WT Pos を動かして。"),
        make ("音作り", "リースベース", "リース (Reese) ベース。デチューンしたノコギリ波の重なり、LP フィルターを少し閉じて、軽い Distortion。"),
        make ("音作り", "ベル系プラック", "ベルのようなプラック音。倍音が整数比の透明なウェーブテーブルを設計して、短いディケイと長いリリース、Delay を少し。"),
        make ("音作り", "レトロなリード", "チップチューン風のレトロなリード。矩形波系、ユニゾンなし、ビブラート (LFO → Pitch) 少し。"),
        make ("音作り", "エレピ風キー", "エレクトリックピアノ風のキー音。Env2 で WT Pos を動かしてアタックにきらめきを、Chorus 薄く。"),
        make ("音作り", "SF 効果音", "SF っぽい効果音。FM from B を使い、LFO でピッチとフィルターを大きく揺らして。"),

        // --- 調整
        make ("調整", "もっと明るく", "今の音をもっと明るくして (フィルターや WT Pos、EQ の高域で)。"),
        make ("調整", "暖かく", "今の音を暖かくして。高域を抑え、少しだけ Drive を足して。"),
        make ("調整", "広げる", "今の音の広がりを増やして (ユニゾン、幅、Chorus、Reverb)。"),
        make ("調整", "アタック短く", "今の音のアタックを短くして、立ち上がりをはっきりさせて。"),
        make ("調整", "歪みを足す", "今の音に程よい歪みを足して。うるさくなりすぎないように。"),
        make ("調整", "動きを付ける", "今の音に LFO やエンベロープで動きを付けて (WT Pos やフィルターの変化)。"),
        make ("調整", "この音を解説", "今の音の仕組みを、各セクションが何をしているか初心者向けに説明して。改善案があれば 2 つ挙げて (パラメータは変えないで)。"),
    };
    return presets;
}

juce::StringArray RequestPresets::categories()
{
    juce::StringArray cats;
    for (const auto& p : all())
        cats.addIfNotAlreadyThere (p.category);
    return cats;
}

juce::File RequestPresets::userFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("WaveForge").getChildFile ("RequestPresets.json");
}

std::vector<RequestPreset> RequestPresets::loadUser()
{
    std::vector<RequestPreset> out;
    const auto f = userFile();
    if (! f.existsAsFile())
        return out;
    if (const auto* arr = juce::JSON::parse (f.loadFileAsString()).getArray())
        for (const auto& v : *arr)
        {
            RequestPreset p;
            p.category = v.getProperty ("category", "").toString();
            p.name = v.getProperty ("name", "").toString();
            p.text = v.getProperty ("text", "").toString();
            p.builtin = false;
            if (p.name.isNotEmpty() && p.text.isNotEmpty())
                out.push_back (p);
        }
    return out;
}

bool RequestPresets::saveUser (const std::vector<RequestPreset>& presets, juce::String& error)
{
    juce::Array<juce::var> arr;
    for (const auto& p : presets)
    {
        if (p.builtin) continue;
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("category", p.category);
        obj->setProperty ("name", p.name);
        obj->setProperty ("text", p.text);
        arr.add (juce::var (obj));
    }
    const auto f = userFile();
    if (! f.getParentDirectory().createDirectory() || ! f.replaceWithText (juce::JSON::toString (juce::var (arr))))
    {
        error = "Could not write " + f.getFullPathName();
        return false;
    }
    return true;
}

std::vector<RequestPreset> RequestPresets::all()
{
    auto out = builtins();
    for (auto& p : loadUser())
        out.push_back (p);
    return out;
}

juce::String RequestPresets::firstText (const juce::String& category)
{
    for (const auto& p : builtins())
        if (p.category == category)
            return p.text;
    return {};
}

} // namespace ai
