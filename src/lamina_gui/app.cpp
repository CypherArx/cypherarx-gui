#include "lamina_gui/app.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "lamina/tokens/motion.hpp"
#include "lamina/tokens/title_bar.hpp"
#include "lamina_sdl3/app.hpp"
#include "lamina_sdl3/resources.hpp"
#include "lamina_sdl3/system_fonts.hpp"
#include "lamina_thorvg/canvas.hpp"

#include "platform/file_dialog.hpp"

namespace cypherarx::lamina_gui {

namespace {

using lm::tokens::ButtonStyle;

constexpr int unsized_pixels = 1;

constexpr int group_of(gui::Scope::Kind kind) {
    return static_cast<int>(kind) + 1;
}

constexpr gui::Scope::Kind kind_of(int group) {
    return static_cast<gui::Scope::Kind>(group - 1);
}

enum EntryAction : std::size_t { entry_login, entry_merge, entry_edit, entry_delete };

enum FieldAction : std::size_t { field_copy, field_totp, field_edit, field_delete };

constexpr unsigned without(std::size_t action) {
    return ~(1u << action);
}

class FixedWidth final : public lm::layout::Node {
public:
    FixedWidth(double width, NodePtr child) : width_(width), child_(std::move(child)) {}

protected:
    Size on_measure(const Constraints& c) override {
        return child_->measure({width_, width_, c.min_height, c.max_height});
    }
    void on_arrange(const Rect& r) override { child_->arrange(r); }

private:
    double width_;
    NodePtr child_;
};

gui::SessionSettings session_settings(const Config& cfg) {
    gui::SessionSettings s;
    s.kdf_params = cfg.security.kdf_params;
    s.clipboard_clear_seconds = cfg.clipboard.clear_after_seconds;
    s.away_poll_seconds = cfg.security.away_poll_seconds;
    s.kdf_benchmark = cfg.security.kdf_benchmark;
    s.category_indent = cfg.category_indent;
    return s;
}

NodePtr grow(NodePtr node, double weight = 1.0) {
    node->weight = weight;
    return node;
}

NodePtr pane(PaneC& background, NodePtr content) {
    return std::make_shared<lm::layout::Stack>(std::vector<NodePtr>{background.node(), std::move(content)});
}

int title_bar_extra(const WindowConfig& window) {
    const bool drawn = window.client_title_bar && lm::sdl3::client_title_bar_supported();
    return drawn ? static_cast<int>(std::ceil(lm::tokens::title_bar_tokens.height)) : 0;
}

std::optional<std::size_t> theme_with_id(const ThemeConfig& config, const std::string& id) {
    for (std::size_t i = 0; i < config.presets.size(); ++i) {
        if (id == config.presets[i].id) {
            return i;
        }
    }
    return std::nullopt;
}

lm::render::Image image_from(const lm::resources::Resource& resource) {
    const std::size_t header = 2 * sizeof(std::uint32_t);
    std::uint32_t size[2] = {0, 0};
    if (resource.data.size() >= header) {
        std::memcpy(size, resource.data.data(), header);
    }
    const std::size_t count = static_cast<std::size_t>(size[0]) * size[1];
    if (count == 0 || resource.data.size() != header + count * sizeof(lm::color::Argb)) {
        throw std::runtime_error(std::string(resource.name) + " is not an image");
    }
    lm::render::Image image{static_cast<int>(size[0]), static_cast<int>(size[1]),
                            std::vector<lm::color::Argb>(count)};
    std::memcpy(image.pixels.data(), resource.data.data() + header, count * sizeof(lm::color::Argb));
    return image;
}

ColorScheme build_scheme(const ThemeConfig& config, const ThemePreset& theme) {
    return lm::color::spec_2025().build({config.seed, theme.dark, theme.variant, config.contrast});
}

double seconds_to_next_tick() {
    using namespace std::chrono;
    const auto now = system_clock::now().time_since_epoch();
    const auto part = duration_cast<milliseconds>(now).count() % 1000;
    return static_cast<double>(1000 - part) / 1000.0;
}

} // namespace

App::App()
    : cfg_(config()),
      window_({cfg_.window.title, cfg_.window.width_dp,
               cfg_.window.height_dp + title_bar_extra(cfg_.window), cfg_.window.client_title_bar},
              std::make_unique<lm::thorvg::Canvas>(unsized_pixels, unsized_pixels)),
      preferences_file_(lm::sdl3::preference_directory(cfg_.preferences.organization,
                                                       cfg_.preferences.application) /
                        cfg_.preferences.file),
      preferences_(gui::load_preferences(preferences_file_)),
      theme_index_(theme_with_id(cfg_.theme, preferences_.theme).value_or(cfg_.theme.initial)),
      kit_{lm::sdl3::system_font_stack(), lm::icons::IconFont::from_resource(res::fonts::symbols), &window_.clipboard(), &cfg_,
           &cfg_.theme.presets[theme_index_]},
      title_bar_({"", kit_.icons, nullptr, kit_.theme->background}, kit_.fonts),
      app_icon_(image_from(res::files::app_icon)),
      window_title_({cfg_.window.title, lm::tokens::title_bar_tokens.title,
                     lm::tokens::title_bar_tokens.title_color, false},
                    kit_.fonts),
      scheme_(build_scheme(cfg_.theme, *kit_.theme)),
      scheme_from_(scheme_),
      scheme_to_(scheme_),
      theme_mix_(1.0, lm::tokens::motion_scheme.slow_effects()),
      clipboard_(window_.clipboard()),
      session_(session_settings(cfg_)),
      base_(kit_),
      sidebar_pane_(base_.add<PaneC>(kit_)),
      center_pane_(base_.add<PaneC>(kit_)),
      detail_pane_(base_.add<PaneC>(kit_)),
      new_(base_.add<ButtonC>(kit_, ButtonSpec{str::new_vault, kit_.theme->vault_action, cfg_.icons.new_vault})),
      open_(base_.add<ButtonC>(kit_, ButtonSpec{str::open_vault, kit_.theme->vault_action, cfg_.icons.open_vault})),
      close_(base_.add<ButtonC>(kit_, ButtonSpec{str::close_vault, ButtonStyle::outlined, cfg_.icons.close_vault})),
      path_(base_.add<LabelC>(kit_, LabelSpec{"", TypeRole::body_medium, Role::on_surface_variant})),
      vault_menu_(base_.add<IconButtonC>(kit_, cfg_.icons.vault_menu, false, str::vault_menu)),
      theme_button_(base_.add<IconButtonC>(kit_, cfg_.icons.theme_menu, false, str::theme)),
      sidebar_(base_.add<ListView>(kit_)),
      tabs_(base_.add<TabsC>(kit_, std::vector<TabSpec>{{str::axis_service, cfg_.icons.service},
                                                         {str::axis_persona, cfg_.icons.persona}})),
      search_(base_.add<TextFieldC>(kit_, str::search_hint)),
      add_entry_(base_.add<ButtonC>(kit_, ButtonSpec{str::add, kit_.theme->entry_action, cfg_.icons.add})),
      organize_(base_.add<IconButtonC>(kit_, cfg_.icons.organize, false, str::organize)),
      tick_all_(base_.add<IconButtonC>(kit_, cfg_.icons.tick_all, true, str::tick_all)),
      set_persona_(base_.add<IconButtonC>(kit_, cfg_.icons.set_persona, false, str::set_persona)),
      tree_(base_.add<ListView>(
          kit_, ListOptions{.columns = {{str::col_name, str::col_type, str::col_persona}, {2.0, 1.0, 1.0}},
                            .tree = true,
                            .select_follows_cursor = true,
                            .checks = true,
                            .row_actions = {{cfg_.icons.login_methods, str::login_methods},
                                            {cfg_.icons.merge_service, str::merge_service},
                                            {cfg_.icons.edit, str::edit_entry},
                                            {cfg_.icons.remove, str::delete_entry}}})),
      title_(base_.add<LabelC>(kit_, LabelSpec{str::fields, TypeRole::title_large})),
      detail_(base_.add<LabelC>(kit_, LabelSpec{"", TypeRole::body_medium, Role::on_surface_variant})),
      add_field_(base_.add<ButtonC>(kit_, ButtonSpec{str::add, kit_.theme->field_action, cfg_.icons.add})),
      fields_(base_.add<ListView>(
          kit_, ListOptions{.columns = {{str::label, str::type, str::notes}, {3.0, 2.0, 3.0}},
                            .select_follows_cursor = true,
                            .row_actions = {{cfg_.icons.copy, str::copy_value},
                                            {cfg_.icons.totp, str::show_totp},
                                            {cfg_.icons.edit, str::edit_field},
                                            {cfg_.icons.remove, str::delete_field}}})),
      totp_(base_.add<TotpC>(kit_)),
      totp_copy_(base_.add<ButtonC>(kit_, ButtonSpec{str::copy_value, ButtonStyle::tonal})),
      totp_close_(base_.add<ButtonC>(kit_, ButtonSpec{str::close, ButtonStyle::text})),
      status_(base_.add<LabelC>(kit_, LabelSpec{"", TypeRole::body_medium, Role::on_surface_variant})),
      password_sheet_(kit_, *this),
      busy_sheet_(kit_),
      entry_sheet_(kit_, *this),
      field_sheet_(kit_, *this),
      organize_sheet_(kit_, *this),
      login_sheet_(kit_, *this),
      settings_sheet_(kit_, *this),
      import_sheet_(kit_, *this),
      change_password_sheet_(kit_, *this),
      merge_sheet_(kit_, *this) {
    window_.set_minimum_size(cfg_.window.min_width_dp,
                             cfg_.window.min_height_dp + title_bar_extra(cfg_.window));
    wire();
    if (!preferences_.theme.empty() && !theme_with_id(cfg_.theme, preferences_.theme)) {
        notify(str::theme_unknown);
    }
    offer_last_vault();
}

void App::notify(const std::string& text) {
    if (!notice_.empty()) {
        notice_ += str::notice_separator;
    }
    notice_ += text;
    session_.report(notice_);
}

void App::save_preferences() {
    if (!gui::save_preferences(preferences_file_, preferences_)) {
        session_.report(str::preferences_not_saved);
    }
}

void App::watch_away() {
    if (!session_.watches_away()) {
        away_watching_ = false;
        return;
    }
    const auto now = gui::Session::Clock::now();
    if (!away_watching_) {
        away_watching_ = true;
        resumed_ = false;
        away_polled_ = now;
        away_.poll();
        return;
    }
    if (now - away_polled_ < std::chrono::duration<double>(cfg_.security.away_poll_seconds)) {
        return;
    }
    away_polled_ = now;
    const gui::platform::AwayState state = away_.poll();
    resumed_ = resumed_ || state.resumed;
    std::optional<gui::Away> reason;
    if (state.screensaver) {
        reason = gui::Away::screensaver;
    } else if (state.screen_locked) {
        reason = gui::Away::screen_locked;
    } else if (state.disconnected) {
        reason = gui::Away::user_switched;
    } else if (resumed_) {
        reason = gui::Away::resumed;
    }
    if (reason && session_.lock_for(clipboard_, *reason)) {
        resumed_ = false;
        locked();
    }
}

void App::locked() {
    const std::string reason = session_.status();
    const std::array<Sheet*, 8> sheets{&entry_sheet_,    &field_sheet_,  &organize_sheet_,
                                       &login_sheet_,    &settings_sheet_, &import_sheet_,
                                       &merge_sheet_,    &change_password_sheet_};
    for (Sheet* sheet : sheets) {
        if (sheet->is_open()) {
            if (sheet->on_cancel) {
                sheet->on_cancel();
            } else {
                sheet->close();
            }
        }
    }
    for (const auto& modal : owned_) {
        modal->cancel();
    }
    offer_last_vault();
    session_.report(reason);
}

void App::offer_last_vault() {
    if (preferences_.vault.empty()) {
        return;
    }
    std::filesystem::path file = gui::path_from_utf8(preferences_.vault);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) {
        notify(std::string(str::last_vault_missing) + str::notice_separator + preferences_.vault);
        return;
    }
    password_sheet_.open(gui::UnlockMode::Open, std::move(file));
}

