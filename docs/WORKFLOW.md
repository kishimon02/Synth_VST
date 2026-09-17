# 開発ワークフロー

## フェーズごとのコミットとタグ

各フェーズの完了時に 1 コミット + 1 タグを作る。タグ名は `phase-<n>`。

| タグ | 内容 |
|---|---|
| `phase-1` | ビルド環境 + ウェーブテーブルエンジン (WT Osc x2、Sub、Noise、Filter、ADSR x2) |
| `phase-2` | LFO x2、モジュレーションマトリクス、プリセット |
| `phase-3` | FX チェーン (Distortion / EQ / Chorus / Delay / Reverb) |
| `phase-4` | 3D ウェーブテーブル表示、本 UI |
| `phase-4.5` | ウェーブテーブル・エディタ、2D / スペクトル表示、出力 3D スペクトログラム、OSC A Warp |
| `phase-5` | AI アシスタント (Claude API 連携、MIDI 提案) |
| `phase-6` | 仕上げ (出力ガード、Panic、dsp 負荷表示、soak テスト) — v1.0.0 |

`phase-0` は `phase-1` のコミットに含めた (同一コミットのため別タグは切っていない)。

## フェーズ完了時の手順

```bash
# 1. ビルドが通り、テストが全部通ることを確認
cmake --build --preset vs2022-release
cmake --build --preset vs2022-release --target WaveForgeTests
./build/vs2022/WaveForgeTests_artefacts/Release/WaveForgeTests.exe

# リリース前は soak を長くする (30 分ぶんの音を 2 分ほどで回す)
./build/vs2022/WaveForgeTests_artefacts/Release/WaveForgeTests.exe --soak 30

# 2. コミット + タグ
git add -A
git commit -m "Phase N: <一行要約>"
git tag -a phase-N -m "<内容>"

# 3. push (タグも一緒に)
git push origin main --follow-tags
```

## コミットに含めないもの

`.gitignore` で除外済み。特に:

- `build/` — CMake の出力と JUCE のソース (`build/_juce-src`) 一式
- `settings.local.json` / `*.env` — Phase 5 で扱う API キーは **絶対にコミットしない**
  (実際の保存先は `%APPDATA%\WaveForge\settings.json` で、DPAPI 暗号化する)

## 注意

- **ビルド前に Studio One と Standalone を閉じる。** 開いていると VST3 の DLL が
  ロックされ、配置ステップが警告を出してスキップされる (ビルド自体は成功する)。
- JUCE は `build/_juce-src` に clone 済み。configure 時に
  `-DFETCHCONTENT_SOURCE_DIR_JUCE=<repo>/build/_juce-src` を渡すと再ダウンロードを避けられる。
