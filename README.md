# CypherArx GUI

[日本語版はこちら / Japanese version](README.ja.md)

Desktop GUI for **CypherArx**, built on top of `cypherarx-core`. There are two front ends over
the same core, each its own executable:

| Executable | Toolkit | Source |
| --- | --- | --- |
| `cypherarx_gui` | Dear ImGui (Win32 + DirectX 11) | `src/main.cpp`, `src/app/`, `src/platform/` |
| `cypherarx_gui_lamina` | [lamina-ui](https://github.com/Capsicum0907/lamina-ui) (Material 3 Expressive, SDL3 + ThorVG) | `src/lamina_gui/`, `src/model/` |

> **Status: pre-alpha, not for public use.** Private during development.

This is Phase 2: the first part of CypherArx with a user interface. The UI-free core lives in
`cypherarx-core` and is included here as a git submodule under `external/cypherarx-core`. The
GUI is plain C++ that links the core directly, so secrets never cross a language boundary and
the core's guarded memory (`Secret`) is used as-is.

## Toolchain

MSVC (Visual Studio 2022) + CMake + Ninja + vcpkg (manifest mode), the same toolchain as
`cypherarx-core`. Dependencies come from vcpkg: `libsodium` and `sqlite3` for the core, `imgui`
(Win32/DX11 bindings, FreeType) for the ImGui front end, and `abseil`, `harfbuzz`, `sdl3`,
`thorvg` for lamina-ui. lamina-ui itself is a git submodule under `external/lamina-ui`.

## Build

Requires a C++23 compiler, CMake >= 3.25, Ninja, and vcpkg (`VCPKG_ROOT` set). Configure from a
Visual Studio developer shell so `cl` is on `PATH`. After cloning, fetch the submodules:

```sh
git submodule update --init --recursive
cmake --preset default
cmake --build --preset default
```

Both front ends are built by default. `-DCYPHERARX_GUI_IMGUI=OFF` or `-DCYPHERARX_GUI_LAMINA=OFF`
leaves one out.

Headless smoke tests (no window):

```sh
./build/cypherarx_gui --selftest
./build/cypherarx_gui_lamina --selftest
```

The first checks that the core is linked (vault, entry and field round trip). The second drives
the model layer in `src/model/`: it creates a vault, adds a service, a persona and an account,
checks both axes and the search, adds a field, copies it, and checks that closing the vault
clears the clipboard.

The lamina-ui front end needs `assets/` (the icon font) next to the executable; the build copies
it there.

## CI

`.github/workflows/ci.yml` builds on `windows-latest` with MSVC + vcpkg and runs both
`--selftest` commands, once normally and once with MSVC AddressSanitizer
(`/fsanitize=address`, RelWithDebInfo). The core is a private submodule, so the workflow checks
it out with a repository secret `CORE_SUBMODULE_TOKEN`: a fine-grained personal access token
with read access to `cypherarx-core` contents. Without that secret the checkout step fails.

## Layout

```
src/strings.hpp                  every UI string (both front ends)
src/app/projection.{hpp,cpp}     builds the entry tree for one axis (service / persona), applies the search/category/tag filter (both front ends)
src/app/secret_buffer.hpp        fixed-size text buffer in guarded memory, for password / secret value inputs (both front ends)

src/main.cpp                     ImGui: entry point, --selftest
src/config.hpp                   ImGui: every tunable value (window, font, theme, pane ratios, clipboard, KDF preset, buffer sizes)
src/platform/                    ImGui: Win32 window + DirectX 11 device, native file dialogs, ImGui context/font/style
src/app/app.{hpp,cpp}            ImGui: application state and per-frame drawing (sidebar, panes, modal dialogs)
src/app/clipboard_guard.*        ImGui: copies a value to the clipboard and clears it after a delay if unchanged

src/model/session.{hpp,cpp}      lamina: the application without a toolkit: vault, selection, filters, entry/field/category/tag/login-method operations, TOTP
src/model/timed_clipboard.*      lamina: timed clipboard clearing behind a small clipboard interface
src/platform/file_dialog*        lamina: native open/save dialogs
src/lamina_gui/config.hpp        lamina: every tunable value (window, colour seed, sizes, icons, clipboard, KDF preset, buffer sizes)
src/lamina_gui/controls.*        lamina: one control interface, a panel that routes input and focus, and wrappers around lamina-ui widgets
src/lamina_gui/list_view.*       lamina: scrolling list / tree / table rows
src/lamina_gui/select_field.*    lamina: drop-down selection
src/lamina_gui/secret_field.*    lamina: text input that keeps its value in guarded memory, masked or shown
src/lamina_gui/sheet.*           lamina: modal sheets with form content, and lamina-ui dialogs as modals
src/lamina_gui/sheets.*          lamina: the password, entry, field, organize and login-method sheets
src/lamina_gui/app.*             lamina: the three-pane window, input routing, the frame loop
src/lamina_gui/main.cpp          lamina: entry point, --selftest
```

Colours, sizes, font paths, the clipboard timeout and the Argon2id preset are all in each front
end's `config.hpp`; nothing of that kind is written inline in the drawing code. UI text lives in
`strings.hpp` for the same reason (and to make a later translation a single-file change).

## What the GUI does today

Both front ends do the following.

- Create / open a vault (`*.cax`) with a master password. The password is typed into guarded
  memory and wiped after key derivation, which runs on a worker thread behind a spinner.
- Three columns: a **sidebar** (view axis, favorites filter, category tree, tags), the **entry
  tree** with a search box, and the **fields** pane for the selected entry.
- Entry tree with two views of the same data: **Service axis** (tree by `parent_id`) and
  **Persona axis** (tree by `persona_id`). An account appears in both; the third column shows
  its parent on the other axis. See the design canon, §10.1.1.
- Search (label / display name / URL, ASCII case-insensitive) and filters by category (including
  sub-categories), tag and favorites. Matching entries keep their ancestors and descendants in
  the tree; ancestors that do not match themselves are dimmed.
- Add / edit / delete entries. An account picks its service and (optionally) persona; a child
  picks its parent account. An account cannot be created before a service exists, and a child
  cannot be created before an account exists. Each entry can have a category, tags and a
  favorite mark.
- Organize dialog: categories (presets are read-only; custom ones can be nested, renamed,
  deleted) and tags (rename, delete). Category and tag names are encrypted at rest.
- Add / edit / delete fields (`text` / `password` / `totp`). The field value is edited in
  guarded memory; `password` values are masked with a Show toggle and a Generate button.
- Copy a field value to the clipboard; it is cleared automatically after the configured delay if
  the clipboard still holds that value.
- TOTP: live code with the remaining seconds; the decoded key is wiped when the panel is closed.
- Login methods (the reference-link layer): per account, add / edit / delete how you log in,
  with "via" limited to accounts on a *different* service (self-reference is structurally
  impossible).
- Delete operations ask for confirmation, default No, and are soft deletes in the core.

Not yet: session continuation via the OS keyring, in-app browser, favicons, import.

## The lamina-ui front end

`cypherarx_gui_lamina` is the same application drawn with lamina-ui. Colour roles, type scale,
corner sizes and springs all come from lamina-ui's tokens; the scheme is built from one seed
colour in `src/lamina_gui/config.hpp`. The window uses lamina-ui's client-drawn title bar
(`WindowConfig::client_title_bar`; with it off, or where lamina-ui cannot draw one, the window
keeps the system frame). The window only repaints on input or while something moves, and wakes
itself for the caret blink, the TOTP countdown and the clipboard deadline.

The logic lives in `src/model/` and knows nothing about the toolkit; `src/lamina_gui/` only
turns it into controls. Buttons, icon buttons, checkboxes, text fields, labels, the progress
ring, the scroll area, focus order and the confirm / message dialogs are lamina-ui widgets.
lamina-ui 0.1.0 has no list, tree, drop-down, masked input or dialog with custom content, so
those are written here on top of lamina-ui's canvas, text layout and tokens:

- `ListView`: rows with columns, sections, indentation and expand / collapse. It serves the
  sidebar, the entry tree, the field list, the lists in the sheets and the drop-down's popup.
- `SelectField`: an outlined field that opens a scrolling popup, so long lists fit the window.
- `SecretField`: the master password and field values never leave guarded memory
  (`SecretBuffer`); masked input draws dots and does not lay the text out at all.
- `Sheet`: a modal surface that holds a form.

Differences from the ImGui front end:

- The secret field moves the caret (arrows, Home, End, click), deletes and pastes, but has no
  selection, copy or cut. lamina-ui's own text field (used for labels, notes and search) has
  all of them, with undo.
- The entry tree and the field list answer the keyboard: arrows move the selection, Left and
  Right collapse and expand, and Enter opens the edit sheet. A double click opens the edit sheet
  too; on an entry that has children it collapses or expands it instead. `Ctrl+F` focuses the
  search box.
- A field's notes are a column of the field list instead of a tooltip.
- The TOTP code is updated once a second, next to a countdown ring.
- Tag chips in the entry sheet take the pointer only.
- The clipboard is also cleared when the window closes.
- The button at the end of the toolbar switches between the dark and the light scheme, both
  built from the same seed colour. The window starts dark; the choice is not saved.

The ImGui front end still carries its own copy of the logic in `src/app/app.cpp`; it has not
been moved onto `src/model/`.

## Design

The canonical design document is in the org repository:
`claude-handover/cypherarx-design-final.md`.
