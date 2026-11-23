#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "cypherarx/secret.hpp"

#include "lamina_gui/app.hpp"
#include "lamina_gui/config.hpp"
#include "model/preferences.hpp"
#include "model/session.hpp"

namespace {

namespace gui = cypherarx::gui;

class MemoryClipboard final : public gui::ClipboardPort {
public:
    std::string text() override { return text_; }
    void set_text(std::string_view utf8) override { text_ = std::string(utf8); }

private:
    std::string text_;
};

bool expect(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "selftest: " << what << " failed\n";
    }
    return ok;
}

std::int64_t add_entry(gui::Session& s, const char* type, const char* label, std::int64_t service = 0,
                       std::int64_t persona = 0) {
    gui::EntryDraft d = s.new_entry_draft();
    d.type = type;
    d.label = label;
    d.service = service;
    d.persona = persona;
    if (!s.commit_entry(d) || !s.selected_entry()) {
        return 0;
    }
    return s.selected_entry()->id;
}

int run_selftest() {
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "cypherarx_gui_lamina_selftest.cax";
    std::error_code ec;
    fs::remove(path, ec);

    gui::SessionSettings settings;
    settings.kdf_params = cypherarx::lamina_gui::config().security.kdf_params_selftest;
    MemoryClipboard clipboard;
    bool ok = true;
    {
        gui::Session s(settings);
        const std::vector<std::uint8_t> pw = {'p', 'w'};
        s.start_unlock(gui::UnlockMode::Create, path, pw);
        std::optional<gui::Status> unlocked;
        while (!(unlocked = s.poll_unlock(clipboard))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ok = expect(unlocked->has_value() && s.has_vault(), "create") && ok;

        const std::int64_t service = add_entry(s, "service", "Example");
        const std::int64_t persona = add_entry(s, "persona", "Me");
        const std::int64_t account = add_entry(s, "account", "me@example.com", service, persona);
        ok = expect(service && persona && account, "add entries") && ok;
        ok = expect(s.projection().roots.size() == 1, "service axis") && ok;
        s.set_axis(gui::Axis::Persona);
        ok = expect(s.projection().roots.size() == 1 && s.projection().roots.front() == persona,
                    "persona axis") &&
             ok;
        s.set_query("example.com");
        ok = expect(s.projection().visible_count() == 1, "search") && ok;
        s.set_query("");
        s.set_scope({gui::Scope::Kind::favorites});
        ok = expect(s.projection().visible_count() == 0, "the favorites scope") && ok;
        s.set_scope({});

        const std::int64_t unnamed = add_entry(s, "account", "", service, persona);
        ok = expect(unnamed != 0 && s.selected_entry() && s.entry_title(*s.selected_entry()) == "Example",
                    "an unnamed account shows its service on the persona axis") &&
             ok;
        s.select_entry(service);
        gui::FieldDraft stray = s.new_field_draft();
        stray.label = "x";
        stray.type = "password";
        ok = expect(!s.commit_field(stray, {}).has_value(), "a service takes only URL fields") && ok;

        const std::int64_t bare = add_entry(s, "service", "");
        gui::FieldDraft site = s.new_field_draft();
        site.label = "URL";
        const std::string_view site_url = "https://bare.example";
        ok = expect(bare != 0 &&
                        s.commit_field(site, {reinterpret_cast<const std::uint8_t*>(site_url.data()),
                                              site_url.size()})
                            .has_value() &&
                        s.selected_entry() && s.entry_title(*s.selected_entry()) == site_url,
                    "an unnamed service shows its URL") &&
             ok;

        s.select_entry(account);
        gui::FieldDraft field = s.new_field_draft();
        field.label = "pw";
        field.type = "password";
        const std::vector<std::uint8_t> secret = {'s', 'e', 'c', 'r', 'e', 't'};
        ok = expect(s.commit_field(field, secret).has_value() && s.fields().size() == 1, "add field") &&
             ok;
        ok = expect(s.copy_selected_field(clipboard).has_value() && clipboard.text() == "secret",
                    "copy") &&
             ok;
        s.set_scope({gui::Scope::Kind::recent});
        ok = expect(s.projection().is_match(account) && !s.projection().is_match(unnamed),
                    "the recently used scope holds what was copied") &&
             ok;
        s.set_scope({});
        cypherarx::Settings changed = s.vault_settings();
        changed.session_lock_idle_minutes = 3;
        ok = expect(s.set_vault_settings(changed).has_value() && s.vault_settings() == changed,
                    "vault settings") &&
             ok;

        const fs::path csv = fs::temp_directory_path() / "cypherarx_gui_selftest_import.csv";
        {
            std::ofstream out(csv, std::ios::binary);
            out << "name,url,username,password\nImported,https://imported.example,me,pw\n";
        }
        const auto imported = s.import_csv(csv, persona);
        fs::remove(csv, ec);
        ok = expect(imported.has_value() && imported->services == 1 && imported->accounts == 1 &&
                        imported->fields == 3,
                    "import") &&
             ok;
        ok = expect(std::any_of(s.entries().begin(), s.entries().end(),
                                [&](const cypherarx::Entry& e) {
                                    return e.label == "me" && e.persona_id == persona;
                                }),
                    "the imported account has the chosen persona") &&
             ok;

        const std::int64_t twin = add_entry(s, "service", "twin");
        const std::int64_t moved = add_entry(s, "account", "moved", twin, 0);
        ok = expect(twin != 0 && moved != 0 && s.merge_service(twin, service).has_value() &&
                        std::none_of(s.entries().begin(), s.entries().end(),
                                     [&](const cypherarx::Entry& e) { return e.id == twin; }) &&
                        std::any_of(s.entries().begin(), s.entries().end(),
                                    [&](const cypherarx::Entry& e) {
                                        return e.id == moved && e.parent_id == service;
                                    }),
                    "merging a service moves its accounts") &&
             ok;

        const std::int64_t alpha = add_entry(s, "service", "alpha");
        add_entry(s, "account", "z", alpha, persona);
        s.set_axis(gui::Axis::Persona);
        {
            std::vector<std::string> services;
            const auto under = s.projection().children.find(persona);
            if (under != s.projection().children.end()) {
                for (const std::int64_t id : under->second) {
                    std::string label = s.projection().label_of(s.projection().find(id)->parent_id);
                    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    services.push_back(std::move(label));
                }
            }
            ok = expect(services.size() == 4 && services.front() == "alpha" &&
                            std::is_sorted(services.begin(), services.end()),
                        "accounts on the persona axis are grouped by service") &&
                 ok;
        }
        s.set_axis(gui::Axis::Service);

        const auto persona_of = [&](std::int64_t id) {
            for (const cypherarx::Entry& e : s.entries()) {
                if (e.id == id) {
                    return e.persona_id;
                }
            }
            return std::optional<std::int64_t>(-1);
        };
        s.clear_marks();
        s.select_entry(account);
        const std::vector<std::int64_t> more{moved};
        s.mark_entries(more, false);
        ok = expect(s.marked().size() == 2 && s.set_persona_of_marked(std::nullopt).has_value() &&
                        !persona_of(account).has_value() && !persona_of(moved).has_value(),
                    "clearing the persona of the marked accounts") &&
             ok;
        ok = expect(s.set_persona_of_marked(persona).has_value() && persona_of(account) == persona &&
                        persona_of(moved) == persona,
                    "setting the persona of the marked accounts") &&
             ok;
        s.clear_marks();

        s.start_rotate(pw);
        std::optional<gui::Status> rotated;
        while (!(rotated = s.poll_rotate())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ok = expect(rotated->has_value(), "rotate key") && ok;
        s.select_entry(account);
        if (!s.fields().empty()) {
            s.select_field(s.fields().front().id);
        }
        ok = expect(s.copy_selected_field(clipboard).has_value() && clipboard.text() == "secret",
                    "copy after the rotation") &&
             ok;

        const auto t0 = gui::Session::Clock::now();
        s.note_activity(t0);
        ok = expect(!s.lock_if_due(clipboard, t0 + std::chrono::minutes(2)) && s.has_vault(),
                    "no lock before the idle time is up") &&
             ok;
        s.note_activity(t0 + std::chrono::minutes(2));
        ok = expect(!s.lock_if_due(clipboard, t0 + std::chrono::minutes(4)) && s.has_vault(),
                    "input restarts the idle time") &&
             ok;
        ok = expect(s.lock_if_due(clipboard, t0 + std::chrono::minutes(5)) && !s.has_vault(),
                    "the vault locks after the idle time") &&
             ok;

        s.close_vault(clipboard);
        ok = expect(clipboard.text().empty() && !s.has_vault(), "close clears the clipboard") && ok;
    }
    fs::remove(path, ec);

    const gui::PathParts parts = gui::path_parts(gui::path_from_utf8("C:\\vaults\\home.cax"));
    ok = expect(parts.name == "home.cax" && parts.folder == "C:\\vaults", "path parts") && ok;

    const fs::path prefs =
        fs::temp_directory_path() / "cypherarx_gui_selftest_preferences" / "preferences.conf";
    fs::remove_all(prefs.parent_path(), ec);
    ok = expect(gui::load_preferences(prefs) == gui::Preferences{}, "missing preferences are the defaults") &&
         ok;
    const gui::Preferences chosen{.theme = "light", .vault = "C:\\vaults\\a = b.cax"};
    ok = expect(gui::save_preferences(prefs, chosen) && gui::load_preferences(prefs) == chosen,
                "preferences round trip") &&
         ok;
    ok = expect(gui::save_preferences(prefs, gui::Preferences{.theme = "dark-tonal"}) &&
                    gui::load_preferences(prefs) == gui::Preferences{.theme = "dark-tonal"},
                "preferences are replaced") &&
         ok;
    fs::remove_all(prefs.parent_path(), ec);
    if (ok) {
        std::cout << "selftest: OK (session: vault, entries on both axes, search, field, clipboard, settings, import, rotation, preferences)\n";
    }
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        cypherarx::init_crypto();
        if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
            return run_selftest();
        }
        cypherarx::lamina_gui::App app;
        app.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
