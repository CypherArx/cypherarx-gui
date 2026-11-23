#include "lamina_gui/controls.hpp"

#include <algorithm>
#include <cstdio>

#include "lamina/render/path.hpp"
#include "lamina/tokens/interaction.hpp"

namespace cypherarx::lamina_gui {

namespace {

lm::widgets::ButtonOptions button_options(const Kit& kit, const ButtonSpec& spec) {
    lm::widgets::ButtonOptions o;
    o.label = spec.label;
    o.style = spec.style;
    o.size = spec.size;
    if (spec.icon) {
        o.leading_icon = lm::icons::Icon{kit.icons, *spec.icon};
    }
    return o;
}

lm::widgets::ButtonOptions disabled_options(const Kit& kit, const ButtonSpec& spec) {
    lm::widgets::ButtonOptions o = button_options(kit, spec);
    const bool solid = spec.style == lm::tokens::ButtonStyle::filled ||
                       spec.style == lm::tokens::ButtonStyle::tonal;
    if (solid) {
        o.colors = lm::widgets::ButtonColors{kit.cfg->theme.disabled_container,
                                             kit.cfg->theme.disabled_content};
    }
    return o;
}

lm::widgets::TabRowOptions tab_options(const Kit& kit, const std::vector<TabSpec>& tabs) {
    lm::widgets::TabRowOptions o;
    for (const TabSpec& tab : tabs) {
        o.tabs.push_back({tab.label, lm::icons::Icon{kit.icons, tab.icon}});
    }
    o.style = lm::tokens::TabStyle::secondary;
    o.icon_position = lm::widgets::TabIconPosition::leading;
    return o;
}

lm::widgets::CircularProgressOptions spinner_options(const Kit& kit) {
    lm::widgets::CircularProgressOptions o;
    o.diameter = kit.cfg->spinner.diameter;
    return o;
}

lm::widgets::CircularProgressOptions ring_options(const Kit& kit) {
    lm::widgets::CircularProgressOptions o;
    o.diameter = kit.cfg->totp.ring;
    o.thickness = kit.cfg->totp.ring_thickness;
    o.track_thickness = kit.cfg->totp.ring_thickness;
    o.wavy = false;
    return o;
}

std::string grouped(const std::string& digits, std::size_t group) {
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && group > 0 && i % group == 0) {
            out += ' ';
        }
        out += digits[i];
    }
    return out;
}

} // namespace

Rect field_box(const Kit& kit, const Rect& arranged) {
    const double h = kit.cfg->field.height;
    return {arranged.x, arranged.y + (arranged.height - h) / 2.0, arranged.width, h};
}

void paint_field_frame(Canvas& canvas, const Kit& kit, const ColorScheme& scheme,
                       const FieldFrame& frame, const lm::text::TextLine& label,
                       const lm::text::TextLine* resting) {
    const FieldConfig& t = kit.cfg->field;
    const Rect& r = frame.box;
    const Role outline = frame.focused ? Role::primary : frame.hovered ? Role::on_surface : Role::outline;
    canvas.fill_path(lm::render::ring_path(r, t.radius, frame.focused ? t.focus_outline : t.outline),
                     scheme[outline], lm::render::FillRule::even_odd);
    const double x = r.x + t.padding;
    if (!frame.floated) {
        if (resting) {
            draw_line_in(canvas, kit, *resting, {x, r.y, std::max(0.0, r.width - 2.0 * t.padding), r.height},
                         scheme[Role::on_surface_variant]);
        }
        return;
    }
    if (label.glyphs.empty()) {
        return;
    }
    const double h = label.box_height();
    canvas.fill_rect({x - t.label_gap, r.y - h / 2.0, label.width + 2.0 * t.label_gap, h},
                     lm::color::with_alpha(scheme[frame.container], 1.0));
    lm::text::draw_line(canvas, kit.fonts, label, {x, r.y - h / 2.0},
                        scheme[frame.focused ? Role::primary : Role::on_surface_variant]);
}

