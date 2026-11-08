// SPDX-License-Identifier: MIT
// AudioBridge — desktop audio output. When Qt Multimedia is available it
// feeds an AudioRingBuffer into a QAudioSink (push mode); otherwise it
// silently drops samples so the rest of the app is unaffected.

#pragma once

#include "gecko/audio_sink.hpp"

#include <memory>

class QAudioSink;

namespace gecko {

class RingIODevice;

class AudioBridge : public AudioSink {
public:
    AudioBridge();
    ~AudioBridge() override;

    // --- AudioSink (called from the emu thread) ---
    void setFormat(double sampleRate) override;
    size_t write(const int16_t* samples, size_t frameCount) override;
    void flush() override;

    // --- GUI thread ---
    void setVolume(int percent);   // 0..100
    void setEnabled(bool enabled); // when disabled, samples are dropped
    bool isWorking() const { return working_; }

private:
    void restart();

    AudioRingBuffer ring_;
    double sampleRate_ = 48000.0;
    int volume_ = 100;
    bool enabled_ = true;
    bool working_ = false; // true when a QAudioSink is live

#if GECKO_WITH_QTMULTIMEDIA
    std::unique_ptr<RingIODevice> pullDevice_; // reads from ring_
    std::unique_ptr<QAudioSink> sink_;
#endif
};

} // namespace gecko
