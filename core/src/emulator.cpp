// SPDX-License-Identifier: MIT
#include "gecko/emulator.hpp"

#include "gecko/log.hpp"

#include <cstdarg>
#include <cstring>
#include <fstream>
#include <sstream>

namespace gecko {

// ---------------------------------------------------------------------------
// libretro delivers callbacks through plain C function pointers without a
// user-data argument, so exactly one Emulator can be "active" at a time. This
// matches how every real frontend (RetroArch included) drives a core, and the
// desktop/Android apps only ever run one core per process.
static Emulator* g_active = nullptr;

// Constants introduced in later libretro.h revisions; this vendored header
// (2020) predates them.
#ifndef RETRO_AV_ENABLE_VIDEO
#define RETRO_AV_ENABLE_VIDEO (1 << 0)
#endif
#ifndef RETRO_AV_ENABLE_AUDIO
#define RETRO_AV_ENABLE_AUDIO (1 << 1)
#endif
#ifndef RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY
#define RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY (64 | RETRO_ENVIRONMENT_EXPERIMENTAL)
#endif

namespace {

std::string trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Forwarded to cores via RETRO_ENVIRONMENT_GET_LOG_INTERFACE. Must be a real
// function: GCC cannot convert a variadic lambda to a C function pointer.
void retroLogPrintf(enum retro_log_level level, const char* fmt, ...) {
    LogLevel mapped = LogLevel::Info;
    switch (level) {
        case RETRO_LOG_DEBUG: mapped = LogLevel::Debug; break;
        case RETRO_LOG_INFO: mapped = LogLevel::Info; break;
        case RETRO_LOG_WARN: mapped = LogLevel::Warning; break;
        case RETRO_LOG_ERROR: mapped = LogLevel::Error; break;
        default: mapped = LogLevel::Info; break;
    }
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    logMessage(mapped, "%s", buffer);
}

} // namespace

// ---------------------------------------------------------------------------
Emulator::Emulator() = default;

Emulator::~Emulator() {
    if (g_active == this) {
        unloadGame();
        unloadCore();
        g_active = nullptr;
    }
}

// --- core lifecycle ---------------------------------------------------------
bool Emulator::loadCore(const std::filesystem::path& corePath, std::string* error) {
    if (coreLoaded()) {
        if (error) *error = "a core is already loaded; call unloadCore() first";
        return false;
    }
    if (g_active != nullptr && g_active != this) {
        if (error) *error = "another Emulator instance is active in this process";
        return false;
    }

    std::string openError;
    if (!library_.open(corePath, &openError)) {
        if (error) *error = "cannot open core: " + openError;
        return false;
    }

    api_.apiVersion = library_.symbolAs<unsigned (*)()>("retro_api_version");
    api_.getSystemInfo = library_.symbolAs<void (*)(retro_system_info*)>("retro_get_system_info");
    api_.getSystemAvInfo =
        library_.symbolAs<void (*)(retro_system_av_info*)>("retro_get_system_av_info");
    api_.init = library_.symbolAs<void (*)()>("retro_init");
    api_.deinit = library_.symbolAs<void (*)()>("retro_deinit");
    api_.setEnvironment = library_.symbolAs<void (*)(retro_environment_t)>("retro_set_environment");
    api_.setVideoRefresh =
        library_.symbolAs<void (*)(retro_video_refresh_t)>("retro_set_video_refresh");
    api_.setAudioSample =
        library_.symbolAs<void (*)(retro_audio_sample_t)>("retro_set_audio_sample");
    api_.setAudioSampleBatch =
        library_.symbolAs<void (*)(retro_audio_sample_batch_t)>("retro_set_audio_sample_batch");
    api_.setInputPoll = library_.symbolAs<void (*)(retro_input_poll_t)>("retro_set_input_poll");
    api_.setInputState =
        library_.symbolAs<void (*)(retro_input_state_t)>("retro_set_input_state");
    api_.setControllerPortDevice =
        library_.symbolAs<void (*)(unsigned, unsigned)>("retro_set_controller_port_device");
    api_.reset = library_.symbolAs<void (*)()>("retro_reset");
    api_.run = library_.symbolAs<bool (*)()>("retro_run");
    api_.loadGame = library_.symbolAs<bool (*)(const retro_game_info*)>("retro_load_game");
    api_.loadGameSpecial = library_.symbolAs<bool (*)(unsigned, const retro_game_info*, size_t)>(
        "retro_load_game_special");
    api_.unloadGame = library_.symbolAs<void (*)()>("retro_unload_game");
    api_.getRegion = library_.symbolAs<unsigned (*)()>("retro_get_region");
    api_.serializeSize = library_.symbolAs<size_t (*)()>("retro_serialize_size");
    api_.serialize = library_.symbolAs<bool (*)(void*, size_t)>("retro_serialize");
    api_.unserialize = library_.symbolAs<bool (*)(const void*, size_t)>("retro_unserialize");
    api_.getMemoryData = library_.symbolAs<void* (*)(unsigned)>("retro_get_memory_data");
    api_.getMemorySize = library_.symbolAs<size_t (*)(unsigned)>("retro_get_memory_size");

    if (!api_.run || !api_.loadGame || !api_.getSystemInfo) {
        library_.close();
        api_ = Api{};
        if (error) *error = corePath.string() + " is not a libretro core (missing symbols)";
        return false;
    }

    if (api_.apiVersion && api_.apiVersion() != RETRO_API_VERSION) {
        const unsigned abi = api_.apiVersion();
        library_.close();
        api_ = Api{};
        if (error)
            *error = "core ABI version " + std::to_string(abi) + " does not match frontend ABI " +
                     std::to_string(RETRO_API_VERSION);
        return false;
    }

    corePath_ = corePath;
    g_active = this;

    // Identity.
    retro_system_info info{};
    api_.getSystemInfo(&info);
    coreDetails_.libraryName = info.library_name ? info.library_name : corePath.stem().string();
    coreDetails_.libraryVersion = info.library_version ? info.library_version : "";
    coreDetails_.validExtensions = info.valid_extensions ? info.valid_extensions : "";
    coreDetails_.needFullPath = info.need_fullpath;
    coreDetails_.blockExtract = info.block_extract;

    // State.
    coreOptions_.clear();
    pixelFormat_ = RETRO_PIXEL_FORMAT_0RGB1555;
    pixelFormatSet_ = false;
    variablesDirty_ = false;
    noGameSupported_ = false;
    shutdownRequested_ = false;
    framesRun_ = 0;
    coreMessage_.clear();
    avInfo_ = AvInfo{};

    installCallbacks();

    if (api_.init) api_.init();

    logMessage(LogLevel::Info, "loaded core '%s' %s (%s)", coreDetails_.libraryName.c_str(),
               coreDetails_.libraryVersion.c_str(), corePath.string().c_str());
    return true;
}

void Emulator::installCallbacks() {
    if (api_.setEnvironment) api_.setEnvironment(&Emulator::environmentTrampoline);
    if (api_.setVideoRefresh) api_.setVideoRefresh(&Emulator::videoRefreshTrampoline);
    if (api_.setAudioSample) api_.setAudioSample(&Emulator::audioSampleTrampoline);
    if (api_.setAudioSampleBatch) api_.setAudioSampleBatch(&Emulator::audioSampleBatchTrampoline);
    if (api_.setInputPoll) api_.setInputPoll(&Emulator::inputPollTrampoline);
    if (api_.setInputState) api_.setInputState(&Emulator::inputStateTrampoline);
}

void Emulator::unloadCore() {
    if (!coreLoaded()) return;
    unloadGame();
    if (api_.deinit) api_.deinit();
    // The library is intentionally kept mapped (libretro cores are not
    // reliably dlclose-safe). DynamicLibrary::close() is therefore NOT called
    // here; repeated loads of the same core simply re-open the same mapping.
    api_ = Api{};
    coreDetails_ = CoreDetails{};
    coreOptions_.clear();
    corePath_.clear();
    if (g_active == this) g_active = nullptr;
}

// --- content ------------------------------------------------------------------
bool Emulator::loadGame(const std::filesystem::path& romPath, std::string* error) {
    if (!coreLoaded()) {
        if (error) *error = "no core loaded";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(romPath, ec)) {
        if (error) *error = "content not found: " + romPath.string();
        return false;
    }

    const std::string pathString = romPath.string();
    retro_game_info game{};
    game.path = pathString.c_str();

    // Load the file unless the core requested the full path.
    if (!coreDetails_.needFullPath) {
        std::ifstream in(romPath, std::ios::binary);
        if (in) {
            in.seekg(0, std::ios::end);
            const std::streamoff size = in.tellg();
            in.seekg(0, std::ios::beg);
            if (size > 0) {
                gameData_.resize(static_cast<size_t>(size));
                in.read(reinterpret_cast<char*>(gameData_.data()), size);
                game.data = gameData_.data();
                game.size = gameData_.size();
            }
        } else {
            logMessage(LogLevel::Warning, "cannot read %s; passing path only",
                       romPath.string().c_str());
        }
    }

    if (!api_.loadGame(&game)) {
        gameData_.clear();
        if (error) *error = "core rejected the content";
        return false;
    }

    gameLoaded_ = true;
    gamePath_ = romPath;
    gameTitle_ = romPath.stem().string();
    framesRun_ = 0;
    shutdownRequested_ = false;

    retro_system_av_info av{};
    api_.getSystemAvInfo(&av);
    applySystemAvInfo(av);

    // Classic joypad on port 0 (what every core understands).
    if (api_.setControllerPortDevice) api_.setControllerPortDevice(0, RETRO_DEVICE_JOYPAD);

    // SRAM restore.
    if (sramAvailable()) {
        const std::filesystem::path sramPath =
            saveDirectory_.empty() ? romPath.parent_path() / (gameTitle_ + ".srm")
                                   : saveDirectory_ / (gameTitle_ + ".srm");
        if (std::filesystem::exists(sramPath, ec)) loadSramFromFile(sramPath);
    }

    logMessage(LogLevel::Info, "content '%s' loaded (%ux%u @ %.3f fps)", gameTitle_.c_str(),
               avInfo_.baseWidth, avInfo_.baseHeight, avInfo_.fps);
    return true;
}

bool Emulator::startWithoutContent(std::string* error) {
    if (!coreLoaded()) {
        if (error) *error = "no core loaded";
        return false;
    }
    if (!noGameSupported_) {
        if (error) *error = "this core requires content";
        return false;
    }
    if (!api_.loadGame(nullptr)) {
        if (error) *error = "core refused to start without content";
        return false;
    }
    gameLoaded_ = true;
    gamePath_.clear();
    gameTitle_ = coreDetails_.libraryName;
    gameData_.clear();
    framesRun_ = 0;

    retro_system_av_info av{};
    api_.getSystemAvInfo(&av);
    applySystemAvInfo(av);
    if (api_.setControllerPortDevice) api_.setControllerPortDevice(0, RETRO_DEVICE_JOYPAD);
    return true;
}

void Emulator::unloadGame() {
    if (!gameLoaded_) return;
    if (sramAvailable() && !saveDirectory_.empty() && !gameTitle_.empty()) {
        saveSramToFile(saveDirectory_ / (gameTitle_ + ".srm"));
    }
    if (api_.unloadGame) api_.unloadGame();
    gameLoaded_ = false;
    gamePath_.clear();
    gameTitle_.clear();
    gameData_.clear();
    if (audioSink_) audioSink_->flush();
}

std::string Emulator::gameTitle() const {
    if (!gameTitle_.empty()) return gameTitle_;
    return coreDetails_.libraryName;
}

// --- running -------------------------------------------------------------------
void Emulator::runFrame() {
    if (!coreLoaded() || !gameLoaded_ || paused_) return;
    if (api_.run) api_.run();
    ++framesRun_;
}

void Emulator::reset() {
    if (api_.reset) api_.reset();
    shutdownRequested_ = false;
}

// --- save states -----------------------------------------------------------------
size_t Emulator::serializeSize() {
    if (!api_.serializeSize) return 0;
    return api_.serializeSize();
}

bool Emulator::saveStateToFile(const std::filesystem::path& path) {
    const size_t size = serializeSize();
    if (size == 0) {
        logMessage(LogLevel::Warning, "core does not support save states");
        return false;
    }
    std::vector<uint8_t> buffer(size);
    if (!api_.serialize(buffer.data(), size)) return false;

    std::error_code ec;
    const auto dir = path.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir, ec))
        std::filesystem::create_directories(dir, ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(size));
    return out.good();
}

