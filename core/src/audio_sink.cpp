// SPDX-License-Identifier: MIT
#include "gecko/audio_sink.hpp"

#include <algorithm>
#include <cstring>

namespace gecko {

AudioRingBuffer::AudioRingBuffer(size_t frameCapacity)
    : buffer_(frameCapacity * 2, 0), capacity_(frameCapacity) {}

size_t AudioRingBuffer::write(const int16_t* samples, size_t frameCount) {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t toStore = std::min(frameCount, capacity_ - stored_);
    size_t frames = toStore;
    size_t pos = writePos_;
    const int16_t* in = samples;
    while (frames--) {
        buffer_[pos * 2 + 0] = *in++;
        buffer_[pos * 2 + 1] = *in++;
        pos = (pos + 1) % capacity_;
    }
    stored_ += toStore;
    return toStore;
}

size_t AudioRingBuffer::read(int16_t* out, size_t frameCount) {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t toRead = std::min(frameCount, stored_);
    size_t frames = toRead;
    size_t pos = readPos_;
    int16_t* o = out;
    while (frames--) {
        *o++ = buffer_[pos * 2 + 0];
        *o++ = buffer_[pos * 2 + 1];
        pos = (pos + 1) % capacity_;
    }
    readPos_ = pos;
    stored_ -= toRead;
    return toRead;
}

size_t AudioRingBuffer::framesAvailable() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return stored_;
}

void AudioRingBuffer::clear() {
    std::lock_guard<std::mutex> guard(mutex_);
    readPos_ = 0;
    writePos_ = 0;
    stored_ = 0;
}

} // namespace gecko
