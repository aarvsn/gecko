// SPDX-License-Identifier: MIT
// GeckoNative — JNI bridge between the Kotlin app and gecko-core.
// One native "session" per handle; the Kotlin side drives the frame loop
// (Choreographer) and the audio loop (AudioTrack thread).

#include "gecko_jni.h"

#include "gecko/audio_sink.hpp"
#include "gecko/emulator.hpp"
#include "gecko/input_source.hpp"
#include "gecko/runner.hpp"
#include "gecko/video_sink.hpp"

#ifdef __ANDROID__
#include <android/log.h>
#define GECKO_LOG(...) __android_log_print(ANDROID_LOG_INFO, "gecko", __VA_ARGS__)
#else
#include <cstdio>
#define GECKO_LOG(...) std::printf(__VA_ARGS__)
#endif

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace gecko;

namespace {

// Collects the latest frame as ARGB ints ready for Bitmap.setPixels.
class JniFrameSink : public VideoSink {
public:
    void frame(const VideoFrame& f) override {
        std::lock_guard<std::mutex> guard(mutex_);
        if (width_ != static_cast<int>(f.width) || height_ != static_cast<int>(f.height)) {
            width_ = static_cast<int>(f.width);
            height_ = static_cast<int>(f.height);
            pixels_.resize(static_cast<size_t>(width_) * height_);
        }
        for (size_t i = 0; i < pixels_.size(); ++i) {
            // XRGB8888 (0x00RRGGBB) -> ARGB int for Android (force opaque).
            pixels_[i] = static_cast<jint>(f.pixels[i] | 0xFF000000u);
        }
        ++generation_;
    }

    bool copyTo(std::vector<jint>& out, int* w, int* h, uint64_t* generation) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (pixels_.empty()) return false;
        out = pixels_;
        *w = width_;
        *h = height_;
        *generation = generation_;
        return true;
    }

private:
    std::mutex mutex_;
    std::vector<jint> pixels_;
    int width_ = 0, height_ = 0;
    uint64_t generation_ = 0;
};

// Ring-buffer audio sink (the Kotlin AudioTrack pulls from it).
class JniAudioSink : public AudioSink {
public:
    void setFormat(double sampleRate) override {
        std::lock_guard<std::mutex> guard(mutex_);
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    }
    size_t write(const int16_t* samples, size_t frameCount) override {
        std::lock_guard<std::mutex> guard(mutex_);
        const size_t capacity = buffer_.size() / 2;
        for (size_t i = 0; i < frameCount; ++i) {
            buffer_[writePos_ * 2 + 0] = samples[2 * i + 0];
            buffer_[writePos_ * 2 + 1] = samples[2 * i + 1];
            writePos_ = (writePos_ + 1) % capacity;
            if (stored_ < capacity) ++stored_;
        }
        return frameCount;
    }
    int read(int16_t* out, int maxFrames) {
        std::lock_guard<std::mutex> guard(mutex_);
        const size_t capacity = buffer_.size() / 2;
        const size_t n = std::min(static_cast<size_t>(maxFrames), stored_);
        for (size_t i = 0; i < n; ++i) {
            out[2 * i + 0] = buffer_[readPos_ * 2 + 0];
            out[2 * i + 1] = buffer_[readPos_ * 2 + 1];
            readPos_ = (readPos_ + 1) % capacity;
        }
        stored_ -= n;
        return static_cast<int>(n);
    }
    double sampleRate() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return sampleRate_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<int16_t> buffer_ = std::vector<int16_t>(16384 * 2, 0);
    size_t readPos_ = 0, writePos_ = 0, stored_ = 0;
    double sampleRate_ = 48000.0;
};

struct Session {
    std::unique_ptr<Emulator> emulator;
    std::unique_ptr<EmulatorRunner> runner;
    std::unique_ptr<JniFrameSink> video;
    std::unique_ptr<JniAudioSink> audio;
    std::unique_ptr<JoypadStateSource> input;
    std::vector<jint> frameCopy;
    int frameW = 0, frameH = 0;
    uint64_t frameGeneration = 0;
};