double App::bar_height() const {
    return window_.client_title_bar() ? title_bar_.height() : 0.0;
}

bool App::handle_title_bar(const PointerEvent& e) {
    const double bar = bar_height();
    if (bar <= 0.0) {
        return false;
    }
    switch (title_bar_.handle(e)) {
    case lm::widgets::TitleBarAction::minimize:
        window_.minimize();
        break;
    case lm::widgets::TitleBarAction::toggle_maximize:
        window_.toggle_maximize();
        break;
    case lm::widgets::TitleBarAction::close:
        window_.close();
        break;
    case lm::widgets::TitleBarAction::none:
        break;
    }
    return e.action == PointerAction::down && e.y_dp < bar;
}

void App::apply_theme(std::size_t index) {
    if (index >= cfg_.theme.presets.size() || index == theme_index_) {
        return;
    }
    theme_index_ = index;
    kit_.theme = &cfg_.theme.presets[index];
    const ThemePreset& theme = *kit_.theme;
    scheme_from_ = scheme_;
    scheme_to_ = build_scheme(cfg_.theme, theme);
    theme_mix_.snap_to(0.0);
    theme_mix_.animate_to(1.0);
    title_bar_ = lm::widgets::TitleBar({"", kit_.icons, nullptr, theme.background}, kit_.fonts);
    new_.set_style(theme.vault_action);
    open_.set_style(theme.vault_action);
    add_entry_.set_style(theme.entry_action);
    add_field_.set_style(theme.field_action);
    preferences_.theme = theme.id;
    save_preferences();
}

