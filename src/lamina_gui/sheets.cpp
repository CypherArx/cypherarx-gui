#include "lamina_gui/sheets.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace cypherarx::lamina_gui {

namespace {

using lm::tokens::ButtonStyle;

NodePtr hrow(const Kit& kit, std::vector<NodePtr> children) {
    auto r = lm::layout::row(std::move(children));
    r->spacing(kit.cfg->layout.control_gap).cross_align(lm::layout::CrossAlign::center);
    return r;
}

NodePtr vcol(const Kit& kit, std::vector<NodePtr> children) {
    auto c = lm::layout::column(std::move(children));
    c->spacing(kit.cfg->layout.form_gap).cross_align(lm::layout::CrossAlign::stretch).fill_cross(true);
    return c;
}

NodePtr grow(NodePtr node, double weight = 1.0) {
    node->weight = weight;
    return node;
}

NodePtr whole(NodePtr node) {
    node->weight = 0.0;
    return node;
}

constexpr std::array<const char*, 4> add_titles = {str::add_service, str::add_persona, str::add_account,
                                                   str::add_child};

std::vector<std::string> labels(const gui::IdLabels& items) {
    std::vector<std::string> out;
    out.reserve(items.size());
    for (const auto& [id, label] : items) {
        out.push_back(label);
    }
    return out;
}

template <std::size_t N>
std::vector<std::string> labels(const std::array<const char*, N>& items) {
    return {items.begin(), items.end()};
}

std::size_t index_of(const gui::IdLabels& items, std::int64_t id) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].first == id) {
            return i;
        }
    }
    return 0;
}

template <std::size_t N>
std::size_t index_of(const std::array<const char*, N>& items, std::string_view value) {
    for (std::size_t i = 0; i < N; ++i) {
        if (value == items[i]) {
            return i;
        }
    }
    return 0;
}

std::int64_t id_at(const gui::IdLabels& items, std::size_t index) {
    return index < items.size() ? items[index].first : 0;
}

LabelSpec muted(std::string text, bool wrap = false) {
    return {std::move(text), TypeRole::body_medium, Role::on_surface_variant, wrap};
}

LabelSpec heading(std::string text) {
    return {std::move(text), TypeRole::title_small, Role::on_surface_variant, false};
}

} // namespace

PasswordSheet::PasswordSheet(const Kit& kit, Host& host)
    : Sheet(kit, str::open_vault, kit.cfg->sheet.password_width),
      host_(host),
      path_(panel_.add<LabelC>(kit, muted(""))),
      password_(panel_.add<SecretField>(kit, str::master_password, kit.cfg->limits.password_buffer,
                                        &ThemePreset::sheet)),
      cancel_(panel_.add<ButtonC>(kit, ButtonSpec{str::cancel, ButtonStyle::text})),
      ok_(panel_.add<ButtonC>(kit, ButtonSpec{str::ok, ButtonStyle::filled})) {
    password_.on_submit = [this] { submit(); };
    ok_.on_click = [this] { submit(); };
    on_submit = [this] { submit(); };
    on_cancel = [this] {
        password_.clear();
        close();
    };
    cancel_.on_click = on_cancel;
}

NodePtr PasswordSheet::body() {
    return vcol(kit_, {path_.node(), password_.node(),
                       hrow(kit_, {lm::layout::flexible_space(), cancel_.node(), ok_.node()})});
}

void PasswordSheet::open(gui::UnlockMode mode, std::filesystem::path file) {
    mode_ = mode;
    file_ = std::move(file);
    set_title(mode == gui::UnlockMode::Create ? str::new_vault : str::open_vault);
    path_.set_text(gui::path_utf8(file_));
    password_.clear();
    password_.set_masked(true);
    show();
    panel_.focus(password_);
    host_.present(*this);
}

