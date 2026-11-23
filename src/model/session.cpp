#include "model/session.hpp"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <system_error>
#include <string_view>
#include <unordered_set>

#include <sodium.h>

#include "cypherarx/passgen.hpp"
#include "cypherarx/totp.hpp"

#include "strings.hpp"

namespace cypherarx::gui {

namespace {

std::uint64_t unix_now() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
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

std::unexpected<Failure> fail(std::string message) {
    return std::unexpected(Failure{std::move(message)});
}

} // namespace

const char* vault_error_text(VaultError e) {
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
        case VaultError::InvalidValue:       return text::err_invalid_value;
        case VaultError::ScrubFailed:        return text::err_scrub_failed;
        default:                             return text::err_unexpected;
    }
}

std::string path_utf8(const std::filesystem::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::filesystem::path path_from_utf8(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

PathParts path_parts(const std::filesystem::path& p) {
    if (!p.has_filename()) {
        return {path_utf8(p), {}};
    }
    return {path_utf8(p.filename()), path_utf8(p.parent_path())};
}

Session::Session(SessionSettings settings) : settings_(settings), status_(text::no_vault) {}

void Session::report(std::string text) {
    status_ = std::move(text);
    touch();
}

Session::~Session() {
    finish_rotation();
    wipe_totp();
}

void Session::finish_rotation() {
    if (!rotate_.valid()) {
        return;
    }
    const auto result = rotate_.get();
    if (!result) {
        status_ = failure(text::rotate_key, result.error()).message;
    }
}

void Session::after_unlock() {
    vault_settings_ = {};
    if (!vault_) {
        return;
    }
    const auto settings = vault_->settings();
    if (!settings) {
        status_ = failure(text::vault_settings, settings.error()).message;
        return;
    }
    vault_settings_ = *settings;
    const auto purged = vault_->purge_access_records(static_cast<std::int64_t>(unix_now()));
    if (!purged) {
        status_ = failure(text::access_log, purged.error()).message;
    }
}

void Session::record(AccessAction action) {
    if (!vault_ || !selected_entry_) {
        return;
    }
    const auto recorded =
        vault_->record_access(*selected_entry_, action, static_cast<std::int64_t>(unix_now()));
    if (!recorded) {
        status_ = failure(text::access_log, recorded.error()).message;
        touch();
    }
}

Status Session::set_vault_settings(const Settings& settings) {
    if (!vault_) {
        return fail(text::no_vault);
    }
    const auto stored = vault_->set_settings(settings);
    if (!stored) {
        return fail(failure(text::vault_settings, stored.error()).message);
    }
    vault_settings_ = settings;
    status_ = text::vault_settings_saved;
    touch();
    return {};
}

void Session::start_rotate(std::span<const std::uint8_t> password) {
    if (!vault_ || rotating() || unlocking()) {
        return;
    }
    Secret pw(password);
    Vault* vault = &*vault_;
    rotate_ = std::async(std::launch::async, [pw = std::move(pw), vault]() mutable {
        auto r = vault->rotate_key(pw.bytes());
        pw = Secret();
        return r;
    });
    touch();
}

std::optional<Status> Session::poll_rotate() {
    if (!rotate_.valid() ||
        rotate_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return std::nullopt;
    }
    const auto result = rotate_.get();
    touch();
    if (!result) {
        return fail(failure(text::rotate_key, result.error()).message);
    }
    status_ = text::key_rotated;
    return Status{};
}

Outcome<ImportSummary> Session::import_csv(const std::filesystem::path& file,
                                           std::optional<std::int64_t> persona) {
    if (!vault_) {
        return fail(text::no_vault);
    }
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec) {
        return fail(text::import_cannot_read);
    }
    if (size == 0) {
        return fail(text::import_empty);
    }
    if (size > settings_.import_max_bytes) {
        return fail(text::import_too_large);
    }
    Secret data(static_cast<std::size_t>(size));
    char* const text_data = reinterpret_cast<char*>(data.data());
    std::ifstream in(file, std::ios::binary);
    in.read(text_data, static_cast<std::streamsize>(data.size()));
    if (!in || static_cast<std::size_t>(in.gcount()) != data.size()) {
        return fail(text::import_cannot_read);
    }
    const auto logins = parse_logins_csv({text_data, data.size()});
    if (!logins) {
        switch (logins.error()) {
        case ImportError::NoHeader:
            return fail(text::import_empty);
        case ImportError::NoSecretColumn:
            return fail(text::import_no_secret_column);
        case ImportError::Malformed:
            return fail(text::import_malformed);
        }
        return fail(text::err_unexpected);
    }
    const auto summary = vault_->import_logins(
        *logins, {.password_label = text::import_password_label,
                  .totp_label = text::import_totp_label,
                  .password_type = text::import_password_type,
                  .totp_type = text::import_totp_type,
                  .email_label = text::import_email_label,
                  .id_label = text::import_id_label,
                  .identifier_type = text::import_identifier_type,
                  .url_label = text::import_url_label,
                  .persona_id = persona});
    if (!summary) {
        return fail(failure(text::import_csv, summary.error()).message);
    }
    reload_entries();
    status_ = format(text::imported_fmt, summary->accounts, summary->services, summary->fields,
                     summary->skipped_existing, summary->skipped_empty);
    touch();
    return *summary;
}

Failure Session::failure(const std::string& what, VaultError e) const {
    return Failure{what + ": " + vault_error_text(e)};
}

void Session::start_unlock(UnlockMode mode, const std::filesystem::path& path,
                           std::span<const std::uint8_t> password) {
    if (unlocking()) {
        return;
    }
    Secret pw(password);
    unlock_mode_ = mode;
    unlock_path_ = path_utf8(path);
    const bool create = mode == UnlockMode::Create;
    const KdfParams params = settings_.kdf_params();
    unlock_ = std::async(std::launch::async,
                         [pw = std::move(pw), path, create, params]() mutable {
                             auto r = create ? Vault::create(path, pw.bytes(), params)
                                             : Vault::open(path, pw.bytes());
                             pw = Secret();
                             return r;
                         });
    touch();
}

std::optional<Status> Session::poll_unlock(ClipboardPort& clipboard) {
    if (!unlock_.valid() ||
        unlock_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return std::nullopt;
    }
    auto result = unlock_.get();
    touch();
    if (!result) {
        return fail(failure(unlock_mode_ == UnlockMode::Create ? text::new_vault : text::open_vault,
                            result.error())
                        .message);
    }
    close_vault(clipboard);
    vault_ = std::move(*result);
    vault_path_ = unlock_path_;
    reload_entries();
    after_unlock();
    return Status{};
}

void Session::close_vault(ClipboardPort& clipboard) {
    finish_rotation();
    wipe_totp();
    vault_settings_ = {};
    clipboard_.clear_now(clipboard);
    vault_.reset();
    vault_path_.clear();
    entries_.clear();
    categories_.clear();
    tags_.clear();
    tags_by_entry_.clear();
    filter_.clear();
    proj_ = {};
    fields_.clear();
    selected_entry_.reset();
    selected_field_.reset();
    status_ = text::no_vault;
    touch();
}

void Session::set_axis(Axis axis) {
    if (axis_ == axis) {
        return;
    }
    axis_ = axis;
    rebuild_projection();
}

void Session::set_query(std::string query) {
    if (filter_.query == query) {
        return;
    }
    filter_.query = std::move(query);
    rebuild_projection();
}

Scope Session::scope() const {
    if (filter_.favorites_only) {
        return {Scope::Kind::favorites};
    }
    if (filter_.category) {
        return {Scope::Kind::category, *filter_.category};
    }
    if (filter_.tag) {
        return {Scope::Kind::tag, *filter_.tag};
    }
    return {};
}

void Session::set_scope(Scope scope) {
    filter_.favorites_only = scope.kind == Scope::Kind::favorites;
    filter_.category = scope.kind == Scope::Kind::category ? std::optional(scope.id) : std::nullopt;
    filter_.tag = scope.kind == Scope::Kind::tag ? std::optional(scope.id) : std::nullopt;
    rebuild_projection();
}
IdLabels Session::flat_categories() const {
    return flatten_categories(categories_, settings_.category_indent);
}

const Category* Session::find_category(std::int64_t id) const {
    for (const auto& c : categories_) {
        if (c.id == id) {
            return &c;
        }
    }
    return nullptr;
}

EntryTraits Session::traits(std::string_view type) {
    if (type == "service") {
        return {.has_category = true, .has_fields = true, .name_optional = true};
    }
    if (type == "account") {
        return {.has_fields = true, .has_login_methods = true, .name_optional = true};
    }
    if (type == "child") {
        return {.has_fields = true};
    }
    return {};
}

std::span<const char* const> Session::field_types(std::string_view entry_type) {
    if (entry_type == "service") {
        return text::service_field_types;
    }
    return text::field_types;
}

std::string Session::entry_title(const Entry& e) const {
    if (e.entry_type == "service" && e.label.empty()) {
        return proj_.label_of(e.id);
    }
    if (!e.label.empty() || e.entry_type != "account") {
        return e.label;
    }
    const auto other = axis_ == Axis::Persona ? e.parent_id : e.persona_id;
    return other ? proj_.label_of(other) : std::string(text::no_persona);
}

std::string Session::caption_of(std::optional<std::int64_t> id) const {
    const Entry* e = id ? proj_.find(*id) : nullptr;
    if (!e || e->entry_type != "account") {
        return proj_.label_of(id);
    }
    const std::string name = !e->label.empty() ? e->label
                             : e->persona_id   ? proj_.label_of(e->persona_id)
                                               : std::string(text::no_persona);
    return name + " (" + proj_.label_of(e->parent_id) + ")";
}

std::string Session::entry_detail(const Entry& e) const {
    std::string detail = e.entry_type;
    if (e.entry_type == "account") {
        detail += "  |  " + proj_.label_of(e.parent_id);
        if (e.persona_id) {
            detail += " \u00d7 " + proj_.label_of(e.persona_id);
        }
    }
    if (e.category_id) {
        if (const Category* c = find_category(*e.category_id)) {
            detail += "  |  " + c->name;
        }
    }
    if (const auto it = tags_by_entry_.find(e.id); it != tags_by_entry_.end()) {
        for (const auto tag_id : it->second) {
            for (const auto& t : tags_) {
                if (t.id == tag_id) {
                    detail += "  #" + t.name;
                }
            }
        }
    }
    return detail;
}

void Session::reload_entries() {
    entries_.clear();
    proj_ = {};
    touch();
    if (!vault_) {
        return;
    }
    auto list = vault_->list_entries();
    if (!list) {
        status_ = failure(text::entries, list.error()).message;
        return;
    }
    entries_ = std::move(*list);
    service_names_.clear();
    const auto urls = vault_->urls();
    if (!urls) {
        status_ = failure(text::entries, urls.error()).message;
        return;
    }
    for (const EntryUrl& url : *urls) {
        service_names_.try_emplace(url.entry_id, url.value);
    }
    for (const Entry& e : entries_) {
        if (e.entry_type == "service" && e.label.empty()) {
            service_names_.try_emplace(e.id, e.url.empty() ? std::string(text::unnamed_service) : e.url);
        }
    }
    reload_organization();
    tags_by_entry_.clear();
    if (auto pairs = vault_->list_entry_tags()) {
        for (const auto& [entry_id, tag_id] : *pairs) {
            tags_by_entry_[entry_id].push_back(tag_id);
        }
    }
    rebuild_projection();
    if (selected_entry_ && !proj_.find(*selected_entry_)) {
        selected_entry_.reset();
    }
    reload_fields();
}

void Session::rebuild_projection() {
    EntryPredicate match;
    if (filter_.active()) {
        std::unordered_set<std::int64_t> subtree;
        if (filter_.category) {
            subtree = category_subtree(categories_, *filter_.category);
        }
        match = [this, subtree = std::move(subtree)](const Entry& e) {
            if (filter_.favorites_only && !e.is_favorite) {
                return false;
            }
            if (filter_.category && !(e.category_id && subtree.contains(*e.category_id))) {
                return false;
            }
            if (filter_.tag) {
                const auto it = tags_by_entry_.find(e.id);
                if (it == tags_by_entry_.end() ||
                    !std::ranges::contains(it->second, *filter_.tag)) {
                    return false;
                }
            }
            if (!filter_.query.empty() && !contains_ci(e.label, filter_.query) &&
                !contains_ci(e.display_name, filter_.query) && !contains_ci(e.url, filter_.query) &&
                !(service_names_.contains(e.id) &&
                  contains_ci(service_names_.at(e.id), filter_.query))) {
                return false;
            }
            return true;
        };
    }
    proj_ = Projection::build(entries_, axis_, match, service_names_);
    if (vault_) {
        const char* axis_name = axis_ == Axis::Persona ? text::axis_persona : text::axis_service;
        status_ = proj_.filtered ? format(text::status_filtered_fmt, proj_.visible_count(),
                                          entries_.size(), axis_name)
                                 : format(text::status_open_fmt, entries_.size(), axis_name);
    }
    touch();
}

void Session::reload_organization() {
    categories_.clear();
    tags_.clear();
    touch();
    if (!vault_) {
        return;
    }
    if (auto c = vault_->list_categories()) {
        categories_ = std::move(*c);
    }
    if (auto t = vault_->list_tags()) {
        tags_ = std::move(*t);
    }
}

void Session::reload_fields() {
    fields_.clear();
    touch();
    if (!vault_ || !selected_entry_) {
        selected_field_.reset();
        return;
    }
    auto list = vault_->list_fields(*selected_entry_);
    if (!list) {
        status_ = failure(text::fields, list.error()).message;
        return;
    }
    fields_ = std::move(*list);
    if (selected_field_ && !selected_field()) {
        selected_field_.reset();
    }
}

const Entry* Session::selected_entry() const {
    return selected_entry_ ? proj_.find(*selected_entry_) : nullptr;
}

const Field* Session::selected_field() const {
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

void Session::select_entry(std::optional<std::int64_t> id) {
    if (selected_entry_ == id) {
        return;
    }
    selected_entry_ = id;
    selected_field_.reset();
    wipe_totp();
    reload_fields();
}

void Session::select_field(std::optional<std::int64_t> id) {
    if (selected_field_ == id) {
        return;
    }
    selected_field_ = id;
    if (totp_field_ && totp_field_ != id) {
        wipe_totp();
    }
    touch();
}

EntryChoices Session::entry_choices() const {
    EntryChoices c;
    c.personas.emplace_back(0, text::none);
    c.categories.emplace_back(0, text::uncategorized);
    for (auto& cat : flat_categories()) {
        c.categories.push_back(std::move(cat));
    }
    for (const auto& e : entries_) {
        if (e.entry_type == "service") {
            c.services.emplace_back(e.id, proj_.label_of(e.id));
        } else if (e.entry_type == "persona") {
            c.personas.emplace_back(e.id, e.label);
        } else if (e.entry_type == "account") {
            c.accounts.emplace_back(e.id, caption_of(e.id));
        }
    }
    return c;
}

EntryDraft Session::new_entry_draft() const {
    EntryDraft d;
    d.type = text::entry_types.front();
    return d;
}

Outcome<EntryDraft> Session::selected_entry_draft() {
    const Entry* cur = selected_entry();
    if (!cur || !vault_) {
        return fail(text::select_entry_first);
    }
    EntryDraft d;
    d.editing = *cur;
    d.label = cur->label;
    d.type = cur->entry_type;
    if (cur->parent_id) {
        d.service = *cur->parent_id;
        d.account = *cur->parent_id;
    }
    d.persona = cur->persona_id.value_or(0);
    d.category = cur->category_id.value_or(0);
    d.favorite = cur->is_favorite;
    if (auto tags = vault_->entry_tags(cur->id)) {
        d.tags = std::move(*tags);
    }
    return d;
}

Status Session::commit_entry(EntryDraft& d) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    const EntryTraits kind = traits(d.type);
    if (d.label.empty() && !kind.name_optional) {
        return fail(text::label_required);
    }
    Entry e = d.editing.value_or(Entry{});
    e.label = d.label;
    e.entry_type = d.type;
    e.parent_id.reset();
    e.persona_id.reset();
    e.is_favorite = d.favorite;
    if (kind.has_category) {
        e.category_id.reset();
        if (d.category != 0) {
            e.category_id = d.category;
        }
    } else if (!d.editing) {
        e.category_id.reset();
    }
    if (!d.new_tag.empty()) {
        if (auto t = vault_->add_tag(d.new_tag)) {
            if (!std::ranges::contains(d.tags, t->id)) {
                d.tags.push_back(t->id);
            }
            d.new_tag.clear();
        }
    }
    if (e.entry_type == "account") {
        if (d.service == 0) {
            return fail(text::need_service_first);
        }
        e.parent_id = d.service;
        if (d.persona != 0) {
            e.persona_id = d.persona;
        }
    } else if (e.entry_type == "child") {
        if (d.account == 0) {
            return fail(text::need_account_first);
        }
        e.parent_id = d.account;
    }

    if (d.editing) {
        if (auto r = vault_->update_entry(e); !r) {
            return fail(failure(text::edit_entry, r.error()).message);
        }
    } else {
        auto r = vault_->add_entry(e);
        if (!r) {
            return fail(failure(text::add_entry, r.error()).message);
        }
        e.id = r->id;
        selected_entry_ = r->id;
        selected_field_.reset();
        wipe_totp();
    }
    Status result;
    if (auto r = vault_->set_entry_tags(e.id, d.tags); !r) {
        result = fail(failure(text::tags, r.error()).message);
    }
    reload_entries();
    return result;
}

Status Session::delete_selected_entry() {
    if (!vault_ || !selected_entry_) {
        return fail(text::select_entry_first);
    }
    if (auto r = vault_->delete_entry(*selected_entry_); !r) {
        return fail(failure(text::delete_entry, r.error()).message);
    }
    select_entry(std::nullopt);
    reload_entries();
    return {};
}

Outcome<std::int64_t> Session::add_category(const std::string& name,
                                            std::optional<std::int64_t> parent) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    if (name.empty()) {
        return fail(text::name_required);
    }
    auto r = vault_->add_category(name, parent);
    if (!r) {
        return fail(failure(text::categories, r.error()).message);
    }
    reload_organization();
    return r->id;
}

