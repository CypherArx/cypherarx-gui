#pragma once

#include <optional>
#include <string>

#include <imgui.h>

#include "config.hpp"

namespace cypherarx::gui::platform {

bool init(const WindowConfig& window);
void attach_imgui();
void detach_imgui();
void shutdown();

float dpi_scale();

bool pump_events();
void begin_frame();
void end_frame(const ImVec4& clear_color);

std::optional<std::string> pick_open_file(const wchar_t* filter, const char* default_extension);
std::optional<std::string> pick_save_file(const wchar_t* filter, const char* default_extension);

} // namespace cypherarx::gui::platform