void paint_tooltip(Canvas& canvas, const Kit& kit, const ColorScheme& scheme,
                   const lm::text::TextLine& text, const Rect& anchor, const Rect& window) {
    const TooltipConfig& t = kit.cfg->tooltip;
    const double width = text.width + 2.0 * t.padding;
    const double left = window.x + t.window_margin;
    const double right = std::max(left, window.x + window.width - t.window_margin - width);
    const double x = std::clamp(anchor.x + (anchor.width - width) / 2.0, left, right);
    double y = anchor.y + anchor.height + t.gap;
    if (y + t.height > window.y + window.height - t.window_margin) {
        y = anchor.y - t.gap - t.height;
    }
    canvas.fill_rounded_rect({x, y, width, t.height}, lm::render::uniform_radii(t.radius),
                             scheme[Role::inverse_surface]);
    draw_line_in(canvas, kit, text, {x + t.padding, y, text.width, t.height},
                 scheme[Role::inverse_on_surface]);
}

void Control::set_visible(bool visible) {
    if (visible_ && !visible) {
        cancel();
        set_focused(false);
    }
    visible_ = visible;
}

void Control::set_enabled(bool enabled) {
    if (enabled_ && !enabled) {
        cancel();
        set_focused(false);
    }
    enabled_ = enabled;
}

NodePtr Control::node() {
    if (!node_) {
        node_ = std::make_shared<lm::layout::Leaf>(
            [this](const Constraints& c) { return measure(c); },
            [this](const Rect& r) { arrange(r); });
    }
    return node_;
}

void Panel::set_window(const Rect& window) {
    for (auto& c : controls_) {
        c->set_window(window);
    }
}

Control* Panel::capturing() const {
    for (auto& c : controls_) {
        if (c->visible() && c->enabled() && c->capturing()) {
            return c.get();
        }
    }
    return nullptr;
}

Control* Panel::focused() const {
    for (auto& c : controls_) {
        if (c->visible() && c->enabled() && c->focused()) {
            return c.get();
        }
    }
    return nullptr;
}

void Panel::handle(const PointerEvent& e) {
    if (Control* c = capturing()) {
        c->handle(e);
        return;
    }
    for (auto& c : controls_) {
        if (c->visible() && c->enabled()) {
            c->handle(e);
        }
    }
}

bool Panel::handle(const WheelEvent& e) {
    if (Control* c = capturing()) {
        c->handle(e);
        return true;
    }
    for (auto& c : controls_) {
        if (c->visible() && c->enabled() && c->handle(e)) {
            return true;
        }
    }
    return false;
}

bool Panel::handle(const KeyEvent& e) {
    if (Control* c = capturing()) {
        c->handle(e);
        return true;
    }
    if (e.key == Key::tab && focus_.handle(e)) {
        return true;
    }
    if (Control* c = focused()) {
        return c->handle(e);
    }
    return false;
}

void Panel::handle(const TextEvent& e) {
    if (Control* c = capturing()) {
        c->handle(e);
        return;
    }
    if (Control* c = focused()) {
        c->handle(e);
    }
}

void Panel::cancel() {
    for (auto& c : controls_) {
        c->cancel();
    }
}

void Panel::step(double dt) {
    for (auto& c : controls_) {
        c->step(dt);
    }
}

bool Panel::animating() const {
    return std::ranges::any_of(controls_,
                               [](const auto& c) { return c->visible() && c->animating(); });
}

void Panel::paint(Canvas& canvas, const ColorScheme& scheme) const {
    const ColorScheme disabled = lm::color::faded(scheme, kit_.cfg->theme.disabled_opacity);
    for (const auto& c : controls_) {
        if (c->visible()) {
            c->paint(canvas, c->enabled() ? scheme : disabled);
        }
    }
}

void Panel::paint_overlays(Canvas& canvas, const ColorScheme& scheme) const {
    for (const auto& c : controls_) {
        if (c->visible()) {
            c->paint_overlay(canvas, scheme);
        }
    }
}

