// SPDX-License-Identifier: MIT
#include "gecko/runner.hpp"

#include <algorithm>
#include <cmath>

namespace gecko {

EmulatorRunner::EmulatorRunner(Emulator& emu) : emu_(emu) {}

EmulatorRunner::~EmulatorRunner() { stop(); }

void EmulatorRunner::start() {
    if (state() != State::Stopped) return;
    framesExecuted_.store(0, std::memory_order_relaxed);
    fpsWindowFrames_ = 0;
    measuredFps_ = 0.0;
    fpsWindowStart_ = std::chrono::steady_clock::now();
    state_.store(State::Running, std::memory_order_relaxed);
    thread_ = std::thread(&EmulatorRunner::runLoop, this);
}

void EmulatorRunner::pause() {
    if (state() == State::Running) state_.store(State::Paused, std::memory_order_relaxed);
}

void EmulatorRunner::resume() {
    if (state() == State::Paused) state_.store(State::Running, std::memory_order_relaxed);
}

void EmulatorRunner::togglePause() {
    if (state() == State::Running)
        pause();
    else if (state() == State::Paused)
        resume();
}

void EmulatorRunner::stop() {
    if (state() == State::Stopped) return;
    state_.store(State::Stopped, std::memory_order_relaxed);
    wakeUp_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void EmulatorRunner::setSpeedMultiplier(double m) {
    speed_.store(std::clamp(m, 0.1, 20.0), std::memory_order_relaxed);
}

double EmulatorRunner::measuredFps() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return measuredFps_;
}

void EmulatorRunner::runLoop() {
    // Pacing strategy: run one frame, then sleep for the remaining time of
    // the tick period (1 / (fps * speed)). With speed > 1 we shorten the
    // sleep and additionally run extra frames when the core is fast enough.
    // If a frame takes longer than the period (slow host), no sleep happens
    // at all — we never fall behind unboundedly.

    const double baseFps = emu_.avInfo().fps > 1.0 ? emu_.avInfo().fps : 60.0;
    auto lastTick = std::chrono::steady_clock::now();

    for (;;) {
        if (state_.load(std::memory_order_relaxed) == State::Stopped) break;

        if (state_.load(std::memory_order_relaxed) == State::Paused) {
            std::unique_lock<std::mutex> lock(mutex_);
            wakeUp_.wait_for(lock, std::chrono::milliseconds(50));
            lastTick = std::chrono::steady_clock::now();
            continue;
        }

        const double speed = speed_.load(std::memory_order_relaxed);
        const int skip = frameSkip_.load(std::memory_order_relaxed);

        // Run `round(speed)` frames this tick (>= 1), rendering every frame
        // that passes the frame-skip test.
        int framesThisTick = std::max(1, static_cast<int>(std::lround(speed)));
        while (framesThisTick-- > 0) {
            emu_.runFrame();
            framesExecuted_.fetch_add(1, std::memory_order_relaxed);
            fpsWindowFrames_++;
            if (onFrame) onFrame();
        }
        (void)skip; // frame skipping is applied by the *video sink* layer:
                    // emu_.runFrame() always executes so that core timing
                    // stays intact; the desktop view drops redraws instead.

        // Measure the real emulation rate over a ~500 ms window.
        const auto now = std::chrono::steady_clock::now();
        const double windowSeconds =
            std::chrono::duration<double>(now - fpsWindowStart_).count();
        if (windowSeconds >= 0.5) {
            std::lock_guard<std::mutex> lock(mutex_);
            measuredFps_ = static_cast<double>(fpsWindowFrames_) / windowSeconds;
            fpsWindowFrames_ = 0;
            fpsWindowStart_ = now;
        }

        // Sleep out the remainder of the tick period.
        const double periodUs = 1e6 / (baseFps * speed);
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(now - lastTick).count();
        const double remainingUs = periodUs - static_cast<double>(elapsed);
        if (remainingUs > 250.0) {
            std::unique_lock<std::mutex> lock(mutex_);
            wakeUp_.wait_for(lock, std::chrono::microseconds(
                                       static_cast<long long>(remainingUs)));
        }
        lastTick = std::chrono::steady_clock::now();
    }
}

} // namespace gecko
