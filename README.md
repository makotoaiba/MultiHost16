# MultiHost 16 — VSTi ホスト型 VST3 インストゥルメント

Cubase Pro 15 で使うことを想定した、**最大 16 台の VSTi を 1 つの VST3 インストゥルメントにまとめるホスト**です。
JUCE 8 ベース。Windows / macOS 両対応。

---

## 機能

| 項目 | 内容 |
|---|---|
| スロット数 | **16**（各スロットに VST3 インストゥルメントを 1 台読み込み） |
| 読み込み対象 | **インストゥルメントのみ**（エフェクトは一覧に出ず、ロードも拒否） |
| キーレンジ | スロット毎に Low Key / High Key（レイヤー・スプリット） |
| ベロシティレンジ | スロット毎に Low Vel / High Vel（ベロシティレイヤー） |
| トランスポーズ | スロット毎に ±48 半音 |
| MIDI ch フィルタ | Omni または Ch 1〜16（**16 パートのマルチティンバー運用が可能**） |
| クイックセットアップ | `Multitimbral`（Slot N → Ch N / Out N）と `Layer`（全 Omni → Main）をワンクリック |
| ミュート / ソロ | スロット毎。Cubase からオートメーション可能 |
| ゲイン / パン | スロット毎（-60〜+12 dB、コンスタントパワーパン） |
| 出力ルーティング | `Main Mix` / `Out 1`〜`Out 16`（パラミックス・パラアウト） |
| 音声リターン | ホスト内で各 VSTi をレンダリングし、選択したバスへ返す |
| レイテンシ補正 | 各子プラグインの申告レイテンシを内部で揃え、最大値を Cubase へ報告 |
| スキャン | **別プロセス**（`MultiHost16Scanner.exe`）で実行。相性の悪い音源でも DAW は落ちない |
| 状態保存 | 子プラグインの種類 + 内部ステートを Cubase プロジェクトに保存 |
| UI | 16 スロットを縦スクロール表示。キーレンジを 16 段で俯瞰 |

### バス構成

```
出力 0      : Main Mix   (stereo, 既定で有効)
出力 1..16  : Out 1..16  (stereo, 既定で無効)
```

合計 17 ステレオバス = 最大 34ch。
`Out N` は Cubase の出力アクティベーションで有効化します。
無効なバスを指しているスロットの音は、自動的に `Main Mix` に回ります。

---

## ビルド検証ステータス

**Linux (Ubuntu 24.04 / GCC 13 / JUCE 8.0.6 / CMake 3.28) でビルド・実行検証済み**

- プラグイン本体・スキャナともにコンパイルエラー 0 件
- 検証用ホストから実際にロードし、以下を実機確認
  - 出力バス 17 系統（`Main Mix` + `Out 1`〜`Out 16`）
  - 全 aux バスを有効化して 34ch になること
  - `isInstrument = 1` / `acceptsMidi = 1`
  - `Master Gain` + `Slot 1..16` の全 177 パラメータが露出すること
- スキャナを単体実行し、実在の VST3 から正しい `PluginDescription` を
  取得できることを確認

生成できたのは Linux 用バイナリ（`.so`）です。
**Cubase Pro 15 用の Windows バイナリは、下記の手順でご自身の環境で
ビルドしてください。** コードは共通です。

---

## ビルド

### 必要環境
- CMake 3.22 以上
- Windows: Visual Studio 2022 (MSVC v143)
- macOS: Xcode 14 以上
- JUCE 8（未指定なら CMake が GitHub から自動取得）

### 手順

```bash
cd MultiHost16

# JUCE をローカルに持っている場合
cmake -B build -DJUCE_PATH=/path/to/JUCE -DCMAKE_BUILD_TYPE=Release

# 自動取得させる場合
cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release
```

`COPY_PLUGIN_AFTER_BUILD TRUE` を有効にしているので、ビルド後に自動で
共有 VST3 フォルダへコピーされます。

- Windows: `C:\Program Files\Common Files\VST3\MultiHost 16.vst3`
- macOS: `~/Library/Audio/Plug-Ins/VST3/MultiHost 16.vst3`

> **注意**: `PLUGIN_CODE` / `PLUGIN_MANUFACTURER_CODE` は他プラグインと衝突すると
> Cubase 側で正しく識別されません。配布する場合は必ず自分の固有値に変更してください。

---

## Cubase Pro 15 での使い方

1. **VST インストゥルメント** ラックに `MultiHost 16` を追加。
2. パネル右上の **Scan Plugins...** で、ホストしたい VSTi をスキャン
   （スキャン結果はユーザー設定フォルダに保存され、次回以降は不要）。
3. 各スロットの **`-- empty --`** ボタンから音源を選択。
4. **Edit** で子プラグインの GUI を別ウィンドウで開く。
5. **Key / Vel / Trans / MIDI** でレイヤーとスプリットを設定。
6. **Out** で `Main Mix` またはパラアウト先を選択。
7. Cubase の VST インストゥルメント欄にある **出力アクティベーション**
   （スピーカーアイコン）から `Out 1`〜`Out 16` を有効化すると、
   MixConsole に個別チャンネルが生成されます。そこから
   グループバス（Group Channel）へ送ってください。

### オートメーション
`Slot N Mute` / `Slot N Solo` / `Slot N Gain` / `Slot N Pan` / `Slot N Low Key` …
はすべて Cubase のオートメーションレーンに現れます。
曲中でレイヤーを切り替える場合は `Slot N Mute` をオートメーションするのが確実です。

---

## 設計上の注意 / 既知の制限

