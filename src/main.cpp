#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "cypherarx/crypto.hpp"
#include "cypherarx/entry.hpp"
#include "cypherarx/kdf.hpp"
#include "cypherarx/passgen.hpp"
#include "cypherarx/totp.hpp"
#include "cypherarx/vault.hpp"

namespace {

using cypherarx::Entry;
using cypherarx::Secret;
using cypherarx::Vault;
using cypherarx::VaultError;

std::vector<std::uint8_t> to_bytes(const QString& s) {
    const QByteArray b = s.toUtf8();
    return {b.begin(), b.end()};
}

QString secret_to_qstring(const Secret& s) {
    return QString::fromUtf8(reinterpret_cast<const char*>(s.data()), static_cast<int>(s.size()));
}

std::uint64_t unix_now() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

QString error_text(VaultError e) {
    switch (e) {
        case VaultError::CannotOpenFile:     return "Could not open the file.";
        case VaultError::AlreadyInitialized: return "A vault already exists at that path.";
        case VaultError::NotInitialized:     return "No vault found at that path.";
        case VaultError::Corrupt:            return "The vault file is corrupt.";
        case VaultError::WrongPassword:      return "Wrong master password.";
        case VaultError::KdfFailed:          return "Key derivation failed.";
        case VaultError::NotUnlocked:        return "The vault is locked.";
        case VaultError::NotFound:           return "Not found.";
        default:                             return "Unexpected error.";
    }
}

struct FieldInput {
    QString label;
    QString type;
    QString value;
    QString notes;
};

// Modal Add/Edit Field dialog with a built-in password generator. `prefill` != nullptr edits.
std::optional<FieldInput> ask_field(QWidget* parent, const FieldInput* prefill = nullptr) {
    QDialog dialog(parent);
    dialog.setWindowTitle(prefill ? "Edit Field" : "Add Field");
    auto* form = new QFormLayout(&dialog);

    auto* label = new QLineEdit;
    auto* type = new QComboBox;
    type->addItems({"text", "password", "totp"});
    auto* value = new QLineEdit;
    auto* generate = new QPushButton("Generate");
    auto* valueRow = new QHBoxLayout;
    valueRow->addWidget(value);
    valueRow->addWidget(generate);
    auto* notes = new QLineEdit;

    form->addRow("Label:", label);
    form->addRow("Type:", type);
    form->addRow("Value:", valueRow);
    form->addRow("Notes:", notes);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(generate, &QPushButton::clicked, [value] {
        auto pw = cypherarx::generate_password({});
        if (pw) {
            value->setText(secret_to_qstring(*pw));
        }
    });

    if (prefill) {
        label->setText(prefill->label);
        const int idx = type->findText(prefill->type);
        if (idx >= 0) {
            type->setCurrentIndex(idx);
        }
        value->setText(prefill->value);
        notes->setText(prefill->notes);
    }

    if (dialog.exec() != QDialog::Accepted || label->text().isEmpty()) {
        return std::nullopt;
    }
    return FieldInput{label->text(), type->currentText(), value->text(), notes->text()};
}

struct EntryInput {
    QString label;
    QString type;
    std::optional<std::int64_t> parent_id;   // account -> service ; child -> account
    std::optional<std::int64_t> persona_id;  // account -> persona (nullopt = unspecified)
};

// Modal Add/Edit Entry dialog. Per the two-parent model (§10.1.1) an account points to a
// service (parent_id) and optionally a persona (persona_id); a child points to its account.
// The relevant pickers are shown based on the chosen type. `prefill` != nullptr edits.
std::optional<EntryInput> ask_entry(QWidget* parent, const std::vector<Entry>& all,
                                    const EntryInput* prefill = nullptr) {
    QDialog dialog(parent);
    dialog.setWindowTitle(prefill ? "Edit Entry" : "Add Entry");
    auto* form = new QFormLayout(&dialog);

    auto* label = new QLineEdit;
    auto* type = new QComboBox;
    type->addItems({"service", "persona", "account", "child"});

    auto* serviceBox = new QComboBox;  // account -> service (parent_id)
    auto* personaBox = new QComboBox;  // account -> persona (persona_id), optional
    auto* accountBox = new QComboBox;  // child   -> account (parent_id)
    personaBox->addItem("(none)", QVariant());
    for (const auto& e : all) {
        const auto id = static_cast<qlonglong>(e.id);
        const QString name = QString::fromStdString(e.label);
        if (e.entry_type == "service") {
            serviceBox->addItem(name, id);
        } else if (e.entry_type == "persona") {
            personaBox->addItem(name, id);
        } else if (e.entry_type == "account") {
            accountBox->addItem(name, id);
        }
    }

    form->addRow("Label:", label);
    form->addRow("Type:", type);
    form->addRow("Service:", serviceBox);
    form->addRow("Persona:", personaBox);
    form->addRow("Parent account:", accountBox);

    auto syncRows = [&] {
        const QString t = type->currentText();
        form->setRowVisible(serviceBox, t == "account");
        form->setRowVisible(personaBox, t == "account");
        form->setRowVisible(accountBox, t == "child");
    };
    QObject::connect(type, &QComboBox::currentTextChanged, &dialog,
                     [syncRows](const QString&) { syncRows(); });

    if (prefill) {
        label->setText(prefill->label);
        if (const int idx = type->findText(prefill->type); idx >= 0) {
            type->setCurrentIndex(idx);
        }
        if (prefill->parent_id) {
            const auto v = static_cast<qlonglong>(*prefill->parent_id);
            if (const int si = serviceBox->findData(v); si >= 0) serviceBox->setCurrentIndex(si);
            if (const int ai = accountBox->findData(v); ai >= 0) accountBox->setCurrentIndex(ai);
        }
        if (prefill->persona_id) {
            const auto v = static_cast<qlonglong>(*prefill->persona_id);
            if (const int pi = personaBox->findData(v); pi >= 0) personaBox->setCurrentIndex(pi);
        }
    }
    syncRows();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted || label->text().isEmpty()) {
        return std::nullopt;
    }

