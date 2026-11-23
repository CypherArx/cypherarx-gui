#include "lamina_gui/list_view.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

#include "lamina/render/path.hpp"
#include "lamina/tokens/interaction.hpp"

namespace cypherarx::lamina_gui {

ListView::ListView(const Kit& kit, ListOptions options)
    : kit_(kit), options_(std::move(options)), empty_(line_of(kit, "", kit.cfg->list.text)) {
    for (const std::string& title : options_.columns.titles) {
        titles_.push_back(line_of(kit_, title, kit_.cfg->list.column_title));
    }
    for (const RowAction& action : options_.row_actions) {
        tips_.push_back(line_of(kit_, action.tooltip, kit_.cfg->tooltip.type));
    }
}

void ListView::set_rows(std::vector<ListRow> rows) {
    std::optional<std::pair<int, std::int64_t>> key;
    if (cursor_ && *cursor_ < rows_.size()) {
        key = {rows_[*cursor_].group, rows_[*cursor_].id};
    }
    rows_ = std::move(rows);
    laid_.clear();
    laid_.reserve(rows_.size());
    const ListConfig& t = kit_.cfg->list;
    for (const ListRow& row : rows_) {
        Laid laid{line_of(kit_, row.text, row.section ? t.section : t.text), {}};
        for (const std::string& cell : row.cells) {
            laid.cells.push_back(line_of(kit_, cell, t.text));
        }
        laid_.push_back(std::move(laid));
    }
    cursor_.reset();
    if (key) {
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (!rows_[i].section && rows_[i].group == key->first && rows_[i].id == key->second) {
                cursor_ = i;
                break;
            }
        }
    }
    hovered_.reset();
    pressed_.reset();
    hovered_action_.reset();
    tip_.reset();
    scroll_.set_content_height(content_height());
}

void ListView::set_column_title(std::size_t column, std::string title) {
    if (column < titles_.size() && options_.columns.titles[column] != title) {
        titles_[column] = line_of(kit_, title, kit_.cfg->list.column_title);
        options_.columns.titles[column] = std::move(title);
    }
}

void ListView::set_empty_text(std::string text) {
    if (text == empty_text_) {
        return;
    }
    empty_text_ = std::move(text);
    empty_ = line_of(kit_, empty_text_, kit_.cfg->list.text);
}

void ListView::reveal_selected() {
    const double h = kit_.cfg->list.row_height;
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i].selected && !rows_[i].section) {
            scroll_.ensure_visible(static_cast<double>(i) * h, static_cast<double>(i + 1) * h);
            return;
        }
    }
}

double ListView::content_height() const {
    return static_cast<double>(rows_.size()) * kit_.cfg->list.row_height;
}

double ListView::header_height() const {
    return titles_.empty() ? 0.0 : kit_.cfg->list.header_height;
}

std::vector<std::size_t> ListView::actions_of(const ListRow& row) const {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < options_.row_actions.size(); ++i) {
        if (row.actions & (1u << i)) {
            out.push_back(i);
        }
    }
    return out;
}

bool ListView::actions_visible(std::size_t index) const {
    if (options_.row_actions.empty()) {
        return false;
    }
    return hovered_ == index || (focused_ && focus_visible_ && cursor_ == index);
}

double ListView::actions_left(std::size_t count) const {
    const ListConfig& t = kit_.cfg->list;
    return rect_.width - t.row_inset_x - static_cast<double>(count) * t.row_height;
}

std::optional<std::size_t> ListView::action_at(const ListRow& row, double content_x) const {
    const std::vector<std::size_t> actions = actions_of(row);
    const double left = actions_left(actions.size());
    const double cell = kit_.cfg->list.row_height;
    if (actions.empty() || content_x < left) {
        return std::nullopt;
    }
    const auto slot = static_cast<std::size_t>((content_x - left) / cell);
    return slot < actions.size() ? std::optional<std::size_t>(actions[slot]) : std::nullopt;
}
double ListView::column_x(std::size_t column) const {
    const ListConfig& t = kit_.cfg->list;
    const std::vector<double>& w = options_.columns.weights;
    if (w.empty()) {
        return t.padding;
    }
    const double total = std::accumulate(w.begin(), w.end(), 0.0);
    const double inner = std::max(0.0, rect_.width - 2.0 * t.padding);
    const double before = std::accumulate(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(column), 0.0);
    return t.padding + inner * before / total;
}