void PasswordSheet::submit() {
    if (password_.empty()) {
        host_.message(str::password_empty);
        return;
    }
    host_.session().start_unlock(mode_, file_, password_.bytes());
    password_.clear();
    close();
    if (on_started) {
        on_started();
    }
}

BusySheet::BusySheet(const Kit& kit)
    : Sheet(kit, str::open_vault, kit.cfg->sheet.password_width),
      path_(panel_.add<LabelC>(kit, muted(""))),
      spinner_(panel_.add<SpinnerC>(kit)),
      note_(panel_.add<LabelC>(kit, LabelSpec{str::unlocking, TypeRole::body_large})) {}

NodePtr BusySheet::body() {
    return vcol(kit_, {path_.node(), hrow(kit_, {spinner_.node(), note_.node()})});
}

void BusySheet::open(const std::string& title, const std::string& path) {
    set_title(title);
    path_.set_text(path);
    show();
}

EntrySheet::EntrySheet(const Kit& kit, Host& host)
    : Sheet(kit, str::add_entry, kit.cfg->sheet.form_width),
      host_(host),
      label_(panel_.add<TextFieldC>(kit, str::label)),
      name_(panel_.add<TextFieldC>(kit, str::name_optional)),
      name_hint_(panel_.add<LabelC>(kit, muted(str::account_name_hint, true))),
      type_(panel_.add<SelectField>(kit, str::type, &ThemePreset::sheet)),
      category_(panel_.add<SelectField>(kit, str::category, &ThemePreset::sheet)),
      service_(panel_.add<SelectField>(kit, str::service, &ThemePreset::sheet)),
      persona_(panel_.add<SelectField>(kit, str::persona, &ThemePreset::sheet)),
      account_(panel_.add<SelectField>(kit, str::parent_account, &ThemePreset::sheet)),
      favorite_(panel_.add<CheckC>(kit, str::favorite)),
      tags_title_(panel_.add<LabelC>(kit, heading(str::tags))),
      tags_(panel_.add<ChipsC>(kit)),
      new_tag_(panel_.add<TextFieldC>(kit, str::new_tag)),
      add_tag_(panel_.add<ButtonC>(kit, ButtonSpec{str::add, ButtonStyle::tonal})),
      cancel_(panel_.add<ButtonC>(kit, ButtonSpec{str::cancel, ButtonStyle::text})),
      ok_(panel_.add<ButtonC>(kit, ButtonSpec{str::ok, ButtonStyle::filled})) {
    type_.on_change = [this](std::size_t) { apply_type(); };
    tags_.on_toggle = [this](std::int64_t id, bool on) {
        std::erase(draft_.tags, id);
        if (on) {
            draft_.tags.push_back(id);
        }
    };
    new_tag_.on_submit = [this] { add_tag(); };
    add_tag_.on_click = [this] { add_tag(); };
    ok_.on_click = [this] { submit(); };
    on_submit = [this] { submit(); };
    on_cancel = [this] { close(); };
    cancel_.on_click = on_cancel;
}

std::string EntrySheet::type() const {
    return str::entry_types[std::min(type_.index(), str::entry_types.size() - 1)];
}

NodePtr EntrySheet::body() {
    std::vector<NodePtr> rows;
    if (name_.visible()) {
        rows.push_back(name_.node());
        rows.push_back(name_hint_.node());
    } else {
        rows.push_back(label_.node());
    }
    if (type_.visible() && category_.visible()) {
        rows.push_back(hrow(kit_, {grow(type_.node()), grow(category_.node())}));
    } else if (type_.visible()) {
        rows.push_back(whole(type_.node()));
    } else if (category_.visible()) {
        rows.push_back(whole(category_.node()));
    }
    if (service_.visible()) {
        rows.push_back(hrow(kit_, {grow(service_.node()), grow(persona_.node())}));
    }
    if (account_.visible()) {
        rows.push_back(account_.node());
    }
    rows.push_back(favorite_.node());
    rows.push_back(tags_title_.node());
    if (tags_.visible()) {
        rows.push_back(tags_.node());
    }
    rows.push_back(hrow(kit_, {grow(new_tag_.node()), add_tag_.node()}));
    rows.push_back(hrow(kit_, {lm::layout::flexible_space(), cancel_.node(), ok_.node()}));
    return vcol(kit_, std::move(rows));
}