inline Session* asSession(jlong handle) {
    return reinterpret_cast<Session*>(static_cast<uintptr_t>(handle));
}

std::string toStdString(JNIEnv* env, jstring str) {
    if (!str) return {};
    const char* chars = env->GetStringUTFChars(str, nullptr);
    std::string out(chars);
    env->ReleaseStringUTFChars(str, chars);
    return out;
}

} // namespace

JNIEXPORT jlong JNICALL Java_com_gecko_emulator_native_1GeckoNative_create(JNIEnv*, jclass) {
    auto* session = new Session();
    session->emulator = std::make_unique<Emulator>();
    session->video = std::make_unique<JniFrameSink>();
    session->audio = std::make_unique<JniAudioSink>();
    session->input = std::make_unique<JoypadStateSource>();
    session->emulator->setVideoSink(session->video.get());
    session->emulator->setAudioSink(session->audio.get());
    session->emulator->setInputSource(session->input.get());
    session->emulator->setSystemDirectory("/data/data/com.gecko.emulator/files/system");
    session->emulator->setSaveDirectory("/data/data/com.gecko.emulator/files/saves");
    GECKO_LOG("session created");
    return reinterpret_cast<jlong>(session);
}

JNIEXPORT void JNICALL Java_com_gecko_emulator_native_1GeckoNative_destroy(JNIEnv*, jclass, jlong handle) {
    auto* session = asSession(handle);
    if (!session) return;
    if (session->runner) session->runner->stop();
    if (session->emulator && session->emulator->gameLoaded()) session->emulator->unloadGame();
    if (session->emulator) session->emulator->unloadCore();
    delete session;
    GECKO_LOG("session destroyed");
}

