#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "cypherarx/secret.hpp"

namespace cypherarx::gui {

class SecretBuffer {
public:
    explicit SecretBuffer(std::size_t capacity) : buf_(capacity) { clear(); }

    char* data() { return reinterpret_cast<char*>(buf_.data()); }
    std::size_t capacity() const { return buf_.size(); }

    std::span<const std::uint8_t> bytes() const {
        return {buf_.data(), std::strlen(reinterpret_cast<const char*>(buf_.data()))};
    }
    bool empty() const { return buf_.data()[0] == 0; }

    void clear() { std::memset(buf_.data(), 0, buf_.size()); }

    void assign(std::span<const std::uint8_t> src) {
        clear();
        const std::size_t n = src.size() < buf_.size() - 1 ? src.size() : buf_.size() - 1;
        std::memcpy(buf_.data(), src.data(), n);
    }

private:
    Secret buf_;
};

} // namespace cypherarx::gui
