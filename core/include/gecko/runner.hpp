// SPDX-License-Identifier: MIT
// EmulatorRunner — runs the emulator on its own thread, paced to the core's
// reported frame rate, with fast-forward and frame skip support. Both the Qt
// desktop frontend and the Android JNI layer use this instead of reinventing
// the pacing loop.

#pragma once

#include "emulator.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace gecko {

class EmulatorRunner {
public:
    enum class State { Stopped, Running, Paused };

    explicit EmulatorRunner(Emulator& emu);
    ~EmulatorRunner();

    EmulatorRunner(const EmulatorRunner&) = delete;
    EmulatorRunner& operator=(const EmulatorRunner&) = delete;

    // Starts the emulation thread. Content must already be loaded.
    void start();
    void pause();
    void resume();
    // Stops and joins the thread (the emulator object stays loaded).
    void stop();
    void togglePause();

    State state() const { return state_.load(std::memory_order_relaxed); }
    bool isRunning() const { return state() != State::Stopped; }

    // Speed multiplier: 1.0 = realtime, 2.0 = fast-forward x2 (runs extra
    // frames per tick rather than racing unpaced). Clamped to [0.1, 20].
    void setSpeedMultiplier(double m);
    double speedMultiplier() const { return speed_.load(std::memory_order_relaxed); }

    // 0 = render every frame; n = render one out of every n+1 frames.
    void setFrameSkip(int n) { frameSkip_.store(n, std::memory_order_relaxed); }
    int frameSkip() const { return frameSkip_.load(std::memory_order_relaxed); }

    // Frames executed since start() (not counting skipped renders).
    uint64_t framesExecuted() const { return framesExecuted_.load(std::memory_order_relaxed); }

    // Rolling measured emulation rate in frames/second.
    double measuredFps() const;

    // Called on the emulation thread after every executed frame. Use for
    // bookkeeping that must be in lockstep with the core (perf HUDs use the
    // measuredFps() interface instead).
    std::function<void()> onFrame;

private:
    void runLoop();

    Emulator& emu_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable wakeUp_;
    std::atomic<State> state_{State::Stopped};
    std::atomic<double> speed_{1.0};
    std::atomic<int> frameSkip_{0};
    std::atomic<uint64_t> framesExecuted_{0};

    // FPS measurement (emu thread only).
    std::chrono::steady_clock::time_point fpsWindowStart_;
    uint64_t fpsWindowFrames_ = 0;
    double measuredFps_ = 0.0;
};

} // namespace gecko
