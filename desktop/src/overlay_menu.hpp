// SPDX-License-Identifier: MIT
// OverlayMenu — the in-game quick menu (Esc). A translucent panel drawn over
// the EmulatorView; keyboard (Up/Down/Enter/Esc) and mouse both work.

#pragma once

#include <QWidget>

class QListWidget;
class QWidget;

namespace gecko {

class OverlayMenu : public QWidget {
    Q_OBJECT

public:
    explicit OverlayMenu(QWidget* parent = nullptr);

    void openMenu();
    void closeMenu();
    bool isOpen() const;

signals:
    void resumeRequested();
    void saveStateRequested(int slot);
    void loadStateRequested(int slot);
    void screenshotRequested();
    void resetRequested();
    void cycleFilterRequested();
    void hudToggleRequested();
    void fullscreenToggleRequested();
    void exitRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void acceptRow();
    void centerPanel();

    QListWidget* list_ = nullptr;
    QWidget* panel_ = nullptr;
};

} // namespace gecko