Status Session::update_category(std::int64_t id, const std::string& name,
                                std::optional<std::int64_t> parent) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    if (name.empty()) {
        return fail(text::name_required);
    }
    if (auto r = vault_->update_category(id, name, parent); !r) {
        return fail(r.error() == VaultError::Forbidden ? text::preset_read_only
                                                       : vault_error_text(r.error()));
    }
    reload_organization();
    return {};
}

Status Session::delete_category(std::int64_t id) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    if (auto r = vault_->delete_category(id); !r) {
        return fail(r.error() == VaultError::Forbidden ? text::preset_read_only
                                                       : vault_error_text(r.error()));
    }
    if (filter_.category == id) {
        filter_.category.reset();
    }
    reload_entries();
    return {};
}

Outcome<std::int64_t> Session::add_tag(const std::string& name) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    if (name.empty()) {
        return fail(text::name_required);
    }
    auto r = vault_->add_tag(name);
    if (!r) {
        return fail(failure(text::tags, r.error()).message);
    }
    reload_organization();
    return r->id;
}

Status Session::rename_tag(std::int64_t id, const std::string& name) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    if (name.empty()) {
        return fail(text::name_required);
    }
    if (auto r = vault_->rename_tag(id, name); !r) {
        return fail(failure(text::tags, r.error()).message);
    }
    reload_organization();
    return {};
}

