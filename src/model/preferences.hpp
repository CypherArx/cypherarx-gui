#pragma once

#include <filesystem>
#include <string>

namespace cypherarx::gui {

struct Preferences {
    std::string theme;
    std::string vault;

    bool operator==(const Preferences&) const = default;
};

Preferences load_preferences(const std::filesystem::path& file);
bool save_preferences(const std::filesystem::path& file, const Preferences& preferences);

}
