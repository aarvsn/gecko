// SPDX-License-Identifier: MIT
#include "emulator_view.hpp"

#include "gecko/log.hpp"
#include "gecko/version.hpp"

#include <QDateTime>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>

namespace gecko {

EmulatorView::EmulatorView(Config& config, QWidget* parent)
    : QWidget(parent), config_(config) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setBackgroundRole(QPalette::Window);

    frames_ = std::make_unique<FrameBuffer>();

    // Config-driven presentation.
    filter_ = filterFromName(QString::fromStdString(config_.getString("video.filter", "none")));
    smoothing_ = config_.getBool("video.smooth", true);
    hudVisible_ = config_.getBool("perf.hud", true);
    frameSkip_ = static_cast<int>(config_.getInt("perf.frameskip", 0));
    ffSpeed_ = config_.getDouble("perf.fastforward_speed", 3.0);

    repaintTimer_ = new QTimer(this);
    repaintTimer_->setTimerType(Qt::PreciseTimer);
    const int interval = config_.getInt("video.repaint_interval_ms", 16);
    repaintTimer_->setInterval(qMax(8, interval));
    connect(repaintTimer_, &QTimer::timeout, this, &EmulatorView::repaintTick);
}

EmulatorView::~EmulatorView() {
    stopSession();
}

bool EmulatorView::startSession(const Session& session, QString* error) {
    stopSession();

    emulator_ = std::make_unique<Emulator>();
    inputManager_ = std::make_unique<InputManager>(config_);
    inputManager_->initGamepads();
    audio_ = std::make_unique<AudioBridge>();
    audio_->setVolume(static_cast<int>(config_.getInt("audio.volume", 100)));
    audio_->setEnabled(config_.getBool("audio.enabled", true));

    emulator_->setVideoSink(frames_.get());
    emulator_->setAudioSink(audio_.get());
    emulator_->setInputSource(&inputManager_->source());
    emulator_->setSystemDirectory(
        config_.getString("paths.system").empty()
            ? Config::defaultConfigDirectory() / "system"
            : std::filesystem::path(config_.getString("paths.system")));
    emulator_->setSaveDirectory(
        config_.getString("paths.saves").empty()
            ? Config::defaultConfigDirectory() / "saves"
            : std::filesystem::path(config_.getString("paths.saves")));

    std::string loadError;
    if (!emulator_->loadCore(session.corePath, &loadError)) {
        if (error) *error = QString::fromStdString(loadError);
        stopSession();
        return false;
    }

    if (session.gamePath.empty()) {
        if (!emulator_->startWithoutContent(&loadError)) {
            if (error) *error = QString::fromStdString(loadError);
            stopSession();
            return false;
        }
    } else if (!emulator_->loadGame(session.gamePath, &loadError)) {
        if (error) *error = QString::fromStdString(loadError);
        stopSession();
        return false;
    }

    runner_ = std::make_unique<EmulatorRunner>(*emulator_);
    runner_->setSpeedMultiplier(1.0);
    runner_->start();

    // Size the window to the core's geometry (integer scale, min 2x).
    const QSize base(static_cast<int>(emulator_->avInfo().baseWidth),
                     static_cast<int>(emulator_->avInfo().baseHeight));
    setMinimumSize(base.width(), base.height());
    updateGeometry();
    repaintTimer_->start();
    return true;
}

void EmulatorView::stopSession() {
    if (repaintTimer_) repaintTimer_->stop();
    if (runner_) runner_->stop();
    if (emulator_ && emulator_->gameLoaded()) emulator_->unloadGame();
    if (emulator_) emulator_->unloadCore();
    runner_.reset();
    emulator_.reset();
    displayFrame_ = QImage();
}

void EmulatorView::togglePause() {
    if (!runner_) return;
    runner_->togglePause();
    update();
}

bool EmulatorView::paused() const {
    return runner_ ? runner_->state() == EmulatorRunner::State::Paused : false;
}

void EmulatorView::resetGame() {
    if (emulator_) emulator_->reset();
}

