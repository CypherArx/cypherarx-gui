#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <future>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cypherarx/entry.hpp"
#include "cypherarx/kdf.hpp"
#include "cypherarx/secret.hpp"
#include "cypherarx/vault.hpp"

#include "app/projection.hpp"
#include "model/timed_clipboard.hpp"

namespace cypherarx::gui {

struct Failure {
    std::string message;
};

using Status = std::expected<void, Failure>;
template <class T>
using Outcome = std::expected<T, Failure>;

using IdLabels = std::vector<std::pair<std::int64_t, std::string>>;
using ViaOptions = std::vector<std::pair<std::optional<std::int64_t>, std::string>>;

enum class UnlockMode { Create, Open };

struct SessionSettings {
    KdfParams (*kdf_params)() = &KdfParams::moderate;
    int clipboard_clear_seconds = 20;
    const char* category_indent = "    ";
    std::uint32_t totp_period_seconds = 30;
};

struct EntryTraits {
    bool has_category = false;
    bool has_fields = false;
    bool has_login_methods = false;
    bool name_optional = false;
};

struct EntryChoices {
    IdLabels services;
    IdLabels personas;
    IdLabels accounts;
    IdLabels categories;
};

struct EntryDraft {
    std::optional<Entry> editing;
    std::string label;
    std::string type;
    std::int64_t service = 0;
    std::int64_t persona = 0;
    std::int64_t account = 0;
    std::int64_t category = 0;
    bool favorite = false;
    std::vector<std::int64_t> tags;
    std::string new_tag;
};

struct FieldDraft {
    std::optional<std::int64_t> editing;
    std::string label;
    std::string type;
    std::string notes;
};

struct LoginMethodChoices {
    std::int64_t entry_id = 0;
    std::string title;
    ViaOptions via;
};

struct TotpReading {
    std::optional<std::string> code;
    std::uint32_t remaining_seconds = 0;
    std::uint32_t period_seconds = 0;
};

class Session {
public:
    explicit Session(SessionSettings settings = {});
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool has_vault() const { return vault_.has_value(); }
    const std::string& vault_path() const { return vault_path_; }
    const std::string& status() const { return status_; }
    std::uint64_t revision() const { return revision_; }

    void start_unlock(UnlockMode mode, const std::filesystem::path& path,
                      std::span<const std::uint8_t> password);
    bool unlocking() const { return unlock_.valid(); }
    UnlockMode unlock_mode() const { return unlock_mode_; }
    const std::string& unlock_path() const { return unlock_path_; }
    std::optional<Status> poll_unlock(ClipboardPort& clipboard);
    void close_vault(ClipboardPort& clipboard);

    Axis axis() const { return axis_; }
    void set_axis(Axis axis);
    const Filter& filter() const { return filter_; }
    void set_query(std::string query);
    void set_category_filter(std::optional<std::int64_t> id);
    void set_tag_filter(std::optional<std::int64_t> id);
    void set_favorites_only(bool on);
    void clear_filters();

    const std::vector<Entry>& entries() const { return entries_; }
    const Projection& projection() const { return proj_; }
    const std::vector<Category>& categories() const { return categories_; }
    const std::vector<Tag>& tags() const { return tags_; }
    IdLabels flat_categories() const;
    const Category* find_category(std::int64_t id) const;
    static EntryTraits traits(std::string_view type);
    std::string entry_title(const Entry& e) const;
    std::string caption_of(std::optional<std::int64_t> id) const;
    std::string entry_detail(const Entry& e) const;

    const Entry* selected_entry() const;
    void select_entry(std::optional<std::int64_t> id);
    const std::vector<Field>& fields() const { return fields_; }
    const Field* selected_field() const;
    void select_field(std::optional<std::int64_t> id);

    EntryChoices entry_choices() const;
    EntryDraft new_entry_draft() const;
    Outcome<EntryDraft> selected_entry_draft();
    Status commit_entry(EntryDraft& draft);
    Status delete_selected_entry();

    Outcome<std::int64_t> add_category(const std::string& name, std::optional<std::int64_t> parent);
    Status update_category(std::int64_t id, const std::string& name,
                           std::optional<std::int64_t> parent);
    Status delete_category(std::int64_t id);
    Outcome<std::int64_t> add_tag(const std::string& name);
    Status rename_tag(std::int64_t id, const std::string& name);
    Status delete_tag(std::int64_t id);

    FieldDraft new_field_draft() const;
    Outcome<FieldDraft> selected_field_draft() const;
    Outcome<Secret> field_value(std::int64_t id);
    Status commit_field(const FieldDraft& draft, std::span<const std::uint8_t> value);
    Status delete_selected_field();
    Status copy_selected_field(ClipboardPort& clipboard);
    static Outcome<Secret> generate_password();

    Status show_totp();
    void close_totp();
    bool totp_shown() const { return totp_field_.has_value(); }
    TotpReading totp() const;
    Status copy_totp(ClipboardPort& clipboard);

    Outcome<LoginMethodChoices> login_method_choices() const;
    Outcome<std::vector<LoginMethod>> login_methods(std::int64_t entry_id);
    Outcome<std::int64_t> add_login_method(std::int64_t entry_id, const std::string& method,
                                           std::optional<std::int64_t> via,
                                           const std::string& notes);
    Status update_login_method(std::int64_t id, const std::string& method,
                               std::optional<std::int64_t> via, const std::string& notes);
    Status delete_login_method(std::int64_t id);

    void tick(ClipboardPort& clipboard);
    std::optional<double> next_wake_seconds() const;

private:
    void touch() { ++revision_; }
    void reload_entries();
    void reload_organization();
    void reload_fields();
    void rebuild_projection();
    void wipe_totp();
    Failure failure(const std::string& what, VaultError e) const;

    SessionSettings settings_;
    std::optional<Vault> vault_;
    std::string vault_path_;
    std::vector<Entry> entries_;
    std::vector<Category> categories_;
    std::vector<Tag> tags_;
    std::unordered_map<std::int64_t, std::vector<std::int64_t>> tags_by_entry_;
    Projection proj_;
    Axis axis_ = Axis::Service;
    Filter filter_;
    std::optional<std::int64_t> selected_entry_;
    std::vector<Field> fields_;
    std::optional<std::int64_t> selected_field_;
    std::string status_;
    std::uint64_t revision_ = 0;

    std::future<std::expected<Vault, VaultError>> unlock_;
    UnlockMode unlock_mode_ = UnlockMode::Open;
    std::string unlock_path_;

    std::optional<std::int64_t> totp_field_;
    std::vector<std::uint8_t> totp_key_;
    TimedClipboard clipboard_;
};

const char* vault_error_text(VaultError e);
std::string path_utf8(const std::filesystem::path& p);

} // namespace cypherarx::gui