JNIEXPORT jboolean JNICALL Java_com_gecko_emulator_native_1GeckoNative_loadCore(JNIEnv* env, jclass, jlong handle, jstring path) {
    auto* session = asSession(handle);
    if (!session || !session->emulator) return JNI_FALSE;
    std::string error;
    const bool ok = session->emulator->loadCore(toStdString(env, path), &error);
    if (!ok) GECKO_LOG("loadCore failed: %s", error.c_str());
    else GECKO_LOG("core loaded: %s", session->emulator->coreDetails().libraryName.c_str());
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_gecko_emulator_native_1GeckoNative_loadGame(JNIEnv* env, jclass, jlong handle, jstring path) {
    auto* session = asSession(handle);
    if (!session || !session->emulator) return JNI_FALSE;
    std::string error;
    const bool ok = session->emulator->loadGame(toStdString(env, path), &error);
    if (!ok) {
        GECKO_LOG("loadGame failed: %s", error.c_str());
        return JNI_FALSE;
    }
    if (!session->runner) {
        session->runner = std::make_unique<EmulatorRunner>(*session->emulator);
    }
    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_gecko_emulator_native_1GeckoNative_unloadGame(JNIEnv*, jclass, jlong handle) {
    auto* session = asSession(handle);
    if (!session) return;
    if (session->runner) session->runner->stop();
    session->runner.reset();
    if (session->emulator && session->emulator->gameLoaded()) session->emulator->unloadGame();
}

JNIEXPORT jstring JNICALL Java_com_gecko_emulator_native_1GeckoNative_coreName(JNIEnv* env, jclass, jlong handle) {
    auto* session = asSession(handle);
    if (!session || !session->emulator) return env->NewStringUTF("");
    return env->NewStringUTF(session->emulator->coreDetails().libraryName.c_str());
}

JNIEXPORT void JNICALL Java_com_gecko_emulator_native_1GeckoNative_runFrame(JNIEnv*, jclass, jlong handle) {
    auto* session = asSession(handle);
    if (session && session->emulator) session->emulator->runFrame();
}

JNIEXPORT jint JNICALL Java_com_gecko_emulator_native_1GeckoNative_frameWidth(JNIEnv*, jclass, jlong handle) {
    auto* session = asSession(handle);
    return session && session->emulator ? static_cast<jint>(session->emulator->avInfo().baseWidth) : 0;
}

JNIEXPORT jint JNICALL Java_com_gecko_emulator_native_1GeckoNative_frameHeight(JNIEnv*, jclass, jlong handle) {
    auto* session = asSession(handle);
    return session && session->emulator ? static_cast<jint>(session->emulator->avInfo().baseHeight) : 0;
}

JNIEXPORT jint JNICALL Java_com_gecko_emulator_native_1GeckoNative_readFramePixels(JNIEnv* env, jclass, jlong handle, jintArray pixels) {
    auto* session = asSession(handle);
    if (!session || !pixels) return 0;
    if (!session->video->copyTo(session->frameCopy, &session->frameW, &session->frameH,
                                &session->frameGeneration))
        return 0;
    const jsize capacity = env->GetArrayLength(pixels);
    const jsize toCopy = std::min(capacity, static_cast<jsize>(session->frameCopy.size()));
    if (toCopy > 0) {
        env->SetIntArrayRegion(pixels, 0, toCopy, session->frameCopy.data());
    }
    return toCopy;
}

JNIEXPORT jdouble JNICALL Java_com_gecko_emulator_native_1GeckoNative_measuredFps(JNIEnv*, jclass, jlong handle) {
    auto* session = asSession(handle);
    return session && session->runner ? session->runner->measuredFps() : 0.0;
}

JNIEXPORT jdouble JNICALL Java_com_gecko_emulator_native_1GeckoNative_audioSampleRate(JNIEnv*, jclass, jlong handle) {
    auto* session = asSession(handle);
    return session && session->audio ? session->audio->sampleRate() : 0.0;
}

JNIEXPORT jint JNICALL Java_com_gecko_emulator_native_1GeckoNative_readAudio(JNIEnv* env, jclass, jlong handle, jshortArray out, jint maxFrames) {
    auto* session = asSession(handle);
    if (!session || !out || maxFrames <= 0) return 0;
    std::vector<int16_t> buffer(static_cast<size_t>(maxFrames) * 2);
    const int frames = session->audio->read(buffer.data(), maxFrames);
    if (frames > 0) {
        env->SetShortArrayRegion(out, 0, frames * 2, buffer.data());
    }
    return frames;
}

JNIEXPORT void JNICALL Java_com_gecko_emulator_native_1GeckoNative_setButton(JNIEnv*, jclass, jlong handle, jint port, jint retroId, jboolean pressed) {
    auto* session = asSession(handle);
    if (session && session->input) session->input->setButton(static_cast<unsigned>(port), static_cast<unsigned>(retroId), pressed == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL Java_com_gecko_emulator_native_1GeckoNative_saveState(JNIEnv* env, jclass, jlong handle, jstring path) {
    auto* session = asSession(handle);
    if (!session || !session->emulator) return JNI_FALSE;
    return session->emulator->saveStateToFile(toStdString(env, path)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_gecko_emulator_native_1GeckoNative_loadState(JNIEnv* env, jclass, jlong handle, jstring path) {
    auto* session = asSession(handle);
    if (!session || !session->emulator) return JNI_FALSE;
    return session->emulator->loadStateFromFile(toStdString(env, path)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_gecko_emulator_native_1GeckoNative_reset(JNIEnv*, jclass, jlong handle) {
    auto* session = asSession(handle);
    if (session && session->emulator) session->emulator->reset();
}

JNIEXPORT void JNICALL Java_com_gecko_emulator_native_1GeckoNative_setPaused(JNIEnv*, jclass, jlong handle, jboolean paused) {
    auto* session = asSession(handle);
    if (session && session->emulator) session->emulator->setPaused(paused == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_com_gecko_emulator_native_1GeckoNative_setSpeed(JNIEnv*, jclass, jlong handle, jdouble multiplier) {
    auto* session = asSession(handle);
    if (session && session->runner) session->runner->setSpeedMultiplier(multiplier);
}