bool Emulator::loadStateFromFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (size <= 0) return false;
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(buffer.data()), size);
    return api_.unserialize(buffer.data(), buffer.size());
}

// --- SRAM ------------------------------------------------------------------------
bool Emulator::sramAvailable() const {
    if (!api_.getMemoryData || !api_.getMemorySize) return false;
    return api_.getMemorySize(RETRO_MEMORY_SAVE_RAM) > 0;
}

bool Emulator::saveSramToFile(const std::filesystem::path& path) {
    if (!sramAvailable()) return false;
    void* data = api_.getMemoryData(RETRO_MEMORY_SAVE_RAM);
    const size_t size = api_.getMemorySize(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0) return false;

    std::error_code ec;
    const auto dir = path.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir, ec))
        std::filesystem::create_directories(dir, ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return out.good();
}

bool Emulator::loadSramFromFile(const std::filesystem::path& path) {
    if (!sramAvailable()) return false;
    void* data = api_.getMemoryData(RETRO_MEMORY_SAVE_RAM);
    const size_t size = api_.getMemorySize(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0) return false;

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
    return true;
}

// --- core options -----------------------------------------------------------------
std::string Emulator::coreOptionValue(const std::string& key) const {
    for (const auto& overridePair : optionOverrides_)
        if (overridePair.first == key) return overridePair.second;
    for (const auto& option : coreOptions_)
        if (option.key == key) return option.defaultValue;
    return {};
}