TextFieldC& EntrySheet::name_field() {
    return name_.visible() ? name_ : label_;
}

void EntrySheet::apply_type() {
    const std::string t = type();
    const gui::EntryTraits kind = gui::Session::traits(t);
    const std::string typed = name_field().text();
    name_.set_visible(kind.name_optional);
    name_hint_.set_visible(kind.name_optional);
    label_.set_visible(!kind.name_optional);
    name_field().set_text(typed);
    category_.set_visible(kind.has_category);
    service_.set_visible(t == "account");
    persona_.set_visible(t == "account");
    account_.set_visible(t == "child");
    rebuild();
}

void EntrySheet::refresh_tags() {
    std::vector<Chip> chips;
    for (const Tag& tag : host_.session().tags()) {
        chips.push_back({tag.id, tag.name, std::ranges::contains(draft_.tags, tag.id)});
    }
    tags_.set_visible(!chips.empty());
    tags_.set_chips(std::move(chips));
    rebuild();
}

void EntrySheet::add_tag() {
    const std::string name = new_tag_.text();
    if (name.empty()) {
        return;
    }
    const auto added = host_.session().add_tag(name);
    if (!added) {
        host_.message(added.error().message);
        return;
    }
    if (!std::ranges::contains(draft_.tags, *added)) {
        draft_.tags.push_back(*added);
    }
    new_tag_.set_text("");
    refresh_tags();
}

void EntrySheet::open_new(const std::string& type) {
    const gui::EntryChoices choices = host_.session().entry_choices();
    if (type == "account" && choices.services.empty()) {
        host_.message(str::need_service_first);
        return;
    }
    if (type == "child" && choices.accounts.empty()) {
        host_.message(str::need_account_first);
        return;
    }
    gui::EntryDraft draft = host_.session().new_entry_draft();
    draft.type = type;
    open(std::move(draft));
}

void EntrySheet::open_selected() {
    auto draft = host_.session().selected_entry_draft();
    if (!draft) {
        host_.message(draft.error().message);
        return;
    }
    open(std::move(*draft));
}

void EntrySheet::open(gui::EntryDraft draft) {
    draft_ = std::move(draft);
    choices_ = host_.session().entry_choices();
    set_title(draft_.editing ? str::edit_entry
                             : add_titles[index_of(str::entry_types, draft_.type)]);
    type_.set_visible(draft_.editing.has_value());
    label_.set_text(draft_.label);
    name_.set_text(draft_.label);
    type_.set_items(labels(str::entry_types), index_of(str::entry_types, draft_.type));
    category_.set_items(labels(choices_.categories), index_of(choices_.categories, draft_.category));
    service_.set_items(labels(choices_.services), index_of(choices_.services, draft_.service));
    persona_.set_items(labels(choices_.personas), index_of(choices_.personas, draft_.persona));
    account_.set_items(labels(choices_.accounts), index_of(choices_.accounts, draft_.account));
    favorite_.set_checked(draft_.favorite);
    new_tag_.set_text("");
    apply_type();
    refresh_tags();
    show();
    panel_.focus(name_field());
    host_.present(*this);
}

void EntrySheet::submit() {
    draft_.label = name_field().text();
    draft_.type = type();
    if (category_.visible()) {
        draft_.category = id_at(choices_.categories, category_.index());
    }
    draft_.service = id_at(choices_.services, service_.index());
    draft_.persona = id_at(choices_.personas, persona_.index());
    draft_.account = id_at(choices_.accounts, account_.index());
    draft_.favorite = favorite_.checked();
    draft_.new_tag = new_tag_.text();
    if (host_.check(host_.session().commit_entry(draft_))) {
        close();
    }
}

