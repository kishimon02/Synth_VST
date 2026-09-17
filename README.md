# WaveForge

Studio One 4 (Windows x64) 向けの Serum 風ウェーブテーブル・シンセサイザー VST3。
3D ウェーブテーブル表示と、LLM (Claude API) によるメロディ / コード / ドラム提案機能を持つ。

- 設計: [docs/ER.md](docs/ER.md) (データモデル)、`~/.claude/plans/` の実装計画
- フレームワーク: JUCE 8 (CMake FetchContent で自動取得)
- コンパイラ: Visual Studio 2022 Build Tools (MSVC)。エディタは VS Code

## 必要なもの

| ツール | 入手 |
|---|---|
| CMake 3.22+ | 導入済み (4.2) |
| Visual Studio 2022 Build Tools + "C++ によるデスクトップ開発" ワークロード | `winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"` |
| Git | 導入済み |

## ビルド (コマンドライン)

```bash
cmake --preset vs2022
cmake --build --preset vs2022-release
```

初回は JUCE のダウンロードとビルドで 5〜10 分かかる。JUCE を手元に clone 済みなら
`cmake --preset vs2022 -DFETCHCONTENT_SOURCE_DIR_JUCE=<path>` でダウンロードを省略できる
(この開発機では `build/_juce-src` に 8.0.15 を shallow clone 済み)。成果物:

- Standalone: `build/vs2022/WaveForge_artefacts/Release/Standalone/WaveForge.exe`
- VST3: `build/vs2022/WaveForge_artefacts/Release/VST3/WaveForge.vst3/` (JUCE のバンドル形式)
  → ビルド後に **単一ファイル形式** `C:\Program Files\Common Files\VST3\kishimon\WaveForge.vst3` として自動コピーされる。
  - Studio One 4.x のスキャナーはバンドル (フォルダ) 形式を認識せず、さらに VST3 は
    `Common Files\VST3` 配下からしか読み込まない (追加ロケーションは効かなかった)。
  - `kishimon` フォルダは管理者権限で 1 回だけ作成し、現在のユーザーに変更権限を付与済み:
    ```powershell
    # 管理者 PowerShell で 1 回だけ
    New-Item -ItemType Directory -Force "C:\Program Files\Common Files\VST3\kishimon"
    icacls "C:\Program Files\Common Files\VST3\kishimon" /grant "$env:USERDOMAIN\$env:USERNAME:(OI)(CI)M" /T
    ```

## ビルド (VS Code)

CMake Tools 拡張が `CMakePresets.json` を読む。コマンドパレットで
「CMake: Select Configure Preset」→ `ninja-msvc` (要 MSVC kit) または `vs2022` を選び、
「CMake: Build」。

## Studio One 4 で認識させる

1. 上記のとおり `C:\Program Files\Common Files\VST3\kishimon\WaveForge.vst3` に配置されていることを確認
2. オプション → 場所 → VST プラグイン で「起動時にスキャン」にチェックを入れ、Studio One を **再起動** する
   (スキャンは起動時に行われる。`Common Files\VST3` は標準ロケーションなので追加不要)
3. ブラウザの「インストゥルメント」→ ベンダー **kishimon** (または「全体」表示) に **WaveForge** が現れるので、
   インストゥルメント・トラックに挿入する
4. 認識されない場合は `%APPDATA%\PreSonus\Studio One 4\x64\PlugInScanner.log` を確認する

## DSP ユニットテスト

```bash
cmake --build --preset vs2022-release --target WaveForgeTests
```

`build/vs2022/WaveForgeTests_artefacts/Release/WaveForgeTests.exe` を実行。ミップマップの帯域制限精度、
オシレーターの周波数精度、ADSR のタイミング、LFO の周波数とテンポ同期、モジュレーションマトリクスの加算、
FX (ディレイのサンプル精度とフィードバック、EQ の利得、ディストーションの上限、リバーブの減衰、バイパス時の完全一致)、
波形編集 (倍音の分析/合成の往復、フレーム操作、Serum 互換 .wav の保存/読込の往復)、Warp の効果と有限性、
アルペジエーター (ステップのタイミング、ゲート、Rest / Tie、オクターブ、ホスト同期、パターン JSON)、
AI (パラメータカタログと変更の適用、キー推定、.mid 書き出しの往復、応答 JSON の解釈、リクエスト本文の形、DPAPI)、
コード進行 (度数の解釈、キーと転調、ボイシングとボイスリーディング、内蔵 71 種の妥当性)、
会話履歴 (メッセージの JSON 往復、セッションの保存/読込、新しい順の一覧、30 日での削除)、
プリセットのパラメータ ID 妥当性と保存/読込、エンジン全体のレンダリング (有限・不連続なし・解放後に無音) を検証する。

