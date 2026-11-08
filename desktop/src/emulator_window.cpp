// SPDX-License-Identifier: MIT
#include "emulator_window.hpp"

#include "overlay_menu.hpp"
#include "video_filters.hpp"

#include <QCloseEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QShortcut>

namespace gecko {

EmulatorWindow::EmulatorWindow(AppContext& context, QWidget* parent)
    : QMainWindow(parent), context_(context) {
    view_ = new EmulatorView(context_.config(), this);
    setCentralWidget(view_);
    overlay_ = new OverlayMenu(view_);

    setWindowIcon(QIcon(":/icons/gecko.svg"));
    setWindowTitle(QStringLiteral("Gecko"));

    connect(overlay_, &OverlayMenu::resumeRequested, this, [this] { view_->togglePause(); });
    // Opening the menu pauses; resuming unpauses (classic frontend behavior).
    connect(overlay_, &OverlayMenu::saveStateRequested, this,
            [this](int slot) { view_->saveState(slot); });
    connect(overlay_, &OverlayMenu::loadStateRequested, this,
            [this](int slot) { view_->loadState(slot); });
    connect(overlay_, &OverlayMenu::screenshotRequested, this,
            [this] { view_->takeScreenshot(); });
    connect(overlay_, &OverlayMenu::resetRequested, this, [this] { view_->resetGame(); });
    connect(overlay_, &OverlayMenu::cycleFilterRequested, this, [this] {
        const int next = (static_cast<int>(view_->videoFilter()) + 1) % 4;
        view_->setVideoFilter(static_cast<VideoFilter>(next));
    });
    connect(overlay_, &OverlayMenu::hudToggleRequested, this,
            [this] { view_->setHudVisible(!view_->hudVisible()); });
    connect(overlay_, &OverlayMenu::fullscreenToggleRequested, this,
            [this] { toggleFullscreen(); });
    connect(overlay_, &OverlayMenu::exitRequested, this, [this] { close(); });

    buildShortcuts();

    const QByteArray geometry =
        QByteArray::fromBase64(context_.config().getString("windows.game").c_str());
    if (!geometry.isEmpty()) restoreGeometry(geometry);
}

bool EmulatorWindow::launch(const EmulatorView::Session& session) {
    QString error;
    if (!view_->startSession(session, &error)) {
        QMessageBox::critical(this, tr("Gecko — cannot start"),
                              tr("Failed to start the game:\n\n%1").arg(error));
        return false;
    }
    const QString title = QString::fromStdString(session.title);
    setWindowTitle(QStringLiteral("Gecko — %1").arg(title));
    resize(view_->sizeHint());
    show();
    view_->setFocus();
    return true;
}

void EmulatorWindow::buildShortcuts() {
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, [this] {
        if (overlay_->isOpen()) {
            overlay_->closeMenu();
        } else {
            if (!view_->paused()) view_->togglePause();
            overlay_->openMenu();
        }
    });
    auto* fullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreenShortcut, &QShortcut::activated, this, [this] { toggleFullscreen(); });
}

bool EmulatorWindow::event(QEvent* event) {
    // Hold Tab to fast-forward. Handled here (not via QShortcut) so that
    // press AND release are both observed reliably.
    if (overlay_ && !overlay_->isOpen()) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Tab && !keyEvent->isAutoRepeat())
                view_->setFastForward(true);
        } else if (event->type() == QEvent::KeyRelease) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Tab && !keyEvent->isAutoRepeat())
                view_->setFastForward(false);
        }
    }
    return QMainWindow::event(event);
}

void EmulatorWindow::toggleFullscreen() {
    if (fullscreen_) {
        showNormal();
        fullscreen_ = false;
    } else {
        showFullScreen();
        fullscreen_ = true;
    }
}

void EmulatorWindow::closeEvent(QCloseEvent* event) {
    context_.config().set("windows.game", saveGeometry().toBase64().toStdString());
    context_.config().save();
    view_->stopSession();
    emit closed();
    QMainWindow::closeEvent(event);
}

} // namespace gecko