FieldSheet::FieldSheet(const Kit& kit, Host& host)
    : Sheet(kit, str::add_field, kit.cfg->sheet.form_width),
      host_(host),
      label_(panel_.add<TextFieldC>(kit, str::label)),
      type_(panel_.add<SelectField>(kit, str::type, &ThemePreset::sheet)),
      value_(panel_.add<SecretField>(kit, str::value, kit.cfg->limits.value_buffer,
                                     &ThemePreset::sheet)),
      show_(panel_.add<IconButtonC>(kit, kit.cfg->icons.show, true, str::show)),
      generate_(panel_.add<ButtonC>(kit, ButtonSpec{str::generate, ButtonStyle::tonal})),
      notes_(panel_.add<TextFieldC>(kit, str::notes)),
      cancel_(panel_.add<ButtonC>(kit, ButtonSpec{str::cancel, ButtonStyle::text})),
      ok_(panel_.add<ButtonC>(kit, ButtonSpec{str::ok, ButtonStyle::filled})) {
    type_.set_items(labels(str::field_types));
    type_.on_change = [this](std::size_t) { apply_type(); };
    show_.on_change = [this](bool) { apply_type(); };
    generate_.on_click = [this] {
        if (auto pw = gui::Session::generate_password()) {
            value_.assign(pw->bytes());
        } else {
            host_.message(pw.error().message);
        }
    };
    ok_.on_click = [this] { submit(); };
    on_submit = [this] { submit(); };
    on_cancel = [this] { dismiss(); };
    cancel_.on_click = on_cancel;
}

std::string FieldSheet::type() const {
    return str::field_types[std::min(type_.index(), str::field_types.size() - 1)];
}

NodePtr FieldSheet::body() {
    std::vector<NodePtr> value{grow(value_.node())};
    if (show_.visible()) {
        value.push_back(show_.node());
        value.push_back(generate_.node());
    }
    return vcol(kit_, {label_.node(), type_.node(), hrow(kit_, std::move(value)), notes_.node(),
                       hrow(kit_, {lm::layout::flexible_space(), cancel_.node(), ok_.node()})});
}

void FieldSheet::apply_type() {
    const bool password = type() == "password";
    show_.set_visible(password);
    generate_.set_visible(password);
    value_.set_masked(password && !show_.checked());
    rebuild();
}

void FieldSheet::open_new() {
    if (!host_.session().selected_entry()) {
        host_.message(str::select_entry_first);
        return;
    }
    value_.clear();
    open(host_.session().new_field_draft());
}

void FieldSheet::open_selected() {
    auto draft = host_.session().selected_field_draft();
    if (!draft) {
        host_.message(draft.error().message);
        return;
    }
    auto value = host_.session().field_value(*draft->editing);
    if (!value) {
        host_.message(value.error().message);
        return;
    }
    value_.assign(value->bytes());
    open(std::move(*draft));
}

void FieldSheet::open(gui::FieldDraft draft) {
    draft_ = std::move(draft);
    set_title(draft_.editing ? str::edit_field : str::add_field);
    label_.set_text(draft_.label);
    type_.set_index(index_of(str::field_types, draft_.type));
    notes_.set_text(draft_.notes);
    show_.set_checked(false);
    apply_type();
    show();
    panel_.focus(label_);
    host_.present(*this);
}

void FieldSheet::submit() {
    draft_.label = label_.text();
    draft_.type = type();
    draft_.notes = notes_.text();
    if (host_.check(host_.session().commit_field(draft_, value_.bytes()))) {
        dismiss();
    }
}

void FieldSheet::dismiss() {
    value_.clear();
    close();
}

