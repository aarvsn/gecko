// SPDX-License-Identifier: MIT
#include "core_manager_dialog.hpp"

#include "gecko/library.hpp"

#include <QCheckBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace gecko {

CoreManagerDialog::CoreManagerDialog(AppContext& context, QWidget* parent)
    : QDialog(parent), context_(context) {
    setWindowTitle(tr("Core manager"));
    resize(640, 520);

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("<b>Installed cores</b> — scanned from <code>%1</code>")
                                     .arg(QString::fromStdString(context_.coresDirectory().string())),
                                 this));

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels(
        {tr("Core"), tr("Version"), tr("Extensions"), tr("Needs full path")});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    layout->addWidget(table_, 2);

    auto* refreshButton = new QPushButton(tr("Rescan"), this);
    connect(refreshButton, &QPushButton::clicked, this, &CoreManagerDialog::refresh);
    layout->addWidget(refreshButton, 0, Qt::AlignLeft);

    layout->addWidget(new QLabel(tr("<b>Extension → core mapping</b>"), this));
    mapTable_ = new QTableWidget(this);
    mapTable_->setColumnCount(2);
    mapTable_->setHorizontalHeaderLabels({tr("Extension"), tr("Core")});
    mapTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mapTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mapTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mapTable_->setAlternatingRowColors(true);
    layout->addWidget(mapTable_, 1);

    auto* mapButton = new QPushButton(tr("Map an extension…"), this);
    connect(mapButton, &QPushButton::clicked, this, &CoreManagerDialog::mapExtensionToCore);
    layout->addWidget(mapButton, 0, Qt::AlignLeft);

    auto* closeLayout = new QHBoxLayout;
    closeLayout->addStretch();
    auto* closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    closeLayout->addWidget(closeButton);
    layout->addLayout(closeLayout);

    refresh();
}

void CoreManagerDialog::refresh() {
    cores_ = context_.scanCores();
    repopulate();
}

void CoreManagerDialog::repopulate() {
    table_->setRowCount(static_cast<int>(cores_.size()));
    for (size_t i = 0; i < cores_.size(); ++i) {
        const CoreInfo& core = cores_[i];
        table_->setItem(static_cast<int>(i), 0,
                        new QTableWidgetItem(QString::fromStdString(core.libraryName)));
        table_->setItem(static_cast<int>(i), 1,
                        new QTableWidgetItem(QString::fromStdString(core.libraryVersion)));
        table_->setItem(
            static_cast<int>(i), 2,
            new QTableWidgetItem(QString::fromStdString(core.validExtensions)));
        table_->setItem(static_cast<int>(i), 3,
                        new QTableWidgetItem(core.needFullPath ? tr("yes") : tr("no")));
        table_->item(static_cast<int>(i), 0)->setToolTip(
            QString::fromStdString(core.path.string()));
    }

    const auto mapping = context_.extensionCoreMap();
    mapTable_->setRowCount(static_cast<int>(mapping.size()));
    int row = 0;
    for (const auto& [ext, corePath] : mapping) {
        mapTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(ext)));
        mapTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(corePath)));
        ++row;
    }
}

void CoreManagerDialog::mapExtensionToCore() {
    std::filesystem::path corePath;
    if (!pickCore(this, context_, {}, &corePath)) return;
    bool ok = false;
    const QString ext = QInputDialog::getText(this, tr("Map extension"),
                                              tr("ROM extension (without dot):"),
                                              QLineEdit::Normal, QString(), &ok)
                            .toLower();
    if (!ok || ext.isEmpty()) return;
    context_.setCoreForExtension(ext.toStdString(), corePath);
    refresh();
}

bool CoreManagerDialog::pickCore(QWidget* parent, AppContext& context,
                                 const std::filesystem::path& romPath,
                                 std::filesystem::path* coreOut) {
    QDialog dialog(parent);
    dialog.setWindowTitle(romPath.empty() ? tr("Choose a core")
                                          : tr("Choose a core for %1")
                                                .arg(QString::fromStdString(
                                                    Library::prettifyTitle(
                                                        romPath.stem().string()))));
    dialog.resize(560, 400);

    auto* layout = new QVBoxLayout(&dialog);
    auto* list = new QListWidget(&dialog);
    const auto cores = context.scanCores();
    if (cores.empty()) {
        const QString hint = tr(
            "No libretro cores found in:\n%1\n\n"
            "Place core libraries (snes9x.so, gba.dll, …) in that folder, "
            "or change the cores directory in Settings.").arg(
            QString::fromStdString(context.coresDirectory().string()));
        layout->addWidget(new QLabel(hint, &dialog));
    } else {
        for (const CoreInfo& core : cores) {
            const QString label = QStringLiteral("%1 %2  (%3)")
                                      .arg(QString::fromStdString(core.libraryName),
                                           QString::fromStdString(core.libraryVersion),
                                           QString::fromStdString(core.validExtensions));
            QListWidgetItem* item = new QListWidgetItem(label, list);
            item->setData(Qt::UserRole, QString::fromStdString(core.path.string()));
        }
        layout->addWidget(list);
    }

    auto* remember = new QCheckBox(tr("Remember this choice for this extension"), &dialog);
    remember->setChecked(true);
    layout->addWidget(remember);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancelButton = new QPushButton(tr("Cancel"), &dialog);
    auto* okButton = new QPushButton(tr("Start"), &dialog);
    okButton->setDefault(true);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttons->addWidget(cancelButton);
    buttons->addWidget(okButton);
    layout->addLayout(buttons);

    if (cores.empty()) {
        okButton->setEnabled(false);
    } else {
        list->setCurrentRow(0);
        connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
    }

    if (dialog.exec() != QDialog::Accepted) return false;
    const QListWidgetItem* item = list->currentItem();
    if (!item) return false;

    const std::filesystem::path chosen = item->data(Qt::UserRole).toString().toStdString();
    if (remember->isChecked() && coreOut && !romPath.empty()) {
        std::string ext = romPath.extension().string();
        if (!ext.empty() && ext.front() == '.') ext.erase(ext.begin());
        if (!ext.empty()) context.setCoreForExtension(ext, chosen);
    }
    if (coreOut) *coreOut = chosen;
    return true;
}

} // namespace gecko