void App::open_theme_menu() {
    lm::widgets::MenuOptions options;
    for (std::size_t i = 0; i < cfg_.theme.presets.size(); ++i) {
        const ThemePreset& theme = cfg_.theme.presets[i];
        options.items.push_back(
            {theme.name, lm::icons::Icon{kit_.icons, i == theme_index_ ? cfg_.icons.checked : theme.icon}});
    }
    auto menu = std::make_unique<MenuModal>(kit_, std::move(options),
                                            [this](std::size_t index) { apply_theme(index); });
    menu->show(theme_button_.bounds(), frame_size_);
    present(*menu);
    owned_.push_back(std::move(menu));
}

void App::open_vault_menu() {
    if (!session_.has_vault()) {
        message(str::no_vault);
        return;
    }
    lm::widgets::MenuOptions options;
    options.items.push_back({str::vault_settings, lm::icons::Icon{kit_.icons, cfg_.icons.vault_settings}});
    options.items.push_back({str::import_csv, lm::icons::Icon{kit_.icons, cfg_.icons.import_csv}});
    options.items.push_back({str::rotate_key, lm::icons::Icon{kit_.icons, cfg_.icons.rotate_key}});
    options.items.push_back(
        {str::change_password, lm::icons::Icon{kit_.icons, cfg_.icons.change_password}});
    auto menu = std::make_unique<MenuModal>(kit_, std::move(options), [this](std::size_t index) {
        const std::array<void (App::*)(), 4> actions{&App::open_vault_settings, &App::import_csv,
                                                     &App::rotate_key, &App::change_password};
        if (index < actions.size()) {
            (this->*actions[index])();
        }
    });
    menu->show(vault_menu_.bounds(), frame_size_);
    present(*menu);
    owned_.push_back(std::move(menu));
}