- **DAW から見えるパラメータ総数は約 2250 個になります。**
  実際に使うのは `Master Gain` + `Slot N ...` の 177 個で、
  残りは JUCE が VST3 インストゥルメントに自動付与する
  MIDI CC 用パラメータ（16ch × 130）です。
  JUCE 製 VST3 共通の仕様で、無効化するオプションはありません。

- **16 台すべてに重い音源を入れれば当然それだけ重くなります。**
  使うスロットだけロードしてください。

- **子プラグインのパラメータは Cubase へ露出しません。**
  ホスト側に固定パラメータ群のみを持つ設計です（PatchWork 等と同様の割り切り）。
  子側を動かしたい場合は **MIDI CC を透過送信している**ので、
  Cubase の MIDI CC オートメーション、または子プラグイン側の
  MIDI Learn を使ってください。
  必要なら「マクロ 8 個 → 子プラグインの任意パラメータへリンク」を
  追加実装するのが定番です。

- **ミュートは MIDI ゲート + 20 ms のゲインフェード**です。
  ミュート時にノートオフを送出するので発音は止まりますが、
  リリーステールも 20 ms で切れます。テールを残したい場合は
  `processBlock` 内の `smoothedL/R` のターゲット設定を
  「MIDI だけ止めてゲインは維持」に変更してください。

- **VST2 / AU を読みたい場合**は `CMakeLists.txt` の
  `JUCE_PLUGINHOST_VST=1`（要 VST2 SDK）/ `JUCE_PLUGINHOST_AU=1` を有効にします。
  ただし Cubase 13 以降は VST2 のサポートが終了しているため、
  ホスト内から VST2 を鳴らす構成は自己責任で。

- **プラグインのスキャンは別プロセス (`MultiHost16Scanner.exe`) で実行されます。**
  相性の悪い音源に当たっても子プロセスが死ぬだけで、DAW は無傷です。
  スキャナはビルド時に `%APPDATA%\MultiHost16\` へコピーされます。
  見つからない場合、プラグイン UI の Scan ボタンは無効化されます。

- **子プラグインの GUI は常に別ウィンドウ**です（`setAlwaysOnTop(true)`）。
  Cubase のプラグインウィンドウ管理からは独立します。

- iLok / eLicenser 系の一部音源は、ホスト内ホスティングを想定していない
  ライセンスチェックを行うことがあり、正常に立ち上がらない場合があります。

- レイテンシ補正は「子プラグインが申告した値」に基づきます。
  申告しないプラグイン（一部のアンプシミュ等）はズレるので、
  必要なら手動でディレイを足す UI を追加してください。

---

## 配布物の作り方

ビルド後に **`make_package.bat`** を実行すると、`dist\` に配布用の
フォルダと ZIP が出来ます。中身は以下の 5 点です。

```
MultiHost16_v1.2.0/
├── MultiHost 16.vst3/         ← プラグイン本体（フォルダごと必要）
├── MultiHost16Scanner.exe     ← これが無いとスキャンできない
├── install_for_test.bat
├── TESTERS.md
└── MANUAL.md
```

### 送ってはいけないもの

| | 理由 |
|---|---|
| `build\` フォルダ全体 | 数 GB あり、中間ファイルばかりで不要 |
| `JUCE\` フォルダ | 約 90 MB。相手のビルドには不要 |
| `juce_download.zip` | 同上 |
| Debug ビルド | デバッグ版 CRT を要求し、動作も遅い |
| `.vst3` の中の DLL 単体 | VST3 はフォルダ構造ごとが本体。単体では動かない |

### 相手側に必要なもの

- Visual C++ 再頒布可能パッケージは **不要**
  （MSVC ランタイムを静的リンクしているため）
- ZIP をダウンロード後、右クリック → プロパティ → **「許可する」**

---

## ライセンス

MultiHost 16 is free software, licensed under the
**GNU Affero General Public License v3.0 or later**.

本ソフトウェアは **AGPL-3.0-or-later** で配布されています。
自由に使用・改変・再配布できますが、改変版を配布する場合は
同じライセンスでソースコードを提供する必要があります。
詳細は [LICENSE](LICENSE) を参照してください。

### 依存ライブラリ

| ライブラリ | ライセンス |
|---|---|
| [JUCE 8](https://juce.com) | AGPLv3 / 商用のデュアルライセンス（本プロジェクトは AGPLv3 を選択） |
| [VST 3 SDK](https://www.steinberg.net/developers/) 3.8.0 以降 | MIT |
| VST 3 SDK 3.7 系（JUCE 8.0.6 同梱） | GPLv3（AGPLv3 との結合が認められています） |

ビルドには JUCE が必要です。CMake が自動で取得するため、
事前の準備は不要です。

### 商標

VST は Steinberg Media Technologies GmbH の登録商標です。
本プロジェクトは Steinberg とは無関係であり、同社による承認や
支援を受けたものではありません。

---

## ファイル構成

```
MultiHost16/
├── CMakeLists.txt
├── .github/workflows/    … Windows 自動ビルド (GitHub Actions)
├── .gitignore
├── build_windows.bat     … Windows ビルド用（JUCE 自動取得）
├── make_package.bat      … 配布パッケージ作成
├── install_for_test.bat  … テスター配布用インストーラ
├── MANUAL.md             … 操作マニュアル
├── TESTERS.md            … テスト依頼用
├── Scanner/
│   └── ScannerMain.cpp   … 別プロセススキャナ (コンソールアプリ)
└── Source/
    ├── PluginProcessor.h     … スロット定義 / MIDI ルーティング / バス構成
    ├── PluginProcessor.cpp   … ホスティング・ミキシング・状態保存の本体
    ├── PluginEditor.h
    └── PluginEditor.cpp      … 16 スロット UI (スクロール) / スキャナ / キーレンジ表示
```
