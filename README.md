# CypherArx GUI

[日本語版はこちら / Japanese version](README.ja.md)

Desktop GUI for **CypherArx**, built on top of `cypherarx-core` and drawn with
[lamina-ui](https://github.com/Capsicum0907/lamina-ui) (Material 3 Expressive on SDL3 + ThorVG).

> **Status: pre-alpha, not for public use.** Private during development.

This is Phase 2: the first part of CypherArx with a user interface. The UI-free core lives in
`cypherarx-core` and is included here as a git submodule under `external/cypherarx-core`. The
GUI is plain C++ that links the core directly, so secrets never cross a language boundary and
the core's guarded memory (`Secret`) is used as-is.

An earlier Dear ImGui front end lived here until 2026-11; it is in the git history.

## Toolchain

MSVC (Visual Studio 2022) + CMake + Ninja + vcpkg (manifest mode), the same toolchain as
`cypherarx-core`. Dependencies come from vcpkg: `libsodium` and `sqlite3` for the core, and
`abseil`, `harfbuzz`, `sdl3`, `thorvg` for lamina-ui. lamina-ui itself is a git submodule under
`external/lamina-ui`.

## Build

Requires a C++23 compiler, CMake >= 3.25, Ninja, vcpkg (`VCPKG_ROOT` set) and Python 3.
Configure from a Visual Studio developer shell so `cl` is on `PATH`. After cloning, fetch the
submodules:

```sh
git submodule update --init --recursive
python -m pip install -r external/lamina-ui/tools/icon-subset/requirements.txt
cmake --preset default
cmake --build --preset default
```

Python is for the icons. The Material Symbols the GUI uses are listed under
`lamina_add_resources` in `CMakeLists.txt`; the build cuts them out of the font that lamina-ui
vendors (with fontTools and brotli, which the `pip` line installs) and compiles them into the
executable, so it needs no asset folder.

Run `build/cypherarx_gui`, or a headless smoke test with no window:

```sh
./build/cypherarx_gui --selftest
```

The smoke test drives the model layer in `src/model/`: it creates a vault, adds a service, a
persona and accounts, checks both axes, the search and the rules for the kinds of entry, adds a
field, copies it, and checks that closing the vault clears the clipboard.

## CI

`.github/workflows/ci.yml` builds on `windows-latest` with MSVC + vcpkg and runs `--selftest`,
once normally and once with MSVC AddressSanitizer (`/fsanitize=address`, RelWithDebInfo). The
core is a private submodule, so the workflow checks it out with a repository secret
`CORE_SUBMODULE_TOKEN`: a fine-grained personal access token with read access to
`cypherarx-core` contents. Without that secret the checkout step fails.

## Layout

```
src/strings.hpp                  every UI string

src/model/session.{hpp,cpp}      the application without a toolkit: vault, selection, filters, entry/field/category/tag/login-method operations, TOTP, the rules for the kinds of entry
src/model/projection.{hpp,cpp}   builds the entry tree for one axis (service / persona), applies the search/category/tag filter
src/model/timed_clipboard.*      timed clipboard clearing behind a small clipboard interface
src/model/secret_buffer.hpp      fixed-size text buffer in guarded memory, for password / secret value inputs

src/platform/file_dialog*        native open/save dialogs

src/lamina_gui/config.hpp        every tunable value (window, themes, sizes, icons, clipboard, KDF preset, buffer sizes)
src/lamina_gui/controls.*        one control interface, a panel that routes input and focus, wrappers around lamina-ui widgets, tooltips
src/lamina_gui/list_view.*       scrolling list / tree / table rows with actions on the row
src/lamina_gui/select_field.*    drop-down selection
src/lamina_gui/secret_field.*    text input that keeps its value in guarded memory, masked or shown
src/lamina_gui/sheet.*           modal sheets with form content, and lamina-ui dialogs and menus as modals
src/lamina_gui/sheets.*          the password, entry, field, organize and login-method sheets
src/lamina_gui/app.*             the three-pane window, input routing, the frame loop
src/lamina_gui/main.cpp          entry point, --selftest
```

Colours, sizes, the clipboard timeout and the Argon2id preset are all in `config.hpp`; nothing
of that kind is written inline in the drawing code. UI text lives in `strings.hpp` for the same
reason (and to make a later translation a single-file change).

## What the GUI does today

- Create / open a vault (`*.cax`) with a master password. The password is typed into guarded
  memory and wiped after key derivation, which runs on a worker thread behind a spinner.
- Three columns: a **sidebar** that chooses what to look at, the **entry tree** with a tab for
  each axis and a search box, and the **fields** pane for the selected entry.
- Entry tree with two views of the same data: **Service axis** (tree by `parent_id`) and
  **Persona axis** (tree by `persona_id`). An account appears in both; the third column shows
  its parent on the other axis. See the design canon, §10.1.1.
- The sidebar picks one place at a time: all items, the favorites, one category (with its
  sub-categories) or one tag, so only one of its rows is ever highlighted. Search (label /
  display name / URL, ASCII case-insensitive) narrows that further. Matching entries keep their
  ancestors and descendants in the tree; ancestors that do not match themselves are dimmed.
- Add / edit / delete entries. Add opens a menu of what to add (Service, Persona, Account,
  Child). An account picks its service and (optionally) persona; a child picks its parent
  account. An account cannot be created before a service exists, and a child cannot be created
  before an account exists.
- The four kinds of entry are treated by their role, not all alike. Only a service has a
  category. Fields belong to accounts and children: on a service or a persona Add is disabled
  and the pane says so, though fields that already exist there can still be edited and deleted.
  An account's name is optional; left empty, its row and title show its parent on the other
  axis, the service on the persona axis and the persona on the service axis. These rules live in
  `Session::traits`.
- What acts on one entry or one field sits on its row. Hovering a row, or moving to it with the
  keyboard, shows icon buttons at its right end: login methods (accounts only), edit and delete
  on an entry; copy, TOTP (`totp` fields only), edit and delete on a field. Icon buttons show a
  tooltip after a short hover.
- Organize sheet: categories (presets are read-only; custom ones can be nested, renamed,
  deleted) and tags (rename, delete). Category and tag names are encrypted at rest.
- Fields are `text` / `password` / `totp`. The value is edited in guarded memory; `password`
  values are masked with a Show toggle and a Generate button.
- Copying a field value: the clipboard is cleared automatically after the configured delay if
  it still holds that value, and also when the window closes.
- TOTP: the code with a countdown ring, updated once a second; the decoded key is wiped when
  the panel is closed.
- Login methods (the reference-link layer): per account, add / edit / delete how you log in,
  with "via" limited to accounts on a *different* service (self-reference is structurally
  impossible).
- Delete operations ask for confirmation, default No, and are soft deletes in the core.
- Keyboard: Tab moves between controls; in the entry tree and the field list the arrows move
  the selection, Left and Right collapse and expand, Enter opens the edit sheet and Delete
  deletes. A double click opens the edit sheet too; on an entry that has children it collapses
  or expands it instead. `Ctrl+F` focuses the search box.
- Themes: the palette button opens a menu of *Dark, outlined* (black ground, panes drawn as
  outlines, one filled button), *Dark, tonal* (neutral greys, panes as raised surfaces) and
  *Light*. A theme is one `ThemePreset` in `config.hpp`: the scheme variant, the colour roles
  of the ground, the panes, the sheets and the popups, whether panes are filled or outlined,
  and the kinds of the main buttons. Every theme is built from the same seed colour. The window
  starts with the first one; the choice is not saved.

Not yet: session continuation via the OS keyring, in-app browser, favicons, import.

## How it is put together

The logic lives in `src/model/` and knows nothing about the toolkit; it reports failures as
messages instead of opening dialogs. `src/lamina_gui/` only turns it into controls.

Colour roles, type scale, corner sizes and springs all come from lamina-ui's tokens. The window
uses lamina-ui's client-drawn title bar (`WindowConfig::client_title_bar`; with it off, or where
lamina-ui cannot draw one, the window keeps the system frame). The window only repaints on input
or while something moves, and wakes itself for the caret blink, the tooltips, the TOTP countdown
and the clipboard deadline.

Buttons, icon buttons, checkboxes, text fields, labels, the progress ring, the scroll area,
focus order, tabs, menus and the confirm / message dialogs are lamina-ui widgets. lamina-ui has no
list, tree, drop-down, masked input, tooltip or dialog with custom content yet, so those are
written here on top of lamina-ui's canvas, text layout and tokens:

- `ListView`: rows with columns, sections, indentation, expand / collapse and actions on the
  row. It serves the sidebar, the entry tree, the field list, the lists in the sheets and the
  drop-down's popup.
- `SelectField`: an outlined field that opens a scrolling popup, so long lists fit the window.
- `SecretField`: the master password and field values never leave guarded memory
  (`SecretBuffer`); masked input draws dots and does not lay the text out at all. It moves the
  caret (arrows, Home, End, click), deletes and pastes, but has no selection, copy or cut.
  lamina-ui's own text field, used for labels, notes and search, has all of them, with undo.
- `Sheet`: a modal surface that holds a form.
- `HoverTip` and `paint_tooltip`: the tooltip of the icon buttons and of the row actions.

Tag chips in the entry sheet take the pointer only.

## Design

The canonical design document is in the org repository:
`claude-handover/cypherarx-design-final.md`.