void App::open_persona_menu() {
    if (session_.marked_accounts().empty()) {
        message(str::select_accounts_first);
        return;
    }
    const gui::IdLabels personas = session_.entry_choices().personas;
    lm::widgets::MenuOptions options;
    for (const auto& [id, label] : personas) {
        options.items.push_back({label, lm::icons::Icon{kit_.icons, cfg_.icons.persona}});
    }
    auto menu = std::make_unique<MenuModal>(kit_, std::move(options), [this, personas](std::size_t index) {
        if (index >= personas.size()) {
            return;
        }
        const std::int64_t id = personas[index].first;
        const std::size_t count = session_.marked_accounts().size();
        char text[256];
        std::snprintf(text, sizeof text, str::confirm_set_persona_fmt, count, personas[index].second.c_str());
        confirm(text, [this, id] {
            check(session_.set_persona_of_marked(id != 0 ? std::optional<std::int64_t>(id) : std::nullopt));
        });
    });
    menu->show(set_persona_.bounds(), frame_size_);
    present(*menu);
    owned_.push_back(std::move(menu));
}

void App::open_vault_settings() {
    settings_sheet_.open();
}

void App::import_csv() {
    const gui::platform::FileDialogOptions options{cfg_.csv_filter, cfg_.csv_extension};
    const auto file = gui::platform::choose_file_to_open(options);
    base_.cancel();
    if (file) {
        import_sheet_.open(*file);
    }
}

void App::run_import(const std::filesystem::path& file, std::optional<std::int64_t> persona, bool erase) {
    const auto summary = session_.import_csv(file, persona);
    if (!summary) {
        message(summary.error().message);
        return;
    }
    const std::string result = session_.status();
    if (!erase) {
        message(result + "\n\n" + str::import_reminder);
        return;
    }
    const auto erased = session_.erase_source(file);
    message(result + "\n\n" + (erased ? session_.status() : erased.error().message));
}

void App::rotate_key() {
    confirm(str::confirm_rotate, [this] {
        const std::filesystem::path file = gui::path_from_utf8(session_.vault_path());
        password_sheet_.ask(
            str::rotate_key, file,
            [this](std::span<const std::uint8_t> password) { session_.start_rotate(password); },
            [this, file] {
                if (session_.rotating()) {
                    busy_sheet_.open(str::rotate_key, file, str::rotating);
                    present(busy_sheet_);
                }
            });
    });
}

void App::change_password() {
    change_password_sheet_.open(gui::path_from_utf8(session_.vault_path()));
}

void App::delete_entry(std::int64_t id) {
    session_.select_entry(id);
    confirm(str::confirm_delete_entry, [this] { check(session_.delete_selected_entry()); });
}

void App::delete_field(std::int64_t id) {
    session_.select_field(id);
    confirm(str::confirm_delete_field, [this] { check(session_.delete_selected_field()); });
}

void App::open_add_menu() {
    const std::array<const char*, 4> labels{str::service, str::persona, str::account, str::child};
    const std::array<lm::icons::Symbol, 4> symbols{cfg_.icons.service, cfg_.icons.persona,
                                                    cfg_.icons.account, cfg_.icons.child};
    lm::widgets::MenuOptions options;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        options.items.push_back({labels[i], lm::icons::Icon{kit_.icons, symbols[i]}});
    }
    auto menu = std::make_unique<MenuModal>(kit_, std::move(options), [this](std::size_t index) {
        entry_sheet_.open_new(str::entry_types[index]);
    });
    menu->show(add_entry_.bounds(), frame_size_);
    present(*menu);
    owned_.push_back(std::move(menu));
}

void App::choose_vault(gui::UnlockMode mode) {
    const gui::platform::FileDialogOptions options{cfg_.file_filter, cfg_.vault_extension};
    auto file = mode == gui::UnlockMode::Create ? gui::platform::choose_file_to_save(options)
                                                : gui::platform::choose_file_to_open(options);
    base_.cancel();
    if (!file) {
        return;
    }
    if (mode == gui::UnlockMode::Create) {
        change_password_sheet_.open_new(std::move(*file));
    } else {
        password_sheet_.open(mode, std::move(*file));
    }
}

