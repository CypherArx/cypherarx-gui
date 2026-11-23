#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lamina/widgets/scroll_area.hpp"

#include "lamina_gui/controls.hpp"

namespace cypherarx::lamina_gui {

struct ListRow {
    std::int64_t id = 0;
    int group = 0;
    std::string text;
    std::vector<std::string> cells;
    int depth = 0;
    bool section = false;
    bool selected = false;
    bool dim = false;
    bool expandable = false;
    bool expanded = true;
    std::optional<lm::icons::Symbol> icon = std::nullopt;
    unsigned actions = ~0u;
    std::optional<bool> check = std::nullopt;
};

struct RowAction {
    lm::icons::Symbol symbol;
    std::string tooltip;
};

struct ListColumns {
    std::vector<std::string> titles;
    std::vector<double> weights;
};

struct ListOptions {
    ListColumns columns;
    bool tree = false;
    bool select_follows_cursor = false;
    bool checks = false;
    std::optional<double> height = std::nullopt;
    std::vector<RowAction> row_actions = {};
};

class ListView final : public Control {
public:
    ListView(const Kit& kit, ListOptions options = {});

    std::function<void(const ListRow&)> on_select;
    std::function<void(const ListRow&)> on_check;
    std::function<void(const ListRow&)> on_toggle;
    std::function<void(const ListRow&)> on_activate;
    std::function<void(const ListRow&, std::size_t)> on_action;
    std::function<void(const ListRow&)> on_remove;

    void set_rows(std::vector<ListRow> rows);
    const std::vector<ListRow>& rows() const { return rows_; }
    void set_column_title(std::size_t column, std::string title);
    void set_empty_text(std::string text);
    void reveal_selected();
    void show_focus_ring(bool visible) { focus_visible_ = visible && focused_; }
    double content_height() const;

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
    bool animating() const override { return scroll_.animating(); }
    bool focusable() const override { return true; }
    bool focused() const override { return focused_; }
    void set_focused(bool focused) override;
    std::optional<double> next_wake_seconds() const override;

private:
    struct Laid {
        lm::text::TextLine text;
        std::vector<lm::text::TextLine> cells;
    };

    double header_height() const;
    std::vector<std::size_t> actions_of(const ListRow& row) const;
    bool actions_visible(std::size_t index) const;
    double actions_left(std::size_t count) const;
    std::optional<std::size_t> action_at(const ListRow& row, double content_x) const;
    double column_x(std::size_t column) const;
    double column_width(std::size_t column) const;
    double text_left(const ListRow& row) const;
    double check_left(const ListRow& row) const;
    std::optional<std::size_t> row_at(double content_y) const;
    std::optional<std::size_t> step_cursor(int direction) const;
    void move_cursor(std::size_t index);
    void click(std::size_t index, double content_x, int clicks);
    void paint_row(Canvas& canvas, const ColorScheme& scheme, std::size_t index) const;

    const Kit& kit_;
    ListOptions options_;
    std::vector<ListRow> rows_;
    std::vector<Laid> laid_;
    std::vector<lm::text::TextLine> titles_;
    std::string empty_text_;
    lm::text::TextLine empty_;
    lm::widgets::ScrollArea scroll_;
    Rect rect_{0.0, 0.0, 0.0, 0.0};
    std::optional<std::size_t> hovered_;
    std::optional<std::size_t> pressed_;
    std::optional<std::size_t> cursor_;
    int press_clicks_ = 0;
    std::vector<lm::text::TextLine> tips_;
    std::optional<std::size_t> hovered_action_;
    HoverTip tip_;
    Rect window_{0.0, 0.0, 0.0, 0.0};
    bool focused_ = false;
    bool focus_visible_ = false;
};

} // namespace cypherarx::lamina_gui
