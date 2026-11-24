#pragma once

#include <array>

namespace cypherarx::gui::text {

inline constexpr const char* app_name = "CypherArx";

inline constexpr const char* new_vault = "New Vault";
inline constexpr const char* open_vault = "Open Vault";
inline constexpr const char* close_vault = "Close Vault";
inline constexpr const char* no_vault = "No vault open.";
inline constexpr const char* vault_created = "Vault created.";
inline constexpr const char* vault_opened = "Vault opened.";
inline constexpr const char* master_password = "Master password";
inline constexpr const char* unlocking = "Deriving key...";

inline constexpr const char* entries = "Entries";
inline constexpr const char* fields = "Fields";
inline constexpr const char* view = "View";
inline constexpr const char* axis_service = "Service axis";
inline constexpr const char* axis_persona = "Persona axis";
inline constexpr const char* col_name = "Name";
inline constexpr const char* col_type = "Type";
inline constexpr const char* col_service = "Service";
inline constexpr const char* col_persona = "Persona";

inline constexpr const char* add_entry = "Add Entry";
inline constexpr const char* edit_entry = "Edit Entry";
inline constexpr const char* delete_entry = "Delete Entry";
inline constexpr const char* set_persona = "Set Persona";
inline constexpr const char* tick_all = "Tick or Untick All Accounts";
inline constexpr const char* select_accounts_first =
    "Tick the accounts to change first.";
inline constexpr const char* confirm_set_persona_fmt = "Set the persona of %zu accounts to \"%s\"?";
inline constexpr const char* marked_fmt = "%zu accounts ticked.";
inline constexpr const char* persona_set_fmt = "Persona set for %zu accounts.";
inline constexpr const char* merge_service = "Merge Into Another Service";
inline constexpr const char* merge_into = "Merge into";
inline constexpr const char* merge_action = "Merge";
inline constexpr const char* merge_hint =
    "The accounts and the URLs of this service move to the chosen one, which keeps its own name, and this "
    "service is removed.";
inline constexpr const char* merge_no_other = "There is no other service to merge into.";
inline constexpr const char* services_merged = "The services were merged.";
inline constexpr const char* login_methods = "Login Methods";
inline constexpr const char* add_field = "Add Field";
inline constexpr const char* edit_field = "Edit Field";
inline constexpr const char* copy_value = "Copy Value";
inline constexpr const char* show_totp = "Show TOTP";
inline constexpr const char* delete_field = "Delete Field";

inline constexpr const char* label = "Label";
inline constexpr const char* type = "Type";
inline constexpr const char* service = "Service";
inline constexpr const char* persona = "Persona";
inline constexpr const char* parent_account = "Parent account";
inline constexpr const char* value = "Value";
inline constexpr const char* notes = "Notes";
inline constexpr const char* generate = "Generate";
inline constexpr const char* show = "Show";
inline constexpr const char* ok = "OK";
inline constexpr const char* cancel = "Cancel";
inline constexpr const char* yes = "Yes";
inline constexpr const char* no = "No";
inline constexpr const char* close = "Close";
inline constexpr const char* add = "Add";
inline constexpr const char* delete_selected = "Delete selected";
inline constexpr const char* none = "(none)";
inline constexpr const char* direct = "(direct)";
inline constexpr const char* via = "via";

inline constexpr const char* search_hint = "Search entries";
inline constexpr const char* filters = "Filters";
inline constexpr const char* all = "All";
inline constexpr const char* favorites_only = "Favorites only";
inline constexpr const char* clear_filters = "Clear";
inline constexpr const char* no_matches = "No entries match the current filter.";
inline constexpr const char* status_filtered_fmt = "Vault open: %zu of %zu entries, %s";
inline constexpr const char* organize = "Organize";
inline constexpr const char* categories = "Categories";
inline constexpr const char* category = "Category";
inline constexpr const char* tags = "Tags";
inline constexpr const char* favorite = "Favorite";
inline constexpr const char* favorite_marker = "* ";
inline constexpr const char* new_tag = "new tag";
inline constexpr const char* new_category = "new category";
inline constexpr const char* parent = "Parent";
inline constexpr const char* rename = "Rename";
inline constexpr const char* update = "Update";
inline constexpr const char* preset = "(preset)";
inline constexpr const char* uncategorized = "(uncategorized)";
inline constexpr const char* preset_read_only = "Preset categories cannot be renamed or deleted.";
inline constexpr const char* confirm_delete_category =
    "Delete this category?\nIts sub-categories and entries become uncategorized.";
inline constexpr const char* confirm_delete_tag = "Delete this tag?\nIt is removed from every entry.";

inline constexpr const char* select_entry_first = "Select an entry first.";
inline constexpr const char* select_field_first = "Select a field first.";
inline constexpr const char* select_totp_field = "Select a field of type \"totp\".";
inline constexpr const char* need_service_first = "Create a service first: an account is service x persona.";
inline constexpr const char* need_account_first = "Create an account first: a child belongs to an account.";
inline constexpr const char* login_methods_accounts_only =
    "Login methods apply to accounts (how you log in to that account).\nSelect an account entry.";
inline constexpr const char* login_methods_hint =
    "How you log in to this account. Leave \"via\" as (direct) for email/password;\n"
    "for OAuth pick the provider account on another service.";
inline constexpr const char* confirm_delete_entry =
    "Delete the selected entry?\n(Recoverable: it is soft-deleted.)";
inline constexpr const char* confirm_delete_field = "Delete this field?\n(Recoverable: it is soft-deleted.)";
inline constexpr const char* copied_fmt = "Value copied to clipboard. Clears in %d s.";
inline constexpr const char* totp_invalid_base32 = "The secret is not valid base32.";
inline constexpr const char* totp_failed = "Could not generate a code.";
inline constexpr const char* totp_code_fmt = "Code: %s";
inline constexpr const char* totp_remaining_fmt = "Valid for %u s";
inline constexpr const char* status_open_fmt = "Vault open: %zu entries, %s";
inline constexpr const char* error = "Error";
inline constexpr const char* confirm = "Confirm";
inline constexpr const char* password_empty = "Enter the master password.";
inline constexpr const char* label_required = "Enter a label.";
inline constexpr const char* name_required = "Enter a name.";
inline constexpr const char* method = "Method";
inline constexpr const char* name = "Name";
inline constexpr const char* no_fields = "This entry has no fields.";
inline constexpr const char* no_entries = "No entries yet.";
inline constexpr const char* totp_remaining_short_fmt = "%u s";
inline constexpr const char* theme_dark_outlined = "Dark, outlined";
inline constexpr const char* theme_dark_tonal = "Dark, tonal";
inline constexpr const char* theme_light = "Light";
inline constexpr const char* theme = "Theme";
inline constexpr const char* theme_unknown = "The saved theme is not known to this version; using the default.";
inline constexpr const char* preferences_not_saved = "Could not save the preferences.";
inline constexpr const char* last_vault_missing = "The vault opened last time is not there:";
inline constexpr const char* notice_separator = " ";
inline constexpr const char* account = "Account";
inline constexpr const char* child = "Child";
inline constexpr const char* add_service = "Add Service";
inline constexpr const char* add_persona = "Add Persona";
inline constexpr const char* add_account = "Add Account";
inline constexpr const char* add_child = "Add Child";
inline constexpr const char* name_optional = "Name (optional)";
inline constexpr const char* account_name_hint =
    "Leave the name empty to show the persona or the service instead.";
inline constexpr const char* no_persona = "(no persona)";
inline constexpr const char* all_items = "All items";
inline constexpr const char* favorites = "Favorites";
inline constexpr const char* recently_used = "Recently used";
inline constexpr const char* used_fmt = "used %zu times";
inline constexpr const char* fields_on_accounts = "Fields belong to services, accounts and their children.";
inline constexpr const char* field_type_not_allowed = "That kind of field does not belong on this entry.";
inline constexpr const char* unnamed_service = "(unnamed)";
inline constexpr const char* service_name_hint = "Leave the name empty to show the service's URL instead.";

inline constexpr std::array<const char*, 4> entry_types = {"service", "persona", "account", "child"};
inline constexpr std::array<const char*, 3> field_types = {"text", "password", "totp"};
inline constexpr std::array<const char*, 1> service_field_types = {"url"};
inline constexpr std::array<const char*, 8> login_method_types = {
    "email", "google", "github", "apple", "twitter", "line", "facebook", "other"};

inline constexpr const char* vault_menu = "Vault";
inline constexpr const char* vault_settings = "Vault settings";
inline constexpr const char* settings_retention = "Keep the access log for (days, 0 = off)";
inline constexpr const char* settings_session = "Session length (minutes)";
inline constexpr const char* settings_idle = "Lock after idle for (minutes, 0 = off)";
inline constexpr const char* settings_keyring = "Keep the session in the OS keyring";
inline constexpr const char* settings_screensaver =
    "Lock when the screensaver starts, the screen is locked, the user is switched or the PC wakes up";
inline constexpr const char* settings_export = "Ask for the password before an export";
inline constexpr const char* settings_note =
    "These are stored in the vault, encrypted. The access log, the idle lock, the session length and the "
    "lock on the screensaver take effect; the keyring and the export settings do not yet.";
inline constexpr const char* locked_idle_fmt = "Locked after %d minutes without input.";
inline constexpr const char* locked_session_fmt = "Locked: the %d-minute session is over.";
inline constexpr const char* locked_screensaver = "Locked: the screensaver started.";
inline constexpr const char* locked_screen = "Locked: the screen was locked.";
inline constexpr const char* locked_user_switched = "Locked: the user was switched.";
inline constexpr const char* locked_resumed = "Locked: the PC woke up from sleep.";
inline constexpr const char* settings_not_numbers = "Enter whole numbers.";
inline constexpr const char* confirm_rotate =
    "Rotate the data key? Every stored value is encrypted again with a new key. The master password is "
    "asked for next.";
inline constexpr const char* rotating = "Encrypting with a new key...";
inline constexpr const char* import_action = "Import";
inline constexpr const char* import_persona_hint =
    "Every account in the file gets this persona. Accounts that are already in the vault are not changed.";
inline constexpr const char* import_reminder =
    "The CSV file still holds every password as plain text. Delete it once the import looks right.";
inline constexpr const char* import_erase = "Delete the CSV file after a successful import";
inline constexpr const char* import_erase_hint =
    "The file holds every password as plain text. It is overwritten and removed, and does not go to the "
    "recycle bin.";
inline constexpr const char* erase_source = "Delete CSV";
inline constexpr const char* source_erased = "The CSV file was overwritten and deleted.";
inline constexpr const char* erase_not_a_file = "It is not a regular file.";
inline constexpr const char* erase_write_failed = "The file could not be overwritten. It is still there.";
inline constexpr const char* erase_remove_failed =
    "The file was overwritten with zeros but could not be removed.";
inline constexpr const char* vault_settings_saved = "Vault settings saved.";
inline constexpr const char* rotate_key = "Rotate key";
inline constexpr const char* key_rotated = "The data key was rotated.";
inline constexpr const char* change_password = "Change master password";
inline constexpr const char* current_password = "Current password";
inline constexpr const char* new_password = "New password";
inline constexpr const char* repeat_password = "New password again";
inline constexpr const char* change_password_note =
    "Every stored value is encrypted again with a new key, so the old password no longer opens this file. "
    "Copies made before the change still open with the old password.";
inline constexpr const char* new_password_empty = "Enter the new password.";
inline constexpr const char* passwords_differ = "The two new passwords are not the same.";
inline constexpr const char* password_changed = "The master password was changed.";
inline constexpr const char* access_log = "Access log";
inline constexpr const char* import_csv = "Import CSV";
inline constexpr const char* imported_fmt =
    "Imported %zu accounts into %zu new services (%zu fields). Skipped: %zu already present, %zu with nothing to store.";
inline constexpr const char* import_cannot_read = "Could not read the file.";
inline constexpr const char* import_too_large = "The file is too large to import.";
inline constexpr const char* import_empty = "The file is empty.";
inline constexpr const char* import_no_secret_column = "The file has no password or TOTP column.";
inline constexpr const char* import_malformed = "The file is not valid CSV.";
inline constexpr const char* import_password_label = "Password";
inline constexpr const char* import_totp_label = "TOTP";
inline constexpr const char* import_password_type = "password";
inline constexpr const char* import_totp_type = "totp";
inline constexpr const char* import_email_label = "Email";
inline constexpr const char* import_id_label = "ID";
inline constexpr const char* import_identifier_type = "text";
inline constexpr const char* import_url_label = "URL";

inline constexpr const char* err_cannot_open_file = "Could not open the file.";
inline constexpr const char* err_already_initialized = "A vault already exists at that path.";
inline constexpr const char* err_not_initialized = "No vault found at that path.";
inline constexpr const char* err_corrupt = "The vault file is corrupt.";
inline constexpr const char* err_wrong_password = "Wrong master password.";
inline constexpr const char* err_kdf_failed = "Key derivation failed.";
inline constexpr const char* err_not_unlocked = "The vault is locked.";
inline constexpr const char* err_not_found = "Not found.";
inline constexpr const char* err_forbidden = "Not allowed for this item.";
inline constexpr const char* err_invalid_value = "A value is out of range.";
inline constexpr const char* err_scrub_failed =
    "The key was rotated, but the old data could not be removed from the file.";
inline constexpr const char* err_unexpected = "Unexpected error.";

} // namespace cypherarx::gui::text
