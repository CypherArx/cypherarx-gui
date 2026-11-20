#include "lamina_gui/select_field.hpp"

#include <algorithm>

#include "lamina/tokens/elevation.hpp"
#include "lamina/tokens/motion.hpp"
#include "lamina/widgets/surface.hpp"

namespace cypherarx::lamina_gui {

SelectField::SelectField(const Kit& kit, std::string label, Role container)
    : kit_(kit),
      container_(container),
      label_(line_of(kit, label, kit.cfg->field.label)),
      value_(line_of(kit, "", kit.cfg->field.input)),
      list_(kit),
      popup_(kit.cfg->sheet.closed_scale, lm::tokens::motion_scheme.fast_spatial(),
             lm::tokens::motion_scheme.fast_effects()) {
    list_.on_select = [this](const ListRow& row) {
        const auto chosen = static_cast<std::size_t>(row.id);
        const bool changed = chosen != index_;
        index_ = chosen;
        relabel();
        close();
        if (changed && on_change) {
            on_change(index_);
        }
    };
}

void SelectField::set_items(std::vector<std::string> items, std::size_t index) {
    items_ = std::move(items);
    index_ = index < items_.size() ? index : 0;
    relabel();
    if (popup_.is_open()) {
        close();
    }
}

void SelectField::set_index(std::size_t index) {
    index_ = index < items_.size() ? index : 0;
    relabel();
}

void SelectField::relabel() {
    value_ = line_of(kit_, items_.empty() ? std::string() : items_[index_], kit_.cfg->field.input);
}

void SelectField::sync_rows() {
    std::vector<ListRow> rows;
    rows.reserve(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) {
        ListRow row;
        row.id = static_cast<std::int64_t>(i);
        row.text = items_[i];
        row.selected = i == index_;
        rows.push_back(std::move(row));
    }
    list_.set_rows(std::move(rows));
}

void SelectField::open(bool keyboard) {
    if (items_.empty()) {
        return;
    }
    const FieldConfig& t = kit_.cfg->field;
    sync_rows();
    const double height = std::min(list_.content_height() + 2.0 * t.popup_padding, t.popup_max_height);
    const double bottom = window_.y + window_.height - t.popup_margin;
    const double top = window_.y + t.popup_margin;
    double y = box_.y + box_.height;
    opens_upward_ = false;
    if (y + height > bottom && box_.y - height >= top) {
        y = box_.y - height;
        opens_upward_ = true;
    } else if (y + height > bottom) {
        y = std::max(top, bottom - height);
    }
    popup_rect_ = {box_.x, y, box_.width, height};
    list_.arrange({popup_rect_.x, popup_rect_.y + t.popup_padding, popup_rect_.width,
                   std::max(0.0, popup_rect_.height - 2.0 * t.popup_padding)});
    list_.set_focused(true);
    list_.show_focus_ring(keyboard);
    list_.reveal_selected();
    popup_.open();
}

void SelectField::close() {
    popup_.close();
    list_.cancel();
    list_.set_focused(false);
}

Size SelectField::measure(const Constraints& c) {
    const FieldConfig& t = kit_.cfg->field;
    return {std::max(t.min_width, finite_or(c.max_width, t.default_width)), t.height};
}

void SelectField::arrange(const Rect& r) {
    box_ = field_box(kit_, r);
}

void SelectField::set_focused(bool focused) {
    focused_ = focused;
    if (!focused && popup_.is_open()) {
        close();
    }
}

void SelectField::handle(const PointerEvent& e) {
    if (popup_.is_open()) {
        const bool outside = !inside(popup_rect_, e.x_dp, e.y_dp);
        if (e.action == PointerAction::down && outside) {
            close();
            return;
        }
        list_.handle(e);
        return;
    }
    const bool clicked = press_.update(e, box_);
    if (e.action == PointerAction::down) {
        focused_ = press_.hovered;
    }
    if (clicked) {
        open(false);
    }
}

bool SelectField::handle(const WheelEvent& e) {
    return popup_.is_open() && list_.handle(e);
}

bool SelectField::handle(const KeyEvent& e) {
    if (popup_.is_open()) {
        if (e.key == Key::escape || e.key == Key::tab) {
            close();
            return true;
        }
        list_.handle(e);
        return true;
    }
    if (!focused_ || e.ctrl || e.alt || e.repeat) {
        return false;
    }
    if (e.key == Key::space || e.key == Key::enter || e.key == Key::down || e.key == Key::up) {
        open(true);
        return true;
    }
    return false;
}

void SelectField::cancel() {
    press_ = {};
    if (popup_.is_open()) {
        close();
    }
}

void SelectField::step(double dt) {
    popup_.step(dt);
    list_.step(dt);
}

bool SelectField::animating() const {
    return !popup_.settled() || list_.animating();
}

std::optional<double> SelectField::next_wake_seconds() const {
    return popup_.is_open() ? list_.next_wake_seconds() : std::nullopt;
}

void SelectField::paint(Canvas& canvas, const ColorScheme& scheme) const {
    const FieldConfig& t = kit_.cfg->field;
    paint_field_frame(canvas, kit_, scheme,
                      {box_, container_, focused_ || popup_.is_open(), press_.hovered}, label_);
    const double icon_x = box_.x + box_.width - t.padding - t.trailing_icon;
    draw_line_in(canvas, kit_, value_,
                 {box_.x + t.padding, box_.y, std::max(0.0, icon_x - t.label_gap - box_.x - t.padding),
                  box_.height},
                 scheme[Role::on_surface]);
    kit_.icons.draw(canvas, kit_.cfg->icons.dropdown,
                    {icon_x, box_.y + (box_.height - t.trailing_icon) / 2.0}, t.trailing_icon,
                    scheme[Role::on_surface_variant]);
}

void SelectField::paint_overlay(Canvas& canvas, const ColorScheme& scheme) const {
    if (!popup_.visible()) {
        return;
    }
    const FieldConfig& t = kit_.cfg->field;
    const ColorScheme s = lm::color::faded(scheme, popup_.opacity());
    canvas.save();
    popup_.scale_about(canvas, {popup_rect_.x + popup_rect_.width / 2.0,
                                opens_upward_ ? popup_rect_.y + popup_rect_.height : popup_rect_.y});
    lm::widgets::paint_surface(canvas, popup_rect_, t.popup_radius, s[kit_.cfg->theme.popup],
                               lm::tokens::elevation_dp[t.popup_elevation], s[Role::shadow]);
    list_.paint(canvas, s);
    canvas.restore();
}

} // namespace cypherarx::lamina_gui
