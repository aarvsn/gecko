// SPDX-License-Identifier: MIT
#include "input_mapper_dialog.hpp"

#include "input_manager.hpp"

#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

namespace gecko {

InputMapperDialog::InputMapperDialog(InputManager& input, QWidget* parent)
    : QDialog(parent), input_(input) {
    setWindowTitle(tr("Input mapping — keyboard"));
    resize(420, 520);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Click a row, then press the key you want to bind to that joypad button."), this));

    table_ = new QTableWidget(kJoypadButtonCount, 2, this);
    table_->setHorizontalHeaderLabels({tr("Joypad button"), tr("Key")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(table_);

    auto* buttons = new QHBoxLayout;
    auto* resetButton = new QPushButton(tr("Reset to defaults"), this);
    connect(resetButton, &QPushButton::clicked, this, &InputMapperDialog::resetToDefaults);
    auto* doneButton = new QPushButton(tr("Done"), this);
    connect(doneButton, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(resetButton);
    buttons->addStretch();
    buttons->addWidget(doneButton);
    layout->addLayout(buttons);

    repopulate();
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (column == 1 && row >= 0) captureKey();
    });
    table_->setToolTip(tr("Double-click a key cell to rebind"));
}

void InputMapperDialog::repopulate() {
    for (unsigned i = 0; i < kJoypadButtonCount; ++i) {
        auto* buttonItem =
            new QTableWidgetItem(QString::fromLatin1(kJoypadButtonNames[i]));
        buttonItem->setFlags(buttonItem->flags() & ~Qt::ItemIsSelectable);
        table_->setItem(static_cast<int>(i), 0, buttonItem);
        auto* keyItem = new QTableWidgetItem(InputManager::keyName(input_.binding(i)));
        keyItem->setData(Qt::UserRole, static_cast<int>(i));
        table_->setItem(static_cast<int>(i), 1, keyItem);
    }
}

void InputMapperDialog::captureKey() {
    setWindowTitle(tr("Input mapping — press any key…"));
    grabKeyboard();
    setCursor(Qt::CrossCursor);
}

void InputMapperDialog::resetToDefaults() {
    for (unsigned i = 0; i < kJoypadButtonCount; ++i)
        input_.setBinding(i, InputManager::kDefaultKeys[i]);
    input_.saveBindings();
    repopulate();
}

bool InputMapperDialog::event(QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const int row = table_->currentRow();
        if (row >= 0) {
            const int key = keyEvent->key();
            const bool cancel = (key == Qt::Key_Escape);
            if (!cancel) {
                input_.setBinding(static_cast<unsigned>(row), key);
                input_.saveBindings();
                table_->item(row, 1)->setText(InputManager::keyName(key));
            }
            releaseKeyboard();
            unsetCursor();
            setWindowTitle(tr("Input mapping — keyboard"));
            event->accept();
            return true;
        }
    }
    return QDialog::event(event);
}

} // namespace gecko
