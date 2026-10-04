#include "platform/imgui_setup.hpp"

#include <filesystem>

#include <imgui.h>

#include "platform/host.hpp"

namespace cypherarx::gui::platform {

namespace {

bool load_font(const FontConfig& font, float scale) {
    ImGuiIO& io = ImGui::GetIO();
    for (const auto& path : font.candidates) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            continue;
        }
        if (io.Fonts->AddFontFromFileTTF(path.c_str(), font.size * scale)) {
            return true;
        }
    }
    io.Fonts->AddFontDefault();
    return false;
}

} // namespace

bool create_imgui_context(const FontConfig& font, const Theme& theme) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    const float scale = dpi_scale();
    const bool cjk = load_font(font, scale);
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = theme.frame_rounding;
    style.ChildRounding = theme.frame_rounding;
    style.PopupRounding = theme.frame_rounding;
    style.WindowPadding = ImVec2(theme.window_padding, theme.window_padding);
    style.ScaleAllSizes(scale);
    return cjk;
}

void destroy_imgui_context() {
    ImGui::DestroyContext();
}

} // namespace cypherarx::gui::platform
