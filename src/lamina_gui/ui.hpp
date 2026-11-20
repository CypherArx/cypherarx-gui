#pragma once

#include <string>
#include <string_view>

#include "lamina/color/mix.hpp"
#include "lamina/color/scheme.hpp"
#include "lamina/icons/icon.hpp"
#include "lamina/input/clipboard.hpp"
#include "lamina/input/events.hpp"
#include "lamina/layout/layout.hpp"
#include "lamina/render/canvas.hpp"
#include "lamina/text/layout.hpp"
#include "lamina/tokens/typography.hpp"

#include "lamina_gui/config.hpp"
#include "strings.hpp"

namespace cypherarx::lamina_gui {

namespace lm = ::lamina;
namespace str = ::cypherarx::gui::text;

using lm::color::Argb;
using lm::color::ColorScheme;
using lm::color::Role;
using lm::input::Key;
using lm::input::KeyEvent;
using lm::input::PointerAction;
using lm::input::PointerEvent;
using lm::input::TextEvent;
using lm::input::TextEventKind;
using lm::input::WheelEvent;
using lm::layout::Constraints;
using lm::layout::NodePtr;
using lm::layout::Size;
using lm::render::Canvas;
using lm::render::Point;
using lm::render::Rect;
using lm::tokens::TypeRole;

using SurfaceRole = Role ThemePreset::*;

struct Kit {
    lm::text::FontStack fonts;
    lm::icons::IconFont icons;
    lm::input::Clipboard* clipboard;
    const Config* cfg;
    const ThemePreset* theme;
};

inline Role pane_surface(const ThemePreset& theme) {
    return theme.pane_style == PaneStyle::outlined ? theme.background : theme.pane;
}

inline lm::text::TextStyle style_of(TypeRole role, bool emphasized = false) {
    return lm::tokens::text_style(lm::tokens::type_style(role, emphasized));
}

inline lm::text::TextLine line_of(const Kit& kit, std::string_view utf8, TypeRole role,
                                  bool emphasized = false) {
    return lm::text::layout_line(kit.fonts, utf8, style_of(role, emphasized));
}

inline void draw_line_in(Canvas& canvas, const Kit& kit, const lm::text::TextLine& line,
                         const Rect& box, Argb color) {
    const Point at{box.x, box.y + (box.height - line.box_height()) / 2.0};
    if (line.width <= box.width) {
        lm::text::draw_line(canvas, kit.fonts, line, at, color);
        return;
    }
    canvas.push_clip(box);
    lm::text::draw_line(canvas, kit.fonts, line, at, color);
    canvas.pop_clip();
}

inline bool inside(const Rect& r, double x, double y) {
    return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}

inline double finite_or(double v, double fallback) {
    return v < lm::layout::unbounded ? v : fallback;
}

} // namespace cypherarx::lamina_gui