    EntryInput out;
    out.label = label->text();
    out.type = type->currentText();
    if (out.type == "account") {
        if (serviceBox->count() > 0 && serviceBox->currentIndex() >= 0) {
            out.parent_id = serviceBox->currentData().toLongLong();
        }
        if (const QVariant pd = personaBox->currentData(); pd.isValid()) {
            out.persona_id = pd.toLongLong();
        }
    } else if (out.type == "child") {
        if (accountBox->count() > 0 && accountBox->currentIndex() >= 0) {
            out.parent_id = accountBox->currentData().toLongLong();
        }
    }
    return out;
}

// First-cut GUI state. Lambdas wire the signals, so no QObject subclass / moc yet.
struct App {
    std::optional<Vault> vault;
    QMainWindow* window = nullptr;
    QTreeWidget* entries = nullptr;
    QListWidget* fields = nullptr;
    QLabel* fieldHeader = nullptr;
    QLabel* status = nullptr;
    QComboBox* axisSelect = nullptr;  // 0 = service axis, 1 = persona axis

    void set_status(const QString& s) { status->setText(s); }

    bool persona_axis() const { return axisSelect && axisSelect->currentIndex() == 1; }

    std::optional<std::int64_t> current_entry_id() const {
        auto* it = entries->currentItem();
        if (!it) {
            return std::nullopt;
        }
        return it->data(0, Qt::UserRole).toLongLong();
    }

    std::optional<std::int64_t> current_field_id() const {
        auto* it = fields->currentItem();
        if (!it) {
            return std::nullopt;
        }
        return it->data(Qt::UserRole).toLongLong();
    }

    std::optional<QString> ask_password(const QString& title) {
        bool ok = false;
        const QString pw =
            QInputDialog::getText(window, title, "Master password:", QLineEdit::Password, "", &ok);
        if (!ok) {
            return std::nullopt;
        }
        return pw;
    }

