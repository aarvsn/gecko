// SPDX-License-Identifier: MIT
// Gecko — desktop entry point.
//
// Modes:
//   gecko                     — normal GUI launch
//   gecko --version           — print version and exit
//   gecko --selftest <core> <outdir>
//                             — headless pipeline self-test: runs the given
//                               core in the real Qt widget stack (offscreen),
//                               writes verification screenshots, exits 0/1.

#include "app_context.hpp"
#include "emulator_view.hpp"
#include "gecko/config.hpp"
#include "gecko/emulator.hpp"
#include "gecko/log.hpp"
#include "gecko/runner.hpp"
#include "gecko/version.hpp"
#include "mainwindow.hpp"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QPalette>
#include <QPixmap>
#include <QStyleFactory>
#include <QTimer>

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {

void applyTheme(gecko::Config& config) {
    const QString theme = QString::fromStdString(config.getString("ui.theme", "system"));
    if (theme == "system") {
        // "System native": keep the platform's default style untouched.
        return;
    }
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    if (theme == "dark") {
        palette.setColor(QPalette::Window, QColor(34, 37, 43));
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor(25, 27, 32));
        palette.setColor(QPalette::AlternateBase, QColor(34, 37, 43));
        palette.setColor(QPalette::ToolTipBase, Qt::white);
        palette.setColor(QPalette::ToolTipText, Qt::white);
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, QColor(34, 37, 43));
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, QColor(42, 130, 218));
        palette.setColor(QPalette::Highlight, QColor(62, 142, 87));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    } else {
        palette.setColor(QPalette::Highlight, QColor(58, 166, 87));
        palette.setColor(QPalette::HighlightedText, Qt::white);
    }
    QApplication::setPalette(palette);
}

void qtLogHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    gecko::LogLevel level = gecko::LogLevel::Info;
    switch (type) {
        case QtDebugMsg: level = gecko::LogLevel::Debug; break;
        case QtInfoMsg: level = gecko::LogLevel::Info; break;
        case QtWarningMsg: level = gecko::LogLevel::Warning; break;
        case QtCriticalMsg:
        case QtFatalMsg: level = gecko::LogLevel::Error; break;
    }
    const char* file = context.file ? context.file : "";
    gecko::logMessage(level, "qt: %s (%s:%u)", message.toUtf8().constData(), file,
                      static_cast<unsigned>(context.line));
}

// ---------------------------------------------------------------------------
// Headless self-test: exercises the real GUI widget stack offscreen.
// ---------------------------------------------------------------------------
int runSelfTest(const std::string& corePath, const std::string& outDir) {
    std::filesystem::create_directories(outDir);
    const std::filesystem::path out(outDir);

    // Isolated config so the test never touches the user's real config.
    gecko::Config config(out / "selftest-config.json");

    // 1) Library window screenshot (empty library state).
    {
        gecko::AppContext context(out / "selftest-config.json");
        gecko::MainWindow window(context);
        window.resize(960, 640);
        window.show();
        QApplication::processEvents();
        const QPixmap shot = window.grab();
        shot.save(QString::fromStdString((out / "screenshot-library.png").string()));
        std::printf("selftest: library screenshot %s\n", (out / "screenshot-library.png").string().c_str());
    }

    // 2) Game view: run the test core through the real EmulatorView.
    const auto runView = [&](gecko::VideoFilter filter, const std::string& name,
                             bool sendInput) -> bool {
        gecko::AppContext context(out / "selftest-config.json");
        gecko::EmulatorView view(context.config());
        view.resize(640, 480);

        gecko::EmulatorView::Session session;
        session.corePath = corePath;
        session.gamePath = (out / "demo.geckotest").string();
        session.title = "selftest";
        { // content file (test core ignores data)
            FILE* f = std::fopen(session.gamePath.string().c_str(), "wb");
            if (f) std::fputc('G', f);
            if (f) std::fclose(f);
        }
        QString error;
        if (!view.startSession(session, &error)) {
            std::printf("selftest FAIL: session start: %s\n", error.toStdString().c_str());
            return false;
        }
        view.setVideoFilter(filter);
        view.setHudVisible(true);

        if (sendInput) {
            view.inputManager()->setBinding(RETRO_DEVICE_ID_JOYPAD_RIGHT, Qt::Key_Right);
            view.inputManager()->source().setButton(0, RETRO_DEVICE_ID_JOYPAD_RIGHT, true);
        }
        // Run ~40 frames of emulation in wall time.
        QEventLoop loop;
        QTimer::singleShot(700, &loop, &QEventLoop::quit);
        loop.exec();
        if (sendInput)
            view.inputManager()->source().setButton(0, RETRO_DEVICE_ID_JOYPAD_RIGHT, false);

        QApplication::processEvents();
        const QImage frame = view.currentFrame();
        bool ok = !frame.isNull();

        // Animation check: grab another frame 300 ms later; the core animates,
        // so the two snapshots must differ.
        if (name == "screenshot-game-anim-a.png") {
            const QImage first = view.currentFrame();
            QEventLoop wait;
            QTimer::singleShot(300, &wait, &QEventLoop::quit);
            wait.exec();
            const QImage second = view.currentFrame();
            ok = ok && first.size() == second.size() && first.constBits() != nullptr &&
                 std::memcmp(first.constBits(), second.constBits(),
                             static_cast<size_t>(first.sizeInBytes())) != 0;
        }

        const QPixmap shot = view.grab();
        shot.save(QString::fromStdString((out / name).string()));
        std::printf("selftest: view screenshot %s (%dx%d frame, filter=%s)\n",
                    (out / name).string().c_str(), frame.width(), frame.height(),
                    name.c_str());
        view.stopSession();
        return ok;
    };

    bool ok = true;
    ok = runView(gecko::VideoFilter::None, "screenshot-game-none.png", true) && ok;
    ok = runView(gecko::VideoFilter::Scanlines, "screenshot-game-scanlines.png", false) && ok;
    ok = runView(gecko::VideoFilter::Crt, "screenshot-game-crt.png", false) && ok;
    ok = runView(gecko::VideoFilter::Lcd, "screenshot-game-lcd.png", false) && ok;
    ok = runView(gecko::VideoFilter::None, "screenshot-game-anim-a.png", false) && ok;

    std::printf("selftest: %s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
    qInstallMessageHandler(qtLogHandler);

    // Offscreen when requested explicitly (self-test), otherwise let the
    // platform pick.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--selftest") qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Gecko"));
    QApplication::setOrganizationName(QStringLiteral("gecko-emu"));

    const QStringList args = app.arguments();
    if (args.contains("--version")) {
        std::printf("Gecko %s (libretro host, Qt %s)\n", gecko::versionString(), QT_VERSION_STR);
        return 0;
    }

    const int selfTestIndex = static_cast<int>(args.indexOf("--selftest"));
    if (selfTestIndex >= 0) {
        if (selfTestIndex + 2 >= args.size()) {
            std::fprintf(stderr, "usage: gecko --selftest <core> <outdir>\n");
            return 2;
        }
        return runSelfTest(args[selfTestIndex + 1].toStdString(),
                           args[selfTestIndex + 2].toStdString());
    }

    gecko::AppContext context;
    applyTheme(context.config());
    gecko::MainWindow window(context);
    window.show();
    return QApplication::exec();
}