## プリセット

- ファクトリー: Init / Supersaw Lead / Soft Pad / Wobble Bass / Glass Bell / Pluck Echo (コード内で定義、インストール不要)
- ユーザー: 画面上部の「Save As...」で `%APPDATA%\WaveForge\Presets\*.wfpreset` に保存、「Load...」で読込
- ソング保存時の状態と同じ内容を書き出すので、プリセットとソングで設定が食い違わない

## 現在の状態

- Phase 0 (環境・骨組み) 完了
- Phase 1 (ウェーブテーブル・エンジン) 完了: WT Osc A/B (ユニゾン 8 まで、ミップマップ帯域制限)、Sub、Noise、
  SVF フィルター (LP/HP/BP 12/24dB、ドライブ、キートラック、Env2 モジュレーション)、ADSR×2、Serum 互換 .wav 読込、
  内蔵テーブル (Basic Shapes / Sine / Harmonics / PWM)
- Phase 2 (モジュレーション) 完了:
  - LFO×2: Sine / Triangle / Saw / Square / S&H、Hz 指定とテンポ同期 (8 小節〜1/32、3 連・付点)、
    リトリガー / フリーラン (フリーラン時は全ボイスが位相同期)、ユニポーラ切替
  - モジュレーションマトリクス 8 スロット: ソース 9 種 (Env1/2、LFO1/2、ベロシティ、モジュレーションホイール、
    アフタータッチ、キートラック、ノートごとのランダム)、デスティネーション 16 種
  - プリセット (上記)、ホストからのテンポ取得
- Phase 3 (FX チェーン) 完了: マスター段に固定順 Distortion → EQ → Chorus → Delay → Reverb (下記)
- Phase 4 (3D 表示・本 UI) 完了: OpenGL の 3D ウェーブテーブル表示、出力スコープ、タブ式 UI (下記)
- Phase 4.5 (音作り・解析) 完了: ウェーブテーブル・エディタ、2D / スペクトル表示、出力の 3D スペクトログラム、
  OSC A Warp (FM / RM / AM from B)
- Phase 5 (AI アシスタント + アルペジエーター) 完了 (下記)
- コード進行ライブラリ (CHORD タブ) 追加
- 次: Phase 6 (仕上げ)

## コード進行 (CHORD タブ)

キー → カテゴリ → コード進行の順に選ぶと、その場でボイシングされた MIDI ができる。API キーは要らない。

- **キー**: 24 種 (C major 〜 B minor)。選んだキーのモードに合う進行だけが一覧に出る (単体コードは両方に出る)
- **カテゴリ**: 王道・ポップス / カノン・循環 / J-POP・切ない / ロック・EDM / ジャズ・おしゃれ / ブルース・ファンク /
  バラード・映画 / 単体コード。内蔵 71 種 + 自作分
- **コード進行**: 選ぶと度数 (ディグリー) が下の Degrees 欄に入り、ピアノロールと各コードのボタンが更新される。
  コードのボタンを押すとそのコードだけ試聴できる
- **Oct / 繰り返し / Bass**: ボイシングの高さ、1x〜4x の繰り返し、ルートを 1 オクターブ下に足すかどうか
- **Audition** で全体を試聴、**Drag to DAW** を Studio One のトラックへドロップ、**Save .mid...** でファイル保存
- **AI に渡す**: 「キー A minor、コード進行 Am - F - C - G (全 4 小節) に合うメロディを作って。」のような依頼文を
  AI タブの入力欄に入れる (送信はしないので、書き足してから Send)

進行は度数で保存されているのでどのキーでも使える。Degrees 欄は手で書き換えられる (`Apply` または Enter で反映):

