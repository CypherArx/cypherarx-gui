#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "model/secret_buffer.hpp"

#include "lamina_gui/controls.hpp"

namespace cypherarx::lamina_gui {

class SecretField final : public Control {
public:
    SecretField(const Kit& kit, std::string label, std::size_t capacity, SurfaceRole container);

    std::function<void()> on_submit;

    std::span<const std::uint8_t> bytes() const { return buffer_.bytes(); }
    bool empty() const { return buffer_.empty(); }
    void clear();
    void assign(std::span<const std::uint8_t> value);
    bool masked() const { return masked_; }
    void set_masked(bool masked);

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void handle(const PointerEvent& e) override;
    bool handle(const KeyEvent& e) override;
    void handle(const TextEvent& e) override;
    void cancel() override { press_ = {}; }
    void step(double dt) override { blink_clock_ += dt; }
    bool focusable() const override { return true; }
    bool focused() const override { return focused_; }
    void set_focused(bool focused) override;
    std::optional<Rect> caret_area() const override;
    std::optional<double> next_wake_seconds() const override;

private:
    std::string_view view() const;
    std::size_t length() const { return view().size(); }
    std::size_t previous(std::size_t at) const;
    std::size_t next(std::size_t at) const;
    std::size_t codepoints_before(std::size_t at) const;
    std::size_t offset_of(std::size_t codepoint) const;
    void insert(std::string_view utf8);
    void erase(std::size_t from, std::size_t to);
    void paste();
    lm::text::TextLine shown_line() const;
    double caret_offset() const;
    double text_left() const;
    double inner_width() const;
    void keep_caret_visible();
    void place_caret(double x);

    const Kit& kit_;
    SurfaceRole container_;
    gui::SecretBuffer buffer_;
    lm::text::TextLine label_;
    lm::text::TextLine resting_label_;
    std::string composition_;
    std::size_t caret_ = 0;
    bool masked_ = true;
    bool focused_ = false;
    double scroll_x_ = 0.0;
    double blink_clock_ = 0.0;
    lm::widgets::PressState press_;
    Rect box_{0.0, 0.0, 0.0, 0.0};
};

} // namespace cypherarx::lamina_gui
