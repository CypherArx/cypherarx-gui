# CypherArx GUI

Qt (Widgets) desktop GUI for **CypherArx**, built on `cypherarx-core`.

> **Status: pre-alpha, not for public use.** Private during development.

This is Phase 2 — the first part of CypherArx with a user interface (and the first that can
be tested by hand). The Qt-free core lives in `cypherarx-core` and is included here as a git
submodule under `external/cypherarx-core`.

## Toolchain

Built with **MinGW** (the toolchain matching the installed Qt Widgets kit), not MSVC, since
the bundled Qt is the `mingw_64` kit. `cypherarx-core` is compiled with the same toolchain
(via `add_subdirectory`) so the ABI matches. Dependencies (libsodium, sqlite3) come from
vcpkg with the `x64-mingw-dynamic` triplet; Qt comes from the local install.

## Build

Requirements: Qt 6 (mingw_64 kit), the bundled MinGW GCC, CMake, Ninja, and vcpkg
(`VCPKG_ROOT` set). After cloning, fetch the submodule:

```sh
git submodule update --init --recursive
cmake --preset default
cmake --build --preset default
```

Adjust the compiler / Qt paths in `CMakePresets.json` if your install differs.

A headless smoke test of the linked core (no window):

```sh
./build/cypherarx_gui --selftest
```

## Current scope

Create / open an encrypted vault and list / add entries. Field editing, password
generation, and TOTP display are being added next.