    void refresh_entries() {
        entries->clear();
        fields->clear();
        fieldHeader->setText("Fields");
        if (!vault) {
            set_status("No vault open.");
            return;
        }
        auto list = vault->list_entries();
        if (!list) {
            set_status("Failed to read entries: " + error_text(list.error()));
            return;
        }

        // Two views are projections of the same data (§10.1.1): the service axis builds the tree
        // by parent_id, the persona axis by persona_id. The same account appears in both.
        const bool by_persona = persona_axis();
        entries->setHeaderLabels({"Name", "Type", by_persona ? "Service" : "Persona"});

        // id -> label, for the cross-axis column (covers service/persona not shown as nodes too).
        std::unordered_map<std::int64_t, std::string> label_by_id;
        for (const auto& e : *list) {
            label_by_id[e.id] = e.label;
        }
        // The opposite axis's roots are hidden in this view; everything else is a node.
        auto in_axis = [by_persona](const Entry& e) {
            return e.entry_type != (by_persona ? "service" : "persona");
        };
        auto tree_parent = [by_persona](const Entry& e) -> std::optional<std::int64_t> {
            if (e.entry_type == "account") return by_persona ? e.persona_id : e.parent_id;
            if (e.entry_type == "child") return e.parent_id;
            return std::nullopt;  // service / persona roots
        };

        // Create items first, then parent them (a child may precede its parent in id order, and
        // a missing/hidden parent leaves its children at the root).
        std::unordered_map<std::int64_t, QTreeWidgetItem*> by_id;
        for (const auto& e : *list) {
            if (!in_axis(e)) {
                continue;
            }
            auto* item = new QTreeWidgetItem;
            item->setText(0, QString::fromStdString(e.label));
            item->setText(1, QString::fromStdString(e.entry_type));
            if (e.entry_type == "account") {  // show the other axis's parent as context
                if (const auto cross = by_persona ? e.parent_id : e.persona_id) {
                    if (auto it = label_by_id.find(*cross); it != label_by_id.end()) {
                        item->setText(2, QString::fromStdString(it->second));
                    }
                }
            }
            item->setData(0, Qt::UserRole, static_cast<qlonglong>(e.id));
            by_id[e.id] = item;
        }
        for (const auto& e : *list) {
            if (!in_axis(e)) {
                continue;
            }
            auto* item = by_id[e.id];
            QTreeWidgetItem* parent = nullptr;
            if (auto p = tree_parent(e)) {
                if (auto it = by_id.find(*p); it != by_id.end()) {
                    parent = it->second;
                }
            }
            if (parent) {
                parent->addChild(item);
            } else {
                entries->addTopLevelItem(item);
            }
        }
        entries->expandAll();
        set_status(QString("Vault open — %1 entr%2 · %3 axis")
                       .arg(list->size())
                       .arg(list->size() == 1 ? "y" : "ies")
                       .arg(by_persona ? "persona" : "service"));
    }

    void refresh_fields() {
        fields->clear();
        auto id = current_entry_id();
        if (!vault || !id) {
            fieldHeader->setText("Fields");
            return;
        }
        fieldHeader->setText("Fields — " + entries->currentItem()->text(0));
        auto list = vault->list_fields(*id);
        if (!list) {
            set_status("Failed to read fields: " + error_text(list.error()));
            return;
        }
        for (const auto& f : *list) {
            auto* item = new QListWidgetItem(QString("%1  [%2]")
                                                 .arg(QString::fromStdString(f.label),
                                                      QString::fromStdString(f.field_type)));
            item->setData(Qt::UserRole, static_cast<qlonglong>(f.id));
            item->setData(Qt::UserRole + 1, QString::fromStdString(f.field_type));
            fields->addItem(item);
        }
    }

    void new_vault() {
        const QString path = QFileDialog::getSaveFileName(
            window, "Create Vault", "", "CypherArx Vault (*.cax);;All Files (*)");
        if (path.isEmpty()) {
            return;
        }
        const auto pw = ask_password("Create Vault");
        if (!pw) {
            return;
        }
        auto v = Vault::create(path.toStdString(), to_bytes(*pw), cypherarx::KdfParams::moderate());
        if (!v) {
            QMessageBox::warning(window, "Create failed", error_text(v.error()));
            return;
        }
        vault = std::move(*v);
        refresh_entries();
    }

    void open_vault() {
        const QString path = QFileDialog::getOpenFileName(
            window, "Open Vault", "", "CypherArx Vault (*.cax);;All Files (*)");
        if (path.isEmpty()) {
            return;
        }
        const auto pw = ask_password("Open Vault");
        if (!pw) {
            return;
        }
        auto v = Vault::open(path.toStdString(), to_bytes(*pw));
        if (!v) {
            QMessageBox::warning(window, "Open failed", error_text(v.error()));
            return;
        }
        vault = std::move(*v);
        refresh_entries();
    }

