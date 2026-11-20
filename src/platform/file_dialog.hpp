#pragma once

#include <filesystem>
#include <optional>

namespace cypherarx::gui::platform {

struct FileDialogOptions {
    const wchar_t* filter;
    const char* default_extension;
};

std::optional<std::filesystem::path> choose_file_to_open(const FileDialogOptions& options);
std::optional<std::filesystem::path> choose_file_to_save(const FileDialogOptions& options);

} // namespace cypherarx::gui::platform
