#pragma once

#include <cstddef>
#include <string>

#include "cypherarx/kdf.hpp"

#include "lamina/color/scheme.hpp"
#include "lamina/icons/symbols.hpp"
#include "lamina/tokens/button.hpp"
#include "lamina/tokens/typography.hpp"

namespace cypherarx::lamina_gui {

struct WindowConfig {
    std::string title = "CypherArx";
    int width_dp = 1280;
    int height_dp = 760;
    int min_width_dp = 960;
    int min_height_dp = 620;
    bool client_title_bar = true;
};

struct ThemeConfig {
    lamina::color::SchemeOptions scheme{.seed = 0xff3d6fb4,
                                        .dark = true,
                                        .variant = lamina::color::Variant::tonal_spot,
                                        .contrast = 0.0};
    lamina::color::Role background = lamina::color::Role::surface;
    lamina::color::Role pane = lamina::color::Role::surface_container_low;
    lamina::color::Role sheet = lamina::color::Role::surface_container_high;
    lamina::color::Role popup = lamina::color::Role::surface_container;
    lamina::color::Role scrim = lamina::color::Role::scrim;
    double scrim_opacity = 0.32;
    double disabled_opacity = 0.38;
    double dim_opacity = 0.6;
};

struct LayoutConfig {
    double margin = 12.0;
    double gap = 12.0;
    double pane_padding = 12.0;
    double pane_radius = 16.0;
    double sidebar_width = 232.0;
    double detail_width = 420.0;
    double control_gap = 8.0;
    double form_gap = 12.0;
};

struct ListConfig {
    double row_height = 36.0;
    double header_height = 32.0;
    double padding = 12.0;
    double indent = 18.0;
    double chevron_size = 20.0;
    double icon_size = 18.0;
    double icon_gap = 8.0;
    double cell_gap = 12.0;
    double row_inset_x = 4.0;
    double row_inset_y = 1.0;
    double row_radius = 8.0;
    double cursor_ring = 2.0;
    double divider = 1.0;
    lamina::tokens::TypeRole text = lamina::tokens::TypeRole::body_medium;
    lamina::tokens::TypeRole section = lamina::tokens::TypeRole::label_medium;
    lamina::tokens::TypeRole column_title = lamina::tokens::TypeRole::label_medium;
};

struct FieldConfig {
    double height = 56.0;
    double min_width = 160.0;
    double default_width = 280.0;
    double padding = 16.0;
    double radius = 4.0;
    double outline = 1.0;
    double focus_outline = 2.0;
    double label_gap = 4.0;
    double caret_width = 2.0;
    double caret_blink_seconds = 0.5;
    double dot_diameter = 8.0;
    double dot_pitch = 14.0;
    double trailing_icon = 24.0;
    double popup_max_height = 300.0;
    double popup_padding = 8.0;
    double popup_margin = 8.0;
    double popup_radius = 4.0;
    std::size_t popup_elevation = 2;
    lamina::tokens::TypeRole input = lamina::tokens::TypeRole::body_large;
    lamina::tokens::TypeRole label = lamina::tokens::TypeRole::body_small;
};

struct ChipConfig {
    double height = 32.0;
    double padding = 12.0;
    double radius = 8.0;
    double gap = 8.0;
    double outline = 1.0;
    double check_size = 18.0;
    double check_gap = 6.0;
    lamina::tokens::TypeRole label = lamina::tokens::TypeRole::label_large;
};

struct SheetConfig {
    double padding = 24.0;
    double radius = 28.0;
    double title_gap = 16.0;
    double window_margin = 24.0;
    double closed_scale = 0.8;
    double form_width = 560.0;
    double password_width = 440.0;
    double organize_width = 760.0;
    double login_width = 640.0;
    double list_height = 196.0;
    lamina::tokens::TypeRole title = lamina::tokens::TypeRole::headline_small;
};

struct TotpConfig {
    double ring = 40.0;
    double ring_thickness = 4.0;
    double gap = 12.0;
    std::size_t group = 3;
    lamina::tokens::TypeRole code = lamina::tokens::TypeRole::headline_small;
    lamina::tokens::TypeRole remaining = lamina::tokens::TypeRole::body_medium;
};

struct SpinnerConfig {
    double diameter = 40.0;
    double progress = 0.7;
};

struct IconConfig {
    lamina::icons::Symbol favorite = lamina::icons::Symbol::star;
    lamina::icons::Symbol axis_service = lamina::icons::Symbol::grid_view;
    lamina::icons::Symbol axis_persona = lamina::icons::Symbol::person;
    lamina::icons::Symbol expanded = lamina::icons::Symbol::expand_more;
    lamina::icons::Symbol collapsed = lamina::icons::Symbol::chevron_right;
    lamina::icons::Symbol dropdown = lamina::icons::Symbol::expand_more;
    lamina::icons::Symbol show = lamina::icons::Symbol::visibility;
    lamina::icons::Symbol hide = lamina::icons::Symbol::visibility_off;
    lamina::icons::Symbol checked = lamina::icons::Symbol::check;
    lamina::icons::Symbol add = lamina::icons::Symbol::add;
    lamina::icons::Symbol edit = lamina::icons::Symbol::edit;
    lamina::icons::Symbol remove = lamina::icons::Symbol::delete_;
    lamina::icons::Symbol copy = lamina::icons::Symbol::content_copy;
    lamina::icons::Symbol search = lamina::icons::Symbol::search;
};

struct ClipboardConfig {
    int clear_after_seconds = 20;
};

struct SecurityConfig {
    KdfParams (*kdf_params)() = &KdfParams::moderate;
    KdfParams (*kdf_params_selftest)() = &KdfParams::interactive;
};

struct Limits {
    std::size_t password_buffer = 256;
    std::size_t value_buffer = 4096;
};

struct Config {
    WindowConfig window;
    ThemeConfig theme;
    LayoutConfig layout;
    ListConfig list;
    FieldConfig field;
    ChipConfig chip;
    SheetConfig sheet;
    TotpConfig totp;
    SpinnerConfig spinner;
    IconConfig icons;
    ClipboardConfig clipboard;
    SecurityConfig security;
    Limits limits;
    const char* category_indent = "    ";
    const char* vault_extension = ".cax";
    const wchar_t* file_filter = L"CypherArx Vault (*.cax)\0*.cax\0All Files (*.*)\0*.*\0";
};

inline const Config& config() {
    static const Config c;
    return c;
}

} // namespace cypherarx::lamina_gui