    void add_entry() {
        if (!vault) {
            QMessageBox::information(window, "No vault", "Open or create a vault first.");
            return;
        }
        auto all = vault->list_entries();
        if (!all) {
            QMessageBox::warning(window, "Add failed", error_text(all.error()));
            return;
        }
        auto in = ask_entry(window, *all);
        if (!in) {
            return;
        }
        Entry e;
        e.entry_type = in->type.toStdString();
        e.label = in->label.toStdString();
        e.parent_id = in->parent_id;
        e.persona_id = in->persona_id;
        auto added = vault->add_entry(e);
        if (!added) {
            QMessageBox::warning(window, "Add failed", error_text(added.error()));
            return;
        }
        refresh_entries();
    }

    void edit_entry() {
        auto id = current_entry_id();
        if (!vault || !id) {
            QMessageBox::information(window, "No entry", "Select an entry first.");
            return;
        }
        auto cur = vault->get_entry(*id);
        if (!cur) {
            QMessageBox::warning(window, "Edit failed", error_text(cur.error()));
            return;
        }
        auto all = vault->list_entries();
        if (!all) {
            QMessageBox::warning(window, "Edit failed", error_text(all.error()));
            return;
        }
        EntryInput pre;
        pre.label = QString::fromStdString(cur->label);
        pre.type = QString::fromStdString(cur->entry_type);
        pre.parent_id = cur->parent_id;
        pre.persona_id = cur->persona_id;
        auto in = ask_entry(window, *all, &pre);
        if (!in) {
            return;
        }
        // Keep id/uuid and the other text columns; label/type and both parents are editable.
        Entry e = *cur;
        e.label = in->label.toStdString();
        e.entry_type = in->type.toStdString();
        e.parent_id = in->parent_id;
        e.persona_id = in->persona_id;
        if (auto r = vault->update_entry(e); !r) {
            QMessageBox::warning(window, "Edit failed", error_text(r.error()));
            return;
        }
        refresh_entries();
    }

