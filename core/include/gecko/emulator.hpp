// SPDX-License-Identifier: MIT
// Emulator — the libretro host. Owns one loaded core, feeds it input,
// consumes video/audio frames and manages content, save states and SRAM.
//
// Threading contract: all public methods must be called from the thread that
// owns the Emulator (or while the runner is stopped), except where noted.
// The callbacks invoked from retro_run() arrive on the caller's thread; the
// desktop and Android layers run retro_run() on a dedicated emulation thread
// (see runner.hpp) and marshal frames/samples out through their sinks.

#pragma once

#include "audio_sink.hpp"
#include "dynamic_library.hpp"
#include "input_source.hpp"
#include "libretro.h"
#include "video_sink.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace gecko {

struct CoreDetails {
    std::string libraryName;
    std::string libraryVersion;
    std::string validExtensions;
    bool needFullPath = false;
    bool blockExtract = false;
    std::string fullName() const {
        return libraryName.empty() ? std::string("unknown")
                                   : libraryName + " " + libraryVersion;
    }
};

struct AvInfo {
    double fps = 60.0;
    double sampleRate = 48000.0;
    unsigned baseWidth = 320;
    unsigned baseHeight = 240;
    unsigned maxWidth = 320;
    unsigned maxHeight = 240;
    double aspectRatio = 4.0 / 3.0;
};

struct CoreOption {
    struct Choice {
        std::string value;
        std::string label;
    };
    std::string key;
    std::string description;
    std::string info;
    std::vector<Choice> choices;
    std::string defaultValue;
    bool visible = true;
};

class Emulator {
public:
    Emulator();
    ~Emulator();

    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;

    // --- wiring -------------------------------------------------------------
    void setVideoSink(VideoSink* sink) { videoSink_ = sink; }
    void setAudioSink(AudioSink* sink) { audioSink_ = sink; }
    void setInputSource(InputSource* source) { inputSource_ = source; }

    // Directories the core sees through the environment callbacks.
    void setSystemDirectory(std::filesystem::path dir) { systemDirectory_ = std::move(dir); }
    void setSaveDirectory(std::filesystem::path dir) { saveDirectory_ = std::move(dir); }

    // Used by RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE. Both default to on.
    void setVideoEnabled(bool enabled) { videoEnabled_ = enabled; }
    void setAudioEnabled(bool enabled) { audioEnabled_ = enabled; }

    // Reported through RETRO_ENVIRONMENT_GET_FASTFORWARDING so cores can
    // reduce their load while the user holds the fast-forward key.
    void setFastForward(bool active) { fastForwardActive_ = active; }
    bool fastForward() const { return fastForwardActive_; }

    // --- core lifecycle -------------------------------------------------------
    // Loads a libretro core. Fails if another core is already loaded (call
    // unloadCore first). Note: loaded core libraries are never unloaded from
    // the process (libretro cores are not dlclose-safe in general).
    bool loadCore(const std::filesystem::path& corePath, std::string* error = nullptr);
    void unloadCore();
    bool coreLoaded() const { return api_.run != nullptr; }
    const CoreDetails& coreDetails() const { return coreDetails_; }
    std::filesystem::path corePath() const { return corePath_; }

    // --- content --------------------------------------------------------------
    // Loads a game (ROM path) into the currently loaded core. Handles both
    // full-path cores and data-loading cores automatically.
    bool loadGame(const std::filesystem::path& romPath, std::string* error = nullptr);
    // Starts a core that declared RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME.
    bool startWithoutContent(std::string* error = nullptr);
    void unloadGame();
    bool gameLoaded() const { return gameLoaded_; }
    std::filesystem::path gamePath() const { return gamePath_; }
    std::string gameTitle() const;

    const AvInfo& avInfo() const { return avInfo_; }
    retro_pixel_format pixelFormat() const { return pixelFormat_; }

    // --- running ---------------------------------------------------------------
    // Runs exactly one retro_run() (one video frame worth of emulation).
    void runFrame();
    void reset();
    void setPaused(bool paused) { paused_ = paused; }
    bool paused() const { return paused_; }
    bool coreRequestedShutdown() const { return shutdownRequested_; }

    // --- save states -------------------------------------------------------------
    size_t serializeSize();
    bool saveStateToFile(const std::filesystem::path& path);
    bool loadStateFromFile(const std::filesystem::path& path);

    // --- SRAM (battery saves) ------------------------------------------------
    bool sramAvailable() const;
    bool saveSramToFile(const std::filesystem::path& path);
    bool loadSramFromFile(const std::filesystem::path& path);

