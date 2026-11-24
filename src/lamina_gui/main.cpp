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
    settings.kdf_benchmark = cypherarx::lamina_gui::config().security.kdf_benchmark_selftest;
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
        ok = expect(s.erase_source(csv).has_value() && !fs::exists(csv), "the imported file is erased") && ok;
        ok = expect(!s.erase_source(csv).has_value(), "erasing a missing file is refused") && ok;
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
        s.toggle_mark(account);
        s.toggle_mark(moved);
        s.toggle_mark(service);
        ok = expect(s.marked().size() == 2 && s.set_persona_of_marked(std::nullopt).has_value() &&
                        !persona_of(account).has_value() && !persona_of(moved).has_value(),
                    "clearing the persona of the marked accounts") &&
             ok;
        ok = expect(s.marked().empty(), "the ticks are cleared after the change") && ok;
        s.toggle_mark(account);
        s.toggle_mark(moved);
        ok = expect(s.set_persona_of_marked(persona).has_value() && persona_of(account) == persona &&
                        persona_of(moved) == persona,
                    "setting the persona of the marked accounts") &&
             ok;
        s.clear_marks();
        s.mark_all(true);
        ok = expect(s.all_marked() && s.marked().size() == s.accounts_in_view().size() &&
                        !s.marked().contains(service),
                    "tick all takes every account in view and nothing else") &&
             ok;
        s.mark_all(false);
        ok = expect(s.marked().empty() && !s.all_marked(), "untick all") && ok;

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

        const std::string next_text = "another-password";
        const std::span<const std::uint8_t> next_pw{reinterpret_cast<const std::uint8_t*>(next_text.data()),
                                                    next_text.size()};
        const auto wait_rekey = [&] {
            std::optional<gui::Status> done;
            while (!(done = s.poll_rotate())) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return *done;
        };
        s.start_change_password(next_pw, next_pw);
        ok = expect(!wait_rekey().has_value(), "a wrong current password does not change it") && ok;
        s.start_change_password(pw, next_pw);
        ok = expect(wait_rekey().has_value(), "change the master password") && ok;
        s.start_rotate(pw);
        ok = expect(!wait_rekey().has_value(), "the old password is refused after the change") && ok;
        ok = expect(s.copy_selected_field(clipboard).has_value() && clipboard.text() == "secret",
                    "copy after the password change") &&
             ok;
        s.start_change_password(next_pw, pw);
        ok = expect(wait_rekey().has_value(), "change the master password back") && ok;

        const auto judged = [](std::string_view candidate, std::string_view current = {}) {
            const auto span_of = [](std::string_view t) {
                return std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(t.data()), t.size()};
            };
            return gui::judge_password(span_of(candidate), span_of(current));
        };
        ok = expect(judged("pw").refused() && judged("password1234").refused() && !judged("").summary.empty(),
                    "short and guessable master passwords are refused") &&
             ok;
        const gui::PasswordJudgement strong = judged("correct horse battery staple");
        ok = expect(!strong.refused() && !strong.needs_confirmation() && !strong.against_current,
                    "a passphrase is accepted without a question") &&
             ok;
        const gui::PasswordJudgement fair = judged("applebanana12");
        ok = expect(!fair.refused() && fair.needs_confirmation(), "a fair password needs a confirmation") && ok;
        const gui::PasswordJudgement step_down = judged("kettle-bluebird", "correct horse battery staple");
        ok = expect(!step_down.refused() && step_down.against_current == -1 && step_down.needs_confirmation(),
                    "a weaker password than the current one needs a confirmation") &&
             ok;
        const gui::PasswordJudgement step_up = judged("correct horse battery staple", "pw");
        ok = expect(step_up.against_current == 1 && !step_up.needs_confirmation(),
                    "a stronger password than the current one") &&
             ok;

        s.start_kdf_benchmark();
        while (!s.poll_kdf_benchmark()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ok = expect(s.kdf_choices().has_value(), "the key strength is measured") && ok;
        if (s.kdf_choices()) {
            const std::string_view phrase = "correct horse battery staple";
            const std::vector<std::string> plain = gui::kdf_choice_labels(*s.kdf_choices(), {});
            const std::vector<std::string> with_years = gui::kdf_choice_labels(
                *s.kdf_choices(), {reinterpret_cast<const std::uint8_t*>(phrase.data()), phrase.size()});
            ok = expect(plain.size() == 3 && with_years.size() == 3 && plain[0].find("year") == std::string::npos &&
                            with_years[0].find("year") != std::string::npos,
                        "three key strengths, with the years once there is a password") &&
                 ok;
            s.start_change_password(pw, next_pw, (*s.kdf_choices())[2].params);
            ok = expect(wait_rekey().has_value(), "change the password with a measured key strength") && ok;
            s.start_change_password(next_pw, pw);
            ok = expect(wait_rekey().has_value(), "and back") && ok;
        }

        cypherarx::Settings away = s.vault_settings();
        away.session_lock_on_screensaver = false;
        ok = expect(s.set_vault_settings(away).has_value() && !s.watches_away() &&
                        !s.lock_for(clipboard, gui::Away::screen_locked) && s.has_vault(),
                    "no lock on a locked screen when the setting is off") &&
             ok;
        away.session_lock_on_screensaver = true;
        ok = expect(s.set_vault_settings(away).has_value() && s.watches_away() &&
                        s.lock_for(clipboard, gui::Away::screen_locked) && !s.has_vault() &&
                        !s.watches_away(),
                    "the vault locks when the screen is locked") &&
             ok;
        ok = expect(!s.lock_for(clipboard, gui::Away::resumed), "nothing to lock without a vault") && ok;
        s.start_unlock(gui::UnlockMode::Open, path, pw);
        std::optional<gui::Status> reopened;
        while (!(reopened = s.poll_unlock(clipboard))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ok = expect(reopened->has_value() && s.has_vault(), "reopen after the lock") && ok;

        const auto t0 = gui::Session::Clock::now();
        s.note_activity(t0);
        ok = expect(!s.lock_if_due(clipboard, t0 + std::chrono::minutes(2)) && s.has_vault(),
                    "no lock before the idle time is up") &&
             ok;
        s.note_activity(t0 + std::chrono::minutes(2));
        ok = expect(!s.lock_if_due(clipboard, t0 + std::chrono::minutes(4)) && s.has_vault(),
                    "input restarts the idle time") &&
             ok;
        s.note_activity(t0 + std::chrono::hours(5));
        ok = expect(!s.lock_if_due(clipboard, t0 + std::chrono::hours(5) + std::chrono::minutes(2)) &&
                        s.has_vault(),
                    "a vault in use is not locked however long it has been open") &&
             ok;
        s.note_activity(t0 + std::chrono::minutes(2));
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
        std::cout << "selftest: OK (session: vault, entries on both axes, search, field, clipboard, settings, import, rotation, password change, locking, password strength, key strength, preferences)\n";
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