void Panel::focus(Control& control) {
    focus_.clear_focus();
    if (control.can_focus()) {
        control.set_focused(true);
    }
}

void Panel::clear_focus() {
    focus_.clear_focus();
}

std::optional<Rect> Panel::caret_area() const {
    if (Control* c = capturing()) {
        return c->caret_area();
    }
    if (Control* c = focused()) {
        return c->caret_area();
    }
    return std::nullopt;
}

std::optional<double> Panel::next_wake_seconds() const {
    std::optional<double> wake;
    for (const auto& c : controls_) {
        if (!c->visible()) {
            continue;
        }
        if (const auto w = c->next_wake_seconds()) {
            wake = wake ? std::min(*wake, *w) : *w;
        }
    }
    return wake;
}

ButtonC::ButtonC(const Kit& kit, ButtonSpec spec)
    : kit_(kit),
      spec_(std::move(spec)),
      w_(button_options(kit, spec_), kit.fonts),
      disabled_(disabled_options(kit, spec_), kit.fonts) {}

void ButtonC::set_style(lm::tokens::ButtonStyle style) {
    if (spec_.style == style) {
        return;
    }
    spec_.style = style;
    w_ = lm::widgets::Button(button_options(kit_, spec_), kit_.fonts);
    disabled_ = lm::widgets::Button(disabled_options(kit_, spec_), kit_.fonts);
    w_.set_origin(origin_);
    disabled_.set_origin(origin_);
}
Size ButtonC::measure(const Constraints&) {
    return {w_.width(), w_.height()};
}

void ButtonC::arrange(const Rect& r) {
    origin_ = {r.x, r.y + (r.height - w_.height()) / 2.0};
    w_.set_origin(origin_);
    disabled_.set_origin(origin_);
}

void ButtonC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    (enabled() ? w_ : disabled_).paint(canvas, scheme);
}

void ButtonC::handle(const PointerEvent& e) {
    if (w_.handle(e) && on_click) {
        on_click();
    }
}

bool ButtonC::handle(const KeyEvent& e) {
    if (!w_.handle(e)) {
        return false;
    }
    if (on_click) {
        on_click();
    }
    return true;
}

IconButtonC::IconButtonC(const Kit& kit, lm::icons::Symbol symbol, bool toggleable,
                         std::string name)
    : kit_(kit),
      w_(lm::widgets::IconButtonOptions{
          .icon = {kit.icons, symbol}, .accessible_name = name, .toggleable = toggleable}),
      tip_(line_of(kit, name, kit.cfg->tooltip.type)),
      has_tip_(!name.empty()) {}

Size IconButtonC::measure(const Constraints&) {
    return {w_.width(), w_.height()};
}

void IconButtonC::arrange(const Rect& r) {
    w_.set_origin({r.x, r.y + (r.height - w_.height()) / 2.0});
}

void IconButtonC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    w_.paint(canvas, scheme);
}

void IconButtonC::paint_overlay(Canvas& canvas, const ColorScheme& scheme) const {
    if (has_tip_ && tip_state_.shown(kit_.cfg->tooltip.delay_seconds)) {
        paint_tooltip(canvas, kit_, scheme, tip_, w_.bounds(), window_);
    }
}

void IconButtonC::activated() {
    if (on_click) {
        on_click();
    }
    if (on_change) {
        on_change(w_.checked());
    }
}

void IconButtonC::handle(const PointerEvent& e) {
    hover_.update(e, w_.bounds());
    tip_state_.track(hover_.hovered);
    if (hover_.pressed) {
        tip_state_.dismiss();
    }
    if (w_.handle(e)) {
        activated();
    }
}

bool IconButtonC::handle(const KeyEvent& e) {
    if (!w_.handle(e)) {
        return false;
    }
    activated();
    return true;
}

void IconButtonC::cancel() {
    w_.cancel_interaction();
    hover_ = {};
    tip_state_.reset();
}

void IconButtonC::step(double dt) {
    w_.step(dt);
    tip_state_.step(dt);
}