bool EmulatorView::saveState(int slot) {
    if (!emulator_) return false;
    const std::string title = emulator_->gameTitle();
    std::filesystem::path dir = config_.getString("paths.saves").empty()
                                    ? Config::defaultConfigDirectory() / "saves"
                                    : std::filesystem::path(config_.getString("paths.saves"));
    const auto path = dir / (title + ".state" + std::to_string(slot));
    const bool ok = emulator_->saveStateToFile(path);
    logMessage(ok ? LogLevel::Info : LogLevel::Error, "save state slot %d: %s", slot,
               ok ? path.string().c_str() : "failed");
    return ok;
}

bool EmulatorView::loadState(int slot) {
    if (!emulator_) return false;
    const std::string title = emulator_->gameTitle();
    std::filesystem::path dir = config_.getString("paths.saves").empty()
                                    ? Config::defaultConfigDirectory() / "saves"
                                    : std::filesystem::path(config_.getString("paths.saves"));
    const auto path = dir / (title + ".state" + std::to_string(slot));
    const bool ok = emulator_->loadStateFromFile(path);
    logMessage(ok ? LogLevel::Info : LogLevel::Error, "load state slot %d: %s", slot,
               ok ? path.string().c_str() : "failed");
    return ok;
}

void EmulatorView::takeScreenshot() {
    const QImage frame = currentFrame();
    if (frame.isNull()) return;
    std::filesystem::path dir = config_.getString("paths.screenshots").empty()
                                    ? Config::defaultConfigDirectory() / "screenshots"
                                    : std::filesystem::path(config_.getString("paths.screenshots"));
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const QString title = emulator_ ? QString::fromStdString(emulator_->gameTitle())
                                    : QStringLiteral("gecko");
    const QString path = QString::fromStdString(dir.string()) + "/" + title + "-" + stamp + ".png";
    frame.save(path);
    coreMessage_ = tr("Screenshot saved: %1").arg(path);
    messageTimer_.restart();
    update();
}

void EmulatorView::setVideoFilter(VideoFilter filter) {
    filter_ = filter;
    config_.set("video.filter", filterName(filter).toStdString());
    config_.save();
    update();
}

void EmulatorView::setHudVisible(bool visible) {
    hudVisible_ = visible;
    config_.set("perf.hud", visible);
    config_.save();
    update();
}

void EmulatorView::setFastForward(bool active) {
    fastForward_ = active;
    if (emulator_) emulator_->setFastForward(active);
    if (runner_) runner_->setSpeedMultiplier(active ? ffSpeed_ : 1.0);
    if (audio_) audio_->setEnabled(active ? !config_.getBool("audio.mute_ff", true)
                                          : config_.getBool("audio.enabled", true));
    update();
}

QImage EmulatorView::currentFrame() const {
    QImage frame;
    frames_->takeLatest(&frame);
    return frame;
}

QSize EmulatorView::sizeHint() const {
    if (emulator_ && emulator_->gameLoaded()) {
        const auto& av = emulator_->avInfo();
        return QSize(static_cast<int>(av.baseWidth) * 2, static_cast<int>(av.baseHeight) * 2);
    }
    return QSize(640, 480);
}

QRectF EmulatorView::videoRect() const {
    if (displayFrame_.isNull()) return QRectF();

    double aspect = 4.0 / 3.0;
    if (emulator_ && emulator_->gameLoaded()) aspect = emulator_->avInfo().aspectRatio;
    if (aspect <= 0.0) aspect = static_cast<double>(displayFrame_.width()) / displayFrame_.height();

    const QString aspectSetting =
        QString::fromStdString(config_.getString("video.aspect", "core"));
    if (aspectSetting == "4:3") aspect = 4.0 / 3.0;
    else if (aspectSetting == "16:9") aspect = 16.0 / 9.0;

    QRectF bounds = rect();
    if (aspectSetting == "stretch") return bounds;

    const double frameAspect = aspect;
    double w = bounds.width();
    double h = w / frameAspect;
    if (h > bounds.height()) {
        h = bounds.height();
        w = h * frameAspect;
    }
    return QRectF(bounds.left() + (bounds.width() - w) / 2.0,
                  bounds.top() + (bounds.height() - h) / 2.0, w, h);
}

