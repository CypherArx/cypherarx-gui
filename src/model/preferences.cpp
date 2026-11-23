#include "model/preferences.hpp"

#include <array>
#include <fstream>
#include <string_view>
#include <system_error>

namespace cypherarx::gui {

namespace {

struct Field {
    std::string_view key;
    std::string Preferences::* value;
};

constexpr std::array fields{
    Field{"theme", &Preferences::theme},
    Field{"vault", &Preferences::vault},
};

constexpr char separator = '=';
constexpr std::string_view blanks = " \t\r";
constexpr std::string_view line_breaks = "\r\n";
constexpr const char* temporary_suffix = ".tmp";

std::string_view trimmed(std::string_view s) {
    const auto first = s.find_first_not_of(blanks);
    if (first == std::string_view::npos) {
        return {};
    }
    return s.substr(first, s.find_last_not_of(blanks) - first + 1);
}

}

Preferences load_preferences(const std::filesystem::path& file) {
    Preferences preferences;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        const auto at = line.find(separator);
        if (at == std::string::npos) {
            continue;
        }
        const std::string_view key = trimmed(std::string_view(line).substr(0, at));
        const std::string_view value = trimmed(std::string_view(line).substr(at + 1));
        for (const Field& field : fields) {
            if (field.key == key) {
                preferences.*field.value = std::string(value);
            }
        }
    }
    return preferences;
}

bool save_preferences(const std::filesystem::path& file, const Preferences& preferences) {
    std::error_code ec;
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path(), ec);
    }
    std::filesystem::path temporary = file;
    temporary += temporary_suffix;
    {
        std::ofstream out(temporary, std::ios::trunc);
        for (const Field& field : fields) {
            const std::string& value = preferences.*field.value;
            if (value.empty() || value.find_first_of(line_breaks) != std::string::npos) {
                continue;
            }
            out << field.key << ' ' << separator << ' ' << value << '\n';
        }
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }
    std::filesystem::rename(temporary, file, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

}
