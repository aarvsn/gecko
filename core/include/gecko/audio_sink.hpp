// SPDX-License-Identifier: MIT
// Audio output abstraction plus a thread-safe SPSC ring buffer used by the
// Android backend (the Qt backend buffers inside Qt's audio classes).

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace gecko {

class AudioSink {
public:
    virtual ~AudioSink() = default;

    // Called before the first samples of a stream (and whenever the core
    // changes its sample rate).
    virtual void setFormat(double sampleRate) { (void)sampleRate; }

    // Writes `frameCount` stereo interleaved frames (L, R, L, R ...).
    // Returns the number of frames actually consumed; the emulator drops
    // what the sink did not take.
    virtual size_t write(const int16_t* samples, size_t frameCount) = 0;

    // Optional flush when the core is reset / unloaded.
    virtual void flush() {}
};

// A discarding sink used when audio is disabled or unavailable.
class NullAudioSink : public AudioSink {
public:
    size_t write(const int16_t* samples, size_t frameCount) override {
        (void)samples;
        return frameCount; // pretend everything was consumed
    }
};

// Simple mutex-protected FIFO of stereo samples with a fixed capacity.
// Producer: emulator thread (write). Consumer: platform audio thread
// (readAvailable / read). Overwrites silently drop samples.
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t frameCapacity);

    // Returns frames actually stored.
    size_t write(const int16_t* samples, size_t frameCount);
    // Reads up to frameCount frames, returns what was read.
    size_t read(int16_t* out, size_t frameCount);
    size_t framesAvailable() const;
    size_t capacity() const { return capacity_; }
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<int16_t> buffer_; // interleaved stereo
    size_t capacity_ = 0;         // in frames
    size_t readPos_ = 0;
    size_t writePos_ = 0;
    size_t stored_ = 0; // frames
};

// An AudioSink that forwards into an AudioRingBuffer (used by the JNI layer).
class RingAudioSink : public AudioSink {
public:
    explicit RingAudioSink(size_t frameCapacity = 8192)
        : ring_(frameCapacity) {}

    void setFormat(double sampleRate) override { currentRate_ = sampleRate; }
    size_t write(const int16_t* samples, size_t frameCount) override {
        return ring_.write(samples, frameCount);
    }

    AudioRingBuffer& ring() { return ring_; }
    double sampleRate() const { return currentRate_; }

private:
    AudioRingBuffer ring_;
    double currentRate_ = 48000.0;
};

} // namespace gecko
