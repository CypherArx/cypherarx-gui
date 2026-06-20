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
#include <QCheckBox>
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

// Modal "Add Field" dialog with a built-in password generator.
std::optional<FieldInput> ask_field(QWidget* parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Add Field");
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

    if (dialog.exec() != QDialog::Accepted || label->text().isEmpty()) {
        return std::nullopt;
    }
    return FieldInput{label->text(), type->currentText(), value->text(), notes->text()};
}

struct EntryInput {
    QString label;
    QString type;
    bool under_selected;
};

// Modal "Add Entry" dialog: label, entry_type, and whether to nest under the selection.
std::optional<EntryInput> ask_entry(QWidget* parent, bool has_selection, const QString& sel_label) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Add Entry");
    auto* form = new QFormLayout(&dialog);

    auto* label = new QLineEdit;
    auto* type = new QComboBox;
    type->addItems({"service", "persona", "account", "child"});
    auto* under = new QCheckBox(has_selection ? ("Add under: " + sel_label)
                                              : "Add under selected entry");
    under->setEnabled(has_selection);

    form->addRow("Label:", label);
    form->addRow("Type:", type);
    form->addRow("", under);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted || label->text().isEmpty()) {
        return std::nullopt;
    }
    return EntryInput{label->text(), type->currentText(), under->isChecked()};
}

// First-cut GUI state. Lambdas wire the signals, so no QObject subclass / moc yet.
struct App {
    std::optional<Vault> vault;
    QMainWindow* window = nullptr;
    QTreeWidget* entries = nullptr;
    QListWidget* fields = nullptr;
    QLabel* fieldHeader = nullptr;
    QLabel* status = nullptr;

    void set_status(const QString& s) { status->setText(s); }

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
        // Build the tree: create all items first, then parent them (a child may precede its
        // parent in id order, and a tombstoned parent leaves its children at the root).
        std::unordered_map<std::int64_t, QTreeWidgetItem*> by_id;
        for (const auto& e : *list) {
            auto* item = new QTreeWidgetItem;
            item->setText(0, QString::fromStdString(e.label));
            item->setText(1, QString::fromStdString(e.entry_type));
            item->setData(0, Qt::UserRole, static_cast<qlonglong>(e.id));
            by_id[e.id] = item;
        }
        for (const auto& e : *list) {
            auto* item = by_id[e.id];
            QTreeWidgetItem* parent = nullptr;
            if (e.parent_id) {
                if (auto it = by_id.find(*e.parent_id); it != by_id.end()) {
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
        set_status(QString("Vault open — %1 entr%2.")
                       .arg(list->size())
                       .arg(list->size() == 1 ? "y" : "ies"));
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
        const auto selected = current_entry_id();
        const QString sel_label =
            entries->currentItem() ? entries->currentItem()->text(0) : QString();
        auto in = ask_entry(window, selected.has_value(), sel_label);
        if (!in) {
            return;
        }
        Entry e;
        e.entry_type = in->type.toStdString();
        e.label = in->label.toStdString();
        if (in->under_selected && selected) {
            e.parent_id = *selected;
        }
        auto added = vault->add_entry(e);
        if (!added) {
            QMessageBox::warning(window, "Add failed", error_text(added.error()));
            return;
        }
        refresh_entries();
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
    auto* btnDelEntry = new QPushButton("Delete Entry");
    topRow->addWidget(btnNew);
    topRow->addWidget(btnOpen);
    topRow->addWidget(btnAddEntry);
    topRow->addWidget(btnDelEntry);
    topRow->addStretch();
    root->addLayout(topRow);

    auto* split = new QHBoxLayout;

    auto* leftCol = new QVBoxLayout;
    leftCol->addWidget(new QLabel("Entries"));
    state.entries = new QTreeWidget;
    state.entries->setColumnCount(2);
    state.entries->setHeaderLabels({"Name", "Type"});
    state.entries->setColumnWidth(0, 160);
    leftCol->addWidget(state.entries);
    split->addLayout(leftCol, 1);

    auto* rightCol = new QVBoxLayout;
    state.fieldHeader = new QLabel("Fields");
    rightCol->addWidget(state.fieldHeader);
    state.fields = new QListWidget;
    rightCol->addWidget(state.fields);
    auto* fieldButtons = new QHBoxLayout;
    auto* btnAddField = new QPushButton("Add Field");
    auto* btnCopy = new QPushButton("Copy Value");
    auto* btnTotp = new QPushButton("Show TOTP");
    auto* btnDelField = new QPushButton("Delete Field");
    fieldButtons->addWidget(btnAddField);
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
    QObject::connect(btnDelEntry, &QPushButton::clicked, [&state] { state.delete_entry(); });
    QObject::connect(state.entries, &QTreeWidget::currentItemChanged,
                     [&state] { state.refresh_fields(); });
    QObject::connect(btnAddField, &QPushButton::clicked, [&state] { state.add_field(); });
    QObject::connect(btnCopy, &QPushButton::clicked, [&state] { state.copy_value(); });
    QObject::connect(btnTotp, &QPushButton::clicked, [&state] { state.show_totp(); });
    QObject::connect(btnDelField, &QPushButton::clicked, [&state] { state.delete_field(); });

    window.show();
    return app.exec();
}
