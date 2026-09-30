#pragma once

#include "config.hpp"

namespace cypherarx::gui::platform {

bool create_imgui_context(const FontConfig& font);
void destroy_imgui_context();

} // namespace cypherarx::gui::platform