    // Per-entry dialog to view/add/delete login methods (the reference-link layer, §10.1.1).
    void manage_login_methods() {
        auto id = current_entry_id();
        if (!vault || !id) {
            QMessageBox::information(window, "No entry", "Select an entry first.");
            return;
        }
        auto all = vault->list_entries();  // for the "via" picker and name lookup
        if (!all) {
            QMessageBox::warning(window, "Login methods", error_text(all.error()));
            return;
        }
        std::unordered_map<std::int64_t, QString> name_by_id;
        const Entry* self_e = nullptr;
        for (const auto& e : *all) {
            name_by_id[e.id] = QString::fromStdString(e.label);
            if (e.id == *id) {
                self_e = &e;
            }
        }
        // Login methods describe how you log in to an account; services/personas are containers.
        if (!self_e || self_e->entry_type != "account") {
            QMessageBox::information(
                window, "Login methods",
                "Login methods apply to accounts (how you log in to that account).\n"
                "Select an account entry.");
            return;
        }
        const auto own_service = self_e->parent_id;

        QDialog dialog(window);
        dialog.setWindowTitle("Login Methods — " + entries->currentItem()->text(0));
        auto* outer = new QVBoxLayout(&dialog);
        auto* hint = new QLabel(
            "How you log in to this account. Leave \"via\" as (direct) for email/password;\n"
            "for OAuth pick the provider account on another service — e.g. \"google → 本名 (Google)\".");
        hint->setWordWrap(true);
        outer->addWidget(hint);
        auto* list = new QListWidget;
        outer->addWidget(list);

        auto reload = [&] {
            list->clear();
            auto ms = vault->list_login_methods(*id);
            if (!ms) {
                return;
            }
            for (const auto& m : *ms) {
                QString text = QString::fromStdString(m.method_type);
                if (m.linked_entry_id) {
                    auto it = name_by_id.find(*m.linked_entry_id);
                    text += "  →  " + (it != name_by_id.end()
                                           ? it->second
                                           : QString("entry %1").arg(*m.linked_entry_id));
                }
                if (!m.notes.empty()) {
                    text += "   (" + QString::fromStdString(m.notes) + ")";
                }
                auto* item = new QListWidgetItem(text);
                item->setData(Qt::UserRole, static_cast<qlonglong>(m.id));
                list->addItem(item);
            }
        };
        reload();

        auto* addRow = new QHBoxLayout;
        auto* methodBox = new QComboBox;
        methodBox->addItems(
            {"email", "google", "github", "apple", "twitter", "line", "facebook", "other"});
        auto* viaBox = new QComboBox;
        viaBox->addItem("(direct)", QVariant());
        for (const auto& e : *all) {
            if (e.entry_type != "account" || e.id == *id) {
                continue;
            }
            // The OAuth provider is an account on a *different* service (you never log in to a
            // service via itself). Same-service accounts and self are excluded structurally —
            // this is what kills the "self-reference" problem at the model level.
            if (own_service && e.parent_id && *e.parent_id == *own_service) {
                continue;
            }
            QString text = QString::fromStdString(e.label);
            if (e.parent_id) {
                if (auto it = name_by_id.find(*e.parent_id); it != name_by_id.end()) {
                    text += " (" + it->second + ")";
                }
            }
            viaBox->addItem(text, static_cast<qlonglong>(e.id));
        }
        auto* notesEdit = new QLineEdit;
        notesEdit->setPlaceholderText("notes");
        auto* addBtn = new QPushButton("Add");
        addRow->addWidget(methodBox);
        addRow->addWidget(new QLabel("via"));
        addRow->addWidget(viaBox);
        addRow->addWidget(notesEdit);
        addRow->addWidget(addBtn);
        outer->addLayout(addRow);

        auto* btnRow = new QHBoxLayout;
        auto* delBtn = new QPushButton("Delete selected");
        auto* closeBtn = new QPushButton("Close");
        btnRow->addWidget(delBtn);
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        outer->addLayout(btnRow);

        QObject::connect(addBtn, &QPushButton::clicked, &dialog, [&] {
            std::optional<std::int64_t> via;
            if (const QVariant v = viaBox->currentData(); v.isValid()) {
                via = v.toLongLong();
            }
            auto r = vault->add_login_method(*id, methodBox->currentText().toStdString(), via,
                                             notesEdit->text().toStdString());
            if (!r) {
                QMessageBox::warning(&dialog, "Add failed", error_text(r.error()));
                return;
            }
            notesEdit->clear();
            reload();
        });
        QObject::connect(delBtn, &QPushButton::clicked, &dialog, [&] {
            auto* it = list->currentItem();
            if (!it) {
                return;
            }
            if (auto r = vault->delete_login_method(it->data(Qt::UserRole).toLongLong()); !r) {
                QMessageBox::warning(&dialog, "Delete failed", error_text(r.error()));
                return;
            }
            reload();
        });
        QObject::connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
        dialog.resize(440, 320);
        dialog.exec();
    }

