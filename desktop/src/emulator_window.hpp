// SPDX-License-Identifier: MIT
// EmulatorWindow — a window hosting one running game: the EmulatorView, the
// overlay quick menu, global shortcuts and window-state persistence.

#pragma once

#include "app_context.hpp"
#include "emulator_view.hpp"

#include <QMainWindow>

namespace gecko {

class OverlayMenu;

class EmulatorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit EmulatorWindow(AppContext& context, QWidget* parent = nullptr);

    // Starts the session; returns false and shows an error dialog when the
    // core/game cannot be loaded.
    bool launch(const EmulatorView::Session& session);

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool event(QEvent* event) override;

private slots:
    void toggleFullscreen();

private:
    void buildShortcuts();

    AppContext& context_;
    EmulatorView* view_ = nullptr;
    OverlayMenu* overlay_ = nullptr;
    bool fullscreen_ = false;
    QByteArray restoreGeometry_;
};

} // namespace gecko