    // --- core options ----------------------------------------------------------
    // Populated after loadCore (the core reports them through the environment
    // callback). Values shown reflect the effective value (user override or
    // the core default).
    const std::vector<CoreOption>& coreOptions() const { return coreOptions_; }
    std::string coreOptionValue(const std::string& key) const;
    // Applies immediately (marks variables-dirty so the core notices through
    // GET_VARIABLE_UPDATE).
    void setCoreOptionValue(const std::string& key, const std::string& value);

    // --- misc ------------------------------------------------------------------
    // Takes a message the core pushed via RETRO_ENVIRONMENT_SET_MESSAGE.
    bool takeCoreMessage(std::string* out);

    // Number of frames run since core load (for the perf HUD).
    uint64_t framesRun() const { return framesRun_; }

    // Registers the libretro callbacks. Exposed for the test harness.
    void installCallbacks();

private:
    // libretro entry points resolved from the dynamic library.
    struct Api {
        void (*init)() = nullptr;
        void (*deinit)() = nullptr;
        unsigned (*apiVersion)() = nullptr;
        void (*getSystemInfo)(retro_system_info*) = nullptr;
        void (*getSystemAvInfo)(retro_system_av_info*) = nullptr;
        void (*setEnvironment)(retro_environment_t) = nullptr;
        void (*setVideoRefresh)(retro_video_refresh_t) = nullptr;
        void (*setAudioSample)(retro_audio_sample_t) = nullptr;
        void (*setAudioSampleBatch)(retro_audio_sample_batch_t) = nullptr;
        void (*setInputPoll)(retro_input_poll_t) = nullptr;
        void (*setInputState)(retro_input_state_t) = nullptr;
        void (*setControllerPortDevice)(unsigned, unsigned) = nullptr;
        void (*reset)() = nullptr;
        bool (*run)() = nullptr;
        bool (*loadGame)(const retro_game_info*) = nullptr;
        bool (*loadGameSpecial)(unsigned, const retro_game_info*, size_t) = nullptr;
        void (*unloadGame)() = nullptr;
        unsigned (*getRegion)() = nullptr;
        bool (*serialize)(void*, size_t) = nullptr;
        bool (*unserialize)(const void*, size_t) = nullptr;
        size_t (*serializeSize)() = nullptr;
        void* (*getMemoryData)(unsigned) = nullptr;
        size_t (*getMemorySize)(unsigned) = nullptr;
        void (*cheatReset)() = nullptr;
        void (*cheatSet)(unsigned, bool, const char*) = nullptr;
    };

    // C trampolines forwarding to g_active (set in loadCore).
    static bool environmentTrampoline(unsigned cmd, void* data);
    static void videoRefreshTrampoline(const void* data, unsigned w, unsigned h, size_t stride);
    static void audioSampleTrampoline(int16_t left, int16_t right);
    static size_t audioSampleBatchTrampoline(const int16_t* data, size_t frames);
    static void inputPollTrampoline();
    static int16_t inputStateTrampoline(unsigned port, unsigned device, unsigned index, unsigned id);

    // Individual environment command handlers.
    bool handleEnvironment(unsigned cmd, void* data);
    void applySystemAvInfo(const retro_system_av_info& info);

    Api api_;
    DynamicLibrary library_;
    std::filesystem::path corePath_;
    CoreDetails coreDetails_;
    AvInfo avInfo_;
    retro_pixel_format pixelFormat_ = RETRO_PIXEL_FORMAT_0RGB1555;

    VideoSink* videoSink_ = nullptr;
    AudioSink* audioSink_ = nullptr;
    InputSource* inputSource_ = nullptr;
    std::filesystem::path systemDirectory_;
    std::filesystem::path saveDirectory_;

    bool videoEnabled_ = true;
    bool audioEnabled_ = true;
    bool fastForwardActive_ = false;
    bool gameLoaded_ = false;
    bool paused_ = false;
    bool shutdownRequested_ = false;
    bool pixelFormatSet_ = false;
    bool variablesDirty_ = false;
    bool noGameSupported_ = false;
    uint64_t framesRun_ = 0;

    std::filesystem::path gamePath_;
    std::string gameTitle_;
    std::vector<uint8_t> gameData_; // kept alive for data-loading cores

    std::vector<CoreOption> coreOptions_;
    std::vector<std::pair<std::string, std::string>> optionOverrides_;

    // Conversion buffer for XRGB8888 frames.
    std::vector<uint32_t> frameBuffer_;
    // Single-sample audio shims: libretro delivers L,R through the pair
    // callback for some cores; forward as one stereo frame.
    int16_t pendingSample_[2] = {0, 0};
    size_t pendingSampleCount_ = 0;

    std::string coreMessage_;
};

} // namespace gecko