double ListView::column_width(std::size_t column) const {
    const ListConfig& t = kit_.cfg->list;
    const std::vector<double>& w = options_.columns.weights;
    const double inner = std::max(0.0, rect_.width - 2.0 * t.padding);
    if (w.empty()) {
        return inner;
    }
    const double total = std::accumulate(w.begin(), w.end(), 0.0);
    const double gap = column + 1 < w.size() ? t.cell_gap : 0.0;
    return std::max(0.0, inner * w[column] / total - gap);
}

double ListView::text_left(const ListRow& row) const {
    const ListConfig& t = kit_.cfg->list;
    double x = t.padding + static_cast<double>(row.depth) * t.indent;
    if (options_.tree) {
        x += t.chevron_size + t.icon_gap + t.icon_size + t.icon_gap;
    } else if (row.icon) {
        x += t.icon_size + t.icon_gap;
    }
    return x;
}

std::optional<std::size_t> ListView::row_at(double content_y) const {
    if (content_y < 0.0) {
        return std::nullopt;
    }
    const auto i = static_cast<std::size_t>(content_y / kit_.cfg->list.row_height);
    if (i >= rows_.size() || rows_[i].section) {
        return std::nullopt;
    }
    return i;
}

std::optional<std::size_t> ListView::step_cursor(int direction) const {
    const auto n = static_cast<std::ptrdiff_t>(rows_.size());
    std::ptrdiff_t i = cursor_ ? static_cast<std::ptrdiff_t>(*cursor_) : (direction > 0 ? -1 : n);
    for (i += direction; i >= 0 && i < n; i += direction) {
        if (!rows_[static_cast<std::size_t>(i)].section) {
            return static_cast<std::size_t>(i);
        }
    }
    return std::nullopt;
}

void ListView::move_cursor(std::size_t index) {
    const double h = kit_.cfg->list.row_height;
    cursor_ = index;
    focus_visible_ = true;
    scroll_.ensure_visible(static_cast<double>(index) * h, static_cast<double>(index + 1) * h);
    if (options_.select_follows_cursor && on_select) {
        const ListRow row = rows_[index];
        on_select(row);
    }
}

void ListView::click(std::size_t index, double content_x, int clicks) {
    const ListConfig& t = kit_.cfg->list;
    const ListRow row = rows_[index];
    if (const auto action = action_at(row, content_x)) {
        if (on_action) {
            on_action(row, *action);
        }
        return;
    }
    const double chevron = t.padding + static_cast<double>(row.depth) * t.indent;
    const bool on_chevron = options_.tree && content_x >= chevron - t.icon_gap &&
                            content_x < chevron + t.chevron_size + t.icon_gap;
    if (row.expandable && (on_chevron || clicks >= 2)) {
        if (on_toggle) {
            on_toggle(row);
        }
        return;
    }
    if (options_.multi && on_mark && (press_ctrl_ || press_shift_) && clicks < 2) {
        std::optional<std::size_t> from;
        if (press_shift_ && anchor_) {
            for (std::size_t i = 0; i < rows_.size(); ++i) {
                if (rows_[i].group == anchor_->first && rows_[i].id == anchor_->second) {
                    from = i;
                    break;
                }
            }
        }
        std::vector<ListRow> picked;
        if (from) {
            for (std::size_t i = std::min(*from, index); i <= std::max(*from, index); ++i) {
                if (!rows_[i].section) {
                    picked.push_back(rows_[i]);
                }
            }
        } else {
            picked.push_back(row);
            anchor_ = {{row.group, row.id}};
        }
        on_mark(picked, press_ctrl_ && !press_shift_);
        return;
    }
    anchor_ = {{row.group, row.id}};
    if (on_select) {
        on_select(row);
    }
    if (clicks >= 2 && on_activate) {
        on_activate(row);
    }
}

