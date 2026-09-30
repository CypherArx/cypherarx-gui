#include "app/clipboard_guard.hpp"

#include <cstring>
#include <string>

#include <imgui.h>
#include <sodium.h>

namespace cypherarx::gui {

void ClipboardGuard::copy(std::span<const std::uint8_t> value, int clear_after_seconds) {
    std::string text(reinterpret_cast<const char*>(value.data()), value.size());
    ImGui::SetClipboardText(text.c_str());
    sodium_memzero(text.data(), text.size());
    copied_ = Secret(value);
    deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(clear_after_seconds);
    armed_ = true;
}

void ClipboardGuard::tick() {
    if (!armed_ || std::chrono::steady_clock::now() < deadline_) {
        return;
    }
    clear_now();
}

void ClipboardGuard::clear_now() {
    if (!armed_) {
        return;
    }
    const char* current = ImGui::GetClipboardText();
    const std::size_t n = current ? std::strlen(current) : 0;
    const bool unchanged = current && n == copied_.size() &&
                           sodium_memcmp(current, copied_.data(), n) == 0;
    if (unchanged) {
        ImGui::SetClipboardText("");
    }
    copied_ = Secret();
    armed_ = false;
}

} // namespace cypherarx::gui
