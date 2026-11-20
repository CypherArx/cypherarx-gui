#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "lamina/widgets/popup_transition.hpp"

#include "lamina_gui/controls.hpp"
#include "lamina_gui/list_view.hpp"

namespace cypherarx::lamina_gui {

class SelectField final : public Control {
public:
    SelectField(const Kit& kit, std::string label, SurfaceRole container);

    std::function<void(std::size_t)> on_change;

    void set_items(std::vector<std::string> items, std::size_t index = 0);
    bool has_items() const { return !items_.empty(); }
    std::size_t index() const { return index_; }
    void set_index(std::size_t index);

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void set_window(const Rect& window) override { window_ = window; }
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void paint_overlay(Canvas& canvas, const ColorScheme& scheme) const override;
    void handle(const PointerEvent& e) override;
    bool handle(const WheelEvent& e) override;
    bool handle(const KeyEvent& e) override;
    void cancel() override;
    void step(double dt) override;
    bool animating() const override;
    bool focusable() const override { return true; }
    bool focused() const override { return focused_; }
    void set_focused(bool focused) override;
    std::optional<double> next_wake_seconds() const override;
    bool capturing() const override { return popup_.is_open(); }

private:
    void open(bool keyboard);
    void close();
    void relabel();
    void sync_rows();

    const Kit& kit_;
    SurfaceRole container_;
    lm::text::TextLine label_;
    lm::text::TextLine value_;
    std::vector<std::string> items_;
    std::size_t index_ = 0;
    ListView list_;
    lm::widgets::PopupTransition popup_;
    lm::widgets::PressState press_;
    bool focused_ = false;
    bool opens_upward_ = false;
    Rect box_{0.0, 0.0, 0.0, 0.0};
    Rect window_{0.0, 0.0, 0.0, 0.0};
    Rect popup_rect_{0.0, 0.0, 0.0, 0.0};
};

} // namespace cypherarx::lamina_gui
