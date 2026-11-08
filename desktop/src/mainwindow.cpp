// SPDX-License-Identifier: MIT
#include "mainwindow.hpp"

#include "core_manager_dialog.hpp"
#include "emulator_window.hpp"
#include "input_manager.hpp"
#include "settings_dialog.hpp"
#include "gecko/log.hpp"
#include "gecko/version.hpp"

#include <QCloseEvent>
#include <QFileDialog>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

#include <algorithm>

namespace gecko {

MainWindow::MainWindow(AppContext& context, QWidget* parent)
    : QMainWindow(parent), context_(context) {
    buildUi();
    applySavedGeometry();
    context_.ensureDirectories();
    refreshLibrary();
}

void MainWindow::buildUi() {
    setWindowIcon(QIcon(":/icons/gecko.svg"));
    setWindowTitle(QStringLiteral("Gecko %1").arg(versionString()));

    // --- menu bar ---
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAction = fileMenu->addAction(tr("&Open ROM…"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openRom);
    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction(tr("E&xit"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    QMenu* libraryMenu = menuBar()->addMenu(tr("&Library"));
    QAction* refreshAction = libraryMenu->addAction(tr("&Refresh library"));
    refreshAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshLibrary);
    QAction* addFolderAction = libraryMenu->addAction(tr("&Add folder…"));
    connect(addFolderAction, &QAction::triggered, this, &MainWindow::addLibraryFolder);

    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction* coresAction = toolsMenu->addAction(tr("&Core manager…"));
    connect(coresAction, &QAction::triggered, this, &MainWindow::showCoresDialog);
    QAction* settingsAction = toolsMenu->addAction(tr("&Settings…"));
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettingsDialog);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* aboutAction = helpMenu->addAction(tr("&About Gecko"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    // --- toolbar ---
    QToolBar* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->addAction(openAction);
    toolbar->addAction(refreshAction);
    toolbar->addAction(addFolderAction);
    toolbar->addSeparator();
    toolbar->addAction(coresAction);
    toolbar->addAction(settingsAction);

    // --- game grid ---
    model_ = new LibraryModel(this);
    grid_ = new QListView(this);
    grid_->setModel(model_);
    grid_->setViewMode(QListView::IconMode);
    grid_->setResizeMode(QListView::Adjust);
    grid_->setMovement(QListView::Static);
    grid_->setUniformItemSizes(true);
    grid_->setGridSize(QSize(224, 200));
    grid_->setIconSize(QSize(200, 150));
    grid_->setWordWrap(true);
    grid_->setSelectionMode(QAbstractItemView::SingleSelection);
    grid_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    grid_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    grid_->setStyleSheet(
        "QListView { background: palette(window); }"
        "QListView::item { padding: 4px; border-radius: 6px; }"
        "QListView::item:selected { background: palette(highlight); color: palette(highlighted-text); }");
    setCentralWidget(grid_);

    connect(grid_, &QListView::doubleClicked, this, &MainWindow::launchGame);
    connect(grid_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& index, const QModelIndex&) {
                if (index.isValid() && statusCount_)
                    statusCount_->setText(tr("%1 — %2")
                                              .arg(QString::fromStdString(model_->gameAt(index)
                                                                              ->title))
                                              .arg(tr("ready")));
            });

    // --- status bar ---
    statusCount_ = new QLabel(this);
    statusCores_ = new QLabel(this);
    statusBar()->addWidget(statusCount_, 1);
    statusBar()->addPermanentWidget(statusCores_);
}

void MainWindow::applySavedGeometry() {
    const QByteArray geometry =
        QByteArray::fromBase64(context_.config().getString("windows.main").c_str());
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    } else {
        resize(960, 640);
    }
}

void MainWindow::updateStatusBar() {
    statusCount_->setText(tr("%1 game(s) in library").arg(model_->rowCount()));
    const size_t cores = context_.scanCores().size();
    statusCores_->setText(tr("%1 core(s) in %2")
                              .arg(cores)
                              .arg(QString::fromStdString(context_.coresDirectory().string())));
}

void MainWindow::refreshLibrary() {
    context_.rescanLibrary();
    model_->setGames(context_.games());
    updateStatusBar();
}

void MainWindow::openRom() {
    const QString filter = tr("Game ROMs and applications (*)");
    const QString path = QFileDialog::getOpenFileName(this, tr("Open ROM"), QString(), filter);
    if (path.isEmpty()) return;

    const std::filesystem::path romPath = path.toStdString();
    std::filesystem::path corePath;
    const std::string ext = romPath.extension().string().empty()
                                ? std::string()
                                : romPath.extension().string().substr(1);
    if (!ext.empty()) corePath = context_.coreForExtension(ext);

    if (corePath.empty()) {
        CoreManagerDialog::pickCore(this, context_, romPath, &corePath);
        if (corePath.empty()) return;
    }

    if (!gameWindow_) gameWindow_ = new EmulatorWindow(context_, nullptr);
    EmulatorView::Session session;
    session.corePath = corePath;
    session.gamePath = romPath;
    session.title = Library::prettifyTitle(romPath.stem().string());
    gameWindow_->launch(session);
}

void MainWindow::addLibraryFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Add library folder"));
    if (dir.isEmpty()) return;
    auto dirs = context_.libraryDirectories();
    if (std::find(dirs.begin(), dirs.end(), dir.toStdString()) == dirs.end()) {
        dirs.push_back(dir.toStdString());
        context_.setLibraryDirectories(dirs);
    }
    refreshLibrary();
}

void MainWindow::showCoresDialog() {
    CoreManagerDialog dialog(context_, this);
    dialog.exec();
    refreshLibrary();
}

void MainWindow::showSettingsDialog() {
    SettingsDialog dialog(context_, this);
    if (dialog.exec() == QDialog::Accepted) refreshLibrary();
}

void MainWindow::showAbout() {
    QMessageBox::about(
        this, tr("About Gecko"),
        tr("<h3>Gecko %1</h3>"
           "<p>A cross-platform libretro frontend.</p>"
           "<p>Desktop build: Qt %2 — Windows · macOS · Linux · Android</p>"
           "<p>Built on the GA330-libretro project. All libretro cores are supported.</p>")
            .arg(versionString())
            .arg(QString(QT_VERSION_STR)));
}

bool MainWindow::resolveCoreForGame(const GameEntry& game, std::filesystem::path* coreOut) {
    if (!game.corePath.empty()) {
        *coreOut = game.corePath;
        return true;
    }
    // No mapping yet: ask the user (once), then remember it.
    return CoreManagerDialog::pickCore(this, context_, game.path, coreOut);
}

void MainWindow::launchGame(const QModelIndex& index) {
    const GameEntry* game = model_->gameAt(index);
    if (!game) return;

    std::filesystem::path corePath;
    if (!resolveCoreForGame(*game, &corePath)) return;

    if (!gameWindow_) gameWindow_ = new EmulatorWindow(context_, nullptr);
    EmulatorView::Session session;
    session.corePath = corePath;
    session.gamePath = game->path;
    session.title = game->title;
    hide();
    gameWindow_->launch(session);
}

void MainWindow::openGameByTitle(const QString& title) {
    for (int row = 0; row < model_->rowCount(); ++row) {
        const QModelIndex index = model_->index(row, 0);
        if (model_->data(index, LibraryModel::TitleRole).toString() == title) {
            launchGame(index);
            return;
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    context_.config().set("windows.main", saveGeometry().toBase64().toStdString());
    context_.config().save();
    if (gameWindow_) gameWindow_->close();
    QMainWindow::closeEvent(event);
}

} // namespace gecko
