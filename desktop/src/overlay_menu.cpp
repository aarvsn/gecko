// SPDX-License-Identifier: MIT
#include "overlay_menu.hpp"

#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace gecko {

OverlayMenu::OverlayMenu(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: rgba(0,0,0,110);");
    setVisible(false);
    setFocusPolicy(Qt::StrongFocus);
}

void OverlayMenu::openMenu() {
    if (!panel_) {
        panel_ = new QWidget(this);
        panel_->setObjectName("geckoOverlayPanel");
        panel_->setStyleSheet(
            "#geckoOverlayPanel { background: rgba(24,26,32,235); border-radius: 10px; }"
            "QLabel#geckoOverlayTitle { color: #8ce596; font-weight: 600; }"
            "QListWidget { background: transparent; border: none; color: #e8e8e8;"
            "              font-size: 14px; outline: none; }"
            "QListWidget::item { padding: 7px 18px; border-radius: 6px; }"
            "QListWidget::item:selected { background: #2f7d43; color: white; }");

        auto* layout = new QVBoxLayout(panel_);
        layout->setContentsMargins(4, 12, 4, 12);
        auto* title = new QLabel(tr("Gecko — quick menu"), panel_);
        title->setObjectName("geckoOverlayTitle");
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);

        list_ = new QListWidget(panel_);
        list_->setFocusPolicy(Qt::StrongFocus);

        const QStringList entries = {
            tr("Resume"),
            tr("Save state — slot 1"),
            tr("Save state — slot 2"),
            tr("Save state — slot 3"),
            tr("Load state — slot 1"),
            tr("Load state — slot 2"),
            tr("Load state — slot 3"),
            tr("Take screenshot"),
            tr("Reset game"),
            tr("Cycle video filter"),
            tr("Toggle performance HUD"),
            tr("Toggle fullscreen"),
            tr("Exit to library"),
        };
        list_->addItems(entries);
        connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem*) { acceptRow(); });

        layout->addWidget(list_);
    }

    setGeometry(parentWidget() ? parentWidget()->rect() : rect());
    centerPanel();
    panel_->show();
    show();
    raise();
    list_->setFocus();
    list_->setCurrentRow(0);
}

void OverlayMenu::closeMenu() {
    hide();
    if (parentWidget()) parentWidget()->setFocus();
}

bool OverlayMenu::isOpen() const { return isVisible(); }

void OverlayMenu::centerPanel() {
    if (!panel_) return;
    panel_->setFixedSize(320, 470);
    panel_->move((width() - panel_->width()) / 2, (height() - panel_->height()) / 2);
}

void OverlayMenu::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    centerPanel();
}

void OverlayMenu::acceptRow() {
    const int row = list_ ? list_->currentRow() : -1;
    closeMenu();
    switch (row) {
        case 0: emit resumeRequested(); break;
        case 1: emit saveStateRequested(1); break;
        case 2: emit saveStateRequested(2); break;
        case 3: emit saveStateRequested(3); break;
        case 4: emit loadStateRequested(1); break;
        case 5: emit loadStateRequested(2); break;
        case 6: emit loadStateRequested(3); break;
        case 7: emit screenshotRequested(); break;
        case 8: emit resetRequested(); break;
        case 9: emit cycleFilterRequested(); break;
        case 10: emit hudToggleRequested(); break;
        case 11: emit fullscreenToggleRequested(); break;
        case 12: emit exitRequested(); break;
        default: break;
    }
}

void OverlayMenu::keyPressEvent(QKeyEvent* event) {
    if (!list_) return QWidget::keyPressEvent(event);
    const int count = list_->count();
    switch (event->key()) {
        case Qt::Key_Up:
            list_->setCurrentRow((list_->currentRow() - 1 + count) % count);
            event->accept();
            return;
        case Qt::Key_Down:
            list_->setCurrentRow((list_->currentRow() + 1) % count);
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            acceptRow();
            event->accept();
            return;
        case Qt::Key_Escape:
            closeMenu();
            emit resumeRequested();
            event->accept();
            return;
        default:
            QWidget::keyPressEvent(event);
    }
}

} // namespace gecko