OrganizeSheet::OrganizeSheet(const Kit& kit, Host& host)
    : Sheet(kit, str::organize, kit.cfg->sheet.organize_width),
      host_(host),
      categories_title_(panel_.add<LabelC>(kit, heading(str::categories))),
      categories_(panel_.add<ListView>(
          kit, ListOptions{.columns = {{}, {3.0, 1.0}}, .height = kit.cfg->sheet.list_height})),
      category_name_(panel_.add<TextFieldC>(kit, str::new_category)),
      category_parent_(panel_.add<SelectField>(kit, str::parent, &ThemePreset::sheet)),
      category_add_(panel_.add<ButtonC>(kit, ButtonSpec{str::add, ButtonStyle::tonal})),
      category_rename_(panel_.add<ButtonC>(kit, ButtonSpec{str::rename, ButtonStyle::tonal})),
      category_delete_(panel_.add<ButtonC>(kit, ButtonSpec{str::delete_selected, ButtonStyle::outlined})),
      tags_title_(panel_.add<LabelC>(kit, heading(str::tags))),
      tags_(panel_.add<ListView>(kit, ListOptions{.height = kit.cfg->sheet.list_height})),
      tag_name_(panel_.add<TextFieldC>(kit, str::new_tag)),
      tag_add_(panel_.add<ButtonC>(kit, ButtonSpec{str::add, ButtonStyle::tonal})),
      tag_rename_(panel_.add<ButtonC>(kit, ButtonSpec{str::rename, ButtonStyle::tonal})),
      tag_delete_(panel_.add<ButtonC>(kit, ButtonSpec{str::delete_selected, ButtonStyle::outlined})),
      close_(panel_.add<ButtonC>(kit, ButtonSpec{str::close, ButtonStyle::filled})) {
    categories_.on_select = [this](const ListRow& row) {
        selected_category_ = row.id;
        if (const Category* c = host_.session().find_category(row.id)) {
            category_name_.set_text(c->name);
            category_parent_.set_index(index_of(parents_, c->parent_id.value_or(0)));
        }
        refresh();
    };
    tags_.on_select = [this](const ListRow& row) {
        selected_tag_ = row.id;
        for (const Tag& t : host_.session().tags()) {
            if (t.id == row.id) {
                tag_name_.set_text(t.name);
            }
        }
        refresh();
    };
    category_add_.on_click = [this] {
        const auto added = host_.session().add_category(category_name_.text(), parent());
        if (!added) {
            host_.message(added.error().message);
            return;
        }
        selected_category_ = *added;
        category_name_.set_text("");
        refresh();
    };
    category_rename_.on_click = [this] {
        if (selected_category_ &&
            host_.check(host_.session().update_category(*selected_category_, category_name_.text(),
                                                        parent()))) {
            refresh();
        }
    };
    category_delete_.on_click = [this] {
        if (!selected_category_) {
            return;
        }
        host_.confirm(str::confirm_delete_category, [this] {
            if (!selected_category_ ||
                !host_.check(host_.session().delete_category(*selected_category_))) {
                return;
            }
            selected_category_.reset();
            category_name_.set_text("");
            refresh();
        });
    };
    tag_add_.on_click = [this] {
        const auto added = host_.session().add_tag(tag_name_.text());
        if (!added) {
            host_.message(added.error().message);
            return;
        }
        selected_tag_ = *added;
        tag_name_.set_text("");
        refresh();
    };
    tag_rename_.on_click = [this] {
        if (selected_tag_ &&
            host_.check(host_.session().rename_tag(*selected_tag_, tag_name_.text()))) {
            refresh();
        }
    };
    tag_delete_.on_click = [this] {
        if (!selected_tag_) {
            return;
        }
        host_.confirm(str::confirm_delete_tag, [this] {
            if (!selected_tag_ || !host_.check(host_.session().delete_tag(*selected_tag_))) {
                return;
            }
            selected_tag_.reset();
            tag_name_.set_text("");
            refresh();
        });
    };
    on_cancel = [this] { close(); };
    close_.on_click = on_cancel;
}

