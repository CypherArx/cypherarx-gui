#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "cypherarx/secret.hpp"

#include "lamina_gui/app.hpp"
#include "lamina_gui/config.hpp"
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
        ok = expect(!s.commit_field(stray, {}).has_value(), "no fields on a service") && ok;

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
        s.close_vault(clipboard);
        ok = expect(clipboard.text().empty() && !s.has_vault(), "close clears the clipboard") && ok;
    }
    fs::remove(path, ec);
    if (ok) {
        std::cout << "selftest: OK (session: vault, entries on both axes, search, field, clipboard)\n";
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