- 度数: `I ii iii IV V vi vii` (大文字 = メジャー系、小文字 = マイナー系)、`bVII` `#Idim7` のような臨時記号も可
- 種類: `M7 m7 7 6 m6 9 M9 m9 add9 sus2 sus4 7sus4 dim dim7 m7b5 aug mM7 5 13 7b9`
- オンコード: `IV/V`、1 コードの長さ (拍): `I:2`。1〜32 コード
- **Save...** で自作の進行を `%APPDATA%\WaveForge\ChordProgressions.json` に登録、**Delete** で削除

## アルペジエーター (MOD タブ)

On / Mode (Up, Down, Up-Down, Down-Up, As Played, Random, Chord) / Rate (LFO と同じ 15 分割) / Octaves 1〜4 /
Gate / Swing / Latch / Pattern。ホスト再生中は PPQ に同期し (ループや頭出しに追従)、停止中と Standalone では内部クロック。
パターンは内蔵 10 種 + **Custom**。ステップ表示をクリックで note → rest → tie、上下ドラッグでベロシティ、右クリックで
note_offset (音の飛ばし)、- / + でステップ数 (1〜32)。編集すると Custom になり、プリセット / ソングに JSON で保存される。
Load... / Save... で `%APPDATA%\WaveForge\ArpPatterns\*.json` に共有できる。

## AI アシスタント (AI タブ)

Claude API (既定 `claude-opus-5`) または OpenAI 互換 API に、作りかけのパートと現在の音の設定を渡して、
フレーズ (メロディ / コード / ドラム / ベース / ハモリ / 続き / バリエーション)、音作り (パラメータ変更 +
ウェーブテーブル設計)、アルペジオパターン、解説を JSON で返してもらう。チャット形式なので「もう少し遅く」の
ような追い指示ができる。

### 準備

1. AI タブ右上の **Settings** でプロバイダーと API キー、モデルを入力し、**Test connection** → **Save**
2. キーは Windows DPAPI で暗号化して `%APPDATA%\WaveForge\settings.json` に保存される。ソング / プリセット /
   ログには一切入らない

### 使い方

- **取り込み**: **Rec** を押してから鍵盤 / MIDI キーボードで演奏、または Studio One を再生 (このトラックに来る MIDI が
  記録される)。もう一度押すと確定し、「12 notes, 2 bars, key A minor」のように表示される。他トラックは Studio One から
  書き出した `.mid` を **Add .mid...** かドラッグ&ドロップで追加
- **依頼**: 入力欄に日本語で書くか、**Presets** (カテゴリ別の依頼文ライブラリ) から選ぶ。下のクイックボタンは
  各カテゴリの代表的な依頼を入力欄に入れる。どちらも送信はしないので、書き足してから **Send** (Enter) で送る。
  送信中はチャット末尾に「AI の回答を待っています... N s」が出て、**Cancel** で中止できる。
  自分で書いた依頼は **Save** でユーザープリセットに登録できる (`%APPDATA%\WaveForge\RequestPresets.json`)
- **フレーズの結果**: ピアノロール表示。**Audition** でこのシンセで試聴、**Drag to DAW** を Studio One のトラックへ
  ドラッグすると `.mid` として置ける、**Save .mid...** で保存