std::optional<std::int64_t> OrganizeSheet::parent() const {
    const std::int64_t id = id_at(parents_, category_parent_.index());
    return id == 0 ? std::nullopt : std::optional<std::int64_t>(id);
}

NodePtr OrganizeSheet::body() {
    auto left = vcol(kit_, {categories_title_.node(), categories_.node(), category_name_.node(),
                            category_parent_.node(),
                            hrow(kit_, {category_add_.node(), category_rename_.node(),
                                        category_delete_.node()})});
    auto right = vcol(kit_, {tags_title_.node(), tags_.node(), tag_name_.node(),
                             hrow(kit_, {tag_add_.node(), tag_rename_.node(), tag_delete_.node()})});
    auto columns = lm::layout::row({grow(left), grow(right)});
    columns->spacing(kit_.cfg->sheet.padding).cross_align(lm::layout::CrossAlign::start);
    return vcol(kit_, {columns, hrow(kit_, {lm::layout::flexible_space(), close_.node()})});
}

void OrganizeSheet::refresh() {
    gui::Session& session = host_.session();
    if (selected_category_ && !session.find_category(*selected_category_)) {
        selected_category_.reset();
    }
    const std::int64_t kept_parent = id_at(parents_, category_parent_.index());
    parents_.clear();
    parents_.emplace_back(0, str::none);
    for (auto& c : session.flat_categories()) {
        parents_.push_back(std::move(c));
    }
    category_parent_.set_items(labels(parents_), index_of(parents_, kept_parent));

    std::vector<ListRow> category_rows;
    for (const auto& [id, name] : session.flat_categories()) {
        ListRow row;
        row.id = id;
        row.text = name;
        const Category* c = session.find_category(id);
        row.cells = {c && c->is_preset ? str::preset : ""};
        row.selected = selected_category_ == id;
        category_rows.push_back(std::move(row));
    }
    categories_.set_rows(std::move(category_rows));

    bool tag_alive = false;
    std::vector<ListRow> tag_rows;
    for (const Tag& t : session.tags()) {
        ListRow row;
        row.id = t.id;
        row.text = t.name;
        row.selected = selected_tag_ == t.id;
        tag_alive = tag_alive || row.selected;
        tag_rows.push_back(std::move(row));
    }
    if (!tag_alive) {
        selected_tag_.reset();
    }
    tags_.set_rows(std::move(tag_rows));

    category_rename_.set_enabled(selected_category_.has_value());
    category_delete_.set_enabled(selected_category_.has_value());
    tag_rename_.set_enabled(selected_tag_.has_value());
    tag_delete_.set_enabled(selected_tag_.has_value());
}

void OrganizeSheet::open() {
    selected_category_.reset();
    selected_tag_.reset();
    category_name_.set_text("");
    tag_name_.set_text("");
    parents_.clear();
    refresh();
    show();
    host_.present(*this);
}

