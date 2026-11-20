#include "platform/file_dialog.hpp"

#include <windows.h>
#include <commdlg.h>

#include <iterator>
#include <string>

namespace cypherarx::gui::platform {

namespace {

std::wstring to_wide(const char* s) {
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 1) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(n - 1), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
    return out;
}

std::optional<std::filesystem::path> file_dialog(bool save, const FileDialogOptions& options) {
    wchar_t buf[MAX_PATH * 4] = L"";
    const char* extension = options.default_extension;
    const std::wstring ext = to_wide(extension[0] == '.' ? extension + 1 : extension);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ::GetActiveWindow();
    ofn.lpstrFilter = options.filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(buf));
    ofn.lpstrDefExt = ext.c_str();
    ofn.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER |
                (save ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));
    const BOOL ok = save ? ::GetSaveFileNameW(&ofn) : ::GetOpenFileNameW(&ofn);
    if (!ok) {
        return std::nullopt;
    }
    return std::filesystem::path(buf);
}

} // namespace

std::optional<std::filesystem::path> choose_file_to_open(const FileDialogOptions& options) {
    return file_dialog(false, options);
}

std::optional<std::filesystem::path> choose_file_to_save(const FileDialogOptions& options) {
    return file_dialog(true, options);
}

} // namespace cypherarx::gui::platform