Size ListView::measure(const Constraints& c) {
    const double width = finite_or(c.max_width, kit_.cfg->field.default_width);
    const double height = options_.height ? *options_.height
                                          : finite_or(c.max_height, content_height() + header_height());
    return {width, height};
}

void ListView::arrange(const Rect& r) {
    rect_ = r;
    const double head = header_height();
    scroll_.set_viewport({r.x, r.y + head, r.width, std::max(0.0, r.height - head)});
}

void ListView::handle(const PointerEvent& e) {
    const Rect viewport = scroll_.viewport();
    if (e.action == PointerAction::down && !inside(rect_, e.x_dp, e.y_dp)) {
        focused_ = false;
        focus_visible_ = false;
    }
    if (scroll_.handle(e)) {
        hovered_.reset();
        pressed_.reset();
        hovered_action_.reset();
        tip_.reset();
        return;
    }
    const PointerEvent c = scroll_.to_content(e);
    const std::optional<std::size_t> over =
        c.action == PointerAction::leave ? std::nullopt : row_at(c.y_dp);
    const std::optional<std::size_t> action =
        over && !options_.row_actions.empty() ? action_at(rows_[*over], c.x_dp) : std::nullopt;
    if (over != hovered_ || action != hovered_action_) {
        tip_.reset();
    }
    hovered_ = over;
    hovered_action_ = action;
    tip_.track(action.has_value());
    if (e.action == PointerAction::down) {
        tip_.dismiss();
    }
    switch (e.action) {
    case PointerAction::down:
        if (inside(viewport, e.x_dp, e.y_dp)) {
            focused_ = true;
            focus_visible_ = false;
            pressed_ = over;
            press_clicks_ = e.clicks;
            press_ctrl_ = e.ctrl;
            press_shift_ = e.shift;
            if (over) {
                cursor_ = over;
            }
        }
        break;
    case PointerAction::up: {
        const std::optional<std::size_t> pressed = pressed_;
        pressed_.reset();
        if (pressed && over == pressed) {
            click(*over, c.x_dp, press_clicks_);
        }
        break;
    }
    case PointerAction::leave:
        pressed_.reset();
        break;
    case PointerAction::move:
        break;
    }
}

bool ListView::handle(const WheelEvent& e) {
    return scroll_.handle(e);
}

bool ListView::handle(const KeyEvent& e) {
    if (!focused_) {
        return false;
    }
    if (scroll_.handle(e)) {
        return true;
    }
    if (e.ctrl || e.alt) {
        return false;
    }
    switch (e.key) {
    case Key::up:
    case Key::down:
        if (const auto next = step_cursor(e.key == Key::up ? -1 : 1)) {
            move_cursor(*next);
        }
        return true;
    case Key::home:
    case Key::end: {
        const std::optional<std::size_t> saved = cursor_;
        cursor_.reset();
        const auto edge = step_cursor(e.key == Key::home ? 1 : -1);
        cursor_ = saved;
        if (edge) {
            move_cursor(*edge);
        }
        return true;
    }
    case Key::left:
    case Key::right: {
        if (!cursor_) {
            return false;
        }
        const ListRow row = rows_[*cursor_];
        if (!row.expandable) {
            return false;
        }
        if (row.expanded == (e.key == Key::left) && on_toggle) {
            on_toggle(row);
        }
        return true;
    }
    case Key::delete_forward: {
        if (!cursor_ || !on_remove || e.repeat) {
            return false;
        }
        const ListRow row = rows_[*cursor_];
        on_remove(row);
        return true;
    }
    case Key::enter:
    case Key::space: {
        if (!cursor_ || e.repeat) {
            return false;
        }
        const ListRow row = rows_[*cursor_];
        if (options_.select_follows_cursor) {
            if (!on_activate) {
                return false;
            }
            on_activate(row);
            return true;
        }
        if (on_select) {
            on_select(row);
        }
        return true;
    }
    default:
        return false;
    }
}