void Emulator::setCoreOptionValue(const std::string& key, const std::string& value) {
    for (auto& overridePair : optionOverrides_) {
        if (overridePair.first == key) {
            overridePair.second = value;
            variablesDirty_ = true;
            return;
        }
    }
    optionOverrides_.emplace_back(key, value);
    variablesDirty_ = true;
}

// --- messages ---------------------------------------------------------------------
bool Emulator::takeCoreMessage(std::string* out) {
    if (coreMessage_.empty()) return false;
    if (out) out->swap(coreMessage_);
    coreMessage_.clear();
    return true;
}

void Emulator::applySystemAvInfo(const retro_system_av_info& info) {
    avInfo_.fps = info.timing.fps > 0.0 ? info.timing.fps : 60.0;
    avInfo_.sampleRate = info.timing.sample_rate;
    avInfo_.baseWidth = info.geometry.base_width;
    avInfo_.baseHeight = info.geometry.base_height;
    avInfo_.maxWidth = info.geometry.max_width;
    avInfo_.maxHeight = info.geometry.max_height;
    avInfo_.aspectRatio =
        info.geometry.aspect_ratio > 0.0
            ? info.geometry.aspect_ratio
            : static_cast<double>(info.geometry.base_width) /
                  (info.geometry.base_height ? info.geometry.base_height : 1);
    if (audioSink_ && avInfo_.sampleRate > 0.0) audioSink_->setFormat(avInfo_.sampleRate);
}