Status Session::delete_tag(std::int64_t id) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    if (auto r = vault_->delete_tag(id); !r) {
        return fail(failure(text::tags, r.error()).message);
    }
    if (filter_.tag == id) {
        filter_.tag.reset();
    }
    reload_entries();
    return {};
}

FieldDraft Session::new_field_draft() const {
    FieldDraft d;
    const Entry* owner = selected_entry();
    d.type = field_types(owner ? std::string_view(owner->entry_type) : std::string_view()).front();
    return d;
}

Outcome<FieldDraft> Session::selected_field_draft() const {
    const Field* cur = selected_field();
    if (!cur) {
        return fail(text::select_field_first);
    }
    FieldDraft d;
    d.editing = cur->id;
    d.label = cur->label;
    d.type = cur->field_type;
    d.notes = cur->notes;
    return d;
}

Outcome<Secret> Session::field_value(std::int64_t id) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    auto value = vault_->field_value(id);
    if (!value) {
        return fail(failure(text::value, value.error()).message);
    }
    return std::move(*value);
}

Status Session::commit_field(const FieldDraft& d, std::span<const std::uint8_t> value) {
    if (!vault_ || !selected_entry_) {
        return fail(text::select_entry_first);
    }
    const Entry* owner = selected_entry();
    if (!d.editing && owner && !traits(owner->entry_type).has_fields) {
        return fail(text::fields_on_accounts);
    }
    if (d.label.empty()) {
        return fail(text::label_required);
    }
    const bool on_service = owner && owner->entry_type == "service";
    if (owner) {
        bool allowed = false;
        for (const char* type : field_types(owner->entry_type)) {
            allowed = allowed || d.type == type;
        }
        if (!allowed) {
            return fail(text::field_type_not_allowed);
        }
    }
    if (d.editing) {
        if (auto r = vault_->update_field(*d.editing, d.label, d.type, value, d.notes); !r) {
            return fail(failure(text::edit_field, r.error()).message);
        }
        if (totp_field_ == d.editing) {
            wipe_totp();
        }
    } else {
        auto r = vault_->add_field(*selected_entry_, d.label, d.type, value, d.notes);
        if (!r) {
            return fail(failure(text::add_field, r.error()).message);
        }
        selected_field_ = r->id;
    }
    if (on_service) {
        reload_entries();
    }
    reload_fields();
    return {};
}

