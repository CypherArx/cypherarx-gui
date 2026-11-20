#include "lamina_gui/sheet.hpp"

#include <algorithm>

#include "lamina/tokens/motion.hpp"

namespace cypherarx::lamina_gui {

Sheet::Sheet(const Kit& kit, std::string title, double width)
    : kit_(kit),
      panel_(kit),
      title_(panel_.add<LabelC>(kit, LabelSpec{std::move(title), kit.cfg->sheet.title})),
      width_(width),
      popup_(kit.cfg->sheet.closed_scale, lm::tokens::motion_scheme.default_spatial(),
             lm::tokens::motion_scheme.default_effects()) {}

void Sheet::set_title(std::string title) {
    title_.set_text(std::move(title));
}

void Sheet::show() {
    panel_.cancel();
    popup_.open();
}

void Sheet::close() {
    popup_.close();
    panel_.cancel();
    panel_.clear_focus();
}

void Sheet::set_area(const Rect& area) {
    const SheetConfig& t = kit_.cfg->sheet;
    area_ = area;
    if (!root_) {
        auto column = lm::layout::column(
            {title_.node(), lm::layout::spacer({0.0, t.title_gap}), body()});
        column->padding(lm::layout::Insets::all(t.padding))
            .cross_align(lm::layout::CrossAlign::stretch)
            .fill_cross(true);
        root_ = column;
    }
    const double width = std::max(0.0, std::min(width_, area.width - 2.0 * t.window_margin));
    const double max_height = std::max(0.0, area.height - 2.0 * t.window_margin);
    const Size s = root_->measure({width, width, 0.0, max_height});
    surface_ = {area.x + (area.width - s.width) / 2.0, area.y + (area.height - s.height) / 2.0,
                s.width, s.height};
    root_->arrange(surface_);
    panel_.set_window(area);
}

void Sheet::handle(const PointerEvent& e) {
    if (popup_.is_open()) {
        panel_.handle(e);
    }
}

void Sheet::handle(const WheelEvent& e) {
    if (popup_.is_open()) {
        panel_.handle(e);
    }
}

void Sheet::handle(const KeyEvent& e) {
    if (!popup_.is_open() || panel_.handle(e)) {
        return;
    }
    if (e.key == Key::escape) {
        if (on_cancel) {
            on_cancel();
        }
    } else if (e.key == Key::enter && !e.repeat && on_submit) {
        on_submit();
    }
}

void Sheet::handle(const TextEvent& e) {
    if (popup_.is_open()) {
        panel_.handle(e);
    }
}

void Sheet::cancel() {
    panel_.cancel();
}

void Sheet::step(double dt) {
    popup_.step(dt);
    panel_.step(dt);
}

bool Sheet::animating() const {
    return !popup_.settled() || (popup_.visible() && panel_.animating());
}

void Sheet::paint(Canvas& canvas, const ColorScheme& scheme) const {
    if (!popup_.visible()) {
        return;
    }
    const ThemeConfig& theme = kit_.cfg->theme;
    const double a = popup_.opacity();
    canvas.fill_rect(area_, lm::color::with_alpha(scheme[theme.scrim], theme.scrim_opacity * a));
    const ColorScheme s = lm::color::faded(scheme, a);
    canvas.save();
    popup_.scale_about(canvas, {surface_.x + surface_.width / 2.0, surface_.y + surface_.height / 2.0});
    canvas.fill_rounded_rect(surface_, lm::render::uniform_radii(kit_.cfg->sheet.radius),
                             s[kit_.theme->sheet]);
    panel_.paint(canvas, s);
    panel_.paint_overlays(canvas, s);
    canvas.restore();
}

std::optional<Rect> Sheet::caret_area() const {
    return popup_.is_open() ? panel_.caret_area() : std::nullopt;
}

std::optional<double> Sheet::next_wake_seconds() const {
    return popup_.is_open() ? panel_.next_wake_seconds() : std::nullopt;
}

DialogModal::DialogModal(const Kit& kit, lm::widgets::DialogOptions options,
                         std::function<void(bool)> on_result)
    : dialog_(std::move(options), kit.fonts), on_result_(std::move(on_result)) {}

void DialogModal::show(const Rect& area) {
    dialog_.open_in(area);
}

void DialogModal::set_area(const Rect& area) {
    if (dialog_.visible()) {
        dialog_.set_area(area);
    }
}

void DialogModal::finish(lm::widgets::DialogResult result) {
    if (result == lm::widgets::DialogResult::none) {
        return;
    }
    if (on_result_) {
        auto callback = std::move(on_result_);
        on_result_ = nullptr;
        callback(result == lm::widgets::DialogResult::confirmed);
    }
}

void DialogModal::handle(const PointerEvent& e) {
    finish(dialog_.handle(e));
}

void DialogModal::handle(const KeyEvent& e) {
    finish(dialog_.handle(e));
}

void DialogModal::paint(Canvas& canvas, const ColorScheme& scheme) const {
    dialog_.paint(canvas, scheme);
}

MenuModal::MenuModal(const Kit& kit, lm::widgets::MenuOptions options,
                     std::function<void(std::size_t)> on_choose)
    : menu_(std::move(options), kit.fonts), on_choose_(std::move(on_choose)) {}

void MenuModal::show(const Rect& anchor, Size window) {
    menu_.open(anchor, window);
}

void MenuModal::finish(const lm::widgets::MenuResponse& response) {
    if (response.chosen && on_choose_) {
        on_choose_(*response.chosen);
    }
}

void MenuModal::handle(const PointerEvent& e) {
    finish(menu_.handle(e));
}

void MenuModal::handle(const KeyEvent& e) {
    finish(menu_.handle(e));
}

void MenuModal::paint(Canvas& canvas, const ColorScheme& scheme) const {
    menu_.paint(canvas, scheme);
}

} // namespace cypherarx::lamina_gui