void App::wire() {
    new_.on_click = [this] { choose_vault(gui::UnlockMode::Create); };
    open_.on_click = [this] { choose_vault(gui::UnlockMode::Open); };
    close_.on_click = [this] { session_.close_vault(clipboard_); };
    theme_button_.on_click = [this] { open_theme_menu(); };
    vault_menu_.on_click = [this] { open_vault_menu(); };
    change_password_sheet_.on_started = [this] {
        if (session_.rotating()) {
            busy_sheet_.open(str::change_password, gui::path_from_utf8(session_.vault_path()), str::rotating);
            present(busy_sheet_);
        } else if (session_.unlocking()) {
            busy_sheet_.open(str::new_vault, gui::path_from_utf8(session_.unlock_path()), str::unlocking);
            present(busy_sheet_);
        }
    };
    import_sheet_.on_import = [this](const std::filesystem::path& file,
                                     std::optional<std::int64_t> persona,
                                     bool erase) { run_import(file, persona, erase); };
    password_sheet_.on_started = [this] {
        busy_sheet_.open(session_.unlock_mode() == gui::UnlockMode::Create ? str::new_vault
                                                                           : str::open_vault,
                         gui::path_from_utf8(session_.unlock_path()), str::unlocking);
        present(busy_sheet_);
    };

    sidebar_.on_select = [this](const ListRow& row) {
        session_.set_scope({kind_of(row.group), row.id});
    };
    tabs_.on_change = [this](std::size_t index) {
        session_.set_axis(index == 0 ? gui::Axis::Service : gui::Axis::Persona);
    };
    search_.on_change = [this](const std::string& query) { session_.set_query(query); };
    tree_.on_select = [this](const ListRow& row) { session_.select_entry(row.id); };
    tree_.on_check = [this](const ListRow& row) { session_.toggle_mark(row.id); };
    set_persona_.on_click = [this] { open_persona_menu(); };
    tick_all_.on_change = [this](bool) { session_.mark_all(!session_.all_marked()); };
    tree_.on_toggle = [this](const ListRow& row) {
        std::unordered_set<std::int64_t>& toggled =
            session_.projection().filtered ? collapsed_ : expanded_;
        if (!toggled.erase(row.id)) {
            toggled.insert(row.id);
        }
        dirty_ = true;
    };
    tree_.on_activate = [this](const ListRow&) { entry_sheet_.open_selected(); };
    add_entry_.on_click = [this] { open_add_menu(); };
    tree_.on_action = [this](const ListRow& row, std::size_t action) {
        session_.select_entry(row.id);
        switch (action) {
        case entry_login:
            login_sheet_.open();
            break;
        case entry_merge:
            merge_sheet_.open();
            break;
        case entry_edit:
            entry_sheet_.open_selected();
            break;
        case entry_delete:
            delete_entry(row.id);
            break;
        default:
            break;
        }
    };
    tree_.on_remove = [this](const ListRow& row) { delete_entry(row.id); };
    organize_.on_click = [this] { organize_sheet_.open(); };

    fields_.on_select = [this](const ListRow& row) { session_.select_field(row.id); };
    fields_.on_activate = [this](const ListRow&) { field_sheet_.open_selected(); };
    fields_.on_action = [this](const ListRow& row, std::size_t action) {
        session_.select_field(row.id);
        switch (action) {
        case field_copy:
            check(session_.copy_selected_field(clipboard_));
            break;
        case field_totp:
            check(session_.show_totp());
            break;
        case field_edit:
            field_sheet_.open_selected();
            break;
        case field_delete:
            delete_field(row.id);
            break;
        default:
            break;
        }
    };
    fields_.on_remove = [this](const ListRow& row) { delete_field(row.id); };
    add_field_.on_click = [this] { field_sheet_.open_new(); };

    totp_copy_.on_click = [this] { check(session_.copy_totp(clipboard_)); };
    totp_close_.on_click = [this] { session_.close_totp(); };
}