std::optional<double> IconButtonC::next_wake_seconds() const {
    return has_tip_ ? tip_state_.wake(kit_.cfg->tooltip.delay_seconds) : std::nullopt;
}
TabsC::TabsC(const Kit& kit, const std::vector<TabSpec>& tabs)
    : kit_(kit), w_(tab_options(kit, tabs), kit.fonts) {}

void TabsC::select(std::size_t index) {
    if (index != w_.selected() && index < w_.count()) {
        w_.select(index);
    }
}

Size TabsC::measure(const Constraints& c) {
    return {finite_or(c.max_width, w_.width()), w_.height()};
}

void TabsC::arrange(const Rect& r) {
    w_.set_width(r.width);
    w_.set_origin({r.x, r.y});
}

void TabsC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    ColorScheme on_pane = scheme;
    on_pane.colors[static_cast<std::size_t>(Role::surface)] = scheme[pane_surface(*kit_.theme)];
    w_.paint(canvas, on_pane);
}

void TabsC::handle(const PointerEvent& e) {
    if (w_.handle(e) && on_change) {
        on_change(w_.selected());
    }
}

bool TabsC::handle(const KeyEvent& e) {
    if (!w_.handle(e)) {
        return false;
    }
    if (on_change) {
        on_change(w_.selected());
    }
    return true;
}

CheckC::CheckC(const Kit& kit, std::string label)
    : kit_(kit), label_(line_of(kit, label, TypeRole::body_large)) {}

void CheckC::set_checked(bool checked) {
    w_.set_state(checked ? lm::tokens::CheckboxState::on : lm::tokens::CheckboxState::off);
}

Size CheckC::measure(const Constraints&) {
    return {w_.width() + kit_.cfg->layout.control_gap + label_.width,
            std::max(w_.height(), label_.box_height())};
}

void CheckC::arrange(const Rect& r) {
    rect_ = r;
    w_.set_origin({r.x, r.y + (r.height - w_.height()) / 2.0});
}

void CheckC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    w_.paint(canvas, scheme);
    const double x = rect_.x + w_.width() + kit_.cfg->layout.control_gap;
    draw_line_in(canvas, kit_, label_, {x, rect_.y, label_.width, rect_.height},
                 scheme[Role::on_surface]);
}

void CheckC::handle(const PointerEvent& e) {
    const bool before = checked();
    w_.handle(e);
    const double x = rect_.x + w_.width();
    const Rect label_box{x, rect_.y, kit_.cfg->layout.control_gap + label_.width, rect_.height};
    if (label_press_.update(e, label_box)) {
        set_checked(!checked());
    }
    if (checked() != before && on_change) {
        on_change(checked());
    }
}

bool CheckC::handle(const KeyEvent& e) {
    const bool before = checked();
    const bool used = w_.handle(e);
    if (checked() != before && on_change) {
        on_change(checked());
    }
    return used;
}

void CheckC::cancel() {
    w_.cancel_interaction();
    label_press_ = {};
}

TextFieldC::TextFieldC(const Kit& kit, std::string label)
    : w_(lm::widgets::TextFieldOptions{.label = std::move(label), .clipboard = kit.clipboard},
         kit.fonts) {}

void TextFieldC::set_text(std::string_view utf8) {
    w_.set_text(utf8);
    last_ = std::string(utf8);
}

Size TextFieldC::measure(const Constraints& c) {
    return {finite_or(c.max_width, w_.width()), w_.height()};
}

void TextFieldC::arrange(const Rect& r) {
    w_.set_width(r.width);
    w_.set_origin({r.x, r.y + (r.height - w_.height()) / 2.0});
}

void TextFieldC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    w_.paint(canvas, scheme);
}

void TextFieldC::notify() {
    std::string now = w_.text();
    if (now == last_) {
        return;
    }
    last_ = std::move(now);
    if (on_change) {
        on_change(last_);
    }
}

void TextFieldC::handle(const PointerEvent& e) {
    w_.handle(e);
}

