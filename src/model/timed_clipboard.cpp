#include "model/timed_clipboard.hpp"

#include <algorithm>

#include <sodium.h>

namespace cypherarx::gui {

void TimedClipboard::copy(ClipboardPort& port, std::span<const std::uint8_t> value,
                          int clear_after_seconds) {
    port.set_text({reinterpret_cast<const char*>(value.data()), value.size()});
    copied_ = Secret(value);
    deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(clear_after_seconds);
    armed_ = true;
}

void TimedClipboard::tick(ClipboardPort& port) {
    if (!armed_ || std::chrono::steady_clock::now() < deadline_) {
        return;
    }
    clear_now(port);
}

void TimedClipboard::clear_now(ClipboardPort& port) {
    if (!armed_) {
        return;
    }
    std::string current = port.text();
    const bool unchanged = current.size() == copied_.size() &&
                           sodium_memcmp(current.data(), copied_.data(), current.size()) == 0;
    sodium_memzero(current.data(), current.size());
    if (unchanged) {
        port.set_text("");
    }
    copied_ = Secret();
    armed_ = false;
}

std::optional<double> TimedClipboard::seconds_until_clear() const {
    if (!armed_) {
        return std::nullopt;
    }
    const std::chrono::duration<double> left = deadline_ - std::chrono::steady_clock::now();
    return std::max(0.0, left.count());
}

} // namespace cypherarx::gui
