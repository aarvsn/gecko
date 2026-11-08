// SPDX-License-Identifier: MIT
// EmulatorView — the game display widget. Owns the emulation runner and its
// supporting objects (frame buffer, audio bridge, input manager), paints the
// latest frame with aspect-correct scaling + filter + perf HUD, and routes
// keyboard events into the joypad.

#pragma once

#include "audio_bridge.hpp"
#include "frame_buffer.hpp"
#include "gecko/config.hpp"
#include "gecko/emulator.hpp"
#include "gecko/input_source.hpp"
#include "gecko/runner.hpp"
#include "input_manager.hpp"
#include "video_filters.hpp"

#include <QElapsedTimer>
#include <QImage>
#include <QWidget>

class QLabel;

namespace gecko {

class EmulatorView : public QWidget {
    Q_OBJECT

public:
    struct Session {
        std::filesystem::path corePath;
        std::filesystem::path gamePath; // may be empty for no-game cores
        std::string title;
    };

    explicit EmulatorView(Config& config, QWidget* parent = nullptr);
    ~EmulatorView() override;

    // Loads the core + game and starts the emulation thread.
    // Returns false and fills `error` when the session cannot start.
    bool startSession(const Session& session, QString* error);
    void stopSession();
    bool hasSession() const { return emulator_ && emulator_->gameLoaded(); }

    Emulator* emulator() { return emulator_.get(); }
    InputManager* inputManager() { return inputManager_.get(); }
    EmulatorRunner* runner() { return runner_.get(); }

    // --- runtime controls (in-game menu / shortcuts) ---
    void togglePause();
    bool paused() const;
    void resetGame();
    bool saveState(int slot);
    bool loadState(int slot);
    void takeScreenshot();
    void setVideoFilter(VideoFilter filter);
    VideoFilter videoFilter() const { return filter_; }
    void setHudVisible(bool visible);
    bool hudVisible() const { return hudVisible_; }
    void setFastForward(bool active);
    bool fastForward() const { return fastForward_; }

    // Latest raw frame (for screenshots taken by the window).
    QImage currentFrame() const;

    QSize sizeHint() const override;

signals:
    void frameDisplayed(const QImage& image);
    void coreMessage(const QString& message);
    void hudStatsChanged(double fps, double speedPercent);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private slots:
    void repaintTick();

private:
    QRectF videoRect() const;
    void drawHud(QPainter& painter);

    Config& config_;
    std::unique_ptr<Emulator> emulator_;
    std::unique_ptr<EmulatorRunner> runner_;
    std::unique_ptr<FrameBuffer> frames_;
    std::unique_ptr<AudioBridge> audio_;
    std::unique_ptr<InputManager> inputManager_;

    QImage displayFrame_;
    VideoFilter filter_ = VideoFilter::None;
    bool smoothing_ = true;
    bool hudVisible_ = true;
    bool fastForward_ = false;
    int frameSkip_ = 0;
    int skipCounter_ = 0;
    double ffSpeed_ = 3.0;

    QString coreMessage_;
    QElapsedTimer messageTimer_;
    QTimer* repaintTimer_ = nullptr;
};

} // namespace gecko