void EmulatorView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    if (displayFrame_.isNull()) {
        if (!hasSession()) {
            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(rect(), Qt::AlignCenter, tr("No content loaded"));
        }
        return;
    }

    const QRectF target = videoRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smoothing_);

    // Black pillars around the video area.
    painter.fillRect(rect(), Qt::black);
    painter.drawImage(target, displayFrame_, QRectF(displayFrame_.rect()));

    const int coreHeight = displayFrame_.height();
    applyFilter(&painter, target, filter_, coreHeight, smoothing_);

    drawHud(painter);
}

void EmulatorView::drawHud(QPainter& painter) {
    // Transient core message.
    if (!coreMessage_.isEmpty() && messageTimer_.isValid() &&
        messageTimer_.elapsed() < 4000) {
        painter.setFont(font());
        const QRect textRect = painter.fontMetrics().boundingRect(
            QRect(0, 0, width(), 0), Qt::AlignCenter, coreMessage_);
        QRect box = textRect.adjusted(-16, -10, 16, 10);
        box.moveCenter(QPoint(width() / 2, height() - 60));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 170));
        painter.drawRoundedRect(box, 8, 8);
        painter.setPen(Qt::white);
        painter.drawText(box, Qt::AlignCenter, coreMessage_);
    }

    if (!hudVisible_) return;

    double fps = 0.0;
    if (runner_) fps = runner_->measuredFps();
    const double coreFps = emulator_ ? emulator_->avInfo().fps : 60.0;
    const double speed = coreFps > 0.0 ? fps / coreFps * 100.0 : 0.0;

    QString line = QStringLiteral("FPS %1  •  %2%")
                       .arg(fps, 0, 'f', 1)
                       .arg(speed, 0, 'f', 0);
    if (paused()) line += tr("  •  PAUSED");
    if (fastForward_) line += tr("  •  FF x%1").arg(ffSpeed_, 0, 'f', 1);

    QFont hudFont = font();
    hudFont.setPointSizeF(qMax(9.0, font().pointSizeF()));
    hudFont.setBold(true);
    painter.setFont(hudFont);

    const QFontMetrics metrics(hudFont);
    const QRect textRect = metrics.boundingRect(line);
    QRect box = textRect.adjusted(-10, -6, 10, 6);
    box.moveTopLeft(QPoint(12, 12));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(box, 6, 6);
    painter.setPen(paused() ? QColor(255, 200, 90) : QColor(140, 235, 150));
    painter.drawText(box, Qt::AlignCenter, line);
}

void EmulatorView::repaintTick() {
    if (!frames_->has()) return;

    if (frameSkip_ > 0) {
        if (++skipCounter_ <= frameSkip_) {
            // Drop this tick's repaint (emulation keeps running).
            return;
        }
        skipCounter_ = 0;
    }

    QImage frame;
    if (frames_->takeLatest(&frame)) {
        displayFrame_ = std::move(frame);
        if (paused()) {
            // keep last frame visible, dim handled in HUD
        }
        update();
    }

    if (emulator_) {
        std::string message;
        if (emulator_->takeCoreMessage(&message)) {
            coreMessage_ = QString::fromStdString(message);
            messageTimer_.restart();
        }
        if (emulator_->coreRequestedShutdown()) {
            coreMessage_ = tr("The core requested shutdown");
            messageTimer_.restart();
        }
    }
}

void EmulatorView::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }
    if (inputManager_) {
        const bool consumed = inputManager_->handleKeyPress(event->key());
        if (consumed) {
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void EmulatorView::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }
    if (inputManager_) {
        const bool consumed = inputManager_->handleKeyRelease(event->key());
        if (consumed) {
            event->accept();
            return;
        }
    }
    QWidget::keyReleaseEvent(event);
}

void EmulatorView::focusOutEvent(QFocusEvent* event) {
    // Don't leave buttons stuck when the window loses focus.
    if (inputManager_) inputManager_->clearAllButtons();
    QWidget::focusOutEvent(event);
}

} // namespace gecko