// ---------------------------------------------------------------------------
// C trampolines
// ---------------------------------------------------------------------------
bool Emulator::environmentTrampoline(unsigned cmd, void* data) {
    return g_active ? g_active->handleEnvironment(cmd, data) : false;
}

void Emulator::videoRefreshTrampoline(const void* data, unsigned w, unsigned h, size_t stride) {
    if (!g_active) return;
    Emulator* self = g_active;
    if (!self->videoSink_ || !self->videoEnabled_) return;

    if (!data) {
        self->videoSink_->duplicateFrame();
        return;
    }

    const size_t pixelCount = static_cast<size_t>(w) * h;
    if (self->frameBuffer_.size() < pixelCount) self->frameBuffer_.resize(pixelCount);

    if (!convertToXrgb8888(data, self->pixelFormat_, stride, w, h, self->frameBuffer_.data())) {
        logMessage(LogLevel::Warning, "unsupported pixel format %d, frame dropped",
                   static_cast<int>(self->pixelFormat_));
        return;
    }

    VideoFrame frame;
    frame.width = w;
    frame.height = h;
    frame.stride = static_cast<size_t>(w) * 4;
    frame.format = RETRO_PIXEL_FORMAT_XRGB8888;
    frame.pixels = self->frameBuffer_.data();
    self->videoSink_->frame(frame);
}

void Emulator::audioSampleTrampoline(int16_t left, int16_t right) {
    if (!g_active) return;
    Emulator* self = g_active;
    const int16_t pair[2] = {left, right};
    if (self->audioSink_ && self->audioEnabled_) self->audioSink_->write(pair, 1);
}

size_t Emulator::audioSampleBatchTrampoline(const int16_t* data, size_t frames) {
    if (!g_active || !data || frames == 0) return 0;
    Emulator* self = g_active;
    if (!self->audioSink_ || !self->audioEnabled_ || self->avInfo_.sampleRate <= 0.0) return frames;
    return self->audioSink_->write(data, frames);
}

void Emulator::inputPollTrampoline() {
    if (g_active && g_active->inputSource_) g_active->inputSource_->poll();
}

