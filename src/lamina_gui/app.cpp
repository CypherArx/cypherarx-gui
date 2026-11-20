#include "lamina_gui/app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

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

enum SidebarGroup { group_view = 1, group_favorites, group_category, group_tag };

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
      kit_{lm::sdl3::system_font_stack(), lm::sdl3::bundled_icon_font(), &window_.clipboard(), &cfg_},
      title_bar_({cfg_.window.title, kit_.icons, nullptr, cfg_.theme.background}, kit_.fonts),
      scheme_(lm::color::spec_2025().build(cfg_.theme.scheme)),
      clipboard_(window_.clipboard()),
      session_(session_settings(cfg_)),
      base_(kit_),
      sidebar_pane_(base_.add<PaneC>(cfg_.theme.pane, cfg_.layout.pane_radius)),
      center_pane_(base_.add<PaneC>(cfg_.theme.pane, cfg_.layout.pane_radius)),
      detail_pane_(base_.add<PaneC>(cfg_.theme.pane, cfg_.layout.pane_radius)),
      new_(base_.add<ButtonC>(kit_, ButtonSpec{str::new_vault, ButtonStyle::tonal})),
      open_(base_.add<ButtonC>(kit_, ButtonSpec{str::open_vault, ButtonStyle::tonal})),
      close_(base_.add<ButtonC>(kit_, ButtonSpec{str::close_vault, ButtonStyle::outlined})),
      path_(base_.add<LabelC>(kit_, LabelSpec{"", TypeRole::body_medium, Role::on_surface_variant})),
      sidebar_(base_.add<ListView>(kit_)),
      clear_(base_.add<ButtonC>(kit_, ButtonSpec{str::clear_filters, ButtonStyle::text})),
      search_(base_.add<TextFieldC>(kit_, str::search_hint)),
      add_entry_(base_.add<ButtonC>(kit_, ButtonSpec{str::add_entry, ButtonStyle::tonal})),
      edit_entry_(base_.add<ButtonC>(kit_, ButtonSpec{str::edit_entry, ButtonStyle::outlined})),
      delete_entry_(base_.add<ButtonC>(kit_, ButtonSpec{str::delete_entry, ButtonStyle::outlined})),
      login_(base_.add<ButtonC>(kit_, ButtonSpec{str::login_methods, ButtonStyle::outlined})),
      organize_(base_.add<ButtonC>(kit_, ButtonSpec{str::organize, ButtonStyle::outlined})),
      tree_(base_.add<ListView>(
          kit_, ListOptions{.columns = {{str::col_name, str::col_type, str::col_persona}, {2.0, 1.0, 1.0}},
                            .tree = true,
                            .select_follows_cursor = true})),
      title_(base_.add<LabelC>(kit_, LabelSpec{str::fields, TypeRole::title_large})),
      detail_(base_.add<LabelC>(kit_, LabelSpec{"", TypeRole::body_medium, Role::on_surface_variant})),
      add_field_(base_.add<ButtonC>(kit_, ButtonSpec{str::add_field, ButtonStyle::tonal})),
      edit_field_(base_.add<ButtonC>(kit_, ButtonSpec{str::edit_field, ButtonStyle::outlined})),
      copy_(base_.add<ButtonC>(kit_, ButtonSpec{str::copy_value, ButtonStyle::outlined})),
      show_totp_(base_.add<ButtonC>(kit_, ButtonSpec{str::show_totp, ButtonStyle::outlined})),
      delete_field_(base_.add<ButtonC>(kit_, ButtonSpec{str::delete_field, ButtonStyle::outlined})),
      fields_(base_.add<ListView>(
          kit_, ListOptions{.columns = {{str::label, str::type, str::notes}, {3.0, 2.0, 3.0}},
                            .select_follows_cursor = true})),
      totp_(base_.add<TotpC>(kit_)),
      totp_copy_(base_.add<ButtonC>(kit_, ButtonSpec{str::copy_value, ButtonStyle::tonal})),
      totp_close_(base_.add<ButtonC>(kit_, ButtonSpec{str::close, ButtonStyle::text})),
      status_(base_.add<LabelC>(kit_, LabelSpec{"", TypeRole::body_medium, Role::on_surface_variant})),
      password_sheet_(kit_, *this),
      busy_sheet_(kit_),
      entry_sheet_(kit_, *this),
      field_sheet_(kit_, *this),
      organize_sheet_(kit_, *this),
      login_sheet_(kit_, *this) {
    window_.set_minimum_size(cfg_.window.min_width_dp,
                             cfg_.window.min_height_dp + title_bar_extra(cfg_.window));
    wire();
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

void App::choose_vault(gui::UnlockMode mode) {
    const gui::platform::FileDialogOptions options{cfg_.file_filter, cfg_.vault_extension};
    auto file = mode == gui::UnlockMode::Create ? gui::platform::choose_file_to_save(options)
                                                : gui::platform::choose_file_to_open(options);
    base_.cancel();
    if (file) {
        password_sheet_.open(mode, std::move(*file));
    }
}

void App::wire() {
    new_.on_click = [this] { choose_vault(gui::UnlockMode::Create); };
    open_.on_click = [this] { choose_vault(gui::UnlockMode::Open); };
    close_.on_click = [this] { session_.close_vault(clipboard_); };
    password_sheet_.on_started = [this] {
        busy_sheet_.open(session_.unlock_mode() == gui::UnlockMode::Create ? str::new_vault
                                                                           : str::open_vault,
                         session_.unlock_path());
        present(busy_sheet_);
    };

    sidebar_.on_select = [this](const ListRow& row) {
        const gui::Filter& filter = session_.filter();
        switch (row.group) {
        case group_view:
            session_.set_axis(row.id == 0 ? gui::Axis::Service : gui::Axis::Persona);
            break;
        case group_favorites:
            session_.set_favorites_only(!filter.favorites_only);
            break;
        case group_category:
            session_.set_category_filter(row.id == 0 || filter.category == row.id
                                             ? std::nullopt
                                             : std::optional<std::int64_t>(row.id));
            break;
        case group_tag:
            session_.set_tag_filter(row.id == 0 || filter.tag == row.id
                                        ? std::nullopt
                                        : std::optional<std::int64_t>(row.id));
            break;
        default:
            break;
        }
    };
    clear_.on_click = [this] { session_.clear_filters(); };
    search_.on_change = [this](const std::string& query) { session_.set_query(query); };

    tree_.on_select = [this](const ListRow& row) { session_.select_entry(row.id); };
    tree_.on_toggle = [this](const ListRow& row) {
        if (!collapsed_.erase(row.id)) {
            collapsed_.insert(row.id);
        }
        dirty_ = true;
    };
    tree_.on_activate = [this](const ListRow&) { entry_sheet_.open_selected(); };
    add_entry_.on_click = [this] { entry_sheet_.open_new(); };
    edit_entry_.on_click = [this] { entry_sheet_.open_selected(); };
    delete_entry_.on_click = [this] {
        confirm(str::confirm_delete_entry, [this] { check(session_.delete_selected_entry()); });
    };
    login_.on_click = [this] { login_sheet_.open(); };
    organize_.on_click = [this] { organize_sheet_.open(); };

    fields_.on_select = [this](const ListRow& row) { session_.select_field(row.id); };
    fields_.on_activate = [this](const ListRow&) { field_sheet_.open_selected(); };
    add_field_.on_click = [this] { field_sheet_.open_new(); };
    edit_field_.on_click = [this] { field_sheet_.open_selected(); };
    copy_.on_click = [this] { check(session_.copy_selected_field(clipboard_)); };
    show_totp_.on_click = [this] { check(session_.show_totp()); };
    delete_field_.on_click = [this] {
        confirm(str::confirm_delete_field, [this] { check(session_.delete_selected_field()); });
    };
    totp_copy_.on_click = [this] { check(session_.copy_totp(clipboard_)); };
    totp_close_.on_click = [this] { session_.close_totp(); };
}

NodePtr App::build_layout() {
    const LayoutConfig& t = cfg_.layout;
    const auto stretch = lm::layout::CrossAlign::stretch;
    const auto center = lm::layout::CrossAlign::center;
    const auto padding = lm::layout::Insets::all(t.pane_padding);

    auto toolbar = lm::layout::row({new_.node(), open_.node(), close_.node(), grow(path_.node())});
    toolbar->spacing(t.control_gap).cross_align(center);

    auto sidebar = lm::layout::column({grow(sidebar_.node()), clear_.node()});
    sidebar->padding(padding).spacing(t.control_gap).cross_align(stretch).fill_cross(true).fill_main(true);

    auto entry_actions = lm::layout::flow({add_entry_.node(), edit_entry_.node(), delete_entry_.node(),
                                           login_.node(), organize_.node()});
    entry_actions->spacing(t.control_gap).line_spacing(t.control_gap);
    auto entries = lm::layout::column({search_.node(), entry_actions, grow(tree_.node())});
    entries->padding(padding).spacing(t.gap).cross_align(stretch).fill_cross(true).fill_main(true);

    auto field_actions = lm::layout::flow({add_field_.node(), edit_field_.node(), copy_.node(),
                                           show_totp_.node(), delete_field_.node()});
    field_actions->spacing(t.control_gap).line_spacing(t.control_gap);
    std::vector<NodePtr> detail_rows{title_.node(), detail_.node(), field_actions, grow(fields_.node())};
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
    const gui::Filter& filter = session_.filter();
    std::vector<ListRow> rows;
    auto section = [&](const char* title) {
        ListRow row;
        row.text = title;
        row.section = true;
        rows.push_back(std::move(row));
    };
    auto item = [&](int group, std::int64_t id, std::string text, bool selected) -> ListRow& {
        ListRow row;
        row.group = group;
        row.id = id;
        row.text = std::move(text);
        row.selected = selected;
        rows.push_back(std::move(row));
        return rows.back();
    };
    section(str::view);
    item(group_view, 0, str::axis_service, session_.axis() == gui::Axis::Service).icon =
        cfg_.icons.axis_service;
    item(group_view, 1, str::axis_persona, session_.axis() == gui::Axis::Persona).icon =
        cfg_.icons.axis_persona;
    section(str::filters);
    item(group_favorites, 0, str::favorites_only, filter.favorites_only).icon = cfg_.icons.favorite;
    section(str::categories);
    item(group_category, 0, str::all, !filter.category);
    for (auto& [id, name] : session_.flat_categories()) {
        item(group_category, id, std::move(name), filter.category == id);
    }
    section(str::tags);
    item(group_tag, 0, str::all, !filter.tag);
    for (const Tag& tag : session_.tags()) {
        item(group_tag, tag.id, tag.name, filter.tag == tag.id);
    }
    sidebar_.set_rows(std::move(rows));
}

void App::sync_tree() {
    const gui::Projection& p = session_.projection();
    const Entry* selected = session_.selected_entry();
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
        row.text = e->label;
        row.cells = {e->entry_type, p.label_of(p.cross_parent(*e, session_.axis()))};
        row.expandable = has_kids;
        row.expanded = !collapsed_.contains(id);
        row.dim = !p.is_match(id);
        row.selected = selected && selected->id == id;
        if (e->is_favorite) {
            row.icon = cfg_.icons.favorite;
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
        row.selected = selected && selected->id == f.id;
        rows.push_back(std::move(row));
    }
    fields_.set_empty_text(entry ? str::no_fields : str::select_entry_first);
    fields_.set_rows(std::move(rows));
    title_.set_text(entry ? std::string(entry->is_favorite ? str::favorite_marker : "") + entry->label
                          : std::string(str::fields));
    detail_.set_text(entry ? session_.entry_detail(*entry) : std::string());
}

void App::sync() {
    seen_revision_ = session_.revision();
    dirty_ = false;
    const bool vault = session_.has_vault();
    const bool entry = session_.selected_entry() != nullptr;
    const bool field = session_.selected_field() != nullptr;

    close_.set_enabled(vault);
    path_.set_text(session_.vault_path());
    sidebar_.set_enabled(vault);
    clear_.set_enabled(vault && session_.filter().active());
    search_.set_enabled(vault);
    if (search_.text() != session_.filter().query) {
        search_.set_text(session_.filter().query);
    }
    add_entry_.set_enabled(vault);
    edit_entry_.set_enabled(entry);
    delete_entry_.set_enabled(entry);
    login_.set_enabled(entry);
    organize_.set_enabled(vault);
    add_field_.set_enabled(entry);
    edit_field_.set_enabled(field);
    copy_.set_enabled(field);
    show_totp_.set_enabled(field);
    delete_field_.set_enabled(field);

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
    dialogs_.push_back(std::move(dialog));
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
    dialogs_.push_back(std::move(dialog));
}

void App::sweep() {
    std::erase_if(modals_, [](const Modal* m) { return !m->visible(); });
    std::erase_if(dialogs_, [](const auto& d) { return !d->visible(); });
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
    window_rect_ = {0.0, bar, frame.width_dp, std::max(0.0, frame.height_dp - bar)};
    if (bar > 0.0) {
        title_bar_.set_maximized(frame.maximized);
        title_bar_.set_active(frame.active);
        title_bar_.set_width(frame.width_dp);
        title_bar_.set_origin({0.0, 0.0});
        window_.set_caption_regions(title_bar_.regions());
    }
    if (dirty_ || session_.revision() != seen_revision_) {
        sync();
    }
    layout();

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

    if (const auto result = session_.poll_unlock(clipboard_)) {
        busy_sheet_.close();
        if (!*result) {
            message(result->error().message);
        }
    }
    session_.tick(clipboard_);
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
    return scheme_[cfg_.theme.background];
}

bool App::paint(Canvas& canvas) {
    base_.paint(canvas, scheme_);
    base_.paint_overlays(canvas, scheme_);
    bool animating = base_.animating() || session_.unlocking();
    for (const Modal* m : modals_) {
        m->paint(canvas, scheme_);
        animating = animating || m->animating();
    }
    if (bar_height() > 0.0) {
        title_bar_.paint(canvas, scheme_);
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
