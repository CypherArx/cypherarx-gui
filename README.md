# CypherArx GUI

[日本語版はこちら / Japanese version](README.ja.md)

Desktop GUI for **CypherArx**, built with Dear ImGui on top of `cypherarx-core`.

> **Status: pre-alpha, not for public use.** Private during development.

This is Phase 2: the first part of CypherArx with a user interface. The UI-free core lives in
`cypherarx-core` and is included here as a git submodule under `external/cypherarx-core`. The
GUI is plain C++ that links the core directly, so secrets never cross a language boundary and
the core's guarded memory (`Secret`) is used as-is.

## Toolchain

MSVC (Visual Studio 2022) + CMake + Ninja + vcpkg (manifest mode), the same toolchain as
`cypherarx-core`. Dependencies (`imgui` with the Win32/DX11 bindings and FreeType,
`libsodium`, `sqlite3`) come from vcpkg.

## Build

Requires a C++23 compiler, CMake >= 3.21, Ninja, and vcpkg (`VCPKG_ROOT` set). Configure from a
Visual Studio developer shell so `cl` is on `PATH`. After cloning, fetch the submodule:

```sh
git submodule update --init --recursive
cmake --preset default
cmake --build --preset default
```

A headless smoke test of the linked core (no window):

```sh
./build/cypherarx_gui --selftest
```

## Layout

```
src/main.cpp                     entry point, --selftest
src/config.hpp                   every tunable value (window, font, theme, clipboard, KDF preset, buffer sizes)
src/strings.hpp                  every UI string
src/platform/                    OS layer: Win32 window + DirectX 11 device, native file dialogs, ImGui context/font
src/app/app.{hpp,cpp}            application state and per-frame drawing (panes, modal dialogs)
src/app/projection.{hpp,cpp}     builds the entry tree for one axis (service / persona) from the flat entry list
src/app/clipboard_guard.*        copies a value to the clipboard and clears it after a delay if unchanged
src/app/secret_buffer.hpp        fixed-size text buffer in guarded memory, for password / secret value inputs
```

Colours, sizes, font paths, the clipboard timeout and the Argon2id preset are all in
`config.hpp`; nothing of that kind is written inline in the drawing code. UI text lives in
`strings.hpp` for the same reason (and to make a later translation a single-file change).

## What the GUI does today

- Create / open a vault (`*.cax`) with a master password. The password is typed into guarded
  memory and wiped after key derivation.
- Entry tree with two views of the same data: **Service axis** (tree by `parent_id`) and
  **Persona axis** (tree by `persona_id`). An account appears in both; the third column shows
  its parent on the other axis. See the design canon, §10.1.1.
- Add / edit / delete entries. An account picks its service and (optionally) persona; a child
  picks its parent account. An account cannot be created before a service exists, and a child
  cannot be created before an account exists.
- Add / edit / delete fields (`text` / `password` / `totp`). The field value is edited in
  guarded memory; `password` values are masked with a Show toggle and a Generate button.
- Copy a field value to the clipboard; it is cleared automatically after the configured delay if
  the clipboard still holds that value.
- TOTP: live code with the remaining seconds, recomputed every frame; the decoded key is wiped
  when the panel is closed.
- Login methods (the reference-link layer): per account, add / delete how you log in, with
  "via" limited to accounts on a *different* service (self-reference is structurally impossible).
- Delete operations ask for confirmation, default No, and are soft deletes in the core.

Not yet: categories, tags, search / filter, login-method edit, session continuation via the OS
keyring, in-app browser, favicons.

## Design

The canonical design document is in the org repository:
`claude-handover/cypherarx-design-final.md`.
