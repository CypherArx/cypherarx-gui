#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "model/session.hpp"

#include "lamina_gui/controls.hpp"
#include "lamina_gui/list_view.hpp"
#include "lamina_gui/secret_field.hpp"
#include "lamina_gui/select_field.hpp"
#include "lamina_gui/sheet.hpp"

namespace cypherarx::lamina_gui {

class Host {
public:
    virtual ~Host() = default;
    virtual gui::Session& session() = 0;
    virtual gui::ClipboardPort& clipboard() = 0;
    virtual void present(Modal& modal) = 0;
    virtual void message(const std::string& text) = 0;
    virtual void confirm(const std::string& text, std::function<void()> on_yes) = 0;

    bool check(const gui::Status& status) {
        if (!status) {
            message(status.error().message);
        }
        return status.has_value();
    }
};

class PasswordSheet final : public Sheet {
public:
    PasswordSheet(const Kit& kit, Host& host);

    using Action = std::function<void(std::span<const std::uint8_t>)>;

    std::function<void()> on_started;
    void open(gui::UnlockMode mode, std::filesystem::path file);
    void ask(const std::string& title, std::filesystem::path file, Action action,
             std::function<void()> after);

protected:
    NodePtr body() override;

private:
    void submit();

    Host& host_;
    Action action_;
    std::function<void()> after_;
    std::filesystem::path file_;
    LabelC& name_;
    LabelC& folder_;
    SecretField& password_;
    ButtonC& cancel_;
    ButtonC& ok_;
};

class BusySheet final : public Sheet {
public:
    explicit BusySheet(const Kit& kit);

    void open(const std::string& title, const std::filesystem::path& file, const std::string& note);

protected:
    NodePtr body() override;

private:
    LabelC& name_;
    LabelC& folder_;
    SpinnerC& spinner_;
    LabelC& note_;
};

class SettingsSheet final : public Sheet {
public:
    SettingsSheet(const Kit& kit, Host& host);

    void open();

protected:
    NodePtr body() override;

private:
    void submit();

    Host& host_;
    LabelC& note_;
    TextFieldC& retention_;
    TextFieldC& session_;
    TextFieldC& idle_;
    CheckC& keyring_;
    CheckC& screensaver_;
    CheckC& export_;
    ButtonC& cancel_;
    ButtonC& ok_;
};

class ImportSheet final : public Sheet {
public:
    ImportSheet(const Kit& kit, Host& host);

    std::function<void(const std::filesystem::path&, std::optional<std::int64_t>, bool)> on_import;
    void open(std::filesystem::path file);

protected:
    NodePtr body() override;

private:
    void submit();

    Host& host_;
    std::filesystem::path file_;
    gui::IdLabels personas_;
    LabelC& name_;
    LabelC& folder_;
    SelectField& persona_;
    LabelC& hint_;
    CheckC& erase_;
    LabelC& erase_hint_;
    ButtonC& cancel_;
    ButtonC& ok_;
};

class ChangePasswordSheet final : public Sheet {
public:
    ChangePasswordSheet(const Kit& kit, Host& host);

    std::function<void()> on_started;
    void open(std::filesystem::path file);
    void open_new(std::filesystem::path file);

protected:
    NodePtr body() override;

private:
    void start(std::filesystem::path file, bool creating);
    void submit();
    void proceed();
    void clear();
    void refresh();

    Host& host_;
    std::filesystem::path file_;
    bool creating_ = false;
    LabelC& name_;
    LabelC& folder_;
    LabelC& note_;
    SecretField& current_;
    SecretField& next_;
    LabelC& strength_;
    SecretField& repeat_;
    ButtonC& cancel_;
    ButtonC& ok_;
};

class MergeSheet final : public Sheet {
public:
    MergeSheet(const Kit& kit, Host& host);

    void open();

protected:
    NodePtr body() override;

private:
    void submit();

    Host& host_;
    std::int64_t source_ = 0;
    gui::IdLabels targets_;
    LabelC& name_;
    SelectField& target_;
    LabelC& hint_;
    ButtonC& cancel_;
    ButtonC& ok_;
};

class EntrySheet final : public Sheet {
public:
    EntrySheet(const Kit& kit, Host& host);

    void open_new(const std::string& type);
    void open_selected();

protected:
    NodePtr body() override;

private:
    void open(gui::EntryDraft draft);
    void apply_type();
    TextFieldC& name_field();
    void refresh_tags();
    void add_tag();
    void submit();
    std::string type() const;

    Host& host_;
    gui::EntryDraft draft_;
    gui::EntryChoices choices_;
    TextFieldC& label_;
    TextFieldC& name_;
    LabelC& name_hint_;
    SelectField& type_;
    SelectField& category_;
    SelectField& service_;
    SelectField& persona_;
    SelectField& account_;
    CheckC& favorite_;
    LabelC& tags_title_;
    ChipsC& tags_;
    TextFieldC& new_tag_;
    ButtonC& add_tag_;
    ButtonC& cancel_;
    ButtonC& ok_;
};

class FieldSheet final : public Sheet {
public:
    FieldSheet(const Kit& kit, Host& host);

    void open_new();
    void open_selected();

protected:
    NodePtr body() override;

private:
    void open(gui::FieldDraft draft);
    void apply_type();
    void submit();
    void dismiss();
    std::string type() const;

    Host& host_;
    gui::FieldDraft draft_;
    std::vector<std::string> types_;
    TextFieldC& label_;
    SelectField& type_;
    SecretField& value_;
    IconButtonC& show_;
    ButtonC& generate_;
    TextFieldC& notes_;
    ButtonC& cancel_;
    ButtonC& ok_;
};

class OrganizeSheet final : public Sheet {
public:
    OrganizeSheet(const Kit& kit, Host& host);

    void open();

protected:
    NodePtr body() override;

private:
    void refresh();
    std::optional<std::int64_t> parent() const;

    Host& host_;
    std::optional<std::int64_t> selected_category_;
    std::optional<std::int64_t> selected_tag_;
    gui::IdLabels parents_;
    LabelC& categories_title_;
    ListView& categories_;
    TextFieldC& category_name_;
    SelectField& category_parent_;
    ButtonC& category_add_;
    ButtonC& category_rename_;
    ButtonC& category_delete_;
    LabelC& tags_title_;
    ListView& tags_;
    TextFieldC& tag_name_;
    ButtonC& tag_add_;
    ButtonC& tag_rename_;
    ButtonC& tag_delete_;
    ButtonC& close_;
};

class LoginSheet final : public Sheet {
public:
    LoginSheet(const Kit& kit, Host& host);

    void open();

protected:
    NodePtr body() override;

private:
    void reload();
    void refresh();
    std::string method() const;
    std::optional<std::int64_t> via() const;

    Host& host_;
    gui::LoginMethodChoices choices_;
    std::vector<LoginMethod> methods_;
    std::optional<std::int64_t> selected_;
    LabelC& hint_;
    ListView& list_;
    SelectField& method_;
    SelectField& via_;
    TextFieldC& notes_;
    ButtonC& add_;
    ButtonC& update_;
    ButtonC& delete_;
    ButtonC& close_;
};

} // namespace cypherarx::lamina_gui
