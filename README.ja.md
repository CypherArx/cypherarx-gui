# CypherArx GUI

[English version / 英語版はこちら](README.md)

**CypherArx** のデスクトップ GUI。`cypherarx-core` の上に Dear ImGui で構築しています。

> **状態: プレアルファ、一般利用不可。** 開発中は非公開です。

これはフェーズ 2、CypherArx で最初にユーザーインターフェースを持つ部分です。UI を持たないコアは
`cypherarx-core` にあり、ここには git サブモジュール `external/cypherarx-core` として取り込んで
います。GUI は素の C++ でコアを直接リンクするため、秘密情報が言語境界を越えることはなく、
コアの保護メモリ（`Secret`）をそのまま使えます。

## ツールチェーン

MSVC（Visual Studio 2022）＋ CMake ＋ Ninja ＋ vcpkg（マニフェストモード）。`cypherarx-core` と
同じです。依存（Win32/DX11 バインディングと FreeType 付きの `imgui`、`libsodium`、`sqlite3`）は
vcpkg から取得します。

## ビルド

C++23 コンパイラ、CMake 3.21 以上、Ninja、vcpkg（`VCPKG_ROOT` 設定済み）が必要です。`cl` が
`PATH` にあるよう、Visual Studio の開発者シェルから構成してください。クローン後にサブモジュールを
取得します。

```sh
git submodule update --init --recursive
cmake --preset default
cmake --build --preset default
```

ウィンドウ無しでリンク済みコアの往復を確認するスモークテスト:

```sh
./build/cypherarx_gui --selftest
```

## 構成

```
src/main.cpp                     入口、--selftest
src/config.hpp                   調整可能な値の全部（ウィンドウ・フォント・配色・クリップボード・KDF プリセット・バッファ長）
src/strings.hpp                  UI 文言の全部
src/platform/                    OS 層: Win32 ウィンドウ＋DirectX 11 デバイス、ネイティブのファイルダイアログ、ImGui コンテキスト/フォント
src/app/app.{hpp,cpp}            アプリ状態と毎フレームの描画（ペイン、モーダルダイアログ）
src/app/projection.{hpp,cpp}     平坦なエントリ一覧から片方の軸（service / persona）のツリーを構築
src/app/clipboard_guard.*        値をクリップボードにコピーし、未変更なら一定時間後に消去
src/app/secret_buffer.hpp        保護メモリ上の固定長テキストバッファ（パスワード・秘密値の入力用）
```

配色・寸法・フォントパス・クリップボードの消去時間・Argon2id プリセットはすべて `config.hpp` に
あり、描画コードの中に直接は書きません。UI 文言も同じ理由（と、後で翻訳するときに 1 ファイルで
済むように）で `strings.hpp` に置いています。

## 現時点でできること

- マスターパスワードで金庫（`*.cax`）を作成／オープン。パスワードは保護メモリに入力され、鍵導出後に
  消去されます。
- 同じデータの 2 つの見え方を持つエントリツリー: **サービス軸**（`parent_id` でツリー化）と
  **名義軸**（`persona_id` でツリー化）。account は両方に現れ、3 列目にもう一方の軸の親を表示。
  設計正典 §10.1.1 を参照。
- エントリの追加／編集／削除。account はサービスと（任意で）名義を選び、child は親 account を
  選びます。サービスが無いうちは account を、account が無いうちは child を作れません。
- フィールドの追加／編集／削除（`text` / `password` / `totp`）。値は保護メモリで編集し、
  `password` はマスク表示＋表示切替＋生成ボタン付き。
- フィールド値のコピー。設定した秒数の後、クリップボードがその値のままなら自動で消去。
- TOTP: 残り秒数付きのコードを毎フレーム再計算。パネルを閉じるとデコード済みの鍵を消去。
- ログイン方法（参照リンク層）: account ごとにログイン手段を追加／削除。「via」は
  *別サービス* の account に限定（自己参照は構造的に不可能）。
- 削除は確認ダイアログ（既定 No）を経て、コア側は論理削除。

未実装: カテゴリ、タグ、検索／絞り込み、ログイン方法の編集、OS キーリングによるセッション継続、
アプリ内ブラウザ、ファビコン。

## 設計

設計の正典は組織リポジトリの `claude-handover/cypherarx-design-final.md` にあります。
