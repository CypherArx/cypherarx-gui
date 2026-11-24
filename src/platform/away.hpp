#pragma once

#include <memory>

namespace cypherarx::gui::platform {

struct AwayState {
    bool screensaver = false;
    bool screen_locked = false;
    bool disconnected = false;
    bool resumed = false;
};

class AwayWatch {
public:
    AwayWatch();
    ~AwayWatch();
    AwayWatch(const AwayWatch&) = delete;
    AwayWatch& operator=(const AwayWatch&) = delete;

    AwayState poll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cypherarx::gui::platform