Status Session::delete_selected_field() {
    if (!vault_ || !selected_field_) {
        return fail(text::select_field_first);
    }
    const Entry* owner = selected_entry();
    const bool on_service = owner && owner->entry_type == "service";
    if (auto r = vault_->delete_field(*selected_field_); !r) {
        return fail(failure(text::delete_field, r.error()).message);
    }
    if (totp_field_ == selected_field_) {
        wipe_totp();
    }
    selected_field_.reset();
    if (on_service) {
        reload_entries();
    }
    reload_fields();
    return {};
}

Status Session::copy_selected_field(ClipboardPort& clipboard) {
    if (!vault_ || !selected_field_) {
        return fail(text::select_field_first);
    }
    auto value = vault_->field_value(*selected_field_);
    if (!value) {
        return fail(failure(text::copy_value, value.error()).message);
    }
    clipboard_.copy(clipboard, value->bytes(), settings_.clipboard_clear_seconds);
    status_ = format(text::copied_fmt, settings_.clipboard_clear_seconds);
    touch();
    record(AccessAction::copy);
    return {};
}

Outcome<Secret> Session::generate_password() {
    auto pw = cypherarx::generate_password({});
    if (!pw) {
        return fail(text::err_unexpected);
    }
    return std::move(*pw);
}

