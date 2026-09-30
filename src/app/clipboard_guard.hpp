#pragma once

#include <chrono>
#include <cstdint>
#include <span>

#include "cypherarx/secret.hpp"

namespace cypherarx::gui {

class ClipboardGuard {
public:
    void copy(std::span<const std::uint8_t> value, int clear_after_seconds);
    void tick();
    void clear_now();

private:
    Secret copied_;
    std::chrono::steady_clock::time_point deadline_;
    bool armed_ = false;
};

} // namespace cypherarx::gui