int16_t Emulator::inputStateTrampoline(unsigned port, unsigned device, unsigned index,
                                        unsigned id) {
    if (!g_active || !g_active->inputSource_) return 0;
    return g_active->inputSource_->state(port, device, index, id);
}

// ---------------------------------------------------------------------------
// Environment command handling
// ---------------------------------------------------------------------------
bool Emulator::handleEnvironment(unsigned cmd, void* data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_OVERSCAN:
            *static_cast<bool*>(data) = true;
            return true;
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *static_cast<bool*>(data) = true;
            return true;
        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            const auto* msg = static_cast<const struct retro_message*>(data);
            if (msg && msg->msg) coreMessage_ = msg->msg;
            return true;
        }
        case RETRO_ENVIRONMENT_SHUTDOWN:
            shutdownRequested_ = true;
            return true;
        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
            return true; // informational only
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
            static std::string systemDir; // must outlive the call
            systemDir = systemDirectory_.string();
            *static_cast<const char**>(data) = systemDir.c_str();
            return true;
        }
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            static std::string saveDir; // must outlive the call
            saveDir = saveDirectory_.string();
            *static_cast<const char**>(data) = saveDir.c_str();
            return true;
        }
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            const auto fmt = *static_cast<const enum retro_pixel_format*>(data);
            if (fmt < RETRO_PIXEL_FORMAT_0RGB1555 || fmt > RETRO_PIXEL_FORMAT_XRGB8888) return false;
            pixelFormat_ = fmt;
            pixelFormatSet_ = true;
            logMessage(LogLevel::Debug, "core pixel format: %d", static_cast<int>(fmt));
            return true;
        }
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
            return true; // accepted, not surfaced in the UI yet
        case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
            return true; // accepted, not surfaced in the UI yet
        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
            return false; // no disk control UI yet
        case RETRO_ENVIRONMENT_SET_HW_RENDER:
        case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
            return false; // software rendering only
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            auto* variable = static_cast<struct retro_variable*>(data);
            if (!variable || !variable->key) return false;
            const std::string value = coreOptionValue(variable->key);
            if (value.empty()) {
                variable->value = nullptr;
                return false;
            }
            // The value string must outlive the call; the core consumes it
            // synchronously, so one static slot is enough (single emu thread).
            static std::string lastValue;
            lastValue = value;
            variable->value = lastValue.c_str();
            return true;
        }
        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            // Legacy definitions: {"key", "Description; value1|value2|..."}
            // where the FIRST value is the default.
            const auto* vars = static_cast<const struct retro_variable*>(data);
            coreOptions_.clear();
            for (; vars && vars->key; ++vars) {
                CoreOption option;
                option.key = vars->key;
                const std::string spec = vars->value ? vars->value : "";
                const size_t firstSemi = spec.find(';');
                option.description =
                    trim(firstSemi == std::string::npos ? spec : spec.substr(0, firstSemi));
                if (firstSemi != std::string::npos) {
                    std::string choice;
                    std::istringstream stream(spec.substr(firstSemi + 1));
                    while (std::getline(stream, choice, '|')) {
                        choice = trim(choice);
                        if (choice.empty()) continue;
                        CoreOption::Choice c;
                        c.value = choice;
                        c.label = choice;
                        option.choices.push_back(std::move(c));
                    }
                }
                if (option.choices.empty()) {
                    option.choices.push_back({"enabled", "enabled"});
                    option.choices.push_back({"disabled", "disabled"});
                }
                option.defaultValue = option.choices.front().value;
                // Reapply any user override that is still valid.
                for (const auto& o : optionOverrides_) {
                    if (o.first != option.key) continue;
                    for (const auto& c : option.choices) {
                        if (c.value == o.second) {
                            option.defaultValue = o.second;
                            break;
                        }
                    }
                }
                coreOptions_.push_back(std::move(option));
            }
            variablesDirty_ = true;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *static_cast<bool*>(data) = variablesDirty_;
            if (variablesDirty_) variablesDirty_ = false;
            return true;
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
            noGameSupported_ = *static_cast<const bool*>(data);
            return true;
        case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH: {
            static std::string corePathString;
            corePathString = corePath_.string();
            *static_cast<const char**>(data) = corePathString.c_str();
            return true;
        }
        case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
        case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
        case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
        case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
        case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
        case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
        case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
        case RETRO_ENVIRONMENT_GET_MIDI_INTERFACE:
            return false;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            auto* cb = static_cast<struct retro_log_callback*>(data);
            cb->log = &retroLogPrintf;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY: {
            static std::string assetsDir;
            assetsDir = (systemDirectory_ / "assets").string();
            *static_cast<const char**>(data) = assetsDir.c_str();
            return true;
        }
        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
            const auto* info = static_cast<const struct retro_system_av_info*>(data);
            if (!info) return false;
            applySystemAvInfo(*info);
            return true;
        }
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
            return true; // accepted, not surfaced in the UI yet
        case RETRO_ENVIRONMENT_SET_GEOMETRY: {
            const auto* geo = static_cast<const struct retro_game_geometry*>(data);
            if (!geo) return false;
            avInfo_.baseWidth = geo->base_width;
            avInfo_.baseHeight = geo->base_height;
            if (geo->aspect_ratio > 0.0) avInfo_.aspectRatio = geo->aspect_ratio;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_USERNAME: {
            static const char* username = "GeckoUser";
            *static_cast<const char**>(data) = username;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_LANGUAGE:
            *static_cast<unsigned*>(data) = RETRO_LANGUAGE_ENGLISH;
            return true;
        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
            return true;
        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
            int flags = 0;
            if (videoEnabled_) flags |= RETRO_AV_ENABLE_VIDEO;
            if (audioEnabled_) flags |= RETRO_AV_ENABLE_AUDIO;
            *static_cast<int*>(data) = flags;
            return true;
        }
        case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
            return true;
        case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
            *static_cast<bool*>(data) = fastForwardActive_;
            return true;
        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            return true; // our input state supports RETRO_DEVICE_ID_JOYPAD_MASK
        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
            *static_cast<unsigned*>(data) = 1; // retro_core_option_definition
            return true;
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
            const auto* defs = static_cast<const struct retro_core_option_definition*>(data);
            coreOptions_.clear();
            for (; defs && defs->key; ++defs) {
                CoreOption option;
                option.key = defs->key;
                option.description = defs->desc ? defs->desc : defs->key;
                option.info = defs->info ? defs->info : "";
                for (const auto& value : defs->values) {
                    if (!value.value) break;
                    CoreOption::Choice c;
                    c.value = value.value;
                    c.label = value.label ? value.label : value.value;
                    option.choices.push_back(std::move(c));
                }
                option.defaultValue = defs->default_value ? defs->default_value : "";
                if (option.defaultValue.empty() && !option.choices.empty())
                    option.defaultValue = option.choices.front().value;
                for (const auto& o : optionOverrides_) {
                    if (o.first != option.key) continue;
                    for (const auto& c : option.choices) {
                        if (c.value == o.second) {
                            option.defaultValue = o.second;
                            break;
                        }
                    }
                }
                coreOptions_.push_back(std::move(option));
            }
            variablesDirty_ = true;
            return true;
        }
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
            const auto* intl = static_cast<const struct retro_core_options_intl*>(data);
            if (!intl || !intl->us) return false;
            const auto* defs = intl->local ? intl->local : intl->us;
            coreOptions_.clear();
            for (; defs && defs->key; ++defs) {
                CoreOption option;
                option.key = defs->key;
                option.description = defs->desc ? defs->desc : defs->key;
                option.info = defs->info ? defs->info : "";
                for (const auto& value : defs->values) {
                    if (!value.value) break;
                    CoreOption::Choice c;
                    c.value = value.value;
                    c.label = value.label ? value.label : value.value;
                    option.choices.push_back(std::move(c));
                }
                option.defaultValue = defs->default_value ? defs->default_value : "";
                if (option.defaultValue.empty() && !option.choices.empty())
                    option.defaultValue = option.choices.front().value;
                for (const auto& o : optionOverrides_) {
                    if (o.first != option.key) continue;
                    for (const auto& c : option.choices) {
                        if (c.value == o.second) {
                            option.defaultValue = o.second;
                            break;
                        }
                    }
                }
                coreOptions_.push_back(std::move(option));
            }
            variablesDirty_ = true;
            return true;
        }
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
            const auto* display = static_cast<const struct retro_core_option_display*>(data);
            if (!display) return false;
            for (auto& option : coreOptions_)
                if (option.key == display->key) option.visible = display->visible;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
            *static_cast<float*>(data) = 60.0f;
            return true;
        default:
            if (cmd & RETRO_ENVIRONMENT_PRIVATE) return false;
            logMessage(LogLevel::Debug, "unhandled environment cmd 0x%x", cmd);
            return false;
    }
}

} // namespace gecko
