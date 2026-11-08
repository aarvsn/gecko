// SPDX-License-Identifier: MIT
// FrameBuffer — a thread-safe gecko::VideoSink that keeps the most recent
// frame as a QImage. The emulation thread writes; the GUI thread swaps out.

#pragma once

#include "gecko/video_sink.hpp"

#include <QImage>
#include <QMutex>
#include <atomic>

namespace gecko {

class FrameBuffer : public VideoSink {
public:
    // --- VideoSink (emu thread) ---
    void frame(const VideoFrame& f) override {
        const QImage::Format format =
            QImage::Format_RGBX8888; // XRGB8888 bytes == RGBX8888 in QImage
        QImage image(f.width, f.height, format);
        const int srcStride = static_cast<int>(f.stride);
        const uint8_t* src = reinterpret_cast<const uint8_t*>(f.pixels);
        for (unsigned y = 0; y < f.height; ++y) {
            std::memcpy(image.scanLine(static_cast<int>(y)), src + y * srcStride,
                        static_cast<size_t>(f.width) * 4);
        }
        QMutexLocker lock(&mutex_);
        image_ = image;
        ++frameId_;
        has_.store(true, std::memory_order_release);
    }

    // --- GUI thread ---
    // Returns true and fills `out` when a newer frame exists. `out` is a
    // cheap implicit-sharing QImage copy.
    bool takeLatest(QImage* out, quint64* frameId = nullptr) {
        QMutexLocker lock(&mutex_);
        if (!has_.load(std::memory_order_acquire)) return false;
        if (frameId) *frameId = frameId_;
        *out = image_;
        return true;
    }

    bool has() const { return has_.load(std::memory_order_acquire); }

private:
    QMutex mutex_;
    QImage image_;
    std::atomic<bool> has_{false};
    quint64 frameId_ = 0;
};

} // namespace gecko
