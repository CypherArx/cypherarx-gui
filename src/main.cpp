#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <QApplication>
#include <QByteArray>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "cypherarx/crypto.hpp"
#include "cypherarx/entry.hpp"
#include "cypherarx/kdf.hpp"
#include "cypherarx/vault.hpp"

namespace {

using cypherarx::Entry;
using cypherarx::Vault;
using cypherarx::VaultError;

std::vector<std::uint8_t> to_bytes(const QString& s) {
    const QByteArray b = s.toUtf8();
    return {b.begin(), b.end()};
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

// Minimal first-cut GUI state. Widget signals are wired with lambdas, so no QObject
// subclass (and no moc) is needed yet.
struct App {
    std::optional<Vault> vault;
    QMainWindow* window = nullptr;
    QListWidget* entries = nullptr;
    QLabel* status = nullptr;

    void set_status(const QString& s) { status->setText(s); }

    void refresh() {
        entries->clear();
        if (!vault) {
            set_status("No vault open.");
            return;
        }
        auto list = vault->list_entries();
        if (!list) {
            set_status("Failed to read entries: " + error_text(list.error()));
            return;
        }
        for (const auto& e : *list) {
            auto* item = new QListWidgetItem(QString::fromStdString(e.label));
            item->setData(Qt::UserRole, static_cast<qlonglong>(e.id));
            entries->addItem(item);
        }
        set_status(QString("Vault open — %1 entr%2.")
                       .arg(list->size())
                       .arg(list->size() == 1 ? "y" : "ies"));
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
        auto v = Vault::create(path.toStdString(), to_bytes(*pw),
                               cypherarx::KdfParams::moderate());
        if (!v) {
            QMessageBox::warning(window, "Create failed", error_text(v.error()));
            return;
        }
        vault = std::move(*v);
        refresh();
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
        refresh();
    }

    void add_entry() {
        if (!vault) {
            QMessageBox::information(window, "No vault", "Open or create a vault first.");
            return;
        }
        bool ok = false;
        const QString label =
            QInputDialog::getText(window, "Add Entry", "Label:", QLineEdit::Normal, "", &ok);
        if (!ok || label.isEmpty()) {
            return;
        }
        Entry e;
        e.entry_type = "service";
        e.label = label.toStdString();
        auto added = vault->add_entry(e);
        if (!added) {
            QMessageBox::warning(window, "Add failed", error_text(added.error()));
            return;
        }
        refresh();
    }
};

// Headless smoke test of the linked core (no GUI). Run: cypherarx_gui --selftest
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
    if (!v->add_entry(e)) {
        std::cerr << "selftest: add_entry failed\n";
        return 1;
    }
    auto list = v->list_entries();
    if (!list || list->size() != 1 || (*list)[0].label != "Example") {
        std::cerr << "selftest: list mismatch\n";
        return 1;
    }
    fs::remove(path, ec);
    std::cout << "selftest: OK (core linked, vault round-trip works)\n";
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
    auto* layout = new QVBoxLayout(central);

    auto* buttons = new QHBoxLayout;
    auto* btnNew = new QPushButton("New Vault");
    auto* btnOpen = new QPushButton("Open Vault");
    auto* btnAdd = new QPushButton("Add Entry");
    buttons->addWidget(btnNew);
    buttons->addWidget(btnOpen);
    buttons->addWidget(btnAdd);
    buttons->addStretch();
    layout->addLayout(buttons);

    state.entries = new QListWidget;
    layout->addWidget(state.entries);

    state.status = new QLabel("No vault open.");
    layout->addWidget(state.status);

    QMainWindow window;
    window.setWindowTitle("CypherArx");
    window.setCentralWidget(central);
    window.resize(520, 420);
    state.window = &window;

    QObject::connect(btnNew, &QPushButton::clicked, [&state] { state.new_vault(); });
    QObject::connect(btnOpen, &QPushButton::clicked, [&state] { state.open_vault(); });
    QObject::connect(btnAdd, &QPushButton::clicked, [&state] { state.add_entry(); });

    window.show();
    return app.exec();
}