bool TextFieldC::handle(const KeyEvent& e) {
    const bool submitted = w_.handle(e);
    notify();
    if (submitted) {
        if (on_submit) {
            on_submit();
            return true;
        }
        return false;
    }
    return e.key != Key::escape && e.key != Key::enter;
}

void TextFieldC::handle(const TextEvent& e) {
    w_.handle(e);
    notify();
}

std::optional<Rect> TextFieldC::caret_area() const {
    return w_.focused() ? std::optional<Rect>(w_.caret_area()) : std::nullopt;
}

std::optional<double> TextFieldC::next_wake_seconds() const {
    const double s = w_.next_blink_seconds();
    return s < 0.0 ? std::nullopt : std::optional<double>(s);
}

LabelC::LabelC(const Kit& kit, LabelSpec spec)
    : w_({spec.text, spec.type, spec.color, spec.wrap}, kit.fonts),
      text_(spec.text),
      wrap_(spec.wrap) {}

void LabelC::set_text(std::string text) {
    if (text == text_) {
        return;
    }
    text_ = text;
    w_.set_text(std::move(text));
}

Size LabelC::measure(const Constraints& c) {
    return w_.measure(c);
}

void LabelC::arrange(const Rect& r) {
    rect_ = r;
    const double h = w_.bounds().height;
    w_.set_origin({r.x, r.y + (r.height - h) / 2.0});
}

void LabelC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    if (text_.empty()) {
        return;
    }
    const bool clip = !wrap_ && w_.bounds().width > rect_.width;
    if (clip) {
        canvas.push_clip(rect_);
    }
    w_.paint(canvas, scheme);
    if (clip) {
        canvas.pop_clip();
    }
}

Size PaneC::measure(const Constraints& c) {
    return {finite_or(c.max_width, 0.0), finite_or(c.max_height, 0.0)};
}

void PaneC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    const ThemeConfig& t = kit_.cfg->theme;
    const double radius = kit_.cfg->layout.pane_radius;
    if (kit_.theme->pane_style == PaneStyle::outlined) {
        canvas.fill_path(lm::render::ring_path(rect_, radius, t.pane_outline_width),
                         scheme[t.pane_outline], lm::render::FillRule::even_odd);
        return;
    }
    canvas.fill_rounded_rect(rect_, lm::render::uniform_radii(radius), scheme[kit_.theme->pane]);
}

SpinnerC::SpinnerC(const Kit& kit) : w_(spinner_options(kit)) {
    w_.set_progress(kit.cfg->spinner.progress);
}

Size SpinnerC::measure(const Constraints&) {
    return {w_.width(), w_.height()};
}

void SpinnerC::arrange(const Rect& r) {
    w_.set_origin({r.x, r.y + (r.height - w_.height()) / 2.0});
}

void SpinnerC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    w_.paint(canvas, scheme);
}

void ChipsC::set_chips(std::vector<Chip> chips) {
    chips_ = std::move(chips);
    items_.clear();
    items_.reserve(chips_.size());
    for (const Chip& c : chips_) {
        items_.push_back({line_of(kit_, c.text, kit_.cfg->chip.label)});
    }
}

Size ChipsC::measure(const Constraints& c) {
    const ChipConfig& t = kit_.cfg->chip;
    const double width = finite_or(c.max_width, kit_.cfg->field.default_width);
    double x = 0.0;
    double y = 0.0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const double w = 2.0 * t.padding + items_[i].label.width +
                         (chips_[i].selected ? t.check_size + t.check_gap : 0.0);
        if (x > 0.0 && x + w > width) {
            x = 0.0;
            y += t.height + t.gap;
        }
        items_[i].box = {x, y, w, t.height};
        x += w + t.gap;
    }
    return {width, items_.empty() ? 0.0 : y + t.height};
}

Rect ChipsC::box_of(std::size_t i) const {
    const Rect& b = items_[i].box;
    return {rect_.x + b.x, rect_.y + b.y, b.width, b.height};
}