LoginSheet::LoginSheet(const Kit& kit, Host& host)
    : Sheet(kit, str::login_methods, kit.cfg->sheet.login_width),
      host_(host),
      hint_(panel_.add<LabelC>(kit, muted(str::login_methods_hint, true))),
      list_(panel_.add<ListView>(
          kit, ListOptions{.columns = {{str::method, str::via, str::notes}, {1.0, 2.0, 2.0}},
                           .height = kit.cfg->sheet.list_height})),
      method_(panel_.add<SelectField>(kit, str::method, &ThemePreset::sheet)),
      via_(panel_.add<SelectField>(kit, str::via, &ThemePreset::sheet)),
      notes_(panel_.add<TextFieldC>(kit, str::notes)),
      add_(panel_.add<ButtonC>(kit, ButtonSpec{str::add, ButtonStyle::tonal})),
      update_(panel_.add<ButtonC>(kit, ButtonSpec{str::update, ButtonStyle::tonal})),
      delete_(panel_.add<ButtonC>(kit, ButtonSpec{str::delete_selected, ButtonStyle::outlined})),
      close_(panel_.add<ButtonC>(kit, ButtonSpec{str::close, ButtonStyle::filled})) {
    method_.set_items(labels(str::login_method_types));
    list_.on_select = [this](const ListRow& row) {
        selected_ = row.id;
        for (const LoginMethod& m : methods_) {
            if (m.id != row.id) {
                continue;
            }
            method_.set_index(index_of(str::login_method_types, m.method_type));
            std::size_t via_index = 0;
            for (std::size_t i = 0; i < choices_.via.size(); ++i) {
                if (choices_.via[i].first == m.linked_entry_id) {
                    via_index = i;
                }
            }
            via_.set_index(via_index);
            notes_.set_text(m.notes);
        }
        refresh();
    };
    add_.on_click = [this] {
        const auto added =
            host_.session().add_login_method(choices_.entry_id, method(), via(), notes_.text());
        if (!added) {
            host_.message(added.error().message);
            return;
        }
        notes_.set_text("");
        selected_ = *added;
        reload();
    };
    update_.on_click = [this] {
        if (selected_ && host_.check(host_.session().update_login_method(*selected_, method(), via(),
                                                                         notes_.text()))) {
            reload();
        }
    };
    delete_.on_click = [this] {
        if (selected_ && host_.check(host_.session().delete_login_method(*selected_))) {
            selected_.reset();
            reload();
        }
    };
    on_cancel = [this] { close(); };
    close_.on_click = on_cancel;
}

std::string LoginSheet::method() const {
    return str::login_method_types[std::min(method_.index(), str::login_method_types.size() - 1)];
}

std::optional<std::int64_t> LoginSheet::via() const {
    return via_.index() < choices_.via.size() ? choices_.via[via_.index()].first : std::nullopt;
}

NodePtr LoginSheet::body() {
    return vcol(kit_, {hint_.node(), list_.node(),
                       hrow(kit_, {grow(method_.node(), 1.0), grow(via_.node(), 2.0)}), notes_.node(),
                       hrow(kit_, {add_.node(), update_.node(), delete_.node(),
                                   lm::layout::flexible_space(), close_.node()})});
}

void LoginSheet::reload() {
    methods_.clear();
    auto list = host_.session().login_methods(choices_.entry_id);
    if (list) {
        methods_ = std::move(*list);
    } else {
        host_.message(list.error().message);
    }
    if (selected_ && std::ranges::none_of(methods_, [this](const LoginMethod& m) {
            return m.id == *selected_;
        })) {
        selected_.reset();
    }
    refresh();
}

void LoginSheet::refresh() {
    std::vector<ListRow> rows;
    for (const LoginMethod& m : methods_) {
        ListRow row;
        row.id = m.id;
        row.text = m.method_type;
        row.cells = {m.linked_entry_id ? host_.session().caption_of(m.linked_entry_id)
                                       : std::string(str::direct),
                     m.notes};
        row.selected = selected_ == m.id;
        rows.push_back(std::move(row));
    }
    list_.set_rows(std::move(rows));
    update_.set_enabled(selected_.has_value());
    delete_.set_enabled(selected_.has_value());
}

void LoginSheet::open() {
    auto choices = host_.session().login_method_choices();
    if (!choices) {
        host_.message(choices.error().message);
        return;
    }
    choices_ = std::move(*choices);
    selected_.reset();
    set_title(std::string(str::login_methods) + ": " + choices_.title);
    std::vector<std::string> via;
    for (const auto& [id, label] : choices_.via) {
        via.push_back(label);
    }
    via_.set_items(std::move(via));
    method_.set_index(0);
    notes_.set_text("");
    reload();
    show();
    host_.present(*this);
}

} // namespace cypherarx::lamina_gui