Status Session::show_totp() {
    const Field* f = selected_field();
    if (!f || !vault_) {
        return fail(text::select_field_first);
    }
    if (f->field_type != "totp") {
        return fail(text::select_totp_field);
    }
    auto secret = vault_->field_value(f->id);
    if (!secret) {
        return fail(failure(text::show_totp, secret.error()).message);
    }
    const std::string_view base32(reinterpret_cast<const char*>(secret->data()), secret->size());
    auto key = base32_decode(base32);
    if (!key) {
        return fail(text::totp_invalid_base32);
    }
    wipe_totp();
    totp_key_ = std::move(*key);
    totp_field_ = f->id;
    touch();
    record(AccessAction::view);
    return {};
}

void Session::close_totp() {
    wipe_totp();
}

void Session::wipe_totp() {
    if (!totp_key_.empty()) {
        sodium_memzero(totp_key_.data(), totp_key_.size());
        totp_key_.clear();
    }
    if (totp_field_) {
        totp_field_.reset();
        touch();
    }
}

TotpReading Session::totp() const {
    TotpReading r;
    r.period_seconds = settings_.totp_period_seconds;
    if (!totp_field_) {
        return r;
    }
    TotpParams params;
    params.period = settings_.totp_period_seconds;
    const auto now = unix_now();
    if (auto code = totp_at(totp_key_, now, params)) {
        r.code = std::move(*code);
    }
    r.remaining_seconds = totp_seconds_remaining(now, params.period);
    return r;
}

