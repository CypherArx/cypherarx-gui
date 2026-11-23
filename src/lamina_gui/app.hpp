#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "lamina/motion/animated.hpp"
#include "lamina/render/image.hpp"
#include "lamina/widgets/label.hpp"
#include "lamina/widgets/title_bar.hpp"
#include "lamina_sdl3/window.hpp"

#include "model/preferences.hpp"
#include "model/session.hpp"

#include "lamina_gui/controls.hpp"
#include "lamina_gui/list_view.hpp"
#include "lamina_gui/sheet.hpp"
#include "lamina_gui/sheets.hpp"

namespace cypherarx::lamina_gui {

class WindowClipboard final : public gui::ClipboardPort {
public:
    explicit WindowClipboard(lm::input::Clipboard& clipboard) : clipboard_(clipboard) {}
    std::string text() override { return clipboard_.text(); }
    void set_text(std::string_view utf8) override { clipboard_.set_text(utf8); }

private:
    lm::input::Clipboard& clipboard_;
};

class App final : public Host {
public:
    App();
    void run();

    gui::Session& session() override { return session_; }
    gui::ClipboardPort& clipboard() override { return clipboard_; }
    void present(Modal& modal) override;
    void message(const std::string& text) override;
    void confirm(const std::string& text, std::function<void()> on_yes) override;

private:
    Argb update(const lm::sdl3::Frame& frame);
    bool paint(Canvas& canvas);
    void wire();
    NodePtr build_layout();
    void layout();
    void sync();
    void sync_sidebar();
    void sync_tree();
    void sync_fields();
    void handle_key(const KeyEvent& e);
    void sweep();
    Modal* top_modal() const;
    void choose_vault(gui::UnlockMode mode);
    void offer_last_vault();
    void save_preferences();
    void notify(const std::string& text);
    void apply_theme(std::size_t index);
    void open_theme_menu();
    void open_add_menu();
    void delete_entry(std::int64_t id);
    void delete_field(std::int64_t id);
    double bar_height() const;
    bool handle_title_bar(const PointerEvent& e);

    const Config& cfg_;
    lm::sdl3::Window window_;
    std::filesystem::path preferences_file_;
    gui::Preferences preferences_;
    std::string notice_;
    std::size_t theme_index_;
    Kit kit_;
    lm::widgets::TitleBar title_bar_;
    lm::render::Image app_icon_;
    lm::widgets::Label window_title_;
    Rect app_icon_rect_{0.0, 0.0, 0.0, 0.0};
    ColorScheme scheme_;
    ColorScheme scheme_from_;
    ColorScheme scheme_to_;
    lm::motion::Animated theme_mix_;
    WindowClipboard clipboard_;
    gui::Session session_;
    Panel base_;

    PaneC& sidebar_pane_;
    PaneC& center_pane_;
    PaneC& detail_pane_;
    ButtonC& new_;
    ButtonC& open_;
    ButtonC& close_;
    LabelC& path_;
    IconButtonC& theme_button_;
    ListView& sidebar_;
    TabsC& tabs_;
    TextFieldC& search_;
    ButtonC& add_entry_;
    IconButtonC& organize_;
    ListView& tree_;
    LabelC& title_;
    LabelC& detail_;
    ButtonC& add_field_;
    ListView& fields_;
    TotpC& totp_;
    ButtonC& totp_copy_;
    ButtonC& totp_close_;
    LabelC& status_;

    PasswordSheet password_sheet_;
    BusySheet busy_sheet_;
    EntrySheet entry_sheet_;
    FieldSheet field_sheet_;
    OrganizeSheet organize_sheet_;
    LoginSheet login_sheet_;

    std::vector<Modal*> modals_;
    std::vector<std::unique_ptr<Modal>> owned_;
    std::unordered_set<std::int64_t> collapsed_;
    NodePtr root_;
    Rect window_rect_{0.0, 0.0, 0.0, 0.0};
    Size frame_size_{0.0, 0.0};
    std::uint64_t seen_revision_ = 0;
    bool dirty_ = true;
    bool totp_in_layout_ = false;
};

} // namespace cypherarx::lamina_gui