NodePtr App::build_layout() {
    const LayoutConfig& t = cfg_.layout;
    const auto stretch = lm::layout::CrossAlign::stretch;
    const auto center = lm::layout::CrossAlign::center;
    const auto padding = lm::layout::Insets::all(t.pane_padding);

    auto toolbar = lm::layout::row({new_.node(), open_.node(), close_.node(), grow(path_.node()), vault_menu_.node(), theme_button_.node()});
    toolbar->spacing(t.control_gap).cross_align(center);

    auto sidebar = lm::layout::column({grow(sidebar_.node())});
    sidebar->padding(padding).spacing(t.control_gap).cross_align(stretch).fill_cross(true).fill_main(true);

    auto entry_bar = lm::layout::row(
        {grow(search_.node()), add_entry_.node(), organize_.node(), tick_all_.node(), set_persona_.node()});
    entry_bar->spacing(t.control_gap).cross_align(center);
    auto entries = lm::layout::column({tabs_.node(), entry_bar, grow(tree_.node())});
    entries->padding(padding).spacing(t.gap).cross_align(stretch).fill_cross(true).fill_main(true);

    auto field_bar = lm::layout::row({grow(title_.node()), add_field_.node()});
    field_bar->spacing(t.control_gap).cross_align(center);
    std::vector<NodePtr> detail_rows{field_bar, detail_.node(), grow(fields_.node())};
    totp_in_layout_ = totp_.visible();
    if (totp_in_layout_) {
        auto totp = lm::layout::row({totp_.node(), lm::layout::flexible_space(), totp_copy_.node(),
                                     totp_close_.node()});
        totp->spacing(t.control_gap).cross_align(center);
        detail_rows.push_back(totp);
    }
    auto detail = lm::layout::column(std::move(detail_rows));
    detail->padding(padding).spacing(t.gap).cross_align(stretch).fill_cross(true).fill_main(true);

    auto body = lm::layout::row(
        {std::make_shared<FixedWidth>(t.sidebar_width, pane(sidebar_pane_, sidebar)),
         grow(pane(center_pane_, entries)),
         std::make_shared<FixedWidth>(t.detail_width, pane(detail_pane_, detail))});
    body->spacing(t.gap).cross_align(stretch).fill_cross(true);

    auto root = lm::layout::column({toolbar, grow(body), status_.node()});
    root->padding(lm::layout::Insets::all(t.margin))
        .spacing(t.gap)
        .cross_align(stretch)
        .fill_cross(true)
        .fill_main(true);
    return root;
}

void App::layout() {
    if (!root_ || totp_in_layout_ != totp_.visible()) {
        root_ = build_layout();
    }
    lm::layout::run(*root_, window_rect_);
    base_.set_window(window_rect_);
    for (Modal* m : modals_) {
        m->set_area(window_rect_);
    }
}

void App::sync_sidebar() {
    using Kind = gui::Scope::Kind;
    const gui::Scope scope = session_.scope();
    std::vector<ListRow> rows;
    auto section = [&](const char* title) {
        ListRow row;
        row.text = title;
        row.section = true;
        rows.push_back(std::move(row));
    };
    auto item = [&](Kind kind, std::int64_t id, std::string text, lm::icons::Symbol icon) {
        ListRow row;
        row.group = group_of(kind);
        row.id = id;
        row.text = std::move(text);
        row.icon = icon;
        row.selected = scope == gui::Scope{kind, id};
        rows.push_back(std::move(row));
    };
    item(Kind::all, 0, str::all_items, cfg_.icons.all_items);
    item(Kind::favorites, 0, str::favorites, cfg_.icons.favorite);
    item(Kind::recent, 0, str::recently_used, cfg_.icons.recent);
    const gui::IdLabels categories = session_.flat_categories();
    if (!categories.empty()) {
        section(str::categories);
        for (const auto& [id, name] : categories) {
            item(Kind::category, id, name, cfg_.icons.category);
        }
    }
    if (!session_.tags().empty()) {
        section(str::tags);
        for (const Tag& tag : session_.tags()) {
            item(Kind::tag, tag.id, tag.name, cfg_.icons.tag);
        }
    }
    sidebar_.set_rows(std::move(rows));
}
void App::sync_tree() {
    const gui::Projection& p = session_.projection();
    const Entry* selected = session_.selected_entry();
    if (!session_.has_vault()) {
        collapsed_.clear();
        expanded_.clear();
        revealed_.reset();
    }
    const std::optional<std::int64_t> current = selected ? std::optional(selected->id) : std::nullopt;
    if (current != revealed_) {
        revealed_ = current;
        for (const Entry* e = selected; e;) {
            const auto parent = gui::tree_parent(*e, session_.axis());
            e = parent ? p.find(*parent) : nullptr;
            if (e) {
                expanded_.insert(e->id);
                collapsed_.erase(e->id);
            }
        }
    }
    std::vector<ListRow> rows;
    std::function<void(std::int64_t, int)> add = [&](std::int64_t id, int depth) {
        const Entry* e = p.find(id);
        if (!e) {
            return;
        }
        const auto kids = p.children.find(id);
        const bool has_kids = kids != p.children.end() && !kids->second.empty();
        ListRow row;
        row.id = id;
        row.depth = depth;
        row.text = session_.entry_title(*e);
        const bool titled_by_cross = e->entry_type == "account" && e->label.empty();
        row.cells = {e->entry_type,
                     titled_by_cross ? std::string() : p.label_of(p.cross_parent(*e, session_.axis()))};
        row.expandable = has_kids;
        row.expanded = p.filtered ? !collapsed_.contains(id) : expanded_.contains(id);
        row.dim = !p.is_match(id);
        row.selected = selected && selected->id == id;
        if (e->entry_type == "account") {
            row.check = session_.marked().contains(id);
        }
        if (e->is_favorite) {
            row.icon = cfg_.icons.favorite;
        }
        if (e->entry_type != "account") {
            row.actions &= without(entry_login);
        }
        if (e->entry_type != "service") {
            row.actions &= without(entry_merge);
        }
        const bool open = row.expanded;
        rows.push_back(std::move(row));
        if (has_kids && open) {
            for (const auto child : kids->second) {
                add(child, depth + 1);
            }
        }
    };
    for (const auto id : p.roots) {
        add(id, 0);
    }
    tree_.set_column_title(2, session_.axis() == gui::Axis::Persona ? str::col_service
                                                                    : str::col_persona);
    tree_.set_empty_text(!session_.has_vault() ? str::no_vault
                         : p.filtered          ? str::no_matches
                                               : str::no_entries);
    tree_.set_rows(std::move(rows));
}