- **音の結果**: 変更一覧 (現在値 → 新しい値)。**Apply** で反映、**Undo** で直前の状態に戻す。ウェーブテーブルが
  設計されていれば **Apply wavetable to OSC A** で `%APPDATA%\WaveForge\Wavetables\` に保存して読み込む
- **アルペジオの結果**: **Apply pattern** で Custom パターンに設定、**Save as preset...** で JSON 保存
- **モデルと思考の強さ**: 入力欄の下 (強さは Send の真下に右寄せ) で、このあとの依頼に使うモデル (claude-opus-5 / sonnet-5 / haiku-4-5、
  OpenAI 互換なら gpt-5 系) と推論の強さ (Low / Medium / High) を切り替えられる。選択は設定ファイルに保存され、
  Settings ダイアログと同じ値を見ている (強さは Anthropic のみ)
- **履歴**: 会話はセッションごとに自動保存される。**History** を押すと過去 30 日間の会話が日付別 (今日 / 昨日 / 日付) に
  並び、時刻・最初の依頼・メッセージ数から選んで開ける。開いた会話はそのまま続けて指示できる (モデルに渡す履歴も復元
  される)。**New chat** で新しいセッションを始める。保存先は `%APPDATA%\WaveForge\Chats\chat-<日時>.json`
  (30 日を過ぎたものは起動時に自動削除。「保存フォルダを開く」でフォルダを開ける)

### 実装メモ

- 1 回の依頼 = `POST /v1/messages` 1 回 (非ストリーミング、構造化出力 `output_config.format` = JSON スキーマ)。
  役割・ルール・パラメータカタログ (全 146 パラメータの ID / 範囲 / 既定値、APVTS から自動生成) は system の
  安定ブロックとして `cache_control` を付け、現在の設定値と音楽コンテキストは毎回変わる 2 つ目のブロックに置く
- 会話履歴は直近 8 往復。エラー時 (401 / 429 / 接続失敗 / refusal / max_tokens) は日本語のメッセージで表示
- HTTP は `juce::URL` (WinHTTP)。リクエストはバックグラウンド 1 スレッド、UI は Cancel で中断できる

## UI

- ヘッダー: プリセット、Master / Voices / Bend
- **OSC** タブ: OSC A / B (テーブル選択、.wav 読込、3D 表示、ピッチ・レベル・ユニゾンのノブ)、Sub / Noise、
  Filter (特性表示付き)、Env 1 / 2
- **MOD** タブ: LFO 1 / 2、アルペジエーター、モジュレーションマトリクス 8 スロット
- **CHORD** タブ: コード進行ライブラリ (下記)
- **AI** タブ: AI アシスタント (下記)
- **FX** タブ: 5 ユニットの全パラメータ
- **SCOPE** タブ: 出力の 3D スペクトログラム (20 Hz〜20 kHz 対数軸、手前が最新)、ピークホールド付き 2D スペクトラム、
  大きな波形スコープ。30 fps で 4096 点 FFT を 1 回だけ行い、3 つの表示で共有する。タブが隠れている間は何もしない
- 下部: 出力スコープ (立ち上がりゼロクロスでトリガー) と鍵盤

### ウェーブテーブル表示 (OSC パネル)

ヘッダーの **3D / 2D / SP** で切替。3D はテーブルを最低 128 本の線 (フレーム間はオシレーターと同じ補間) で
面として描き、WT Position の位置の補間済みフレームを白で重ねる。2D は現在位置の (補間済み) 1 フレームを
全幅で表示、SP はそのフレームの倍音 (1〜64 次) を棒グラフで表示。

### エンベロープ表示 (ENV パネル)

ADSR の曲線を表示する。実際のボイスが使う `wf::Envelope` をそのまま 1 kHz で走らせて描くので、
アタックのオーバーシュート型カーブや指数的なディケイ/リリースが音と一致する。3 つの点をドラッグすると
Attack (横)、Decay (横) + Sustain (縦)、Release (横) が変わる。発音中は最後に鳴らしたボイスの位置が
白い点で曲線上を動く。

### フィルター表示 (FILTER パネル)

右側に周波数特性を表示。**2D** は現在の Cutoff / Reso / Type の振幅特性 (20 Hz〜20 kHz 対数軸、-60〜+24 dB)、
**3D** は Cutoff を 20 Hz〜20 kHz に振ったときの特性を奥行きに並べ、現在の Cutoff の特性を白で重ねる。
表示はアナログ 2 次プロトタイプ (TPT SVF の元) の計算値で、音声処理は行わない。

### ウェーブテーブル・エディタ (OSC パネルの Edit)

自分だけの波形を作るモード。開くと現在のテーブルをコピーして編集する。

- **波形を描く**: 波形エリアをドラッグ (Shift でドラッグ開始点からの直線)
- **倍音を描く**: 下の倍音バー (1〜64 次) をドラッグ。位相は保持され、ピークが 1 を超えたら自動で正規化
- **フレーム操作**: < > で移動、Add / Dup / Del、Morph 1→N (先頭と末尾のフレームの間を補間して中間フレームを生成)、
  下の帯をクリックしてフレーム選択
- **形状と加工**: Sine / Tri / Saw / Square / Pulse、Normalize / Invert / Reverse / Remove DC / Smooth / Clear
- **Undo / Redo**: ボタンまたは Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z)。描画は 1 ストロークで 1 手、フレーム操作は表全体で 1 手 (64 手まで)
- **Apply to OSC**: `%APPDATA%\WaveForge\Wavetables\<名前>.wav` に Serum 互換 (clm チャンク付き 32-bit float) で保存し、
  そのオシレーターに読み込む。プリセット / ソングにはそのパスが記録される。**Save .wav...** で任意の場所にも保存できる
  (Serum でそのまま読める)

### OSC A Warp

OSC B の生の出力 (レベル・パン適用前) で OSC A を変調する。B の On が OFF でも変調源として動く。

| モード | 動作 |
|---|---|
| FM from B | 位相変調。Amount 1.0 で最大 ±半周期 |
| Ring Mod B | A × B。Amount 0 でドライ、1 で完全なリングモジュレーション |
| AM from B | 単極 (0〜1) の B で A の振幅を揺らす |

Warp Amount はモジュレーションマトリクスのデスティネーション「OSC A Warp」で動かせる。

3D 表示 (`src/ui/Wavetable3DView`) はフレームごとに 1 本のポリライン (256 点) を奥行き方向に並べ、
WT Position のフレームを白で強調する。頂点データはテーブルが変わったときだけ VBO に転送し、再描画は
表示中のみ 30 fps のタイマーで要求する (連続描画はしない)。ドラッグで回転、ホイールでズーム、
ダブルクリックで視点リセット。OpenGL が使えない環境では 2D の重ね描きにフォールバックする。

## FX チェーン

ボイスをミックスした後、マスターボリュームの前に固定順で通る。各ユニットは個別に ON/OFF でき、
OFF のユニットは処理を完全にスキップする (全 OFF ならバッファに触らない)。OFF → ON にした瞬間に
そのユニットの内部状態をリセットするので、古い残響やディレイが突然鳴り出すことはない。

| ユニット | パラメータ | 実装 |
|---|---|---|
| Distortion | Mode (Soft / Hard / Fold)、Drive 0〜40 dB、Oversample 2x、Output、Mix | 波形整形 + 2x オーバーサンプリング (ポリフェーズ IIR ハーフバンド) + DC ブロッカー |
| EQ | Low shelf / Mid peak (Q) / High shelf、各 ±18 dB | RBJ バイクアッド (係数はパラメータが変わったときだけ再計算、ヒープ不使用) |
| Chorus | Rate、Depth、Feedback、Delay (中心)、Mix | `juce::dsp::Chorus` |
| Delay | Tempo Sync / Time (ms) / Division (LFO と同じ 15 分割)、Feedback、Lowpass、Ping Pong、Mix | 自前の線形補間ディレイライン (最大 5 秒)、時間変更は 50 ms でグライド |
| Reverb | Size、Damping、Width、Predelay (0〜250 ms)、Mix | `juce::Reverb` (Freeverb 系、軽量) + プリディレイ |

Delay か Reverb が ON のときはホストにテール 6 秒を申告するので、バウンス時に余韻が切れない。

## 音のテスト時の注意

- **Bluetooth イヤホンでは判断しない。** 純粋なサイン波など単純な音は Bluetooth コーデック/電波の微小な
  途切れが「パチパチ」として聞こえる (Phase 0 で計測済み: プラグイン出力とループバック録音はクリーン、
  スピーカー出力では無音)。有線イヤホンかスピーカーで確認する。
- グリッチ調査には `-DWAVEFORGE_DIAGNOSTICS=1` でビルドすると `%TEMP%\WaveForge_diag.log` に
  出力の不連続・処理時間・ホスト呼び出し間隔が 2 秒ごとに記録される。

## Standalone で MIDI キーボードを使う

Standalone を起動 → 左上の「Options」→「Audio/MIDI Settings」→ MIDI Inputs で鍵盤を有効化。
画面下の鍵盤はマウス / PC キーボードでも演奏できる。