void ChipsC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    const ChipConfig& t = kit_.cfg->chip;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Rect b = box_of(i);
        const bool on = chips_[i].selected;
        const Argb content = scheme[on ? Role::on_secondary_container : Role::on_surface_variant];
        if (on) {
            canvas.fill_rounded_rect(b, lm::render::uniform_radii(t.radius),
                                     scheme[Role::secondary_container]);
        } else {
            canvas.fill_path(lm::render::ring_path(b, t.radius, t.outline),
                             scheme[Role::outline_variant], lm::render::FillRule::even_odd);
        }
        const double layer = items_[i].press.state_layer_opacity();
        if (layer > 0.0) {
            canvas.fill_rounded_rect(b, lm::render::uniform_radii(t.radius),
                                     lm::color::with_alpha(content, layer));
        }
        double x = b.x + t.padding;
        if (on) {
            kit_.icons.draw(canvas, kit_.cfg->icons.checked,
                            {x, b.y + (b.height - t.check_size) / 2.0}, t.check_size, content);
            x += t.check_size + t.check_gap;
        }
        draw_line_in(canvas, kit_, items_[i].label, {x, b.y, items_[i].label.width, b.height},
                     content);
    }
}

void ChipsC::handle(const PointerEvent& e) {
    std::optional<std::size_t> clicked;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].press.update(e, box_of(i))) {
            clicked = i;
        }
    }
    if (!clicked) {
        return;
    }
    Chip& chip = chips_[*clicked];
    chip.selected = !chip.selected;
    if (on_toggle) {
        on_toggle(chip.id, chip.selected);
    }
}

void ChipsC::cancel() {
    for (Item& item : items_) {
        item.press = {};
    }
}

TotpC::TotpC(const Kit& kit)
    : kit_(kit),
      ring_(ring_options(kit)),
      code_(line_of(kit, "", kit.cfg->totp.code)),
      remaining_(line_of(kit, "", kit.cfg->totp.remaining)) {}

void TotpC::set_reading(const std::optional<std::string>& code, std::uint32_t remaining_seconds,
                        std::uint32_t period_seconds) {
    const std::string code_text = code ? grouped(*code, kit_.cfg->totp.group) : str::totp_failed;
    if (code_text != code_text_ || failed_ != !code) {
        code_text_ = code_text;
        failed_ = !code;
        code_ = line_of(kit_, code_text_, failed_ ? kit_.cfg->totp.remaining : kit_.cfg->totp.code);
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), str::totp_remaining_fmt, remaining_seconds);
    if (remaining_text_ != buf) {
        remaining_text_ = buf;
        remaining_ = line_of(kit_, remaining_text_, kit_.cfg->totp.remaining);
        const double progress = period_seconds == 0 ? 0.0
                                                    : static_cast<double>(remaining_seconds) /
                                                          static_cast<double>(period_seconds);
        ring_.ease_progress(progress);
    }
}

Size TotpC::measure(const Constraints&) {
    return {ring_.width() + kit_.cfg->totp.gap + std::max(code_.width, remaining_.width),
            std::max(ring_.height(), code_.box_height() + remaining_.box_height())};
}

void TotpC::arrange(const Rect& r) {
    rect_ = r;
    ring_.set_origin({r.x, r.y + (r.height - ring_.height()) / 2.0});
}

void TotpC::paint(Canvas& canvas, const ColorScheme& scheme) const {
    ring_.paint(canvas, scheme);
    const double x = rect_.x + ring_.width() + kit_.cfg->totp.gap;
    const double block = code_.box_height() + remaining_.box_height();
    const double top = rect_.y + (rect_.height - block) / 2.0;
    lm::text::draw_line(canvas, kit_.fonts, code_, {x, top},
                        scheme[failed_ ? Role::error : Role::on_surface]);
    lm::text::draw_line(canvas, kit_.fonts, remaining_, {x, top + code_.box_height()},
                        scheme[Role::on_surface_variant]);
}

} // namespace cypherarx::lamina_gui