void App::sync_fields() {
    const Entry* entry = session_.selected_entry();
    const Field* selected = session_.selected_field();
    std::vector<ListRow> rows;
    for (const Field& f : session_.fields()) {
        ListRow row;
        row.id = f.id;
        row.text = f.label;
        row.cells = {f.field_type, f.notes};
        if (f.field_type != "totp") {
            row.actions = without(field_totp);
        }
        row.selected = selected && selected->id == f.id;
        rows.push_back(std::move(row));
    }
    const bool holds_fields = entry && gui::Session::traits(entry->entry_type).has_fields;
    fields_.set_empty_text(!entry         ? str::select_entry_first
                           : holds_fields ? str::no_fields
                                          : str::fields_on_accounts);
    add_field_.set_enabled(holds_fields);
    fields_.set_rows(std::move(rows));
    title_.set_text(entry ? std::string(entry->is_favorite ? str::favorite_marker : "") +
                                session_.entry_title(*entry)
                          : std::string(str::fields));
    detail_.set_text(entry ? session_.entry_detail(*entry) : std::string());
}

void App::sync() {
    seen_revision_ = session_.revision();
    dirty_ = false;
    const bool vault = session_.has_vault();

    close_.set_enabled(vault);
    path_.set_text(session_.vault_path());
    sidebar_.set_enabled(vault);
    tabs_.set_enabled(vault);
    tabs_.select(session_.axis() == gui::Axis::Persona ? 1 : 0);
    search_.set_enabled(vault);
    if (search_.text() != session_.filter().query) {
        search_.set_text(session_.filter().query);
    }
    add_entry_.set_enabled(vault);
    organize_.set_enabled(vault);
    set_persona_.set_enabled(vault);
    tick_all_.set_enabled(vault);
    tick_all_.set_checked(session_.all_marked());

    const bool totp = session_.totp_shown();
    totp_.set_visible(totp);
    totp_copy_.set_visible(totp);
    totp_close_.set_visible(totp);

    sync_sidebar();
    sync_tree();
    sync_fields();
    status_.set_text(session_.status());
}

Modal* App::top_modal() const {
    for (auto it = modals_.rbegin(); it != modals_.rend(); ++it) {
        if ((*it)->is_open()) {
            return *it;
        }
    }
    return nullptr;
}

void App::present(Modal& modal) {
    std::erase(modals_, &modal);
    for (Modal* m : modals_) {
        m->cancel();
    }
    base_.cancel();
    modals_.push_back(&modal);
    modal.set_area(window_rect_);
}

void App::message(const std::string& text) {
    auto dialog = std::make_unique<DialogModal>(
        kit_,
        lm::widgets::DialogOptions{str::app_name, text, str::ok, "", std::nullopt,
                                   lm::widgets::DialogAction::confirm},
        nullptr);
    dialog->show(window_rect_);
    present(*dialog);
    owned_.push_back(std::move(dialog));
}

void App::confirm(const std::string& text, std::function<void()> on_yes) {
    auto dialog = std::make_unique<DialogModal>(
        kit_,
        lm::widgets::DialogOptions{str::confirm, text, str::yes, str::no, std::nullopt,
                                   lm::widgets::DialogAction::dismiss},
        [on_yes = std::move(on_yes)](bool yes) {
            if (yes && on_yes) {
                on_yes();
            }
        });
    dialog->show(window_rect_);
    present(*dialog);
    owned_.push_back(std::move(dialog));
}

void App::sweep() {
    std::erase_if(modals_, [](const Modal* m) { return !m->visible(); });
    std::erase_if(owned_, [](const auto& m) { return !m->visible(); });
}

void App::handle_key(const KeyEvent& e) {
    if (e.ctrl && !e.shift && !e.repeat && e.key == Key::f) {
        base_.focus(search_);
        return;
    }
    base_.handle(e);
}

