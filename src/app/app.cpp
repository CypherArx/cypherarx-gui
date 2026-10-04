#include "app/app.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <numbers>
#include <cstdio>
#include <string_view>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <sodium.h>

#include "cypherarx/passgen.hpp"
#include "cypherarx/totp.hpp"

#include "platform/host.hpp"
#include "strings.hpp"

namespace cypherarx::gui {

namespace {

std::uint64_t unix_now() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

std::span<const std::uint8_t> as_bytes(std::string_view s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}

std::string format(const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    std::va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    std::string out;
    if (n > 0) {
        out.resize(static_cast<std::size_t>(n) + 1);
        std::vsnprintf(out.data(), out.size(), fmt, args);
        out.resize(static_cast<std::size_t>(n));
    }
    va_end(args);
    return out;
}

template <std::size_t N>
bool combo_fixed(const char* label, int& idx, const std::array<const char*, N>& items) {
    return ImGui::Combo(label, &idx, items.data(), static_cast<int>(items.size()));
}

template <class Id>
bool combo_pairs(const char* label, int& idx, const std::vector<std::pair<Id, std::string>>& items) {
    if (items.empty()) {
        ImGui::BeginDisabled();
        if (ImGui::BeginCombo(label, "")) {
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        return false;
    }
    if (idx < 0 || idx >= static_cast<int>(items.size())) {
        idx = 0;
    }
    bool changed = false;
    if (ImGui::BeginCombo(label, items[static_cast<std::size_t>(idx)].second.c_str())) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const bool selected = i == idx;
            if (ImGui::Selectable(items[static_cast<std::size_t>(i)].second.c_str(), selected)) {
                idx = i;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

template <std::size_t N>
int index_of(const std::array<const char*, N>& items, std::string_view value) {
    for (std::size_t i = 0; i < N; ++i) {
        if (value == items[i]) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

template <class Id>
int index_of_id(const std::vector<std::pair<Id, std::string>>& items, Id id) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].first == id) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

bool ok_cancel(bool& ok) {
    ok = ImGui::Button(text::ok, ImVec2(100, 0));
    ImGui::SameLine();
    const bool cancel = ImGui::Button(text::cancel, ImVec2(100, 0)) ||
                        ImGui::IsKeyPressed(ImGuiKey_Escape);
    return ok || cancel;
}

} // namespace

const char* error_text(VaultError e) {
    switch (e) {
        case VaultError::CannotOpenFile:     return text::err_cannot_open_file;
        case VaultError::AlreadyInitialized: return text::err_already_initialized;
        case VaultError::NotInitialized:     return text::err_not_initialized;
        case VaultError::Corrupt:            return text::err_corrupt;
        case VaultError::WrongPassword:      return text::err_wrong_password;
        case VaultError::KdfFailed:          return text::err_kdf_failed;
        case VaultError::NotUnlocked:        return text::err_not_unlocked;
        case VaultError::NotFound:           return text::err_not_found;
        case VaultError::Forbidden:          return text::err_forbidden;
        default:                             return text::err_unexpected;
    }
}

bool Modal::begin() {
    if (pending) {
        ImGui::OpenPopup(id);
        pending = false;
    }
    return ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
}

void TotpView::close() {
    if (!key.empty()) {
        sodium_memzero(key.data(), key.size());
        key.clear();
    }
    field_id.reset();
}

App::App()
    : cfg_(config()),
      status_(text::no_vault),
      password_dlg_(config().limits.password_buffer),
      field_dlg_(config().limits.value_buffer) {}

void App::draw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin(text::app_name, nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    draw_toolbar();
    ImGui::Separator();

    const float status_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    const float left_w = ImGui::GetContentRegionAvail().x * cfg_.theme.left_pane_ratio;
    if (ImGui::BeginChild("entries_pane", ImVec2(left_w, -status_h))) {
        draw_entries_pane();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("fields_pane", ImVec2(0, -status_h))) {
        draw_fields_pane();
    }
    ImGui::EndChild();

    ImGui::Separator();
    draw_status();
    draw_dialogs();
    ImGui::End();

    clipboard_.tick();
}

void App::draw_toolbar() {
    if (ImGui::Button(text::new_vault)) {
        request_new_vault();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::open_vault)) {
        request_open_vault();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!vault_);
    if (ImGui::Button(text::close_vault)) {
        close_vault();
    }
    ImGui::EndDisabled();
    if (vault_) {
        ImGui::SameLine();
        ImGui::TextColored(cfg_.theme.muted_text, "%s", vault_path_.c_str());
    }
}

void App::draw_entries_pane() {
    ImGui::TextUnformatted(text::entries);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9);
    int axis = axis_ == Axis::Persona ? 1 : 0;
    const char* axes[] = {text::axis_service, text::axis_persona};
    if (ImGui::Combo(text::view, &axis, axes, 2)) {
        axis_ = axis == 1 ? Axis::Persona : Axis::Service;
        reload_entries();
    }

    ImGui::BeginDisabled(!vault_);
    if (ImGui::Button(text::add_entry)) {
        begin_add_entry();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_entry_);
    if (ImGui::Button(text::edit_entry)) {
        begin_edit_entry();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::delete_entry)) {
        request_delete_entry();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::login_methods)) {
        begin_login_methods();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(text::organize)) {
        begin_organize();
    }
    ImGui::EndDisabled();

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("entry_tree", 3, flags, ImVec2(0, 0))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(text::col_name, ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn(text::col_type, ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(axis_ == Axis::Persona ? text::col_service : text::col_persona,
                                ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();
        for (const auto id : proj_.roots) {
            draw_tree_node(id);
        }
        ImGui::EndTable();
    }
}

void App::draw_tree_node(std::int64_t id) {
    const Entry* e = proj_.find(id);
    if (!e) {
        return;
    }
    const auto kids = proj_.children.find(id);
    const bool has_kids = kids != proj_.children.end() && !kids->second.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen;
    if (!has_kids) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selected_entry_ == id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::intptr_t>(id)),
                                        flags, "%s%s", e->is_favorite ? text::favorite_marker : "",
                                        e->label.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        select_entry(id);
    }
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(e->entry_type.c_str());
    ImGui::TableNextColumn();
    const std::string cross = proj_.label_of(proj_.cross_parent(*e, axis_));
    ImGui::TextUnformatted(cross.c_str());

