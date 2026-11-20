# CypherArx GUI

[English version / 英語版はこちら](README.md)

**CypherArx** のデスクトップ GUI。`cypherarx-core` の上に構築しています。同じコアの上に表示側が
2 つあり、それぞれ別の実行ファイルになります。

| 実行ファイル | ツールキット | ソース |
| --- | --- | --- |
| `cypherarx_gui` | Dear ImGui（Win32 ＋ DirectX 11） | `src/main.cpp`、`src/app/`、`src/platform/` |
| `cypherarx_gui_lamina` | [lamina-ui](https://github.com/Capsicum0907/lamina-ui)（Material 3 Expressive、SDL3 ＋ ThorVG） | `src/lamina_gui/`、`src/model/` |

> **状態: プレアルファ、一般利用不可。** 開発中は非公開です。

これはフェーズ 2、CypherArx で最初にユーザーインターフェースを持つ部分です。UI を持たないコアは
`cypherarx-core` にあり、ここには git サブモジュール `external/cypherarx-core` として取り込んで
います。GUI は素の C++ でコアを直接リンクするため、秘密情報が言語境界を越えることはなく、
コアの保護メモリ（`Secret`）をそのまま使えます。

## ツールチェーン

MSVC（Visual Studio 2022）＋ CMake ＋ Ninja ＋ vcpkg（マニフェストモード）。`cypherarx-core` と
同じです。依存は vcpkg から取得します。コア用の `libsodium` と `sqlite3`、ImGui 版用の `imgui`
（Win32/DX11 バインディングと FreeType）、lamina-ui 用の `abseil`・`harfbuzz`・`sdl3`・`thorvg`
です。lamina-ui 本体は git サブモジュール `external/lamina-ui` として取り込んでいます。

## ビルド

C++23 コンパイラ、CMake 3.25 以上、Ninja、vcpkg（`VCPKG_ROOT` 設定済み）が必要です。`cl` が
`PATH` にあるよう、Visual Studio の開発者シェルから構成してください。クローン後にサブモジュールを
取得します。

```sh
git submodule update --init --recursive
cmake --preset default
cmake --build --preset default
```

既定では両方の版をビルドします。`-DCYPHERARX_GUI_IMGUI=OFF` か `-DCYPHERARX_GUI_LAMINA=OFF` で
片方を外せます。

ウィンドウ無しのスモークテスト:

```sh
./build/cypherarx_gui --selftest
./build/cypherarx_gui_lamina --selftest
```

1 つ目は、コアがリンクされていること（金庫・エントリ・フィールドの往復）を確かめます。2 つ目は
`src/model/` のモデル層を動かします。金庫を作り、サービス・名義・アカウントを追加し、両方の軸と
検索を確かめ、フィールドを追加してコピーし、金庫を閉じるとクリップボードが消えることを確かめます。

lamina-ui 版はアイコンを埋め込みます。使う Material Symbols は `CMakeLists.txt` の
`lamina_add_resources` に並べてあり、ビルドが lamina-ui の同梱フォントからその分だけ切り出して
実行ファイルに組み込みます。そのため素材のフォルダーは要りません。この段階には、fontTools と
brotli の入った Python 3 が要ります。

```sh
python -m pip install -r external/lamina-ui/tools/icon-subset/requirements.txt
```

## CI

`.github/workflows/ci.yml` は `windows-latest` で MSVC ＋ vcpkg によりビルドし、2 つの
`--selftest` を実行します。通常ビルドと、MSVC AddressSanitizer（`/fsanitize=address`、
RelWithDebInfo）の 2 本。コアは非公開のサブモジュールなので、リポジトリシークレット
`CORE_SUBMODULE_TOKEN`（`cypherarx-core` の内容を読み取れる細粒度の個人アクセストークン）で
チェックアウトします。このシークレットが無いとチェックアウトで失敗します。

## 構成

```
src/strings.hpp                  UI 文言の全部（両方の版）
src/app/projection.{hpp,cpp}     片方の軸（service / persona）のツリー構築と、検索/カテゴリ/タグの絞り込み（両方の版）
src/app/secret_buffer.hpp        保護メモリ上の固定長テキストバッファ（パスワード・秘密値の入力用。両方の版）

src/main.cpp                     ImGui: 入口、--selftest
src/config.hpp                   ImGui: 調整可能な値の全部（ウィンドウ・フォント・配色・ペイン比率・クリップボード・KDF プリセット・バッファ長）
src/platform/                    ImGui: Win32 ウィンドウ＋DirectX 11 デバイス、ネイティブのファイルダイアログ、ImGui コンテキスト/フォント/スタイル
src/app/app.{hpp,cpp}            ImGui: アプリ状態と毎フレームの描画（サイドバー、ペイン、モーダルダイアログ）
src/app/clipboard_guard.*        ImGui: 値をクリップボードにコピーし、未変更なら一定時間後に消去

src/model/session.{hpp,cpp}      lamina: ツールキットを持たないアプリ本体（金庫・選択・絞り込み・エントリ/フィールド/カテゴリ/タグ/ログイン方法の操作・TOTP）
src/model/timed_clipboard.*      lamina: 小さなクリップボードの口の向こうで行う、時間で消すコピー
src/platform/file_dialog*        lamina: ネイティブの開く/保存ダイアログ
src/lamina_gui/config.hpp        lamina: 調整可能な値の全部（ウィンドウ・配色の元の色・寸法・アイコン・クリップボード・KDF プリセット・バッファ長）
src/lamina_gui/controls.*        lamina: 部品の共通の口、入力とフォーカスを振り分けるパネル、lamina-ui の部品の包み
src/lamina_gui/list_view.*       lamina: 送れる一覧・木・表の行
src/lamina_gui/select_field.*    lamina: 選択欄（ドロップダウン）
src/lamina_gui/secret_field.*    lamina: 値を保護メモリに持つ入力欄（伏せ字と表示の切り替え）
src/lamina_gui/sheet.*           lamina: フォームを載せるモーダルのシートと、lamina-ui のダイアログのモーダル化
src/lamina_gui/sheets.*          lamina: パスワード・エントリ・フィールド・整理・ログイン方法の各シート
src/lamina_gui/app.*             lamina: 3 ペインのウィンドウ、入力の振り分け、フレームの繰り返し
src/lamina_gui/main.cpp          lamina: 入口、--selftest
```

配色・寸法・フォントパス・クリップボードの消去時間・Argon2id プリセットはすべて各版の
`config.hpp` にあり、描画コードの中に直接は書きません。UI 文言も同じ理由（と、後で翻訳するときに
1 ファイルで済むように）で `strings.hpp` に置いています。

## 現時点でできること

以下はどちらの版でもできます。

- マスターパスワードで金庫（`*.cax`）を作成／オープン。パスワードは保護メモリに入力され、鍵導出後に
  消去されます。鍵導出はワーカースレッドで行い、その間はスピナーを表示します。
- 3 カラム構成: **サイドバー**（表示軸、お気に入り絞り込み、カテゴリ木、タグ）、検索欄付きの
  **エントリツリー**、選択エントリの **フィールド** ペイン。
- 同じデータの 2 つの見え方を持つエントリツリー: **サービス軸**（`parent_id` でツリー化）と
  **名義軸**（`persona_id` でツリー化）。account は両方に現れ、3 列目にもう一方の軸の親を表示。
  設計正典 §10.1.1 を参照。
- 検索（ラベル／表示名／URL、ASCII は大文字小文字を無視）と、カテゴリ（サブカテゴリ含む）・タグ・
  お気に入りによる絞り込み。一致したエントリは祖先と子孫をツリーに残し、自身は一致しない祖先は
  薄く表示します。
- エントリの追加／編集／削除。account はサービスと（任意で）名義を選び、child は親 account を
  選びます。サービスが無いうちは account を、account が無いうちは child を作れません。各エントリに
  カテゴリ・タグ・お気に入り印を付けられます。
- Organize ダイアログ: カテゴリ（プリセットは読み取り専用。カスタムは入れ子・改名・削除可）と
  タグ（改名・削除）。カテゴリ名・タグ名は保存時に暗号化されます。
- フィールドの追加／編集／削除（`text` / `password` / `totp`）。値は保護メモリで編集し、
  `password` はマスク表示＋表示切替＋生成ボタン付き。
- フィールド値のコピー。設定した秒数の後、クリップボードがその値のままなら自動で消去。
- TOTP: 残り秒数付きのコードを表示。パネルを閉じるとデコード済みの鍵を消去。
- ログイン方法（参照リンク層）: account ごとにログイン手段を追加／編集／削除。「via」は
  *別サービス* の account に限定（自己参照は構造的に不可能）。
- 削除は確認ダイアログ（既定 No）を経て、コア側は論理削除。

未実装: OS キーリングによるセッション継続、アプリ内ブラウザ、ファビコン、インポート。

## lamina-ui 版

`cypherarx_gui_lamina` は、同じアプリを lamina-ui で描いたものです。色の役割・文字の段階・
角の大きさ・ばねはすべて lamina-ui のトークンから取り、配色は `src/lamina_gui/config.hpp` の
元の色 1 つから作ります。ウィンドウは lamina-ui が自分で描くタイトルバーを使います
（`WindowConfig::client_title_bar`。これを切ったとき、または lamina-ui が描けない環境では、
OS の枠のままです）。ウィンドウが描き直すのは、入力があったときと何かが動いている間だけで、
カーソルの点滅・TOTP の残り秒数・クリップボードの消去時刻のためには自分で起きます。

ロジックは `src/model/` にあり、ツールキットのことを何も知りません。`src/lamina_gui/` はそれを
部品に置き換えるだけです。ボタン・アイコンボタン・チェックボックス・入力欄・ラベル・進み具合の輪・
スクロールする範囲・フォーカスの順番・テーマのメニュー・確認と知らせのダイアログは lamina-ui の
部品です。
lamina-ui 0.1.0 には一覧・木・選択欄・伏せ字の入力・中身を持てるダイアログが無いので、それらは
lamina-ui の描画面・文字の配置・トークンの上に、ここで書いています。

- `ListView`: 列・見出し・字下げ・開閉を持つ行。サイドバー、エントリツリー、フィールドの一覧、
  シートの中の一覧、選択欄のポップアップに使います。
- `SelectField`: 枠線つきの欄で、送れるポップアップを開きます。長い一覧でもウィンドウに収まります。
- `SecretField`: マスターパスワードとフィールドの値は保護メモリ（`SecretBuffer`）から出ません。
  伏せ字のときは点を描くだけで、文字の配置そのものを行いません。
- `Sheet`: フォームを載せるモーダルの面。

ImGui 版との違い:

- 秘密の入力欄は、カーソルの移動（矢印・Home・End・クリック）、削除、貼り付けができますが、
  選択・コピー・切り取りはありません。lamina-ui 自身の入力欄（ラベル・メモ・検索に使用）には
  すべてあり、取り消しもできます。
- エントリツリーとフィールドの一覧はキーボードに応じます。矢印で選択を動かし、← と → で畳んで
  開き、Enter で編集のシートを開きます。ダブルクリックでも編集のシートを開きますが、子を持つ
  エントリでは代わりに畳むか開きます。`Ctrl+F` で検索欄にフォーカスを移します。
- フィールドのメモは、ツールチップではなくフィールドの一覧の列に出します。
- TOTP のコードは 1 秒ごとに更新し、残り時間の輪を添えます。
- エントリのシートのタグのチップは、ポインターでだけ操作できます。
- ウィンドウを閉じるときにもクリップボードを消します。
- ツールバーの端のパレットのボタンで、テーマのメニューを開きます。*Dark, outlined*（地は黒、
  ペインは線で囲む、塗ったボタンは 1 つ）、*Dark, tonal*（無彩色の灰色、ペインは一段上の面）、
  *Light* の 3 つです。テーマは `src/lamina_gui/config.hpp` の `ThemePreset` 1 つ分のデータで、
  配色の種類、地・ペイン・シート・ポップアップの色の役割、ペインを塗るか線で囲むか、主なボタンの
  種類を持ちます。どのテーマも同じ元の色から作ります。起動時は先頭のテーマで、選んだ結果は
  保存しません。

ImGui 版は今も `src/app/app.cpp` に自分のロジックを持っていて、`src/model/` には載せ替えて
いません。

## 設計

設計の正典は組織リポジトリの `claude-handover/cypherarx-design-final.md` にあります。