Argb App::update(const lm::sdl3::Frame& frame) {
    const double bar = bar_height();
    frame_size_ = {frame.width_dp, frame.height_dp};
    window_rect_ = {0.0, bar, frame.width_dp, std::max(0.0, frame.height_dp - bar)};
    if (bar > 0.0) {
        title_bar_.set_maximized(frame.maximized);
        title_bar_.set_active(frame.active);
        title_bar_.set_width(frame.width_dp);
        title_bar_.set_origin({0.0, 0.0});
        const double icon = cfg_.window.title_icon_size;
        app_icon_rect_ = {lm::tokens::title_bar_tokens.leading_space, (bar - icon) / 2.0, icon, icon};
        const double title_x = app_icon_rect_.x + icon + cfg_.window.title_icon_gap;
        const Size title_size =
            window_title_.measure({0.0, std::max(0.0, frame.width_dp - title_x), 0.0, bar});
        window_title_.set_origin({title_x, (bar - title_size.height) / 2.0});
        window_.set_caption_regions(title_bar_.regions());
    }
    if (dirty_ || session_.revision() != seen_revision_) {
        sync();
    }
    layout();

    if (!frame.pointer.empty() || !frame.wheel.empty() || !frame.keys.empty() || !frame.text.empty()) {
        session_.note_activity();
    }
    for (const auto& e : frame.pointer) {
        if (handle_title_bar(e)) {
            continue;
        }
        if (Modal* m = top_modal()) {
            m->handle(e);
        } else {
            base_.handle(e);
        }
    }
    for (const auto& e : frame.wheel) {
        if (Modal* m = top_modal()) {
            m->handle(e);
        } else {
            base_.handle(e);
        }
    }
    for (const auto& e : frame.keys) {
        if (Modal* m = top_modal()) {
            m->handle(e);
        } else {
            handle_key(e);
        }
    }
    for (const auto& e : frame.text) {
        if (Modal* m = top_modal()) {
            m->handle(e);
        } else {
            base_.handle(e);
        }
    }

    if (session_.poll_kdf_benchmark()) {
        change_password_sheet_.measured();
    }
    if (const auto result = session_.poll_rotate()) {
        busy_sheet_.close();
        message(*result ? session_.status() : result->error().message);
    }
    if (const auto result = session_.poll_unlock(clipboard_)) {
        busy_sheet_.close();
        if (!*result) {
            message(result->error().message);
        } else if (preferences_.vault != session_.vault_path()) {
            preferences_.vault = session_.vault_path();
            save_preferences();
        }
    }
    session_.tick(clipboard_);
    if (session_.lock_if_due(clipboard_)) {
        locked();
    }
    watch_away();
    if (dirty_ || session_.revision() != seen_revision_) {
        sync();
    }
    if (session_.totp_shown()) {
        const gui::TotpReading reading = session_.totp();
        totp_.set_reading(reading.code, reading.remaining_seconds, reading.period_seconds);
    }
    layout();

    base_.step(frame.dt_seconds);
    for (Modal* m : modals_) {
        m->step(frame.dt_seconds);
    }
    title_bar_.step(frame.dt_seconds);
    theme_mix_.step(frame.dt_seconds);
    scheme_ = theme_mix_.settled() ? scheme_to_
                                   : lm::color::mix(scheme_from_, scheme_to_, theme_mix_.value());
    sweep();

    const Modal* top = top_modal();
    window_.set_text_input(top ? top->caret_area() : base_.caret_area());

    std::optional<double> wake;
    auto wake_at = [&](const std::optional<double>& seconds) {
        if (seconds) {
            wake = wake ? std::min(*wake, *seconds) : *seconds;
        }
    };
    wake_at(base_.next_wake_seconds());
    for (const Modal* m : modals_) {
        wake_at(m->next_wake_seconds());
    }
    wake_at(session_.next_wake_seconds());
    if (session_.totp_shown()) {
        wake_at(seconds_to_next_tick());
    }
    if (wake) {
        window_.wake_after(*wake);
    }
    return scheme_[kit_.theme->background];
}

bool App::paint(Canvas& canvas) {
    base_.paint(canvas, scheme_);
    base_.paint_overlays(canvas, scheme_);
    bool animating =
        base_.animating() || session_.unlocking() || session_.rotating() || !theme_mix_.settled();
    for (const Modal* m : modals_) {
        m->paint(canvas, scheme_);
        animating = animating || m->animating();
    }
    if (bar_height() > 0.0) {
        title_bar_.paint(canvas, scheme_);
        canvas.draw_image(app_icon_, app_icon_rect_);
        window_title_.paint(canvas, scheme_);
        animating = animating || title_bar_.animating();
    }
    return animating;
}

void App::run() {
    lm::sdl3::run_app(
        window_, [this](const lm::sdl3::Frame& frame) { return update(frame); },
        [this](Canvas& canvas, const lm::sdl3::Frame&) { return paint(canvas); });
    session_.close_vault(clipboard_);
}

} // namespace cypherarx::lamina_gui
