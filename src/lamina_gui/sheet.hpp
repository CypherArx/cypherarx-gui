#pragma once

#include <functional>
#include <optional>
#include <string>

#include "lamina/widgets/dialog.hpp"
#include "lamina/widgets/popup_transition.hpp"

#include "lamina_gui/controls.hpp"

namespace cypherarx::lamina_gui {

class Modal {
public:
    virtual ~Modal() = default;

    virtual bool is_open() const = 0;
    virtual bool visible() const = 0;
    virtual void set_area(const Rect& area) = 0;
    virtual void handle(const PointerEvent& e) = 0;
    virtual void handle(const WheelEvent& e) = 0;
    virtual void handle(const KeyEvent& e) = 0;
    virtual void handle(const TextEvent& e) = 0;
    virtual void cancel() = 0;
    virtual void step(double dt) = 0;
    virtual bool animating() const = 0;
    virtual void paint(Canvas& canvas, const ColorScheme& scheme) const = 0;
    virtual std::optional<Rect> caret_area() const { return std::nullopt; }
    virtual std::optional<double> next_wake_seconds() const { return std::nullopt; }
};

class Sheet : public Modal {
public:
    Sheet(const Kit& kit, std::string title, double width);

    std::function<void()> on_submit;
    std::function<void()> on_cancel;

    void set_title(std::string title);
    void show();
    void close();

    bool is_open() const override { return popup_.is_open(); }
    bool visible() const override { return popup_.visible(); }
    void set_area(const Rect& area) override;
    void handle(const PointerEvent& e) override;
    void handle(const WheelEvent& e) override;
    void handle(const KeyEvent& e) override;
    void handle(const TextEvent& e) override;
    void cancel() override;
    void step(double dt) override;
    bool animating() const override;
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;
    std::optional<Rect> caret_area() const override;
    std::optional<double> next_wake_seconds() const override;

protected:
    virtual NodePtr body() = 0;
    void rebuild() { root_.reset(); }

    const Kit& kit_;
    Panel panel_;

private:
    LabelC& title_;
    double width_;
    NodePtr root_;
    lm::widgets::PopupTransition popup_;
    Rect area_{0.0, 0.0, 0.0, 0.0};
    Rect surface_{0.0, 0.0, 0.0, 0.0};
};

class DialogModal final : public Modal {
public:
    DialogModal(const Kit& kit, lm::widgets::DialogOptions options,
                std::function<void(bool)> on_result);

    void show(const Rect& area);

    bool is_open() const override { return dialog_.is_open(); }
    bool visible() const override { return dialog_.visible(); }
    void set_area(const Rect& area) override;
    void handle(const PointerEvent& e) override;
    void handle(const WheelEvent&) override {}
    void handle(const KeyEvent& e) override;
    void handle(const TextEvent&) override {}
    void cancel() override { dialog_.cancel_interaction(); }
    void step(double dt) override { dialog_.step(dt); }
    bool animating() const override { return dialog_.animating(); }
    void paint(Canvas& canvas, const ColorScheme& scheme) const override;

private:
    void finish(lm::widgets::DialogResult result);

    lm::widgets::Dialog dialog_;
    std::function<void(bool)> on_result_;
};

} // namespace cypherarx::lamina_gui