void ListView::cancel() {
    scroll_.cancel_interaction();
    hovered_.reset();
    pressed_.reset();
    hovered_action_.reset();
    tip_.reset();
}

void ListView::step(double dt) {
    scroll_.step(dt);
    tip_.step(dt);
    if (const auto e = scroll_.rehover()) {
        hovered_ = e->action == PointerAction::leave ? std::nullopt : row_at(e->y_dp);
    }
}

void ListView::set_focused(bool focused) {
    focused_ = focused;
    focus_visible_ = focused;
    if (!focused || cursor_) {
        return;
    }
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i].selected && !rows_[i].section) {
            cursor_ = i;
            return;
        }
    }
    cursor_ = step_cursor(1);
}

std::optional<double> ListView::next_wake_seconds() const {
    const double s = scroll_.next_wake_seconds();
    std::optional<double> wake = s < 0.0 ? std::nullopt : std::optional<double>(s);
    if (const auto tip = tip_.wake(kit_.cfg->tooltip.delay_seconds)) {
        wake = wake ? std::min(*wake, *tip) : *tip;
    }
    return wake;
}

void ListView::paint_overlay(Canvas& canvas, const ColorScheme& scheme) const {
    if (!hovered_ || !hovered_action_ || !tip_.shown(kit_.cfg->tooltip.delay_seconds)) {
        return;
    }
    const std::vector<std::size_t> actions = actions_of(rows_[*hovered_]);
    const auto at = std::ranges::find(actions, *hovered_action_);
    if (at == actions.end()) {
        return;
    }
    const double cell = kit_.cfg->list.row_height;
    const auto slot = static_cast<double>(at - actions.begin());
    const Rect anchor = scroll_.to_window({actions_left(actions.size()) + slot * cell,
                                           static_cast<double>(*hovered_) * cell, cell, cell});
    paint_tooltip(canvas, kit_, scheme, tips_[*hovered_action_], anchor, window_);
}
void ListView::paint_row(Canvas& canvas, const ColorScheme& scheme, std::size_t index) const {
    const ListConfig& t = kit_.cfg->list;
    const ListRow& row = rows_[index];
    const Laid& laid = laid_[index];
    const Rect box{0.0, static_cast<double>(index) * t.row_height, rect_.width, t.row_height};
    if (row.section) {
        draw_line_in(canvas, kit_, laid.text,
                     {t.padding, box.y, std::max(0.0, box.width - 2.0 * t.padding), box.height},
                     scheme[Role::on_surface_variant]);
        return;
    }
    const Rect shape{box.x + t.row_inset_x, box.y + t.row_inset_y, box.width - 2.0 * t.row_inset_x,
                     box.height - 2.0 * t.row_inset_y};
    const lm::render::CornerRadii radii = lm::render::uniform_radii(t.row_radius);
    if (row.selected) {
        canvas.fill_rounded_rect(shape, radii, scheme[Role::secondary_container]);
    }
    const double layer = pressed_ == index   ? lm::tokens::state_layer_opacity::pressed
                         : hovered_ == index ? lm::tokens::state_layer_opacity::hover
                                             : 0.0;
    if (layer > 0.0) {
        canvas.fill_rounded_rect(shape, radii, lm::color::with_alpha(scheme[Role::on_surface], layer));
    }
    if (focused_ && focus_visible_ && cursor_ == index) {
        canvas.fill_path(lm::render::ring_path(shape, t.row_radius, t.cursor_ring),
                         scheme[Role::secondary], lm::render::FillRule::even_odd);
    }

    const double dim = row.dim ? kit_.cfg->theme.dim_opacity : 1.0;
    const Argb main = lm::color::scale_alpha(
        scheme[row.selected ? Role::on_secondary_container : Role::on_surface], dim);
    const Argb side = lm::color::scale_alpha(
        scheme[row.selected ? Role::on_secondary_container : Role::on_surface_variant], dim);

    const double left = text_left(row);
    if (options_.tree && row.expandable) {
        const double x = t.padding + static_cast<double>(row.depth) * t.indent;
        kit_.icons.draw(canvas, row.expanded ? kit_.cfg->icons.expanded : kit_.cfg->icons.collapsed,
                        {x, box.y + (box.height - t.chevron_size) / 2.0}, t.chevron_size, side);
    }
    if (row.icon) {
        kit_.icons.draw(canvas, *row.icon,
                        {left - t.icon_gap - t.icon_size, box.y + (box.height - t.icon_size) / 2.0},
                        t.icon_size, side);
    }
    const std::vector<std::size_t> actions =
        actions_visible(index) ? actions_of(row) : std::vector<std::size_t>{};
    const double actions_x = actions_left(actions.size());
    const double limit = actions.empty() ? box.width : actions_x;
    auto fitted = [&](double x, double width) { return std::max(0.0, std::min(x + width, limit) - x); };
    const double first_right = column_x(0) + column_width(0);
    draw_line_in(canvas, kit_, laid.text, {left, box.y, fitted(left, first_right - left), box.height},
                 main);
    for (std::size_t j = 0; j < laid.cells.size(); ++j) {
        const double x = column_x(j + 1);
        const double width = fitted(x, column_width(j + 1));
        if (width > 0.0) {
            draw_line_in(canvas, kit_, laid.cells[j], {x, box.y, width, box.height}, side);
        }
    }
    for (std::size_t slot = 0; slot < actions.size(); ++slot) {
        const Point middle{actions_x + (static_cast<double>(slot) + 0.5) * t.row_height,
                           box.y + box.height / 2.0};
        if (hovered_ == index && hovered_action_ == actions[slot]) {
            canvas.fill_path(lm::render::circle_path(middle, t.row_height / 2.0 - t.action_inset),
                             lm::color::with_alpha(scheme[Role::on_surface],
                                                   lm::tokens::state_layer_opacity::hover));
        }
        kit_.icons.draw(canvas, options_.row_actions[actions[slot]].symbol,
                        {middle.x - t.action_icon / 2.0, middle.y - t.action_icon / 2.0},
                        t.action_icon, side);
    }
}
void ListView::paint(Canvas& canvas, const ColorScheme& scheme) const {
    const ListConfig& t = kit_.cfg->list;
    const double head = header_height();
    if (head > 0.0) {
        for (std::size_t i = 0; i < titles_.size(); ++i) {
            draw_line_in(canvas, kit_, titles_[i],
                         {rect_.x + column_x(i), rect_.y, column_width(i), head},
                         scheme[Role::on_surface_variant]);
        }
        canvas.fill_rect({rect_.x + t.padding, rect_.y + head - t.divider,
                          std::max(0.0, rect_.width - 2.0 * t.padding), t.divider},
                         scheme[Role::outline_variant]);
    }

    const Rect viewport = scroll_.viewport();
    if (rows_.empty()) {
        if (!empty_text_.empty()) {
            lm::text::draw_line(canvas, kit_.fonts, empty_,
                                {viewport.x + std::max(t.padding, (viewport.width - empty_.width) / 2.0),
                                 viewport.y + t.padding},
                                scheme[Role::on_surface_variant]);
        }
        return;
    }

    const double offset = scroll_.offset();
    const auto first = static_cast<std::size_t>(std::max(0.0, std::floor(offset / t.row_height)));
    const auto last = std::min(
        rows_.size(), static_cast<std::size_t>(std::ceil((offset + viewport.height) / t.row_height)));
    scroll_.begin_content(canvas);
    for (std::size_t i = first; i < last; ++i) {
        paint_row(canvas, scheme, i);
    }
    scroll_.end_content(canvas);
    scroll_.paint(canvas, scheme);
}

} // namespace cypherarx::lamina_gui
