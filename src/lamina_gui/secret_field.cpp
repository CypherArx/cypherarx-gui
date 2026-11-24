#include "lamina_gui/secret_field.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <sodium.h>

#include "lamina/render/path.hpp"

namespace cypherarx::lamina_gui {

namespace {

bool continuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

bool control(char c) {
    const auto u = static_cast<unsigned char>(c);
    return u < 0x20 || u == 0x7f;
}

} // namespace

SecretField::SecretField(const Kit& kit, std::string label, std::size_t capacity,
                         SurfaceRole container)
    : kit_(kit),
      container_(container),
      buffer_(capacity),
      label_(line_of(kit, label, kit.cfg->field.label)),
      resting_label_(line_of(kit, label, kit.cfg->field.input)) {}

std::string_view SecretField::view() const {
    const std::span<const std::uint8_t> b = buffer_.bytes();
    return {reinterpret_cast<const char*>(b.data()), b.size()};
}

std::size_t SecretField::previous(std::size_t at) const {
    const std::string_view v = view();
    std::size_t i = std::min(at, v.size());
    while (i > 0) {
        --i;
        if (!continuation(v[i])) {
            break;
        }
    }
    return i;
}

std::size_t SecretField::next(std::size_t at) const {
    const std::string_view v = view();
    std::size_t i = std::min(at, v.size());
    if (i < v.size()) {
        ++i;
    }
    while (i < v.size() && continuation(v[i])) {
        ++i;
    }
    return i;
}

std::size_t SecretField::codepoints_before(std::size_t at) const {
    const std::string_view v = view();
    std::size_t n = 0;
    for (std::size_t i = 0; i < std::min(at, v.size()); ++i) {
        n += continuation(v[i]) ? 0 : 1;
    }
    return n;
}

std::size_t SecretField::offset_of(std::size_t codepoint) const {
    std::size_t at = 0;
    for (std::size_t i = 0; i < codepoint && at < length(); ++i) {
        at = next(at);
    }
    return at;
}

void SecretField::insert(std::string_view utf8) {
    const auto accepted =
        static_cast<std::size_t>(std::ranges::count_if(utf8, [](char c) { return !control(c); }));
    const std::size_t len = length();
    if (accepted == 0 || len + accepted > buffer_.capacity() - 1) {
        return;
    }
    char* data = buffer_.data();
    std::memmove(data + caret_ + accepted, data + caret_, len - caret_);
    std::size_t at = caret_;
    for (const char c : utf8) {
        if (!control(c)) {
            data[at++] = c;
        }
    }
    caret_ = at;
}

void SecretField::erase(std::size_t from, std::size_t to) {
    const std::size_t len = length();
    if (from >= to || to > len) {
        return;
    }
    char* data = buffer_.data();
    std::memmove(data + from, data + to, len - to);
    std::memset(data + len - (to - from), 0, to - from);
    caret_ = from;
}

void SecretField::paste() {
    if (!kit_.clipboard || !paste_allowed_) {
        return;
    }
    std::string clip = kit_.clipboard->text();
    insert(clip);
    sodium_memzero(clip.data(), clip.size());
}

void SecretField::clear() {
    buffer_.clear();
    composition_.clear();
    caret_ = 0;
    scroll_x_ = 0.0;
}

void SecretField::assign(std::span<const std::uint8_t> value) {
    buffer_.assign(value);
    composition_.clear();
    caret_ = length();
    scroll_x_ = 0.0;
    keep_caret_visible();
}

void SecretField::set_masked(bool masked) {
    if (masked_ == masked) {
        return;
    }
    masked_ = masked;
    scroll_x_ = 0.0;
    keep_caret_visible();
}

void SecretField::set_focused(bool focused) {
    if (focused_ == focused) {
        return;
    }
    focused_ = focused;
    blink_clock_ = 0.0;
    if (!focused_) {
        composition_.clear();
    }
}

lm::text::TextLine SecretField::shown_line() const {
    return lm::text::layout_line(kit_.fonts, view(), style_of(kit_.cfg->field.input));
}

double SecretField::caret_offset() const {
    const std::size_t index = codepoints_before(caret_);
    if (masked_) {
        return static_cast<double>(index) * kit_.cfg->field.dot_pitch;
    }
    return lm::text::caret_x(shown_line(), index);
}

double SecretField::text_left() const {
    return box_.x + kit_.cfg->field.padding;
}

double SecretField::inner_width() const {
    return std::max(0.0, box_.width - 2.0 * kit_.cfg->field.padding);
}

void SecretField::keep_caret_visible() {
    const double x = caret_offset();
    const double visible = inner_width();
    if (x - scroll_x_ > visible) {
        scroll_x_ = x - visible;
    }
    if (x < scroll_x_) {
        scroll_x_ = x;
    }
    scroll_x_ = std::max(0.0, scroll_x_);
}

void SecretField::place_caret(double x) {
    const double rel = x - text_left() + scroll_x_;
    std::size_t index = 0;
    if (masked_) {
        const double count = static_cast<double>(codepoints_before(length()));
        index = static_cast<std::size_t>(
            std::clamp(std::round(rel / kit_.cfg->field.dot_pitch), 0.0, count));
    } else {
        index = lm::text::index_at(shown_line(), rel);
    }
    caret_ = offset_of(index);
}

Size SecretField::measure(const Constraints& c) {
    return {finite_or(c.max_width, kit_.cfg->field.default_width), kit_.cfg->field.height};
}

void SecretField::arrange(const Rect& r) {
    box_ = field_box(kit_, r);
    keep_caret_visible();
}

void SecretField::handle(const PointerEvent& e) {
    press_.update(e, box_);
    if (e.action != PointerAction::down) {
        return;
    }
    if (!press_.hovered) {
        set_focused(false);
        return;
    }
    set_focused(true);
    if (composition_.empty()) {
        place_caret(e.x_dp);
    }
    blink_clock_ = 0.0;
}

bool SecretField::handle(const KeyEvent& e) {
    if (!focused_) {
        return false;
    }
    if (!composition_.empty()) {
        return true;
    }
    switch (e.key) {
    case Key::backspace:
        erase(previous(caret_), caret_);
        break;
    case Key::delete_forward:
        erase(caret_, next(caret_));
        break;
    case Key::left:
        caret_ = previous(caret_);
        break;
    case Key::right:
        caret_ = next(caret_);
        break;
    case Key::home:
        caret_ = 0;
        break;
    case Key::end:
        caret_ = length();
        break;
    case Key::enter:
        if (on_submit) {
            on_submit();
            return true;
        }
        return false;
    case Key::escape:
        return false;
    case Key::v:
        if (e.ctrl) {
            paste();
        }
        break;
    default:
        break;
    }
    keep_caret_visible();
    blink_clock_ = 0.0;
    if (on_change) {
        on_change();
    }
    return true;
}

void SecretField::handle(const TextEvent& e) {
    if (!focused_) {
        return;
    }
    if (e.kind == TextEventKind::commit) {
        composition_.clear();
        insert(e.text);
    } else {
        composition_ = e.text;
    }
    keep_caret_visible();
    blink_clock_ = 0.0;
    if (on_change) {
        on_change();
    }
}

std::optional<Rect> SecretField::caret_area() const {
    if (!focused_) {
        return std::nullopt;
    }
    const FieldConfig& t = kit_.cfg->field;
    const double h = style_of(t.input).line_height;
    return Rect{text_left() + caret_offset() - scroll_x_, box_.y + (box_.height - h) / 2.0,
                t.caret_width, h};
}

std::optional<double> SecretField::next_wake_seconds() const {
    if (!focused_) {
        return std::nullopt;
    }
    const double period = kit_.cfg->field.caret_blink_seconds;
    return period - std::fmod(blink_clock_, period);
}

void SecretField::paint(Canvas& canvas, const ColorScheme& scheme) const {
    const FieldConfig& t = kit_.cfg->field;
    const bool floated = focused_ || !empty() || !composition_.empty();
    paint_field_frame(canvas, kit_, scheme, {box_, kit_.theme->*container_, focused_, press_.hovered, floated},
                      label_, &resting_label_);

    canvas.push_clip({text_left() - t.caret_width, box_.y, inner_width() + 2.0 * t.caret_width,
                      box_.height});
    const double left = text_left() - scroll_x_;
    const double middle = box_.y + box_.height / 2.0;
    const Argb ink = scheme[Role::on_surface];
    double end = left;
    if (masked_) {
        const std::size_t count = codepoints_before(length());
        for (std::size_t i = 0; i < count; ++i) {
            canvas.fill_path(
                lm::render::circle_path({left + (static_cast<double>(i) + 0.5) * t.dot_pitch, middle},
                                        t.dot_diameter / 2.0),
                ink);
        }
        end = left + static_cast<double>(count) * t.dot_pitch;
    } else {
        const lm::text::TextLine line = shown_line();
        lm::text::draw_line(canvas, kit_.fonts, line, {left, middle - line.box_height() / 2.0}, ink);
        end = left + line.width;
    }
    if (!composition_.empty() && !masked_) {
        const lm::text::TextLine pending =
            lm::text::layout_line(kit_.fonts, composition_, style_of(t.input));
        const double top = middle - pending.box_height() / 2.0;
        lm::text::draw_line(canvas, kit_.fonts, pending, {end, top}, ink);
        canvas.fill_rect({end, top + pending.baseline + 2.0, pending.width, 1.0}, ink);
    }
    const bool caret_on = std::fmod(blink_clock_, 2.0 * t.caret_blink_seconds) < t.caret_blink_seconds;
    if (focused_ && caret_on && composition_.empty()) {
        if (const auto c = caret_area()) {
            canvas.fill_rect({c->x - t.caret_width / 2.0, c->y, t.caret_width, c->height},
                             scheme[Role::primary]);
        }
    }
    canvas.pop_clip();
}

} // namespace cypherarx::lamina_gui
