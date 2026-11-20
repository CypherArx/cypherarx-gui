#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "cypherarx/secret.hpp"

namespace cypherarx::gui {

class ClipboardPort {
public:
    virtual ~ClipboardPort() = default;
    virtual std::string text() = 0;
    virtual void set_text(std::string_view utf8) = 0;
};

class TimedClipboard {
public:
    void copy(ClipboardPort& port, std::span<const std::uint8_t> value, int clear_after_seconds);
    void tick(ClipboardPort& port);
    void clear_now(ClipboardPort& port);
    std::optional<double> seconds_until_clear() const;

private:
    Secret copied_;
    std::chrono::steady_clock::time_point deadline_;
    bool armed_ = false;
};

} // namespace cypherarx::gui