    if (open && has_kids) {
        for (const auto child : kids->second) {
            draw_tree_node(child);
        }
        ImGui::TreePop();
    }
}

void App::draw_fields_pane() {
    const Entry* e = selected_entry();
    if (e) {
        ImGui::Text("%s: %s", text::fields, e->label.c_str());
    } else {
        ImGui::TextUnformatted(text::fields);
    }

    ImGui::BeginDisabled(!e);
    if (ImGui::Button(text::add_field)) {
        begin_add_field();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_field_);
    if (ImGui::Button(text::edit_field)) {
        begin_edit_field();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::copy_value)) {
        copy_field_value();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::show_totp)) {
        show_totp();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::delete_field)) {
        request_delete_field();
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    const float totp_h = totp_.field_id ? ImGui::GetFrameHeightWithSpacing() * 3 : 0.0f;
    if (ImGui::BeginChild("field_list", ImVec2(0, -totp_h), ImGuiChildFlags_Borders)) {
        for (const auto& f : fields_) {
            ImGui::PushID(static_cast<int>(f.id));
            const std::string row = f.label + "  [" + f.field_type + "]";
            if (ImGui::Selectable(row.c_str(), selected_field_ == f.id)) {
                selected_field_ = f.id;
                if (totp_.field_id && totp_.field_id != f.id) {
                    totp_.close();
                }
            }
            if (!f.notes.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", f.notes.c_str());
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    draw_totp();
}

void App::draw_totp() {
    if (!totp_.field_id) {
        return;
    }
    const auto now = unix_now();
    const auto code = totp_now(totp_.key);
    const auto remaining = totp_seconds_remaining(now);
    if (code) {
        ImGui::Text(text::totp_code_fmt, code->c_str());
        ImGui::SameLine();
        ImGui::TextColored(cfg_.theme.muted_text, text::totp_remaining_fmt, remaining);
        ImGui::SameLine();
        if (ImGui::SmallButton(text::copy_value)) {
            clipboard_.copy(as_bytes(*code), cfg_.clipboard.clear_after_seconds);
            status_ = format(text::copied_fmt, cfg_.clipboard.clear_after_seconds);
        }
    } else {
        ImGui::TextColored(cfg_.theme.error_text, "%s", text::totp_failed);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(text::close)) {
        totp_.close();
    }
}

void App::draw_status() {
    ImGui::TextUnformatted(status_.c_str());
}

void App::draw_dialogs() {
    draw_password_dialog();
    draw_busy_dialog();
    draw_entry_dialog();
    draw_organize_dialog();
    draw_field_dialog();
    draw_login_methods_dialog();
    draw_confirm_dialog();
    draw_message_dialog();
}

void App::draw_password_dialog() {
    auto& d = password_dlg_;
    if (!d.modal.begin()) {
        return;
    }
    ImGui::TextUnformatted(d.mode == PasswordDialog::Mode::Create ? text::new_vault
                                                                   : text::open_vault);
    ImGui::TextColored(cfg_.theme.muted_text, "%s", d.path_text.c_str());
    if (d.focus) {
        ImGui::SetKeyboardFocusHere();
        d.focus = false;
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20);
    const bool enter = ImGui::InputText(text::master_password, d.password.data(),
                                        d.password.capacity(),
                                        ImGuiInputTextFlags_Password |
                                            ImGuiInputTextFlags_EnterReturnsTrue);
    bool ok = false;
    const bool done = ok_cancel(ok);
    if (enter || ok) {
        if (d.password.empty()) {
            message(text::password_empty);
        } else {
            start_unlock();
            ImGui::CloseCurrentPopup();
        }
    } else if (done) {
        d.password.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::draw_busy_dialog() {
    auto& job = unlock_job_;
    if (!job.modal.begin()) {
        return;
    }
    ImGui::TextUnformatted(job.mode == PasswordDialog::Mode::Create ? text::new_vault
                                                                     : text::open_vault);
    ImGui::TextColored(cfg_.theme.muted_text, "%s", job.path_text.c_str());
    draw_spinner();
    ImGui::SameLine();
    ImGui::TextUnformatted(text::unlocking);
    if (job.active() && job.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        finish_unlock(job.future.get());
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::draw_spinner() {
    const float r = cfg_.theme.spinner_radius * platform::dpi_scale();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(r * 2, r * 2);
    ImGui::Dummy(size);
    const ImVec2 center(pos.x + r, pos.y + r);
    const float t = static_cast<float>(ImGui::GetTime()) * cfg_.theme.spinner_turns_per_second;
    const float start = t * 2.0f * std::numbers::pi_v<float>;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PathArcTo(center, r - cfg_.theme.spinner_thickness, start,
                  start + 1.5f * std::numbers::pi_v<float>, 24);
    dl->PathStroke(ImGui::GetColorU32(ImGuiCol_ButtonHovered), ImDrawFlags_None,
                   cfg_.theme.spinner_thickness);
}

void App::draw_entry_dialog() {
    auto& d = entry_dlg_;
    if (!d.modal.begin()) {
        return;
    }
    ImGui::TextUnformatted(d.editing ? text::edit_entry : text::add_entry);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20);
    ImGui::InputText(text::label, &d.label);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20);
    combo_fixed(text::type, d.type_idx, text::entry_types);
    const std::string_view type = text::entry_types[static_cast<std::size_t>(d.type_idx)];
    if (type == "account") {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20);
        combo_pairs(text::service, d.service_idx, d.services);
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20);
        combo_pairs(text::persona, d.persona_idx, d.personas);
    } else if (type == "child") {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20);
        combo_pairs(text::parent_account, d.account_idx, d.accounts);
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20);
    combo_pairs(text::category, d.category_idx, d.categories);
    ImGui::Checkbox(text::favorite, &d.favorite);

    ImGui::SeparatorText(text::tags);
    for (const auto& t : tags_) {
        auto it = std::ranges::find(d.checked_tags, t.id);
        bool checked = it != d.checked_tags.end();
        ImGui::PushID(static_cast<int>(t.id));
        if (ImGui::Checkbox(t.name.c_str(), &checked)) {
            if (checked) {
                d.checked_tags.push_back(t.id);
            } else {
                d.checked_tags.erase(it);
            }
        }
        ImGui::PopID();
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
    ImGui::InputTextWithHint("##new_tag", text::new_tag, &d.new_tag);
    ImGui::SameLine();
    if (ImGui::Button(text::add) && !d.new_tag.empty()) {
        if (auto t = vault_->add_tag(d.new_tag)) {
            if (!std::ranges::contains(d.checked_tags, t->id)) {
                d.checked_tags.push_back(t->id);
            }
            d.new_tag.clear();
            reload_organization();
        } else {
            fail(text::tags, t.error());
        }
    }

    bool ok = false;
    const bool done = ok_cancel(ok);
    if (ok ? commit_entry() : done) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::draw_organize_dialog() {
    auto& d = organize_dlg_;
    if (!d.modal.begin()) {
        return;
    }
    const float w = ImGui::GetFontSize() * 22;

    ImGui::PushID("categories");
    ImGui::SeparatorText(text::categories);
    if (ImGui::BeginChild("cat_list", ImVec2(w, ImGui::GetFontSize() * 9), ImGuiChildFlags_Borders)) {
        for (const auto& [id, name] : flatten_categories(categories_, cfg_.category_indent)) {
            const Category* c = nullptr;
            for (const auto& x : categories_) {
                if (x.id == id) {
                    c = &x;
                }
            }
            std::string row = name;
            if (c && c->is_preset) {
                row += "  ";
                row += text::preset;
            }
            ImGui::PushID(static_cast<int>(id));
            if (ImGui::Selectable(row.c_str(), d.selected_category == id)) {
                d.selected_category = id;
                if (c) {
                    d.category_name = c->name;
                    d.parent_idx = index_of_id(d.parent_options, c->parent_id.value_or(0));
                }
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    ImGui::InputTextWithHint("##cat_name", text::new_category, &d.category_name);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    combo_pairs(text::parent, d.parent_idx, d.parent_options);
    if (ImGui::Button(text::add)) {
        add_category();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!d.selected_category);
    if (ImGui::Button(text::rename)) {
        rename_category();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::delete_selected)) {
        request_delete_category();
    }
    ImGui::EndDisabled();
    ImGui::PopID();

    ImGui::PushID("tags");
    ImGui::SeparatorText(text::tags);
    if (ImGui::BeginChild("tag_list", ImVec2(w, ImGui::GetFontSize() * 6), ImGuiChildFlags_Borders)) {
        for (const auto& t : tags_) {
            ImGui::PushID(static_cast<int>(t.id));
            if (ImGui::Selectable(t.name.c_str(), d.selected_tag == t.id)) {
                d.selected_tag = t.id;
                d.tag_name = t.name;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    ImGui::InputTextWithHint("##tag_name", text::new_tag, &d.tag_name);
    ImGui::SameLine();
    if (ImGui::Button(text::add)) {
        add_tag_from_organize();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!d.selected_tag);
    if (ImGui::Button(text::rename)) {
        rename_tag();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::delete_selected)) {
        request_delete_tag();
    }
    ImGui::EndDisabled();
    ImGui::PopID();

    ImGui::Separator();
    if (ImGui::Button(text::close, ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::draw_field_dialog() {
    auto& d = field_dlg_;
    if (!d.modal.begin()) {
        return;
    }
    ImGui::TextUnformatted(d.editing ? text::edit_field : text::add_field);
    const float w = ImGui::GetFontSize() * 24;
    ImGui::SetNextItemWidth(w);
    ImGui::InputText(text::label, &d.label);
    ImGui::SetNextItemWidth(w);
    combo_fixed(text::type, d.type_idx, text::field_types);
    const std::string_view type = text::field_types[static_cast<std::size_t>(d.type_idx)];
    const bool masked = type == "password" && !d.show_value;
    ImGui::SetNextItemWidth(w);
    ImGui::InputText(text::value, d.value.data(), d.value.capacity(),
                     masked ? ImGuiInputTextFlags_Password : 0);
    if (type == "password") {
        ImGui::SameLine();
        ImGui::Checkbox(text::show, &d.show_value);
        ImGui::SameLine();
        if (ImGui::Button(text::generate)) {
            if (auto pw = generate_password({})) {
                d.value.assign(pw->bytes());
            }
        }
    }
    ImGui::SetNextItemWidth(w);
    ImGui::InputText(text::notes, &d.notes);
    bool ok = false;
    const bool done = ok_cancel(ok);
    if (ok ? commit_field() : done) {
        d.value.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::draw_login_methods_dialog() {
    auto& d = login_dlg_;
    if (!d.modal.begin()) {
        return;
    }
    ImGui::Text("%s: %s", text::login_methods, d.title.c_str());
    ImGui::TextColored(cfg_.theme.muted_text, "%s", text::login_methods_hint);

    const float w = ImGui::GetFontSize() * 30;
    if (ImGui::BeginChild("lm_list", ImVec2(w, ImGui::GetFontSize() * 8), ImGuiChildFlags_Borders)) {
        for (const auto& m : d.methods) {
            std::string row = m.method_type;
            if (m.linked_entry_id) {
                row += "  ->  " + proj_.label_of(m.linked_entry_id);
            }
            if (!m.notes.empty()) {
                row += "   (" + m.notes + ")";
            }
            ImGui::PushID(static_cast<int>(m.id));
            if (ImGui::Selectable(row.c_str(), d.selected == m.id)) {
                d.selected = m.id;
                d.method_idx = index_of(text::login_method_types, m.method_type);
                d.via_idx = 0;
                for (std::size_t i = 0; i < d.via_options.size(); ++i) {
                    if (d.via_options[i].first == m.linked_entry_id) {
                        d.via_idx = static_cast<int>(i);
                    }
                }
                d.notes = m.notes;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    combo_fixed("##method", d.method_idx, text::login_method_types);
    ImGui::SameLine();
    ImGui::TextUnformatted(text::via);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
    combo_pairs("##via", d.via_idx, d.via_options);
    ImGui::SetNextItemWidth(w);
    ImGui::InputTextWithHint("##lm_notes", text::notes, &d.notes);
    if (ImGui::Button(text::add)) {
        add_login_method();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!d.selected);
    if (ImGui::Button(text::update)) {
        update_login_method();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::delete_selected)) {
        delete_login_method();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(text::close, ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::draw_confirm_dialog() {
    auto& d = confirm_dlg_;
    if (!d.modal.begin()) {
        return;
    }
    ImGui::TextUnformatted(d.text.c_str());
    if (ImGui::Button(text::yes, ImVec2(100, 0))) {
        if (d.on_yes) {
            d.on_yes();
        }
        d.on_yes = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(text::no, ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        d.on_yes = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();
    ImGui::EndPopup();
}

void App::draw_message_dialog() {
    auto& d = message_dlg_;
    if (!d.modal.begin()) {
        return;
    }
    ImGui::TextUnformatted(d.text.c_str());
    if (ImGui::Button(text::ok, ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
        ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::request_new_vault() {
    auto path = platform::pick_save_file(cfg_.file_filter, cfg_.vault_extension);
    if (!path) {
        return;
    }
    password_dlg_.mode = PasswordDialog::Mode::Create;
    password_dlg_.path_text = utf8(*path);
    password_dlg_.path = std::move(*path);
    password_dlg_.password.clear();
    password_dlg_.focus = true;
    password_dlg_.modal.open();
}

void App::request_open_vault() {
    auto path = platform::pick_open_file(cfg_.file_filter, cfg_.vault_extension);
    if (!path) {
        return;
    }
    password_dlg_.mode = PasswordDialog::Mode::Open;
    password_dlg_.path_text = utf8(*path);
    password_dlg_.path = std::move(*path);
    password_dlg_.password.clear();
    password_dlg_.focus = true;
    password_dlg_.modal.open();
}

void App::start_unlock() {
    auto& d = password_dlg_;
    auto& job = unlock_job_;
    if (job.active()) {
        return;
    }
    Secret pw(d.password.bytes());
    d.password.clear();
    job.mode = d.mode;
    job.path_text = d.path_text;
    const bool create = d.mode == PasswordDialog::Mode::Create;
    const KdfParams params = cfg_.security.kdf_params();
    job.future = std::async(std::launch::async,
                            [pw = std::move(pw), path = d.path, create, params]() mutable {
                                auto r = create ? Vault::create(path, pw.bytes(), params)
                                                : Vault::open(path, pw.bytes());
                                pw = Secret();
                                return r;
                            });
    job.modal.open();
}

void App::finish_unlock(std::expected<Vault, VaultError> result) {
    auto& job = unlock_job_;
    if (!result) {
        fail(job.mode == PasswordDialog::Mode::Create ? text::new_vault : text::open_vault,
             result.error());
        return;
    }
    close_vault();
    vault_ = std::move(*result);
    vault_path_ = job.path_text;
    reload_entries();
}

void App::close_vault() {
    totp_.close();
    clipboard_.clear_now();
    vault_.reset();
    vault_path_.clear();
    entries_.clear();
    proj_ = {};
    fields_.clear();
    selected_entry_.reset();
    selected_field_.reset();
    status_ = text::no_vault;
}

void App::reload_entries() {
    entries_.clear();
    proj_ = {};
    if (!vault_) {
        return;
    }
    auto list = vault_->list_entries();
    if (!list) {
        fail(text::entries, list.error());
        return;
    }
    entries_ = std::move(*list);
    proj_ = Projection::build(entries_, axis_);
    reload_organization();
    if (selected_entry_ && !proj_.find(*selected_entry_)) {
        selected_entry_.reset();
    }
    status_ = format(text::status_open_fmt, entries_.size(),
                     axis_ == Axis::Persona ? text::axis_persona : text::axis_service);
    reload_fields();
}

void App::reload_organization() {
    categories_.clear();
    tags_.clear();
    if (!vault_) {
        return;
    }
    if (auto c = vault_->list_categories()) {
        categories_ = std::move(*c);
    } else {
        fail(text::categories, c.error());
    }
    if (auto t = vault_->list_tags()) {
        tags_ = std::move(*t);
    } else {
        fail(text::tags, t.error());
    }
}

void App::reload_fields() {
    fields_.clear();
    if (!vault_ || !selected_entry_) {
        selected_field_.reset();
        return;
    }
    auto list = vault_->list_fields(*selected_entry_);
    if (!list) {
        fail(text::fields, list.error());
        return;
    }
    fields_ = std::move(*list);
    if (selected_field_ && !selected_field()) {
        selected_field_.reset();
    }
}

void App::select_entry(std::optional<std::int64_t> id) {
    if (selected_entry_ == id) {
        return;
    }
    selected_entry_ = id;
    selected_field_.reset();
    totp_.close();
    reload_fields();
}

const Entry* App::selected_entry() const {
    return selected_entry_ ? proj_.find(*selected_entry_) : nullptr;
}

const Field* App::selected_field() const {
    if (!selected_field_) {
        return nullptr;
    }
    for (const auto& f : fields_) {
        if (f.id == *selected_field_) {
            return &f;
        }
    }
    return nullptr;
}

void App::begin_add_entry() {
    auto& d = entry_dlg_;
    d = EntryDialog{};
    d.personas.emplace_back(0, text::none);
    d.categories.emplace_back(0, text::uncategorized);
    for (auto& c : flatten_categories(categories_, cfg_.category_indent)) {
        d.categories.push_back(std::move(c));
    }
    for (const auto& e : entries_) {
        if (e.entry_type == "service") {
            d.services.emplace_back(e.id, e.label);
        } else if (e.entry_type == "persona") {
            d.personas.emplace_back(e.id, e.label);
        } else if (e.entry_type == "account") {
            d.accounts.emplace_back(e.id, e.label + " (" + proj_.label_of(e.parent_id) + ")");
        }
    }
    d.modal.open();
}

void App::begin_edit_entry() {
    const Entry* cur = selected_entry();
    if (!cur) {
        message(text::select_entry_first);
        return;
    }
    begin_add_entry();
    auto& d = entry_dlg_;
    d.editing = *cur;
    d.label = cur->label;
    d.type_idx = index_of(text::entry_types, cur->entry_type);
    if (cur->parent_id) {
        d.service_idx = index_of_id(d.services, *cur->parent_id);
        d.account_idx = index_of_id(d.accounts, *cur->parent_id);
    }
    d.persona_idx = index_of_id(d.personas, cur->persona_id.value_or(0));
    d.category_idx = index_of_id(d.categories, cur->category_id.value_or(0));
    d.favorite = cur->is_favorite;
    if (auto tags = vault_->entry_tags(cur->id)) {
        d.checked_tags = std::move(*tags);
    }
}

bool App::commit_entry() {
    auto& d = entry_dlg_;
    if (d.label.empty()) {
        return false;
    }
    Entry e = d.editing.value_or(Entry{});
    e.label = d.label;
    e.entry_type = text::entry_types[static_cast<std::size_t>(d.type_idx)];
    e.parent_id.reset();
    e.persona_id.reset();
    e.is_favorite = d.favorite;
    e.category_id.reset();
    if (const auto cid = d.categories[static_cast<std::size_t>(d.category_idx)].first; cid != 0) {
        e.category_id = cid;
    }
    if (!d.new_tag.empty()) {
        if (auto t = vault_->add_tag(d.new_tag)) {
            if (!std::ranges::contains(d.checked_tags, t->id)) {
                d.checked_tags.push_back(t->id);
            }
            d.new_tag.clear();
        }
    }
    if (e.entry_type == "account") {
        if (d.services.empty()) {
            message(text::need_service_first);
            return false;
        }
        e.parent_id = d.services[static_cast<std::size_t>(d.service_idx)].first;
        const auto pid = d.personas[static_cast<std::size_t>(d.persona_idx)].first;
        if (pid != 0) {
            e.persona_id = pid;
        }
    } else if (e.entry_type == "child") {
        if (d.accounts.empty()) {
            message(text::need_account_first);
            return false;
        }
        e.parent_id = d.accounts[static_cast<std::size_t>(d.account_idx)].first;
    }

    if (d.editing) {
        if (auto r = vault_->update_entry(e); !r) {
            fail(text::edit_entry, r.error());
            return false;
        }
    } else {
        auto r = vault_->add_entry(e);
        if (!r) {
            fail(text::add_entry, r.error());
            return false;
        }
        e.id = r->id;
        selected_entry_ = r->id;
    }
    if (auto r = vault_->set_entry_tags(e.id, d.checked_tags); !r) {
        fail(text::tags, r.error());
    }
    reload_entries();
    return true;
}

void App::begin_organize() {
    organize_dlg_ = OrganizeDialog{};
    refresh_organize_options();
    organize_dlg_.modal.open();
}

void App::refresh_organize_options() {
    auto& d = organize_dlg_;
    d.parent_options.clear();
    d.parent_options.emplace_back(0, text::none);
    for (auto& c : flatten_categories(categories_, cfg_.category_indent)) {
        d.parent_options.push_back(std::move(c));
    }
    if (d.parent_idx >= static_cast<int>(d.parent_options.size())) {
        d.parent_idx = 0;
    }
}

void App::add_category() {
    auto& d = organize_dlg_;
    if (d.category_name.empty()) {
        return;
    }
    std::optional<std::int64_t> parent;
    if (const auto pid = d.parent_options[static_cast<std::size_t>(d.parent_idx)].first; pid != 0) {
        parent = pid;
    }
    auto r = vault_->add_category(d.category_name, parent);
    if (!r) {
        fail(text::categories, r.error());
        return;
    }
    d.category_name.clear();
    d.selected_category = r->id;
    reload_organization();
    refresh_organize_options();
}

void App::rename_category() {
    auto& d = organize_dlg_;
    if (!d.selected_category || d.category_name.empty()) {
        return;
    }
    std::optional<std::int64_t> parent;
    if (const auto pid = d.parent_options[static_cast<std::size_t>(d.parent_idx)].first; pid != 0) {
        parent = pid;
    }
    if (auto r = vault_->update_category(*d.selected_category, d.category_name, parent); !r) {
        message(r.error() == VaultError::Forbidden ? text::preset_read_only
                                                    : error_text(r.error()));
        return;
    }
    reload_organization();
    refresh_organize_options();
}

void App::request_delete_category() {
    auto& d = organize_dlg_;
    if (!d.selected_category) {
        return;
    }
    confirm_dlg_.text = text::confirm_delete_category;
    confirm_dlg_.on_yes = [this] {
        auto& dlg = organize_dlg_;
        if (auto r = vault_->delete_category(*dlg.selected_category); !r) {
            message(r.error() == VaultError::Forbidden ? text::preset_read_only
                                                        : error_text(r.error()));
            return;
        }
        dlg.selected_category.reset();
        dlg.category_name.clear();
        reload_entries();
        refresh_organize_options();
    };
    confirm_dlg_.modal.open();
}

void App::add_tag_from_organize() {
    auto& d = organize_dlg_;
    if (d.tag_name.empty()) {
        return;
    }
    auto r = vault_->add_tag(d.tag_name);
    if (!r) {
        fail(text::tags, r.error());
        return;
    }
    d.tag_name.clear();
    d.selected_tag = r->id;
    reload_organization();
}

void App::rename_tag() {
    auto& d = organize_dlg_;
    if (!d.selected_tag || d.tag_name.empty()) {
        return;
    }
    if (auto r = vault_->rename_tag(*d.selected_tag, d.tag_name); !r) {
        fail(text::tags, r.error());
        return;
    }
    reload_organization();
}

void App::request_delete_tag() {
    auto& d = organize_dlg_;
    if (!d.selected_tag) {
        return;
    }
    confirm_dlg_.text = text::confirm_delete_tag;
    confirm_dlg_.on_yes = [this] {
        auto& dlg = organize_dlg_;
        if (auto r = vault_->delete_tag(*dlg.selected_tag); !r) {
            fail(text::tags, r.error());
            return;
        }
        dlg.selected_tag.reset();
        dlg.tag_name.clear();
        reload_organization();
    };
    confirm_dlg_.modal.open();
}

void App::request_delete_entry() {
    if (!selected_entry_) {
        return;
    }
    confirm_dlg_.text = text::confirm_delete_entry;
    confirm_dlg_.on_yes = [this] {
        if (auto r = vault_->delete_entry(*selected_entry_); !r) {
            fail(text::delete_entry, r.error());
            return;
        }
        select_entry(std::nullopt);
        reload_entries();
    };
    confirm_dlg_.modal.open();
}

void App::begin_add_field() {
    if (!selected_entry_) {
        message(text::select_entry_first);
        return;
    }
    auto& d = field_dlg_;
    d.editing.reset();
    d.label.clear();
    d.type_idx = 0;
    d.value.clear();
    d.show_value = false;
    d.notes.clear();
    d.modal.open();
}

void App::begin_edit_field() {
    const Field* cur = selected_field();
    if (!cur) {
        message(text::select_field_first);
        return;
    }
    auto value = vault_->field_value(cur->id);
    if (!value) {
        fail(text::edit_field, value.error());
        return;
    }
    begin_add_field();
    auto& d = field_dlg_;
    d.editing = cur->id;
    d.label = cur->label;
    d.type_idx = index_of(text::field_types, cur->field_type);
    d.value.assign(value->bytes());
    d.notes = cur->notes;
}

bool App::commit_field() {
    auto& d = field_dlg_;
    if (d.label.empty() || !selected_entry_) {
        return false;
    }
    const std::string type = text::field_types[static_cast<std::size_t>(d.type_idx)];
    if (d.editing) {
        if (auto r = vault_->update_field(*d.editing, d.label, type, d.value.bytes(), d.notes); !r) {
            fail(text::edit_field, r.error());
            return false;
        }
    } else {
        auto r = vault_->add_field(*selected_entry_, d.label, type, d.value.bytes(), d.notes);
        if (!r) {
            fail(text::add_field, r.error());
            return false;
        }
        selected_field_ = r->id;
    }
    if (d.editing && totp_.field_id == d.editing) {
        totp_.close();
    }
    reload_fields();
    return true;
}

void App::request_delete_field() {
    if (!selected_field_) {
        return;
    }
    confirm_dlg_.text = text::confirm_delete_field;
    confirm_dlg_.on_yes = [this] {
        if (auto r = vault_->delete_field(*selected_field_); !r) {
            fail(text::delete_field, r.error());
            return;
        }
        if (totp_.field_id == selected_field_) {
            totp_.close();
        }
        selected_field_.reset();
        reload_fields();
    };
    confirm_dlg_.modal.open();
}

void App::copy_field_value() {
    if (!selected_field_) {
        return;
    }
    auto value = vault_->field_value(*selected_field_);
    if (!value) {
        fail(text::copy_value, value.error());
        return;
    }
    clipboard_.copy(value->bytes(), cfg_.clipboard.clear_after_seconds);
    status_ = format(text::copied_fmt, cfg_.clipboard.clear_after_seconds);
}

void App::show_totp() {
    const Field* f = selected_field();
    if (!f) {
        return;
    }
    if (f->field_type != "totp") {
        message(text::select_totp_field);
        return;
    }
    auto secret = vault_->field_value(f->id);
    if (!secret) {
        fail(text::show_totp, secret.error());
        return;
    }
    const std::string_view base32(reinterpret_cast<const char*>(secret->data()), secret->size());
    auto key = base32_decode(base32);
    if (!key) {
        message(text::totp_invalid_base32);
        return;
    }
    totp_.close();
    totp_.key = std::move(*key);
    totp_.field_id = f->id;
}

void App::begin_login_methods() {
    const Entry* self = selected_entry();
    if (!self) {
        message(text::select_entry_first);
        return;
    }
    if (self->entry_type != "account") {
        message(text::login_methods_accounts_only);
        return;
    }
    auto& d = login_dlg_;
    d = LoginMethodsDialog{};
    d.entry_id = self->id;
    d.title = self->label;
    d.via_options.emplace_back(std::nullopt, text::direct);
    for (const auto& e : entries_) {
        if (e.entry_type != "account" || e.id == self->id) {
            continue;
        }
        if (self->parent_id && e.parent_id && *e.parent_id == *self->parent_id) {
            continue;
        }
        d.via_options.emplace_back(e.id, e.label + " (" + proj_.label_of(e.parent_id) + ")");
    }
    reload_login_methods();
    d.modal.open();
}

void App::reload_login_methods() {
    auto& d = login_dlg_;
    d.methods.clear();
    auto list = vault_->list_login_methods(d.entry_id);
    if (!list) {
        fail(text::login_methods, list.error());
        return;
    }
    d.methods = std::move(*list);
    if (d.selected) {
        bool found = false;
        for (const auto& m : d.methods) {
            found = found || m.id == *d.selected;
        }
        if (!found) {
            d.selected.reset();
        }
    }
}

void App::add_login_method() {
    auto& d = login_dlg_;
    const std::string method = text::login_method_types[static_cast<std::size_t>(d.method_idx)];
    const auto via = d.via_options[static_cast<std::size_t>(d.via_idx)].first;
    auto r = vault_->add_login_method(d.entry_id, method, via, d.notes);
    if (!r) {
        fail(text::add, r.error());
        return;
    }
    d.notes.clear();
    d.selected = r->id;
    reload_login_methods();
}

void App::update_login_method() {
    auto& d = login_dlg_;
    if (!d.selected) {
        return;
    }
    const std::string method = text::login_method_types[static_cast<std::size_t>(d.method_idx)];
    const auto via = d.via_options[static_cast<std::size_t>(d.via_idx)].first;
    if (auto r = vault_->update_login_method(*d.selected, method, via, d.notes); !r) {
        fail(text::update, r.error());
        return;
    }
    reload_login_methods();
}

void App::delete_login_method() {
    auto& d = login_dlg_;
    if (!d.selected) {
        return;
    }
    if (auto r = vault_->delete_login_method(*d.selected); !r) {
        fail(text::delete_selected, r.error());
        return;
    }
    d.selected.reset();
    reload_login_methods();
}

std::string App::utf8(const std::filesystem::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

void App::fail(const std::string& what, VaultError e) {
    message(what + ": " + error_text(e));
}

void App::message(const std::string& t) {
    message_dlg_.text = t;
    message_dlg_.modal.open();
}

} // namespace cypherarx::gui