Status Session::copy_totp(ClipboardPort& clipboard) {
    const TotpReading r = totp();
    if (!r.code) {
        return fail(text::totp_failed);
    }
    clipboard_.copy(clipboard,
                    {reinterpret_cast<const std::uint8_t*>(r.code->data()), r.code->size()},
                    settings_.clipboard_clear_seconds);
    status_ = format(text::copied_fmt, settings_.clipboard_clear_seconds);
    touch();
    record(AccessAction::copy);
    return {};
}

Outcome<LoginMethodChoices> Session::login_method_choices() const {
    const Entry* self = selected_entry();
    if (!self) {
        return fail(text::select_entry_first);
    }
    if (self->entry_type != "account") {
        return fail(text::login_methods_accounts_only);
    }
    LoginMethodChoices c;
    c.entry_id = self->id;
    c.title = caption_of(self->id);
    c.via.emplace_back(std::nullopt, text::direct);
    for (const auto& e : entries_) {
        if (e.entry_type != "account" || e.id == self->id) {
            continue;
        }
        if (self->parent_id && e.parent_id && *e.parent_id == *self->parent_id) {
            continue;
        }
        c.via.emplace_back(e.id, caption_of(e.id));
    }
    return c;
}

Outcome<std::vector<LoginMethod>> Session::login_methods(std::int64_t entry_id) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    auto list = vault_->list_login_methods(entry_id);
    if (!list) {
        return fail(failure(text::login_methods, list.error()).message);
    }
    return std::move(*list);
}

Outcome<std::int64_t> Session::add_login_method(std::int64_t entry_id, const std::string& method,
                                                std::optional<std::int64_t> via,
                                                const std::string& notes) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    auto r = vault_->add_login_method(entry_id, method, via, notes);
    if (!r) {
        return fail(failure(text::add, r.error()).message);
    }
    return r->id;
}

Status Session::update_login_method(std::int64_t id, const std::string& method,
                                    std::optional<std::int64_t> via, const std::string& notes) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    if (auto r = vault_->update_login_method(id, method, via, notes); !r) {
        return fail(failure(text::update, r.error()).message);
    }
    return {};
}

Status Session::delete_login_method(std::int64_t id) {
    if (!vault_) {
        return fail(text::err_not_unlocked);
    }
    if (auto r = vault_->delete_login_method(id); !r) {
        return fail(failure(text::delete_selected, r.error()).message);
    }
    return {};
}

void Session::tick(ClipboardPort& clipboard) {
    clipboard_.tick(clipboard);
}

std::optional<double> Session::next_wake_seconds() const {
    return clipboard_.seconds_until_clear();
}

} // namespace cypherarx::gui
