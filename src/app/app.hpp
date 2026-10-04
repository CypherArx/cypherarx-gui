#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cypherarx/entry.hpp"
#include "cypherarx/vault.hpp"

#include "app/clipboard_guard.hpp"
#include "app/projection.hpp"
#include "app/secret_buffer.hpp"
#include "config.hpp"

namespace cypherarx::gui {

struct Modal {
    const char* id;
    bool pending = false;
    void open() { pending = true; }
    bool begin();
};

struct PasswordDialog {
    enum class Mode { Create, Open };
    Modal modal{"Master password###password"};
    Mode mode = Mode::Open;
    std::filesystem::path path;
    std::string path_text;
    SecretBuffer password;
    bool focus = false;
    explicit PasswordDialog(std::size_t cap) : password(cap) {}
};

struct UnlockJob {
    Modal modal{"Please wait###busy"};
    std::future<std::expected<Vault, VaultError>> future;
    PasswordDialog::Mode mode = PasswordDialog::Mode::Open;
    std::string path_text;
    bool active() const { return future.valid(); }
};

using IdLabels = std::vector<std::pair<std::int64_t, std::string>>;

struct EntryDialog {
    Modal modal{"Entry###entry"};
    std::optional<Entry> editing;
    std::string label;
    int type_idx = 0;
    int service_idx = 0;
    int persona_idx = 0;
    int account_idx = 0;
    int category_idx = 0;
    bool favorite = false;
    IdLabels services;
    IdLabels personas;
    IdLabels accounts;
    IdLabels categories;
    std::vector<std::int64_t> checked_tags;
    std::string new_tag;
};

struct OrganizeDialog {
    Modal modal{"Organize###organize"};
    std::optional<std::int64_t> selected_category;
    std::optional<std::int64_t> selected_tag;
    std::string category_name;
    int parent_idx = 0;
    IdLabels parent_options;
    std::string tag_name;
};

struct FieldDialog {
    Modal modal{"Field###field"};
    std::optional<std::int64_t> editing;
    std::string label;
    int type_idx = 0;
    SecretBuffer value;
    bool show_value = false;
    std::string notes;
    explicit FieldDialog(std::size_t cap) : value(cap) {}
};

struct LoginMethodsDialog {
    Modal modal{"Login methods###login_methods"};
    std::int64_t entry_id = 0;
    std::string title;
    std::vector<LoginMethod> methods;
    std::optional<std::int64_t> selected;
    int method_idx = 0;
    int via_idx = 0;
    std::vector<std::pair<std::optional<std::int64_t>, std::string>> via_options;
    std::string notes;
};

struct ConfirmDialog {
    Modal modal{"Confirm###confirm"};
    std::string text;
    std::function<void()> on_yes;
};

struct MessageDialog {
    Modal modal{"CypherArx###message"};
    std::string text;
};

struct TotpView {
    std::optional<std::int64_t> field_id;
    std::vector<std::uint8_t> key;
    void close();
    ~TotpView() { close(); }
};

class App {
public:
    App();
    void draw();

private:
    void draw_toolbar();
    void draw_sidebar();
    void draw_entries_pane();
    void draw_tree_node(std::int64_t id);
    void draw_fields_pane();
    void draw_totp();
    void draw_status();
    void draw_dialogs();
    void draw_password_dialog();
    void draw_busy_dialog();
    void draw_spinner();
    void draw_entry_dialog();
    void draw_organize_dialog();
    void draw_field_dialog();
    void draw_login_methods_dialog();
    void draw_confirm_dialog();
    void draw_message_dialog();

    void request_new_vault();
    void request_open_vault();
    void close_vault();
    void start_unlock();
    void finish_unlock(std::expected<Vault, VaultError> result);

    void reload_entries();
    void rebuild_projection();
    void reload_organization();
    void reload_fields();
    void select_entry(std::optional<std::int64_t> id);

    void begin_add_entry();
    void begin_edit_entry();
    bool commit_entry();
    void request_delete_entry();

    void begin_organize();
    void refresh_organize_options();
    void add_category();
    void rename_category();
    void request_delete_category();
    void add_tag_from_organize();
    void rename_tag();
    void request_delete_tag();

    void begin_add_field();
    void begin_edit_field();
    bool commit_field();
    void request_delete_field();
    void copy_field_value();
    void show_totp();

    void begin_login_methods();
    void reload_login_methods();
    void add_login_method();
    void update_login_method();
    void delete_login_method();

    static std::string utf8(const std::filesystem::path& p);
    void fail(const std::string& what, VaultError e);
    void message(const std::string& text);
    const Entry* selected_entry() const;
    const Field* selected_field() const;

    const Config& cfg_;
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
    ClipboardGuard clipboard_;
    TotpView totp_;

    PasswordDialog password_dlg_;
    UnlockJob unlock_job_;
    EntryDialog entry_dlg_;
    OrganizeDialog organize_dlg_;
    FieldDialog field_dlg_;
    LoginMethodsDialog login_dlg_;
    ConfirmDialog confirm_dlg_;
    MessageDialog message_dlg_;
};

const char* error_text(VaultError e);

} // namespace cypherarx::gui
