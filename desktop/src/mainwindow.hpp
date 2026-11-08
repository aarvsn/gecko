// SPDX-License-Identifier: MIT
// MainWindow — the library browser: toolbar, game grid, menus, status bar,
// and the launch flow (core resolution + EmulatorWindow).

#pragma once

#include "app_context.hpp"
#include "library_model.hpp"

#include <QMainWindow>

class QListView;
class QLabel;
class QProgressBar;

namespace gecko {

class EmulatorWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(AppContext& context, QWidget* parent = nullptr);

    // Test hooks (used by the headless self-test to capture screenshots).
    void openGameByTitle(const QString& title);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openRom();
    void addLibraryFolder();
    void refreshLibrary();
    void showCoresDialog();
    void showSettingsDialog();
    void showAbout();
    void launchGame(const QModelIndex& index);

private:
    void buildUi();
    void applySavedGeometry();
    void updateStatusBar();
    bool resolveCoreForGame(const GameEntry& game, std::filesystem::path* coreOut);

    AppContext& context_;
    LibraryModel* model_ = nullptr;
    QListView* grid_ = nullptr;
    QLabel* statusCount_ = nullptr;
    QLabel* statusCores_ = nullptr;
    EmulatorWindow* gameWindow_ = nullptr;
};

} // namespace gecko
