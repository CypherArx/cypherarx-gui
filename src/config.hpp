#pragma once

#include <string>
#include <vector>

#include <imgui.h>

#include "cypherarx/kdf.hpp"

namespace cypherarx::gui {

struct WindowConfig {
    const wchar_t* class_name = L"CypherArx";
    const wchar_t* title = L"CypherArx";
    int width = 1000;
    int height = 660;
    bool dpi_aware = true;
};

struct FontConfig {
    std::vector<std::string> candidates = {
        R"(C:\Windows\Fonts\meiryo.ttc)",
        R"(C:\Windows\Fonts\YuGothM.ttc)",
        R"(C:\Windows\Fonts\msgothic.ttc)",
    };
    float size = 17.0f;
};

struct Theme {
    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    ImVec4 muted_text = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
    ImVec4 warning_text = ImVec4(0.95f, 0.70f, 0.30f, 1.0f);
    ImVec4 error_text = ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
    float left_pane_ratio = 0.45f;
};

struct ClipboardConfig {
    int clear_after_seconds = 20;
};

struct SecurityConfig {
    KdfParams (*kdf_params)() = &KdfParams::moderate;
    KdfParams (*kdf_params_selftest)() = &KdfParams::interactive;
};

struct Limits {
    std::size_t path_buffer = 1024;
    std::size_t password_buffer = 256;
    std::size_t value_buffer = 4096;
};

struct Config {
    WindowConfig window;
    FontConfig font;
    Theme theme;
    ClipboardConfig clipboard;
    SecurityConfig security;
    Limits limits;
    const char* vault_extension = ".cax";
    const wchar_t* file_filter = L"CypherArx Vault (*.cax)\0*.cax\0All Files (*.*)\0*.*\0";
};

inline const Config& config() {
    static const Config c;
    return c;
}

} // namespace cypherarx::gui
