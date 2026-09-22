# GitHub で公開する手順

このプロジェクトを **AGPL-3.0** で公開すれば、JUCE の商用ライセンスを
購入せずにバイナリを配布できます。

---

## 0. 先に確認してほしいこと

**業務として作ったものであれば、著作権は会社に帰属している可能性が高いです。**
公開は「会社の資産をオープンソースにする」行為なので、
先に上長または法務の承認を取ってください。ここが一番重要です。

また、一度 AGPL で公開したバージョンは取り消せません。
後からクローズドにしたくなった場合、そのバージョンは AGPL のまま残り、
新しいバージョンに JUCE 商用ライセンスを買う必要があります。

---

## 1. リポジトリを作る

1. GitHub で **New repository**
2. Repository name: `MultiHost16`（任意）
3. **Public** を選択（AGPL でソース提供義務を果たす最も簡単な方法）
4. **Add a license** → **GNU Affero General Public License v3.0** を選択

> ライセンス本文は GitHub が自動で `LICENSE` ファイルとして追加します。
> 手で書き写すと誤りが混入するので、必ずこの方法を使ってください。

---

## 2. ソースを push

```
cd C:\dev\MultiHost16
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin https://github.com/<ユーザー名>/MultiHost16.git
git push -u origin main
```

`.gitignore` により、`build/`、`JUCE/`、`juce_download.zip`、`dist/` は
除外されます。リポジトリは 100 KB 程度に収まります。

> Git が入っていない場合は https://git-scm.com/download/win から入れるか、
> GitHub Desktop（https://desktop.github.com/）を使ってください。

---

## 3. 自動ビルドを使う（任意）

`.github/workflows/build-windows.yml` を同梱しています。
push すると GitHub 上で Windows ビルドが走り、
**Actions タブから配布用の zip をダウンロード**できます。

- 自分の PC でビルドしなくても配布物が手に入る
- テスターに「Actions の最新ビルドを落として」と言うだけで済む
- ビルドが壊れたらすぐ気づける

---

## 4. 配布するときに書き添えること

バイナリを配る際は、**対応するソースの入手方法**を示す必要があります。
`TESTERS.md` の冒頭などに 1 行入れておけば十分です。

```
This software is licensed under the AGPL-3.0.
Source: https://github.com/<ユーザー名>/MultiHost16
```

---

## 5. ライセンスの根拠（2026年9月時点）

| 依存 | ライセンス | 必要な対応 |
|---|---|---|
| JUCE 8 | AGPLv3 または商用のデュアル | AGPLv3 で公開 → 商用ライセンス不要 |
| VST 3 SDK 3.8.0 以降 | MIT | 著作権表示とライセンス文を残す |
| VST 3 SDK 3.7 系（JUCE 8.0.6 同梱） | GPLv3 または Steinberg 独自 | GPLv3 側を選択。AGPLv3 との結合は許容されている |

Steinberg は 2025年10月の VST 3.8.0 で SDK を MIT ライセンスへ移行しました。
以前の「Steinberg と契約するか GPLv3 か」という二択は解消されています。

> **VST は Steinberg Media Technologies GmbH の商標です。**
> VST ロゴを使う場合は別途 Steinberg の規約に従ってください。
> 名称や配布自体に契約は不要ですが、ロゴ利用には条件があります。

最新の条件は必ず以下で確認してください。
- JUCE: https://juce.com/legal/
- VST3 SDK: https://www.steinberg.net/developers/

ここに書いた内容は法律上の助言ではありません。
判断に迷う場合は会社の法務に確認してください。
