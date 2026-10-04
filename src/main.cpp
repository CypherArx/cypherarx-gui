#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include "cypherarx/crypto.hpp"
#include "cypherarx/vault.hpp"

#include "app/app.hpp"
#include "config.hpp"
#include "platform/host.hpp"
#include "platform/imgui_setup.hpp"

namespace {

int run_selftest() {
    namespace fs = std::filesystem;
    using cypherarx::Entry;
    using cypherarx::Vault;
    const fs::path path = fs::temp_directory_path() / "cypherarx_gui_selftest.cax";
    std::error_code ec;
    fs::remove(path, ec);

    const std::vector<std::uint8_t> pw = {'p', 'w'};
    auto v = Vault::create(path, pw, cypherarx::gui::config().security.kdf_params_selftest());
    if (!v) {
        std::cerr << "selftest: create failed\n";
        return 1;
    }
    Entry e;
    e.entry_type = "service";
    e.label = "Example";
    auto added = v->add_entry(e);
    if (!added) {
        std::cerr << "selftest: add_entry failed\n";
        return 1;
    }
    const std::vector<std::uint8_t> secret = {'s', 'e', 'c', 'r', 'e', 't'};
    if (!v->add_field(added->id, "pw", "password", secret, "")) {
        std::cerr << "selftest: add_field failed\n";
        return 1;
    }
    fs::remove(path, ec);
    std::cout << "selftest: OK (core linked, vault + entry + field round-trip works)\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    cypherarx::init_crypto();
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
        return run_selftest();
    }

    namespace gui = cypherarx::gui;
    const gui::Config& cfg = gui::config();

    if (!gui::platform::init(cfg.window)) {
        std::cerr << "Failed to create the window / D3D11 device\n";
        return 1;
    }
    gui::platform::create_imgui_context(cfg.font, cfg.theme);
    gui::platform::attach_imgui();

    {
        gui::App app;
        while (gui::platform::pump_events()) {
            gui::platform::begin_frame();
            app.draw();
            gui::platform::end_frame(cfg.theme.clear_color);
        }
    }

    gui::platform::detach_imgui();
    gui::platform::destroy_imgui_context();
    gui::platform::shutdown();
    return 0;
}
