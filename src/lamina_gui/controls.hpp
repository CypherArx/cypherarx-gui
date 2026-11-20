#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lamina/widgets/button.hpp"
#include "lamina/widgets/checkbox.hpp"
#include "lamina/widgets/focus_manager.hpp"
#include "lamina/widgets/icon_button.hpp"
#include "lamina/widgets/interaction.hpp"
#include "lamina/widgets/label.hpp"
#include "lamina/widgets/progress_indicator.hpp"
#include "lamina/widgets/text_field.hpp"

#include "lamina_gui/ui.hpp"

namespace cypherarx::lamina_gui {

class Control {
public:
    Control() = default;
    virtual ~Control() = default;
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;

    virtual Size measure(const Constraints& c) = 0;
    virtual void arrange(const Rect& r) = 0;
    virtual void set_window(const Rect&) {}
    virtual void paint(Canvas& canvas, const ColorScheme& scheme) const = 0;
    virtual void paint_overlay(Canvas&, const ColorScheme&) const {}

    virtual void handle(const PointerEvent&) {}
    virtual bool handle(const WheelEvent&) { return false; }
    virtual bool handle(const KeyEvent&) { return false; }
    virtual void handle(const TextEvent&) {}
    virtual void cancel() {}
    virtual void step(double) {}
    virtual bool animating() const { return false; }

    virtual bool focusable() const { return false; }
    virtual bool focused() const { return false; }
    virtual void set_focused(bool) {}
    virtual std::optional<Rect> caret_area() const { return std::nullopt; }
    virtual std::optional<double> next_wake_seconds() const { return std::nullopt; }
    virtual bool capturing() const { return false; }

    bool can_focus() const { return visible_ && enabled_ && focusable(); }
    bool visible() const { return visible_; }
    bool enabled() const { return enabled_; }
    void set_visible(bool visible);
    void set_enabled(bool enabled);
    NodePtr node();

private:
    bool visible_ = true;
    bool enabled_ = true;
    NodePtr node_;
};

class Panel {
public:
    explicit Panel(const Kit& kit) : kit_(kit) {}

    template <class C, class... Args>
    C& add(Args&&... args) {
        auto owned = std::make_unique<C>(std::forward<Args>(args)...);
        C& ref = *owned;
        focus_.add(static_cast<Control&>(ref));
        controls_.push_back(std::move(owned));
        return ref;
    }

    void set_window(const Rect& window);

    void handle(const PointerEvent& e);
    bool handle(const WheelEvent& e);
    bool handle(const KeyEvent& e);
    void handle(const TextEvent& e);
    void cancel();
    void step(double dt);
    bool animating() const;
    void paint(Canvas& canvas, const ColorScheme& scheme) const;
    void paint_overlays(Canvas& canvas, const ColorScheme& scheme) const;

    void focus(Control& control);
    void clear_focus();
    std::optional<Rect> caret_area() const;
    std::optional<double> next_wake_seconds() const;

private:
    Control* capturing() const;
    Control* focused() const;

    const Kit& kit_;
    std::vector<std::unique_ptr<Control>> controls_;
    lm::widgets::FocusManager focus_;
};

struct FieldFrame {
    Rect box;
    Role container;
    bool focused = false;
    bool hovered = false;
    bool floated = true;
};

Rect field_box(const Kit& kit, const Rect& arranged);
void paint_field_frame(Canvas& canvas, const Kit& kit, const ColorScheme& scheme,
                       const FieldFrame& frame, const lm::text::TextLine& label,
                       const lm::text::TextLine* resting = nullptr);

struct ButtonSpec {
    std::string label;
    lm::tokens::ButtonStyle style = lm::tokens::ButtonStyle::tonal;
    std::optional<lm::icons::Symbol> icon = std::nullopt;
    lm::tokens::ButtonSize size = lm::tokens::ButtonSize::small;
};

class ButtonC final : public Control {
public:
    ButtonC(const Kit& kit, ButtonSpec spec);

    std::function<void()> on_click;

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void handle(const PointerEvent& e) override;
    bool handle(const KeyEvent& e) override;
    void cancel() override { w_.cancel_interaction(); }
    void step(double dt) override { w_.step(dt); }
    bool animating() const override { return w_.animating(); }
    bool focusable() const override { return true; }
    bool focused() const override { return w_.focused(); }
    void set_focused(bool focused) override { w_.set_focused(focused); }
    void set_style(lm::tokens::ButtonStyle style);

private:
    const Kit& kit_;
    ButtonSpec spec_;
    Point origin_{0.0, 0.0};
    lm::widgets::Button w_;
};

class IconButtonC final : public Control {
public:
    IconButtonC(const Kit& kit, lm::icons::Symbol symbol, bool toggleable);