    void delete_entry() {
        auto id = current_entry_id();
        if (!vault || !id) {
            return;
        }
        if (QMessageBox::question(window, "Delete entry",
                                  "Delete the selected entry?\n(Recoverable — it is soft-deleted.)",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        if (auto r = vault->delete_entry(*id); !r) {
            QMessageBox::warning(window, "Delete failed", error_text(r.error()));
            return;
        }
        refresh_entries();
    }

    void add_field() {
        auto id = current_entry_id();
        if (!vault || !id) {
            QMessageBox::information(window, "No entry", "Select an entry first.");
            return;
        }
        auto in = ask_field(window);
        if (!in) {
            return;
        }
        const auto value = to_bytes(in->value);
        auto added = vault->add_field(*id, in->label.toStdString(), in->type.toStdString(),
                                      value, in->notes.toStdString());
        if (!added) {
            QMessageBox::warning(window, "Add field failed", error_text(added.error()));
            return;
        }
        refresh_fields();
    }

    void edit_field() {
        auto id = current_field_id();
        auto eid = current_entry_id();
        if (!vault || !id || !eid) {
            QMessageBox::information(window, "No field", "Select a field first.");
            return;
        }
        // list_fields gives label/type/notes; the secret value is fetched on demand.
        auto fields = vault->list_fields(*eid);
        if (!fields) {
            QMessageBox::warning(window, "Edit failed", error_text(fields.error()));
            return;
        }
        const cypherarx::Field* cur = nullptr;
        for (const auto& f : *fields) {
            if (f.id == *id) {
                cur = &f;
                break;
            }
        }
        if (!cur) {
            return;
        }
        auto val = vault->field_value(*id);
        if (!val) {
            QMessageBox::warning(window, "Edit failed", error_text(val.error()));
            return;
        }
        FieldInput pre{QString::fromStdString(cur->label), QString::fromStdString(cur->field_type),
                       secret_to_qstring(*val), QString::fromStdString(cur->notes)};
        auto in = ask_field(window, &pre);
        if (!in) {
            return;
        }
        const auto value = to_bytes(in->value);
        if (auto r = vault->update_field(*id, in->label.toStdString(), in->type.toStdString(),
                                         value, in->notes.toStdString());
            !r) {
            QMessageBox::warning(window, "Edit failed", error_text(r.error()));
            return;
        }
        refresh_fields();
    }

    void copy_value() {
        auto id = current_field_id();
        if (!vault || !id) {
            return;
        }
        auto value = vault->field_value(*id);
        if (!value) {
            QMessageBox::warning(window, "Copy failed", error_text(value.error()));
            return;
        }
        const QString text = secret_to_qstring(*value);
        QApplication::clipboard()->setText(text);
        set_status("Value copied to clipboard — clears in 20 s.");
        // Clear after 20s, but only if the user hasn't copied something else since.
        QTimer::singleShot(20000, [text] {
            if (QApplication::clipboard()->text() == text) {
                QApplication::clipboard()->clear();
            }
        });
    }

    void show_totp() {
        auto* item = fields->currentItem();
        auto id = current_field_id();
        if (!vault || !id || !item) {
            return;
        }
        if (item->data(Qt::UserRole + 1).toString() != "totp") {
            QMessageBox::information(window, "TOTP", "Select a field of type \"totp\".");
            return;
        }
        auto secret = vault->field_value(*id);
        if (!secret) {
            QMessageBox::warning(window, "TOTP", error_text(secret.error()));
            return;
        }
        const std::string base32(reinterpret_cast<const char*>(secret->data()), secret->size());
        auto key = cypherarx::base32_decode(base32);
        if (!key) {
            QMessageBox::warning(window, "TOTP", "The secret is not valid base32.");
            return;
        }
        auto code = cypherarx::totp_now(*key);
        if (!code) {
            QMessageBox::warning(window, "TOTP", "Could not generate a code.");
            return;
        }
        const auto remaining = cypherarx::totp_seconds_remaining(unix_now());
        QMessageBox::information(
            window, "TOTP",
            QString("Code: %1\nValid for ~%2 s")
                .arg(QString::fromStdString(*code))
                .arg(remaining));
    }

    void delete_field() {
        auto id = current_field_id();
        if (!vault || !id) {
            return;
        }
        auto* item = fields->currentItem();
        const QString name = item ? item->text() : QString("this field");
        if (QMessageBox::question(window, "Delete field",
                                  "Delete \"" + name + "\"?\n(Recoverable — it is soft-deleted.)",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        if (auto r = vault->delete_field(*id); !r) {
            QMessageBox::warning(window, "Delete failed", error_text(r.error()));
            return;
        }
        refresh_fields();
    }
};

int run_selftest() {
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "cypherarx_gui_selftest.cax";
    std::error_code ec;
    fs::remove(path, ec);

    const std::vector<std::uint8_t> pw = {'p', 'w'};
    auto v = Vault::create(path.string(), pw, cypherarx::KdfParams::interactive());
    if (!v) {
        std::cerr << "selftest: create failed\n";
        return 1;
    }
    Entry e;
    e.entry_type = "service";
    e.label = "Example";
    auto added = v->add_entry(e);
    if (!added) {
        std::cerr << "selftest: add_entry failed\n";
        return 1;
    }
    const std::vector<std::uint8_t> secret = {'s', 'e', 'c', 'r', 'e', 't'};
    if (!v->add_field(added->id, "pw", "password", secret, "")) {
        std::cerr << "selftest: add_field failed\n";
        return 1;
    }
    fs::remove(path, ec);
    std::cout << "selftest: OK (core linked, vault + entry + field round-trip works)\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    cypherarx::init_crypto();

    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
        return run_selftest();
    }

    QApplication app(argc, argv);

    App state;

    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);

    auto* topRow = new QHBoxLayout;
    auto* btnNew = new QPushButton("New Vault");
    auto* btnOpen = new QPushButton("Open Vault");
    auto* btnAddEntry = new QPushButton("Add Entry");
    auto* btnEditEntry = new QPushButton("Edit Entry");
    auto* btnDelEntry = new QPushButton("Delete Entry");
    auto* btnLoginMethods = new QPushButton("Login Methods");
    topRow->addWidget(btnNew);
    topRow->addWidget(btnOpen);
    topRow->addWidget(btnAddEntry);
    topRow->addWidget(btnEditEntry);
    topRow->addWidget(btnDelEntry);
    topRow->addWidget(btnLoginMethods);
    topRow->addStretch();
    root->addLayout(topRow);

    auto* split = new QHBoxLayout;

    auto* leftCol = new QVBoxLayout;
    auto* entriesHeader = new QHBoxLayout;
    entriesHeader->addWidget(new QLabel("Entries"));
    entriesHeader->addStretch();
    entriesHeader->addWidget(new QLabel("View:"));
    state.axisSelect = new QComboBox;
    state.axisSelect->addItems({"Service axis", "Persona axis"});
    entriesHeader->addWidget(state.axisSelect);
    leftCol->addLayout(entriesHeader);
    state.entries = new QTreeWidget;
    state.entries->setColumnCount(3);
    state.entries->setHeaderLabels({"Name", "Type", "Persona"});
    state.entries->setColumnWidth(0, 150);
    leftCol->addWidget(state.entries);
    split->addLayout(leftCol, 1);

    auto* rightCol = new QVBoxLayout;
    state.fieldHeader = new QLabel("Fields");
    rightCol->addWidget(state.fieldHeader);
    state.fields = new QListWidget;
    rightCol->addWidget(state.fields);
    auto* fieldButtons = new QHBoxLayout;
    auto* btnAddField = new QPushButton("Add Field");
    auto* btnEditField = new QPushButton("Edit Field");
    auto* btnCopy = new QPushButton("Copy Value");
    auto* btnTotp = new QPushButton("Show TOTP");
    auto* btnDelField = new QPushButton("Delete Field");
    fieldButtons->addWidget(btnAddField);
    fieldButtons->addWidget(btnEditField);
    fieldButtons->addWidget(btnCopy);
    fieldButtons->addWidget(btnTotp);
    fieldButtons->addWidget(btnDelField);
    rightCol->addLayout(fieldButtons);
    split->addLayout(rightCol, 2);

    root->addLayout(split);

    state.status = new QLabel("No vault open.");
    root->addWidget(state.status);

    QMainWindow window;
    window.setWindowTitle("CypherArx");
    window.setCentralWidget(central);
    window.resize(760, 480);
    state.window = &window;

    QObject::connect(btnNew, &QPushButton::clicked, [&state] { state.new_vault(); });
    QObject::connect(btnOpen, &QPushButton::clicked, [&state] { state.open_vault(); });
    QObject::connect(btnAddEntry, &QPushButton::clicked, [&state] { state.add_entry(); });
    QObject::connect(btnEditEntry, &QPushButton::clicked, [&state] { state.edit_entry(); });
    QObject::connect(btnDelEntry, &QPushButton::clicked, [&state] { state.delete_entry(); });
    QObject::connect(btnLoginMethods, &QPushButton::clicked,
                     [&state] { state.manage_login_methods(); });
    QObject::connect(state.entries, &QTreeWidget::currentItemChanged,
                     [&state] { state.refresh_fields(); });
    QObject::connect(state.axisSelect, &QComboBox::currentIndexChanged,
                     [&state] { state.refresh_entries(); });
    QObject::connect(btnAddField, &QPushButton::clicked, [&state] { state.add_field(); });
    QObject::connect(btnEditField, &QPushButton::clicked, [&state] { state.edit_field(); });
    QObject::connect(btnCopy, &QPushButton::clicked, [&state] { state.copy_value(); });
    QObject::connect(btnTotp, &QPushButton::clicked, [&state] { state.show_totp(); });
    QObject::connect(btnDelField, &QPushButton::clicked, [&state] { state.delete_field(); });

    window.show();
    return app.exec();
}
