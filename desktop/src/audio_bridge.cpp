// SPDX-License-Identifier: MIT
#include "audio_bridge.hpp"

#include "gecko/log.hpp"

#include <QCoreApplication>
#include <QIODevice>
#include <QMetaObject>
#include <QThread>

#if GECKO_WITH_QTMULTIMEDIA
#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>
#endif

namespace gecko {

// Pull-mode QIODevice over the lock-protected ring buffer. Qt's audio thread
// calls readData() periodically; underruns just produce short reads, which
// QAudioSink handles gracefully.
class RingIODevice : public QIODevice {
public:
    explicit RingIODevice(AudioRingBuffer& ring) : ring_(ring) {
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    bool isSequential() const override { return true; }

    qint64 readData(char* data, qint64 maxSize) override {
        if (maxSize < 0) return 0;
        return static_cast<qint64>(
            ring_.read(reinterpret_cast<int16_t*>(data), static_cast<size_t>(maxSize) / 4) * 4);
    }

    qint64 writeData(const char*, qint64) override { return 0; }
    qint64 bytesAvailable() const override {
        return static_cast<qint64>(ring_.framesAvailable()) * 4 + QIODevice::bytesAvailable();
    }

private:
    AudioRingBuffer& ring_;
};

AudioBridge::AudioBridge() : ring_(16384) {}

AudioBridge::~AudioBridge() = default;

void AudioBridge::setFormat(double sampleRate) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    restart();
}

size_t AudioBridge::write(const int16_t* samples, size_t frameCount) {
    if (!enabled_) return frameCount;
    return ring_.write(samples, frameCount);
}

void AudioBridge::flush() {
    ring_.clear();
}

void AudioBridge::setVolume(int percent) {
    volume_ = percent;
#if GECKO_WITH_QTMULTIMEDIA
    if (sink_) sink_->setVolume(qBound(0, percent, 100) / 100.0f);
#endif
}

void AudioBridge::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled) ring_.clear();
}

void AudioBridge::restart() {
#if GECKO_WITH_QTMULTIMEDIA
    // QAudioSink requires an event loop, so it must live on the GUI thread.
    // The emu thread calls setFormat() when the core starts.
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        QMetaObject::invokeMethod(QCoreApplication::instance(),
                                  [this] { restart(); }, Qt::QueuedConnection);
        return;
    }

    sink_.reset();
    pullDevice_.reset();
    working_ = false;

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        logMessage(LogLevel::Warning, "audio: no output device found");
        return;
    }

    QAudioFormat format;
    format.setSampleRate(static_cast<int>(sampleRate_));
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!device.isFormatSupported(format)) {
        logMessage(LogLevel::Warning,
                   "audio: %d Hz stereo unsupported; falling back to device default",
                   static_cast<int>(sampleRate_));
        format = device.preferredFormat();
        if (format.channelCount() != 2 || format.sampleFormat() != QAudioFormat::Int16) {
            logMessage(LogLevel::Warning, "audio: unsupported output format (%d Hz, %d ch)",
                       format.sampleRate(), format.channelCount());
            return;
        }
        sampleRate_ = format.sampleRate();
    }

    pullDevice_ = std::make_unique<RingIODevice>(ring_);
    sink_ = std::make_unique<QAudioSink>(device, format);
    sink_->setVolume(qBound(0, volume_, 100) / 100.0f);
    sink_->setBufferSize(static_cast<int>(sampleRate_ * 2 * sizeof(int16_t) * 0.1)); // ~100 ms
    sink_->start(pullDevice_.get());
    working_ = sink_->state() != QAudio::StoppedState;
    logMessage(LogLevel::Info, "audio: %d Hz stereo output started", format.sampleRate());
#endif
}

} // namespace gecko