    std::function<void()> on_click;
    std::function<void(bool)> on_change;
    bool checked() const { return w_.checked(); }
    void set_checked(bool checked) { w_.set_checked(checked); }
    Rect bounds() const { return w_.bounds(); }

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void handle(const PointerEvent& e) override;
    bool handle(const KeyEvent& e) override;
    void cancel() override { w_.cancel_interaction(); }
    void step(double dt) override { w_.step(dt); }
    bool animating() const override { return w_.animating(); }
    bool focusable() const override { return true; }
    bool focused() const override { return w_.focused(); }
    void set_focused(bool focused) override { w_.set_focused(focused); }

private:
    void activated();

    lm::widgets::IconButton w_;
};

class CheckC final : public Control {
public:
    CheckC(const Kit& kit, std::string label);

    std::function<void(bool)> on_change;
    bool checked() const { return w_.state() == lm::tokens::CheckboxState::on; }
    void set_checked(bool checked);

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void handle(const PointerEvent& e) override;
    bool handle(const KeyEvent& e) override;
    void cancel() override;
    void step(double dt) override { w_.step(dt); }
    bool animating() const override { return w_.animating(); }
    bool focusable() const override { return true; }
    bool focused() const override { return w_.focused(); }
    void set_focused(bool focused) override { w_.set_focused(focused); }

private:
    const Kit& kit_;
    lm::widgets::Checkbox w_;
    lm::text::TextLine label_;
    lm::widgets::PressState label_press_;
    Rect rect_{0.0, 0.0, 0.0, 0.0};
};

class TextFieldC final : public Control {
public:
    TextFieldC(const Kit& kit, std::string label);

    std::function<void(const std::string&)> on_change;
    std::function<void()> on_submit;
    std::string text() const { return w_.text(); }
    void set_text(std::string_view utf8);

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void handle(const PointerEvent& e) override;
    bool handle(const KeyEvent& e) override;
    void handle(const TextEvent& e) override;
    void cancel() override { w_.cancel_interaction(); }
    void step(double dt) override { w_.step(dt); }
    bool animating() const override { return w_.animating(); }
    bool focusable() const override { return true; }
    bool focused() const override { return w_.focused(); }
    void set_focused(bool focused) override { w_.set_focused(focused); }
    std::optional<Rect> caret_area() const override;
    std::optional<double> next_wake_seconds() const override;

private:
    void notify();

    lm::widgets::TextField w_;
    std::string last_;
};

struct LabelSpec {
    std::string text;
    TypeRole type = TypeRole::body_medium;
    Role color = Role::on_surface;
    bool wrap = false;
};

class LabelC final : public Control {
public:
    LabelC(const Kit& kit, LabelSpec spec);

    void set_text(std::string text);
    const std::string& text() const { return text_; }

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;

private:
    lm::widgets::Label w_;
    std::string text_;
    bool wrap_;
    Rect rect_{0.0, 0.0, 0.0, 0.0};
};

class PaneC final : public Control {
public:
    explicit PaneC(const Kit& kit) : kit_(kit) {}

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override { rect_ = r; }
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;

private:
    const Kit& kit_;
    Rect rect_{0.0, 0.0, 0.0, 0.0};
};

class SpinnerC final : public Control {
public:
    explicit SpinnerC(const Kit& kit);

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void step(double dt) override { w_.step(dt); }
    bool animating() const override { return true; }

private:
    lm::widgets::CircularProgressIndicator w_;
};

struct Chip {
    std::int64_t id = 0;
    std::string text;
    bool selected = false;
};

class ChipsC final : public Control {
public:
    explicit ChipsC(const Kit& kit) : kit_(kit) {}

    std::function<void(std::int64_t, bool)> on_toggle;
    void set_chips(std::vector<Chip> chips);
    const std::vector<Chip>& chips() const { return chips_; }

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override { rect_ = r; }
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void handle(const PointerEvent& e) override;
    void cancel() override;

private:
    struct Item {
        lm::text::TextLine label;
        Rect box{0.0, 0.0, 0.0, 0.0};
        lm::widgets::PressState press;
    };

    Rect box_of(std::size_t i) const;

    const Kit& kit_;
    std::vector<Chip> chips_;
    std::vector<Item> items_;
    Rect rect_{0.0, 0.0, 0.0, 0.0};
};

class TotpC final : public Control {
public:
    explicit TotpC(const Kit& kit);

    void set_reading(const std::optional<std::string>& code, std::uint32_t remaining_seconds,
                     std::uint32_t period_seconds);

    Size measure(const Constraints& c) override;
    void arrange(const Rect& r) override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    void step(double dt) override { ring_.step(dt); }
    bool animating() const override { return ring_.animating(); }

private:
    const Kit& kit_;
    lm::widgets::CircularProgressIndicator ring_;
    std::string code_text_;
    std::string remaining_text_;
    bool failed_ = false;
    lm::text::TextLine code_;
    lm::text::TextLine remaining_;
    Rect rect_{0.0, 0.0, 0.0, 0.0};
};

} // namespace cypherarx::lamina_gui
